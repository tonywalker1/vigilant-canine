//
// vigilant-canine - Journal Monitor
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_JOURNAL_MONITOR_H
#define VIGILANT_CANINE_JOURNAL_MONITOR_H

#include <events/event_bus.h>
#include <journal/journal_rule.h>

#include <atomic>
#include <expected>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Forward declare sd_journal type
struct sd_journal;

namespace vigilant_canine {

    //
    // Configuration for journal monitoring.
    //
    struct JournalMonitorConfig {
        std::uint8_t max_priority{6};  // LOG_INFO (0=emerg, 7=debug)
        std::vector<std::string> exclude_units;
        std::vector<std::string> exclude_identifiers;
    };

    //
    // Journal monitor - reads systemd journal and matches against rules.
    // Follows FanotifyMonitor pattern: background thread + poll loop.
    //
    class JournalMonitor {
    public:
        JournalMonitor(EventBus& event_bus,
                       std::vector<JournalRule> rules,
                       JournalMonitorConfig config = {});

        ~JournalMonitor();

        // Non-copyable, non-movable (owns thread)
        JournalMonitor(JournalMonitor const&) = delete;
        JournalMonitor& operator=(JournalMonitor const&) = delete;
        JournalMonitor(JournalMonitor&&) = delete;
        JournalMonitor& operator=(JournalMonitor&&) = delete;

        [[nodiscard]] auto initialize() -> std::expected<void, std::string>;
        [[nodiscard]] auto start() -> std::expected<void, std::string>;
        void stop();
        [[nodiscard]] auto is_running() const -> bool;

        // Hot reload rules (SIGHUP handler)
        void update_rules(std::vector<JournalRule> rules);

    private:
        void monitor_loop();
        auto extract_fields(sd_journal* journal) -> JournalEntry;
        void evaluate_entry(JournalEntry const& entry);
        auto build_event(JournalEntry const& entry, JournalRule const& rule) -> Event;
        [[nodiscard]] auto should_exclude(JournalEntry const& entry) const -> bool;

        EventBus& m_event_bus;
        JournalMonitorConfig m_config;

        std::vector<JournalRule> m_rules;
        mutable std::mutex m_rules_mutex;  // Protects m_rules for hot reload

        sd_journal* m_journal{nullptr};
        std::atomic<bool> m_running{false};
        std::atomic<bool> m_should_stop{false};
        std::thread m_monitor_thread;
    };

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_JOURNAL_MONITOR_H
