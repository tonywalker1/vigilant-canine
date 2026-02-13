//
// vigilant-canine - D-Bus Notifier Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <notify/dbus_notifier.h>

#include <systemd/sd-bus.h>

#include <format>

namespace vigilant_canine {

    DbusNotifier::~DbusNotifier() {
        if (m_bus) {
            sd_bus_unref(m_bus);
        }
    }

    auto DbusNotifier::initialize() -> std::expected<void, std::string> {
        // Try to open user session bus
        int ret = sd_bus_open_user(&m_bus);
        if (ret < 0) {
            m_available = false;
            return std::unexpected(
                std::format("D-Bus not available (headless system?): {}", strerror(-ret)));
        }

        m_available = true;
        return {};
    }

    void DbusNotifier::notify(Alert const& alert) {
        if (!m_available || !m_bus) {
            // Graceful degradation - no notification on headless systems
            return;
        }

        // Freedesktop notification specification
        // See: https://specifications.freedesktop.org/notification-spec/latest/
        
        std::string app_name = "Vigilant Canine";
        std::uint32_t replaces_id = 0;  // 0 = new notification
        std::string icon = "security-high";  // Standard icon name
        std::string summary = alert.summary;
        std::string body = alert.details.value_or("");
        auto urgency = get_urgency(alert.severity);

        sd_bus_error error = SD_BUS_ERROR_NULL;
        sd_bus_message* reply = nullptr;

        // Call org.freedesktop.Notifications.Notify
        int ret = sd_bus_call_method(
            m_bus,
            "org.freedesktop.Notifications",           // service
            "/org/freedesktop/Notifications",          // object path
            "org.freedesktop.Notifications",           // interface
            "Notify",                                  // method
            &error,
            &reply,
            "susssasa{sv}i",                           // signature
            app_name.c_str(),                          // app_name (string)
            replaces_id,                               // replaces_id (uint32)
            icon.c_str(),                              // app_icon (string)
            summary.c_str(),                           // summary (string)
            body.c_str(),                              // body (string)
            0,                                         // actions (array, empty)
            1,                                         // hints dict size
            "urgency", "y", urgency,                   // urgency hint (byte)
            -1                                         // timeout (-1 = default)
        );

        if (ret < 0) {
            // Failed to send notification - log but don't fail
            // (notification is non-critical, alerts are logged elsewhere)
        }

        sd_bus_error_free(&error);
        if (reply) {
            sd_bus_message_unref(reply);
        }
    }

    auto DbusNotifier::is_available() const -> bool {
        return m_available;
    }

    auto DbusNotifier::get_urgency(AlertSeverity severity) const -> std::uint8_t {
        // Freedesktop urgency levels: 0 = low, 1 = normal, 2 = critical
        switch (severity) {
        case AlertSeverity::INFO:
            return 0;  // low
        case AlertSeverity::WARNING:
            return 1;  // normal
        case AlertSeverity::CRITICAL:
            return 2;  // critical
        }
        return 1;  // default to normal
    }

}  // namespace vigilant_canine
