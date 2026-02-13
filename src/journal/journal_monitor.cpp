//
// vigilant-canine - Journal Monitor Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <journal/journal_monitor.h>

#include <systemd/sd-journal.h>

#include <chrono>
#include <cstring>
#include <format>

namespace vigilant_canine {

    JournalMonitor::JournalMonitor(EventBus& event_bus,
                                   std::vector<JournalRule> rules,
                                   JournalMonitorConfig config)
        : m_event_bus(event_bus),
          m_config(std::move(config)),
          m_rules(std::move(rules)) {}

    JournalMonitor::~JournalMonitor() {
        stop();
        if (m_journal) {
            sd_journal_close(m_journal);
        }
    }

    auto JournalMonitor::initialize() -> std::expected<void, std::string> {
        // Open journal for reading
        int ret = sd_journal_open(&m_journal, SD_JOURNAL_LOCAL_ONLY);
        if (ret < 0) {
            return std::unexpected(std::format("Failed to open journal: {}",
                                                std::strerror(-ret)));
        }

        // Seek to end - we only want new entries
        ret = sd_journal_seek_tail(m_journal);
        if (ret < 0) {
            sd_journal_close(m_journal);
            m_journal = nullptr;
            return std::unexpected(std::format("Failed to seek journal: {}",
                                                std::strerror(-ret)));
        }

        return {};
    }

    auto JournalMonitor::start() -> std::expected<void, std::string> {
        if (m_running.load()) {
            return std::unexpected("Journal monitor already running");
        }

        if (!m_journal) {
            return std::unexpected("Journal not initialized");
        }

        m_should_stop.store(false);
        m_running.store(true);

        m_monitor_thread = std::thread([this] { monitor_loop(); });

        return {};
    }

    void JournalMonitor::stop() {
        if (!m_running.load()) {
            return;
        }

        m_should_stop.store(true);

        if (m_monitor_thread.joinable()) {
            m_monitor_thread.join();
        }

        m_running.store(false);
    }

    auto JournalMonitor::is_running() const -> bool {
        return m_running.load();
    }

    void JournalMonitor::update_rules(std::vector<JournalRule> rules) {
        std::lock_guard lock(m_rules_mutex);
        m_rules = std::move(rules);
    }

    void JournalMonitor::monitor_loop() {
        while (!m_should_stop.load()) {
            // Wait for new journal entries (with timeout)
            int ret = sd_journal_wait(m_journal, 1000000);  // 1 second timeout
            
            if (ret < 0) {
                // Error waiting - log and continue
                continue;
            }

            if (ret == SD_JOURNAL_NOP) {
                // Timeout, no new entries
                continue;
            }

            // Process new entries
            while (sd_journal_next(m_journal) > 0 && !m_should_stop.load()) {
                auto entry = extract_fields(m_journal);

                // Skip if should be excluded
                if (should_exclude(entry)) {
                    continue;
                }

                // Skip if priority too low
                if (entry.priority > m_config.max_priority) {
                    continue;
                }

                // Evaluate against rules
                evaluate_entry(entry);
            }
        }
    }

    auto JournalMonitor::extract_fields(sd_journal* journal) -> JournalEntry {
        JournalEntry entry;

        // Helper to get field value
        auto get_field = [journal](char const* field) -> std::string {
            char const* data = nullptr;
            size_t length = 0;
            
            if (sd_journal_get_data(journal, field, 
                                    reinterpret_cast<void const**>(&data),
                                    &length) >= 0) {
                // Skip field name and '='
                char const* value = static_cast<char const*>(
                    std::memchr(data, '=', length));
                if (value) {
                    ++value;  // Skip '='
                    size_t value_len = length - (value - data);
                    return std::string(value, value_len);
                }
            }
            return "";
        };

        // Extract standard fields
        entry.message = get_field(journal_fields::MESSAGE);
        entry.syslog_identifier = get_field(journal_fields::SYSLOG_IDENTIFIER);
        entry.systemd_unit = get_field(journal_fields::SYSTEMD_UNIT);
        entry.comm = get_field(journal_fields::COMM);
        entry.exe = get_field(journal_fields::EXE);

        // Priority
        auto priority_str = get_field(journal_fields::PRIORITY);
        if (!priority_str.empty()) {
            entry.priority = static_cast<std::uint8_t>(std::stoi(priority_str));
        }

        // PID
        auto pid_str = get_field(journal_fields::PID);
        if (!pid_str.empty()) {
            entry.pid = static_cast<std::uint32_t>(std::stoul(pid_str));
        }

        // UID
        auto uid_str = get_field(journal_fields::UID);
        if (!uid_str.empty()) {
            entry.uid = static_cast<std::uint32_t>(std::stoul(uid_str));
        }

        // Timestamp
        std::uint64_t usec = 0;
        if (sd_journal_get_realtime_usec(journal, &usec) >= 0) {
            entry.timestamp = std::chrono::system_clock::time_point(
                std::chrono::microseconds(usec));
        } else {
            entry.timestamp = std::chrono::system_clock::now();
        }

        return entry;
    }

    void JournalMonitor::evaluate_entry(JournalEntry const& entry) {
        std::lock_guard lock(m_rules_mutex);

        // First match wins
        for (auto const& rule : m_rules) {
            if (matches_rule(rule, entry)) {
                auto event = build_event(entry, rule);
                m_event_bus.publish(event);
                return;  // Only fire first matching rule
            }
        }
    }

    auto JournalMonitor::build_event(JournalEntry const& entry,
                                      JournalRule const& rule) -> Event {
        EventData event_data;

        switch (rule.action) {
        case JournalRuleAction::auth_failure: {
            // Extract username from message if possible
            std::string username;
            std::string service = entry.syslog_identifier;
            std::optional<std::string> remote_host;

            // Simple parsing - could be enhanced
            if (entry.message.find("for ") != std::string::npos) {
                auto pos = entry.message.find("for ");
                auto end = entry.message.find(" from", pos);
                if (end == std::string::npos) {
                    end = entry.message.find(" ", pos + 4);
                }
                if (end != std::string::npos) {
                    username = entry.message.substr(pos + 4, end - pos - 4);
                }

                // Extract remote host
                auto from_pos = entry.message.find("from ");
                if (from_pos != std::string::npos) {
                    auto host_start = from_pos + 5;
                    auto host_end = entry.message.find(" ", host_start);
                    if (host_end == std::string::npos) {
                        host_end = entry.message.length();
                    }
                    remote_host = entry.message.substr(host_start, host_end - host_start);
                }
            }

            event_data = AuthFailureEvent{
                .username = username,
                .service = service,
                .remote_host = remote_host,
                .message = entry.message
            };
            break;
        }

        case JournalRuleAction::privilege_escalation: {
            std::string username;
            std::string target_user = "root";  // Default assumption
            std::string method = entry.syslog_identifier;
            std::string command;

            // Extract command from sudo message
            auto cmd_pos = entry.message.find("COMMAND=");
            if (cmd_pos != std::string::npos) {
                command = entry.message.substr(cmd_pos + 8);
            }

            // Extract username
            auto user_pos = entry.message.find("USER=");
            if (user_pos != std::string::npos) {
                auto end = entry.message.find(" ", user_pos);
                if (end == std::string::npos) {
                    end = entry.message.length();
                }
                target_user = entry.message.substr(user_pos + 5, end - user_pos - 5);
            }

            event_data = PrivilegeEscalationEvent{
                .username = username,
                .target_user = target_user,
                .method = method,
                .command = command,
                .message = entry.message
            };
            break;
        }

        case JournalRuleAction::service_state: {
            std::string unit_name = entry.systemd_unit;
            std::string new_state = "failed";  // Default assumption
            std::optional<std::string> exit_code;

            if (entry.message.find("started") != std::string::npos) {
                new_state = "started";
            } else if (entry.message.find("stopped") != std::string::npos) {
                new_state = "stopped";
            }

            event_data = ServiceStateEvent{
                .unit_name = unit_name,
                .new_state = new_state,
                .exit_code = exit_code,
                .message = entry.message
            };
            break;
        }

        case JournalRuleAction::suspicious_log: {
            event_data = SuspiciousLogEvent{
                .rule_name = rule.name,
                .unit_name = entry.systemd_unit,
                .message = entry.message,
                .priority = entry.priority
            };
            break;
        }
        }

        return Event{event_data, rule.severity, "journal_monitor"};
    }

    auto JournalMonitor::should_exclude(JournalEntry const& entry) const -> bool {
        // Self-monitoring loop prevention
        if (entry.syslog_identifier == "vigilant-canined") {
            return true;
        }

        // Check exclude units
        for (auto const& unit : m_config.exclude_units) {
            if (entry.systemd_unit == unit) {
                return true;
            }
        }

        // Check exclude identifiers
        for (auto const& ident : m_config.exclude_identifiers) {
            if (entry.syslog_identifier == ident) {
                return true;
            }
        }

        return false;
    }

}  // namespace vigilant_canine
