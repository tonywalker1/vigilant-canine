//
// vigilant-canine - D-Bus Desktop Notifier
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_NOTIFY_DBUS_NOTIFIER_H
#define VIGILANT_CANINE_NOTIFY_DBUS_NOTIFIER_H

#include <storage/alert_store.h>

#include <expected>
#include <string>

// Forward declare sd_bus type
struct sd_bus;

namespace vigilant_canine {

    //
    // D-Bus notifier for sending desktop notifications.
    // Uses freedesktop.org notification specification via sd_bus.
    //
    // Gracefully degrades on headless systems or when D-Bus is unavailable.
    //
    class DbusNotifier {
    public:
        DbusNotifier() = default;
        ~DbusNotifier();

        // Non-copyable, non-movable (owns sd_bus connection)
        DbusNotifier(DbusNotifier const&) = delete;
        DbusNotifier& operator=(DbusNotifier const&) = delete;
        DbusNotifier(DbusNotifier&&) = delete;
        DbusNotifier& operator=(DbusNotifier&&) = delete;

        [[nodiscard]] auto initialize() -> std::expected<void, std::string>;
        void notify(Alert const& alert);
        [[nodiscard]] auto is_available() const -> bool;

    private:
        [[nodiscard]] auto get_urgency(AlertSeverity severity) const -> std::uint8_t;

        sd_bus* m_bus{nullptr};
        bool m_available{false};
    };

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_NOTIFY_DBUS_NOTIFIER_H
