//
// vigilant-canine - Event Correlation Engine
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_CORRELATION_ENGINE_H
#define VIGILANT_CANINE_CORRELATION_ENGINE_H

#include <config/config.h>
#include <events/event_bus.h>

#include <chrono>
#include <expected>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace vigilant_canine {

    //
    // Correlation rule for time-windowed event aggregation.
    //
    struct CorrelationRule {
        std::string name;
        std::string event_match;  // Rule name or event type to match
        std::uint32_t threshold{5};
        std::chrono::seconds window{60};
        EventSeverity escalated_severity{EventSeverity::critical};
    };

    //
    // Event correlation engine - aggregates events over time windows.
    // Fires escalated events when thresholds are exceeded.
    //
    // CRITICAL: drain_escalated_events() must be called from daemon main loop,
    // NOT from within an EventBus handler (would deadlock).
    //
    class CorrelationEngine {
    public:
        CorrelationEngine(EventBus& event_bus, std::vector<CorrelationRule> rules);

        [[nodiscard]] auto start() -> std::expected<void, std::string>;
        void stop();

        // Must be called from daemon main loop (not from handler)
        void drain_escalated_events(EventBus& event_bus);

        // Hot reload (SIGHUP)
        void update_rules(std::vector<CorrelationRule> rules);

    private:
        void handle_event(Event const& event);
        [[nodiscard]] auto get_match_key(Event const& event) const -> std::string;
        [[nodiscard]] auto should_track(Event const& event) const -> bool;
        void cleanup_old_entries();

        EventBus& m_event_bus;

        std::vector<CorrelationRule> m_rules;
        mutable std::mutex m_rules_mutex;

        // State: match_key -> vector<timestamp>
        std::map<std::string, std::vector<std::chrono::system_clock::time_point>> m_event_history;
        std::map<std::string, std::chrono::system_clock::time_point> m_last_fired;  // Debounce
        mutable std::mutex m_state_mutex;

        // Pending escalated events (buffered, not published from handler)
        std::vector<Event> m_pending_events;
        mutable std::mutex m_pending_mutex;

        EventSubscription m_subscription;

        static constexpr std::size_t MAX_TRACKED_KEYS = 1000;
    };

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_CORRELATION_ENGINE_H
