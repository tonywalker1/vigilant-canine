//
// vigilant-canine - Alert Dispatcher Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <dispatch/alert_dispatcher.h>

#include <distro/detector.h>
#include <notify/dbus_notifier.h>

#include <systemd/sd-journal.h>

#include <format>
#include <iostream>

namespace vigilant_canine {

    AlertDispatcher::AlertDispatcher(EventBus& event_bus,
                                      AlertStore& alert_store,
                                      BaselineStore& baseline_store,
                                      AlertDispatcherConfig config,
                                      DbusNotifier* dbus_notifier)
        : m_event_bus(event_bus),
          m_alert_store(alert_store),
          m_baseline_store(baseline_store),
          m_config(config),
          m_dbus_notifier(dbus_notifier) {}

    AlertDispatcher::~AlertDispatcher() {
        stop();
    }

    AlertDispatcher::AlertDispatcher(AlertDispatcher&& other) noexcept
        : m_event_bus(other.m_event_bus),
          m_alert_store(other.m_alert_store),
          m_baseline_store(other.m_baseline_store),
          m_config(other.m_config),
          m_dbus_notifier(other.m_dbus_notifier),
          m_running(other.m_running),
          m_subscription(std::move(other.m_subscription)) {
        other.m_running = false;
        other.m_dbus_notifier = nullptr;
    }

    AlertDispatcher& AlertDispatcher::operator=(AlertDispatcher&& other) noexcept {
        if (this != &other) {
            stop();
            m_config = other.m_config;
            m_dbus_notifier = other.m_dbus_notifier;
            m_running = other.m_running;
            m_subscription = std::move(other.m_subscription);
            other.m_running = false;
            other.m_dbus_notifier = nullptr;
        }
        return *this;
    }

    auto AlertDispatcher::start() -> std::expected<void, std::string> {
        if (m_running) {
            return std::unexpected("AlertDispatcher already running");
        }

        // Subscribe to all events
        m_subscription = std::make_unique<EventSubscription>(
            m_event_bus.subscribe([this](Event const& event) {
                handle_event(event);
            })
        );

        m_running = true;
        return {};
    }

    void AlertDispatcher::stop() {
        if (!m_running) {
            return;
        }

        m_subscription.reset();
        m_running = false;
    }

    void AlertDispatcher::handle_event(Event const& event) {
        // Convert event to alert
        auto alert = event_to_alert(event);

        // Store in database
        auto insert_result = m_alert_store.insert(alert);
        if (!insert_result) {
            std::cerr << "Failed to store alert: " << insert_result.error() << "\n";
            return;
        }

        alert.id = *insert_result;

        // Log to systemd journal
        if (m_config.log_to_journal) {
            log_to_journal(alert);
        }

        // Send D-Bus notification
        if (m_config.send_dbus) {
            send_dbus_notification(alert);
        }
    }

    auto AlertDispatcher::event_to_alert(Event const& event) -> Alert {
        Alert alert;

        // Map severity
        switch (event.severity) {
            case EventSeverity::info:
                alert.severity = AlertSeverity::INFO;
                break;
            case EventSeverity::warning:
                alert.severity = AlertSeverity::WARNING;
                break;
            case EventSeverity::critical:
                alert.severity = AlertSeverity::CRITICAL;
                break;
        }

        alert.source = event.source;

        // Extract event-specific details
        std::visit([&alert, this](auto const& event_data) {
            using T = std::decay_t<decltype(event_data)>;

            if constexpr (std::is_same_v<T, FileModifiedEvent>) {
                alert.category = "file_modified";
                alert.path = event_data.path;

                // Check if this is a user file by querying the baseline
                auto baseline_result = m_baseline_store.find_by_path(event_data.path, std::nullopt);
                std::string username;
                if (baseline_result && baseline_result->has_value()) {
                    auto const& baseline = baseline_result->value();
                    if (baseline.source.starts_with("user:")) {
                        username = baseline.source.substr(5);  // Extract username
                        alert.summary = std::format("User {} file modified: {}",
                                                     username,
                                                     (*event_data.path).string());
                    } else {
                        alert.summary = std::format("File modified: {}",
                                                     (*event_data.path).string());
                    }
                } else {
                    alert.summary = std::format("File modified: {}",
                                                 (*event_data.path).string());
                }

                alert.details = std::format("Old hash: {}\nNew hash: {}\n{}",
                                             *event_data.old_hash,
                                             *event_data.new_hash,
                                             event_data.change_description);
                if (!username.empty()) {
                    alert.details = std::format("{}\nUsername: {}", alert.details.value_or(""), username);
                }
            } else if constexpr (std::is_same_v<T, FileCreatedEvent>) {
                alert.category = "file_created";
                alert.path = event_data.path;

                // Check if this is a user file
                std::string username;
                if (event_data.source && (*event_data.source).starts_with("user:")) {
                    username = (*event_data.source).substr(5);
                    alert.summary = std::format("User {} new file detected: {}",
                                                 username,
                                                 (*event_data.path).string());
                } else {
                    alert.summary = std::format("New file detected: {}",
                                                 (*event_data.path).string());
                }

                if (event_data.source) {
                    alert.details = std::format("Source: {}\nHash: {}",
                                                 *event_data.source,
                                                 *event_data.hash);
                } else {
                    alert.details = std::format("Hash: {}", *event_data.hash);
                }
                if (!username.empty()) {
                    alert.details = std::format("{}\nUsername: {}", alert.details.value_or(""), username);
                }
            } else if constexpr (std::is_same_v<T, FileDeletedEvent>) {
                alert.category = "file_deleted";
                alert.path = event_data.path;
                alert.summary = std::format("File deleted: {}",
                                             (*event_data.path).string());
                alert.details = std::format("Last known hash: {}",
                                             *event_data.last_known_hash);
            } else if constexpr (std::is_same_v<T, FilePermissionChangedEvent>) {
                alert.category = "permission_changed";
                alert.path = event_data.path;
                alert.summary = std::format("File permissions changed: {}",
                                             (*event_data.path).string());
                alert.details = std::format("Old mode: {:o}\nNew mode: {:o}",
                                             event_data.old_mode,
                                             event_data.new_mode);
            } else if constexpr (std::is_same_v<T, ScanCompletedEvent>) {
                alert.category = "scan_completed";
                alert.summary = std::format("Scan completed: {}",
                                             event_data.scan_path.string());
                auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    event_data.elapsed).count();
                alert.details = std::format("Files scanned: {}\nChanges: {}\nElapsed: {}ms",
                                             event_data.files_scanned,
                                             event_data.changes_detected,
                                             elapsed_ms);
            } else if constexpr (std::is_same_v<T, SystemStartupEvent>) {
                alert.category = "system_startup";
                alert.summary = std::format("System startup: {}",
                                             event_data.distro_name);
                alert.details = std::format("Distribution type: {}",
                                             to_string(event_data.distro_type));
            }
            // Phase 2 events
            else if constexpr (std::is_same_v<T, AuthFailureEvent>) {
                alert.category = "auth_failure";
                alert.summary = std::format("Authentication failure: {} on {}",
                                             event_data.username,
                                             event_data.service);
                alert.details = event_data.message;
                if (event_data.remote_host) {
                    alert.details = std::format("{}\nRemote host: {}",
                                                 alert.details.value_or(""),
                                                 *event_data.remote_host);
                }
            } else if constexpr (std::is_same_v<T, PrivilegeEscalationEvent>) {
                alert.category = "privilege_escalation";
                alert.summary = std::format("Privilege escalation: {} -> {} via {}",
                                             event_data.username,
                                             event_data.target_user,
                                             event_data.method);
                alert.details = std::format("Command: {}\n{}",
                                             event_data.command,
                                             event_data.message);
            } else if constexpr (std::is_same_v<T, ServiceStateEvent>) {
                alert.category = "service_state";
                alert.summary = std::format("Service {}: {}",
                                             event_data.unit_name,
                                             event_data.new_state);
                alert.details = event_data.message;
                if (event_data.exit_code) {
                    alert.details = std::format("{}\nExit code: {}",
                                                 alert.details.value_or(""),
                                                 *event_data.exit_code);
                }
            } else if constexpr (std::is_same_v<T, SuspiciousLogEvent>) {
                alert.category = "suspicious_log";
                alert.summary = std::format("Suspicious log entry (rule: {})",
                                             event_data.rule_name);
                alert.details = std::format("Unit: {}\nPriority: {}\nMessage: {}",
                                             event_data.unit_name,
                                             event_data.priority,
                                             event_data.message);
            }
            // Phase 3 events
            else if constexpr (std::is_same_v<T, ProcessExecutionEvent>) {
                alert.category = "process_execution";
                alert.summary = std::format("Process executed: {} by {}",
                                             (*event_data.exe_path).string(),
                                             event_data.username);
                alert.path = event_data.exe_path;
                std::string details_str = std::format("PID: {}\nPPID: {}\nUID: {}\nCommand: {}",
                                                       event_data.pid,
                                                       event_data.ppid,
                                                       event_data.uid,
                                                       event_data.command_line);
                if (event_data.cwd) {
                    details_str += std::format("\nCWD: {}", *event_data.cwd);
                }
                alert.details = details_str;
            } else if constexpr (std::is_same_v<T, NetworkConnectionEvent>) {
                alert.category = "network_connection";
                alert.summary = std::format("Network connection by {} ({})",
                                             event_data.username,
                                             event_data.protocol);
                alert.details = std::format("PID: {}\nUID: {}\nProtocol: {}\nLocal: {}:{}\nRemote: {}:{}",
                                             event_data.pid,
                                             event_data.uid,
                                             event_data.protocol,
                                             event_data.local_addr,
                                             event_data.local_port,
                                             event_data.remote_addr,
                                             event_data.remote_port);
            } else if constexpr (std::is_same_v<T, FailedAccessEvent>) {
                alert.category = "failed_access";
                alert.summary = std::format("Failed {} access to {} by {}",
                                             event_data.access_type,
                                             (*event_data.path).string(),
                                             event_data.username);
                alert.path = event_data.path;
                alert.details = std::format("PID: {}\nUID: {}\nError: {} ({})",
                                             event_data.pid,
                                             event_data.uid,
                                             event_data.error_message,
                                             event_data.error_code);
            } else if constexpr (std::is_same_v<T, PrivilegeChangeEvent>) {
                alert.category = "privilege_change";
                alert.summary = std::format("Privilege change: {} → {}",
                                             event_data.old_username,
                                             event_data.new_username);
                alert.details = std::format("PID: {}\nOld UID: {} ({})\nNew UID: {} ({})\nOperation: {}",
                                             event_data.pid,
                                             event_data.old_uid,
                                             event_data.old_username,
                                             event_data.new_uid,
                                             event_data.new_username,
                                             event_data.operation);
            }
        }, event.data);

        return alert;
    }

    void AlertDispatcher::log_to_journal(Alert const& alert) {
        // Use sd_journal_send to log to systemd journal
        // This ensures alerts are always logged even without optional components
        sd_journal_send(
            "MESSAGE=%s", alert.summary.c_str(),
            "PRIORITY=%d", alert.severity == AlertSeverity::CRITICAL ? 2 :  // LOG_CRIT
                           alert.severity == AlertSeverity::WARNING ? 4 :   // LOG_WARNING
                           6,  // LOG_INFO
            "VC_ALERT_ID=%ld", alert.id,
            "VC_CATEGORY=%s", alert.category.c_str(),
            "VC_SOURCE=%s", alert.source.c_str(),
            alert.path ? "VC_PATH=%s" : nullptr,
            alert.path ? (**alert.path).string().c_str() : nullptr,
            alert.details ? "VC_DETAILS=%s" : nullptr,
            alert.details ? alert.details->c_str() : nullptr,
            nullptr
        );
    }

    void AlertDispatcher::send_dbus_notification(Alert const& alert) {
        // Phase 2: Use DbusNotifier if available
        if (m_dbus_notifier && m_dbus_notifier->is_available()) {
            m_dbus_notifier->notify(alert);
        }
    }

}  // namespace vigilant_canine
