//
// vigilant-canine - Event Bus Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <events/event_bus.h>

#include <algorithm>
#include <iostream>

namespace vigilant_canine {

    void EventBus::publish(Event const& event) {
        std::lock_guard<std::mutex> lock(m_mutex);

        for (auto const& entry : m_handlers) {
            // Check severity filter
            if (entry.min_severity) {
                if (event.severity < *entry.min_severity) {
                    continue;
                }
            }

            // Call handler, catching any exceptions
            try {
                entry.handler(event);
            } catch (std::exception const& e) {
                // Log error but continue with other handlers
                std::cerr << "Event handler exception: " << e.what() << "\n";
            } catch (...) {
                std::cerr << "Event handler threw unknown exception\n";
            }
        }
    }

    auto EventBus::subscribe(EventHandler handler) -> EventSubscription {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto id = m_next_id++;
        m_handlers.push_back(HandlerEntry{id, std::move(handler), std::nullopt});

        return EventSubscription{id};
    }

    auto EventBus::subscribe_severity(EventSeverity min_severity,
                                        EventHandler handler)
        -> EventSubscription {

        std::lock_guard<std::mutex> lock(m_mutex);

        auto id = m_next_id++;
        m_handlers.push_back(HandlerEntry{id, std::move(handler), min_severity});

        return EventSubscription{id};
    }

    void EventBus::unsubscribe(EventSubscription const& subscription) {
        if (!subscription.is_valid()) {
            return;
        }

        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = std::remove_if(m_handlers.begin(), m_handlers.end(),
            [&](HandlerEntry const& entry) {
                return entry.id == subscription.id();
            });

        m_handlers.erase(it, m_handlers.end());
    }

    auto EventBus::subscription_count() const -> std::size_t {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_handlers.size();
    }

}  // namespace vigilant_canine
