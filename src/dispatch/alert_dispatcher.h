//
// vigilant-canine - Alert Dispatcher
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_DISPATCH_ALERT_DISPATCHER_H
#define VIGILANT_CANINE_DISPATCH_ALERT_DISPATCHER_H

#include <events/event.h>
#include <events/event_bus.h>
#include <storage/alert_store.h>

#include <expected>
#include <memory>
#include <string>

namespace vigilant_canine {

    // Forward declaration
    class DbusNotifier;

    //
    // Alert dispatcher configuration.
    //
    struct AlertDispatcherConfig {
        bool log_to_journal{true};   // Log to systemd journal
        bool send_dbus{false};        // Send D-Bus notifications (requires session bus)
    };

    //
    // Alert dispatcher.
    //
    // Subscribes to the event bus and dispatches alerts to multiple destinations:
    //   - SQLite database (via AlertStore)
    //   - systemd journal (always enabled for zero-dependency alerting)
    //   - D-Bus signals (optional, for desktop notifications)
    //
    // This is the primary mechanism for alert delivery in the system.
    //
    class AlertDispatcher {
    public:
        AlertDispatcher(EventBus& event_bus,
                        AlertStore& alert_store,
                        AlertDispatcherConfig config = {},
                        DbusNotifier* dbus_notifier = nullptr);  // Phase 2

        ~AlertDispatcher();

        // Non-copyable, movable
        AlertDispatcher(AlertDispatcher const&) = delete;
        AlertDispatcher& operator=(AlertDispatcher const&) = delete;
        AlertDispatcher(AlertDispatcher&&) noexcept;
        AlertDispatcher& operator=(AlertDispatcher&&) noexcept;

        //
        // Start dispatching alerts.
        //
        // Subscribes to the event bus and begins processing events.
        //
        [[nodiscard]] auto start() -> std::expected<void, std::string>;

        //
        // Stop dispatching alerts.
        //
        void stop();

        //
        // Check if dispatcher is running.
        //
        [[nodiscard]] auto is_running() const -> bool { return m_running; }

    private:
        //
        // Handle an event from the event bus.
        //
        void handle_event(Event const& event);

        //
        // Convert an event to an alert.
        //
        [[nodiscard]] auto event_to_alert(Event const& event) -> Alert;

        //
        // Log alert to systemd journal.
        //
        void log_to_journal(Alert const& alert);

        //
        // Send D-Bus notification.
        //
        void send_dbus_notification(Alert const& alert);

        EventBus& m_event_bus;
        AlertStore& m_alert_store;
        AlertDispatcherConfig m_config;
        DbusNotifier* m_dbus_notifier{nullptr};  // Phase 2: optional notifier
        bool m_running{false};
        std::unique_ptr<EventSubscription> m_subscription;
    };

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_DISPATCH_ALERT_DISPATCHER_H
