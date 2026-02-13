//
// vigilant-canine - Correlation Engine Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <correlation/correlation_engine.h>

#include <algorithm>
#include <format>

namespace vigilant_canine {

    CorrelationEngine::CorrelationEngine(EventBus& event_bus,
                                         std::vector<CorrelationRule> rules)
        : m_event_bus(event_bus),
          m_rules(std::move(rules)) {}

    auto CorrelationEngine::start() -> std::expected<void, std::string> {
        // Subscribe to all events
        m_subscription = m_event_bus.subscribe([this](Event const& event) {
            handle_event(event);
        });

        return {};
    }

    void CorrelationEngine::stop() {
        if (m_subscription.is_valid()) {
            m_event_bus.unsubscribe(m_subscription);
        }
    }

    void CorrelationEngine::drain_escalated_events(EventBus& event_bus) {
        std::vector<Event> events_to_publish;

        {
            std::lock_guard lock(m_pending_mutex);
            events_to_publish = std::move(m_pending_events);
            m_pending_events.clear();
        }

        // Publish outside the lock (EventBus has its own lock)
        for (auto const& event : events_to_publish) {
            event_bus.publish(event);
        }
    }

    void CorrelationEngine::update_rules(std::vector<CorrelationRule> rules) {
        std::lock_guard lock(m_rules_mutex);
        m_rules = std::move(rules);
    }

    void CorrelationEngine::handle_event(Event const& event) {
        if (!should_track(event)) {
            return;
        }

        auto match_key = get_match_key(event);
        auto now = event.timestamp;

        std::lock_guard lock(m_state_mutex);

        // Clean up old entries periodically
        cleanup_old_entries();

        // Add this event to history
        m_event_history[match_key].push_back(now);

        // Check each rule
        std::lock_guard rules_lock(m_rules_mutex);
        for (auto const& rule : m_rules) {
            // Check if this event matches the rule
            auto rule_key = rule.event_match;
            if (match_key != rule_key) {
                continue;
            }

            // Count events in the time window
            auto& history = m_event_history[match_key];
            auto window_start = now - rule.window;

            // Remove events outside the window
            history.erase(
                std::remove_if(history.begin(), history.end(),
                    [window_start](auto const& ts) { return ts < window_start; }),
                history.end());

            auto count = history.size();

            // Check if threshold exceeded
            if (count >= rule.threshold) {
                // Check debounce - don't fire if recently fired
                auto& last_fired = m_last_fired[rule.name];
                auto debounce_window = rule.window;  // Use same window for debounce
                
                if (now - last_fired < debounce_window) {
                    continue;  // Still in debounce period
                }

                // Create escalated event
                auto escalated_message = std::format(
                    "Correlation rule '{}' triggered: {} events in {} seconds (threshold: {})",
                    rule.name, count, rule.window.count(), rule.threshold);

                SuspiciousLogEvent escalated{
                    .rule_name = rule.name,
                    .unit_name = "correlation",
                    .message = escalated_message,
                    .priority = 2  // LOG_CRIT
                };

                Event escalated_event{escalated, rule.escalated_severity, "correlation_engine"};

                // Buffer the event (don't publish from handler - deadlock risk)
                {
                    std::lock_guard pending_lock(m_pending_mutex);
                    m_pending_events.push_back(escalated_event);
                }

                // Update debounce timestamp
                m_last_fired[rule.name] = now;
            }
        }
    }

    auto CorrelationEngine::get_match_key(Event const& event) const -> std::string {
        // Get event type name
        return std::string(event_type_name(event.data));
    }

    auto CorrelationEngine::should_track(Event const& event) const -> bool {
        // Don't track our own escalated events (infinite loop prevention)
        if (event.source == "correlation_engine") {
            return false;
        }

        return true;
    }

    void CorrelationEngine::cleanup_old_entries() {
        // Limit number of tracked keys to prevent unbounded memory growth
        if (m_event_history.size() > MAX_TRACKED_KEYS) {
            // Remove oldest entries (simple strategy: clear half)
            auto it = m_event_history.begin();
            auto mid = it;
            std::advance(mid, m_event_history.size() / 2);
            m_event_history.erase(it, mid);
        }
    }

}  // namespace vigilant_canine
