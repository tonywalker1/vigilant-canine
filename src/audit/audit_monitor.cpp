//
// vigilant-canine - Audit Monitor Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <audit/audit_monitor.h>

#include <libaudit.h>
#include <auparse.h>

#include <systemd/sd-journal.h>

#include <pwd.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <format>

namespace vigilant_canine {

    AuditMonitor::AuditMonitor(EventBus& event_bus,
                               std::vector<AuditRule> rules,
                               AuditMonitorConfig config)
        : m_event_bus(event_bus),
          m_config(std::move(config)),
          m_rules(std::move(rules)) {}

    AuditMonitor::~AuditMonitor() {
        stop();
        if (m_auparse) {
            auparse_destroy(m_auparse);
        }
        if (m_audit_fd >= 0) {
            audit_close(m_audit_fd);
        }
    }

    auto AuditMonitor::initialize() -> std::expected<void, std::string> {
        // Open audit netlink socket
        m_audit_fd = audit_open();
        if (m_audit_fd < 0) {
            // Check if this is EPROTONOSUPPORT (audit not available)
            if (errno == EPROTONOSUPPORT) {
                return std::unexpected("Audit subsystem not available (EPROTONOSUPPORT)");
            }
            return std::unexpected(std::format("Failed to open audit: {}",
                                                std::strerror(errno)));
        }

        // Create auparse state
        m_auparse = auparse_init(AUSOURCE_FEED, nullptr);
        if (!m_auparse) {
            audit_close(m_audit_fd);
            m_audit_fd = -1;
            return std::unexpected("Failed to initialize auparse");
        }

        sd_journal_print(LOG_INFO, "vigilant-canined: Audit monitor initialized successfully");
        return {};
    }

    auto AuditMonitor::start() -> std::expected<void, std::string> {
        if (m_running.load()) {
            return std::unexpected("Audit monitor already running");
        }

        if (m_audit_fd < 0 || !m_auparse) {
            return std::unexpected("Audit not initialized");
        }

        m_should_stop.store(false);
        m_running.store(true);

        m_monitor_thread = std::thread([this] { monitor_loop(); });

        sd_journal_print(LOG_INFO, "vigilant-canined: Audit monitor started");
        return {};
    }

    void AuditMonitor::stop() {
        if (!m_running.load()) {
            return;
        }

        m_should_stop.store(true);

        if (m_monitor_thread.joinable()) {
            m_monitor_thread.join();
        }

        m_running.store(false);
        sd_journal_print(LOG_INFO, "vigilant-canined: Audit monitor stopped");
    }

    auto AuditMonitor::is_running() const -> bool {
        return m_running.load();
    }

    void AuditMonitor::update_rules(std::vector<AuditRule> rules) {
        std::lock_guard lock(m_rules_mutex);
        m_rules = std::move(rules);
        sd_journal_print(LOG_INFO, "vigilant-canined: Audit rules updated (hot reload)");
    }

    void AuditMonitor::monitor_loop() {
        while (!m_should_stop.load()) {
            // Read from audit netlink socket
            struct audit_reply reply;
            int rc = audit_get_reply(m_audit_fd, &reply, GET_REPLY_NONBLOCKING, 0);

            if (rc > 0) {
                // Feed to auparse
                if (auparse_feed(m_auparse, reply.msg.data, reply.len) > 0) {
                    // Process all events in the buffer
                    while (auparse_next_event(m_auparse) > 0) {
                        process_record(m_auparse);
                    }
                }
            } else if (rc < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                sd_journal_print(LOG_WARNING,
                    "vigilant-canined: Error reading audit: %s",
                    std::strerror(errno));
            }

            // Flush timed-out pending events
            flush_pending_events();

            // Small sleep to avoid busy-waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        sd_journal_print(LOG_DEBUG, "vigilant-canined: Audit monitor loop exited");
    }

    void AuditMonitor::process_record(auparse_state_t* au) {
        // Get record type
        int type = auparse_get_type(au);
        std::uint64_t audit_id = get_audit_id(au);

        if (audit_id == 0) {
            return;  // Invalid event
        }

        std::lock_guard lock(m_pending_mutex);

        // Get or create accumulator
        auto& acc = m_pending_events[audit_id];
        if (acc.audit_id == 0) {
            acc.audit_id = audit_id;
            acc.received = std::chrono::steady_clock::now();
        }

        // Parse based on type
        if (type == AUDIT_SYSCALL) {
            if (auto result = parse_syscall_record(au); result.has_value()) {
                acc.syscall = *result;
            }
        } else if (type == AUDIT_EXECVE) {
            if (auto result = parse_execve_record(au); result.has_value()) {
                acc.execve = *result;
            }
        } else if (type == AUDIT_CWD) {
            if (auto result = parse_cwd_record(au); result.has_value()) {
                acc.cwd = *result;
            }
        } else if (type == AUDIT_PATH) {
            if (auto result = parse_path_record(au); result.has_value()) {
                acc.paths.push_back(*result);
            }
        } else if (type == AUDIT_EOE) {
            // End of Event - event is complete
            if (is_event_complete(acc)) {
                evaluate_event(acc);
                m_pending_events.erase(audit_id);
            }
        }
    }

    void AuditMonitor::flush_pending_events() {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard lock(m_pending_mutex);

        auto it = m_pending_events.begin();
        while (it != m_pending_events.end()) {
            auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - it->second.received
            );

            if (age > INCOMPLETE_EVENT_TIMEOUT) {
                sd_journal_print(LOG_DEBUG,
                    "vigilant-canined: Flushing incomplete audit event %lu (age: %ldms)",
                    it->first, age.count());

                // Emit warning but evaluate anyway
                if (it->second.syscall.has_value()) {
                    evaluate_event(it->second);
                }

                it = m_pending_events.erase(it);
            } else {
                ++it;
            }
        }
    }

    void AuditMonitor::evaluate_event(AuditEventAccumulator const& event) {
        // Check exclusions
        if (should_exclude(event)) {
            return;
        }

        // Check rules
        std::lock_guard lock(m_rules_mutex);
        for (auto const& rule : m_rules) {
            if (matches_rule(rule, event)) {
                auto evt = build_event(event, rule);
                m_event_bus.publish(std::move(evt));
            }
        }
    }

    auto AuditMonitor::build_event(AuditEventAccumulator const& event,
                                    AuditRule const& rule)
        -> Event {
        if (!event.syscall.has_value()) {
            // Shouldn't happen, but handle gracefully
            return Event{
                SuspiciousLogEvent{
                    .rule_name = rule.name,
                    .unit_name = "audit",
                    .message = "Incomplete audit event",
                    .priority = 4,
                },
                rule.severity,
                "audit"
            };
        }

        auto const& syscall = *event.syscall;
        std::string username = get_username(syscall.uid);

        switch (rule.action) {
        case AuditRuleAction::process_execution: {
            std::string cmdline;
            if (event.execve.has_value()) {
                cmdline = join_argv(event.execve->argv);
                if (m_config.sanitize_command_lines) {
                    cmdline = sanitize_command_line(cmdline);
                }
            } else {
                cmdline = syscall.comm;
            }

            return Event{
                ProcessExecutionEvent{
                    .pid = syscall.pid,
                    .ppid = syscall.ppid,
                    .uid = syscall.uid,
                    .username = username,
                    .exe_path = FilePath{syscall.exe},
                    .command_line = cmdline,
                    .cwd = event.cwd ? std::optional{event.cwd->cwd} : std::nullopt,
                },
                rule.severity,
                "audit"
            };
        }

        case AuditRuleAction::network_connection: {
            if (event.network.has_value()) {
                auto const& net = *event.network;
                return Event{
                    NetworkConnectionEvent{
                        .pid = syscall.pid,
                        .uid = syscall.uid,
                        .username = username,
                        .protocol = net.protocol,
                        .local_addr = net.local_addr,
                        .local_port = net.local_port,
                        .remote_addr = net.remote_addr,
                        .remote_port = net.remote_port,
                    },
                    rule.severity,
                    "audit"
                };
            }
            break;
        }

        case AuditRuleAction::failed_access: {
            std::string path = !event.paths.empty() ? event.paths[0].name : "";
            return Event{
                FailedAccessEvent{
                    .pid = syscall.pid,
                    .uid = syscall.uid,
                    .username = username,
                    .path = FilePath{path},
                    .access_type = "unknown",
                    .error_code = syscall.exit_code,
                    .error_message = std::strerror(-syscall.exit_code),
                },
                rule.severity,
                "audit"
            };
        }

        case AuditRuleAction::privilege_change: {
            return Event{
                PrivilegeChangeEvent{
                    .pid = syscall.pid,
                    .old_uid = syscall.uid,
                    .new_uid = syscall.euid,
                    .old_username = username,
                    .new_username = get_username(syscall.euid),
                    .operation = "syscall_" + std::to_string(syscall.syscall),
                },
                rule.severity,
                "audit"
            };
        }

        case AuditRuleAction::suspicious_syscall:
        default: {
            std::string message = std::format(
                "Suspicious syscall {} by {} ({})",
                syscall.syscall,
                username,
                syscall.comm
            );
            return Event{
                SuspiciousLogEvent{
                    .rule_name = rule.name,
                    .unit_name = "audit",
                    .message = message,
                    .priority = 4,
                },
                rule.severity,
                "audit"
            };
        }
        }

        // Fallback
        return Event{
            SuspiciousLogEvent{
                .rule_name = rule.name,
                .unit_name = "audit",
                .message = "Audit rule matched",
                .priority = 4,
            },
            rule.severity,
            "audit"
        };
    }

    auto AuditMonitor::should_exclude(AuditEventAccumulator const& event) const -> bool {
        if (!event.syscall.has_value()) {
            return true;
        }

        auto const& syscall = *event.syscall;

        // Check exclude_comms
        for (auto const& excluded : m_config.exclude_comms) {
            if (syscall.comm == excluded) {
                return true;
            }
        }

        // Check exclude_uids
        for (auto uid : m_config.exclude_uids) {
            if (syscall.uid == uid) {
                return true;
            }
        }

        return false;
    }

    auto AuditMonitor::get_username(std::uint32_t uid) -> std::string {
        // Check cache first
        {
            std::lock_guard lock(m_cache_mutex);
            auto it = m_username_cache.find(uid);
            if (it != m_username_cache.end()) {
                return it->second;
            }
        }

        // Lookup username
        struct passwd* pw = getpwuid(uid);
        std::string username = pw ? pw->pw_name : std::to_string(uid);

        // Cache result
        {
            std::lock_guard lock(m_cache_mutex);
            m_username_cache[uid] = username;
        }

        return username;
    }

}  // namespace vigilant_canine
