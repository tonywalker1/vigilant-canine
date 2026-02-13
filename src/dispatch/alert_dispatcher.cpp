//
// vigilant-canine - Alert Dispatcher Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <dispatch/alert_dispatcher.h>

#include <distro/detector.h>

#include <systemd/sd-journal.h>

#include <format>
#include <iostream>

namespace vigilant_canine {

    AlertDispatcher::AlertDispatcher(EventBus& event_bus,
                                      AlertStore& alert_store,
                                      AlertDispatcherConfig config)
        : m_event_bus(event_bus), m_alert_store(alert_store), m_config(config) {}

    AlertDispatcher::~AlertDispatcher() {
        stop();
    }

    AlertDispatcher::AlertDispatcher(AlertDispatcher&& other) noexcept
        : m_event_bus(other.m_event_bus),
          m_alert_store(other.m_alert_store),
          m_config(other.m_config),
          m_running(other.m_running),
          m_subscription(std::move(other.m_subscription)) {
        other.m_running = false;
    }

    AlertDispatcher& AlertDispatcher::operator=(AlertDispatcher&& other) noexcept {
        if (this != &other) {
            stop();
            m_config = other.m_config;
            m_running = other.m_running;
            m_subscription = std::move(other.m_subscription);
            other.m_running = false;
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
        std::visit([&alert](auto const& event_data) {
            using T = std::decay_t<decltype(event_data)>;

            if constexpr (std::is_same_v<T, FileModifiedEvent>) {
                alert.category = "file_modified";
                alert.path = event_data.path;
                alert.summary = std::format("File modified: {}",
                                             (*event_data.path).string());
                alert.details = std::format("Old hash: {}\nNew hash: {}\n{}",
                                             *event_data.old_hash,
                                             *event_data.new_hash,
                                             event_data.change_description);
            } else if constexpr (std::is_same_v<T, FileCreatedEvent>) {
                alert.category = "file_created";
                alert.path = event_data.path;
                alert.summary = std::format("New file detected: {}",
                                             (*event_data.path).string());
                if (event_data.source) {
                    alert.details = std::format("Source: {}\nHash: {}",
                                                 *event_data.source,
                                                 *event_data.hash);
                } else {
                    alert.details = std::format("Hash: {}", *event_data.hash);
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

    void AlertDispatcher::send_dbus_notification(Alert const& /* alert */) {
        // TODO: Implement D-Bus notification
        // This will send a signal on the session bus that the notification client can listen to
        // For now, this is a no-op
    }

}  // namespace vigilant_canine
