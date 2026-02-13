//
// vigilant-canine - Audit Monitor
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_AUDIT_MONITOR_H
#define VIGILANT_CANINE_AUDIT_MONITOR_H

#include <audit/audit_parsing.h>
#include <audit/audit_rule.h>
#include <events/event_bus.h>

#include <atomic>
#include <chrono>
#include <expected>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace vigilant_canine {

    //
    // Configuration for audit monitoring.
    //
    struct AuditMonitorConfig {
        bool sanitize_command_lines{true};
        std::vector<std::string> exclude_comms;
        std::vector<std::uint32_t> exclude_uids;
    };

    //
    // Audit monitor - reads Linux audit subsystem and matches against rules.
    // Follows JournalMonitor pattern: background thread + event correlation.
    //
    class AuditMonitor {
    public:
        AuditMonitor(EventBus& event_bus,
                     std::vector<AuditRule> rules,
                     AuditMonitorConfig config = {});

        ~AuditMonitor();

        // Non-copyable, non-movable (owns thread)
        AuditMonitor(AuditMonitor const&) = delete;
        AuditMonitor& operator=(AuditMonitor const&) = delete;
        AuditMonitor(AuditMonitor&&) = delete;
        AuditMonitor& operator=(AuditMonitor&&) = delete;

        [[nodiscard]] auto initialize() -> std::expected<void, std::string>;
        [[nodiscard]] auto start() -> std::expected<void, std::string>;
        void stop();
        [[nodiscard]] auto is_running() const -> bool;

        // Hot reload rules (SIGHUP handler)
        void update_rules(std::vector<AuditRule> rules);

    private:
        void monitor_loop();
        void process_record(auparse_state_t* au);
        void flush_pending_events();
        void evaluate_event(AuditEventAccumulator const& event);
        auto build_event(AuditEventAccumulator const& event, AuditRule const& rule) -> Event;
        [[nodiscard]] auto should_exclude(AuditEventAccumulator const& event) const -> bool;
        auto get_username(std::uint32_t uid) -> std::string;

        EventBus& m_event_bus;
        AuditMonitorConfig m_config;

        std::vector<AuditRule> m_rules;
        mutable std::mutex m_rules_mutex;  // Protects m_rules for hot reload

        auparse_state_t* m_auparse{nullptr};
        int m_audit_fd{-1};

        std::atomic<bool> m_running{false};
        std::atomic<bool> m_should_stop{false};
        std::thread m_monitor_thread;

        // Multi-record correlation
        std::map<std::uint64_t, AuditEventAccumulator> m_pending_events;
        mutable std::mutex m_pending_mutex;

        // Username cache
        std::map<std::uint32_t, std::string> m_username_cache;
        mutable std::mutex m_cache_mutex;
    };

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_AUDIT_MONITOR_H
