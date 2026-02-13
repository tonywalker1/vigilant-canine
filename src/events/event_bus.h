//
// vigilant-canine - Event Bus
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_EVENTS_EVENT_BUS_H
#define VIGILANT_CANINE_EVENTS_EVENT_BUS_H

#include <events/event.h>

#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace vigilant_canine {

    //
    // Event handler callback type.
    //
    using EventHandler = std::function<void(Event const&)>;

    //
    // Event subscription handle for unsubscribing.
    //
    class EventSubscription {
    public:
        EventSubscription() = default;
        explicit EventSubscription(std::size_t id) : m_id(id) {}

        [[nodiscard]] auto id() const -> std::size_t { return m_id; }
        [[nodiscard]] auto is_valid() const -> bool { return m_id != 0; }

    private:
        std::size_t m_id{0};
    };

    //
    // Event bus for publishing and subscribing to events.
    //
    // The event bus is the central message broker for the daemon.
    // Components publish events, and handlers subscribe to process them.
    //
    // Thread-safe: Multiple threads can publish and subscribe concurrently.
    //
    class EventBus {
    public:
        EventBus() = default;

        // Non-copyable, movable
        EventBus(EventBus const&) = delete;
        EventBus& operator=(EventBus const&) = delete;
        EventBus(EventBus&&) noexcept = default;
        EventBus& operator=(EventBus&&) noexcept = default;

        ~EventBus() = default;

        //
        // Publish an event to all subscribers.
        //
        // Events are delivered synchronously in the order handlers were subscribed.
        // If a handler throws, the exception is caught and logged, but other
        // handlers still receive the event.
        //
        void publish(Event const& event);

        //
        // Subscribe to all events with a handler.
        //
        // Returns a subscription handle that can be used to unsubscribe.
        //
        [[nodiscard]] auto subscribe(EventHandler handler) -> EventSubscription;

        //
        // Subscribe to events of a specific severity or higher.
        //
        [[nodiscard]] auto subscribe_severity(EventSeverity min_severity,
                                                EventHandler handler)
            -> EventSubscription;

        //
        // Unsubscribe a handler.
        //
        void unsubscribe(EventSubscription const& subscription);

        //
        // Get count of active subscriptions.
        //
        [[nodiscard]] auto subscription_count() const -> std::size_t;

    private:
        struct HandlerEntry {
            std::size_t id;
            EventHandler handler;
            std::optional<EventSeverity> min_severity;
        };

        mutable std::mutex m_mutex;
        std::vector<HandlerEntry> m_handlers;
        std::size_t m_next_id{1};
    };

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_EVENTS_EVENT_BUS_H
