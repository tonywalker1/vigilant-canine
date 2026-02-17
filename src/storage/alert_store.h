//
// vigilant-canine - Alert Storage
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_STORAGE_ALERT_STORE_H
#define VIGILANT_CANINE_STORAGE_ALERT_STORE_H

#include <core/types.h>
#include <storage/database.h>

#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace vigilant_canine {

    //
    // Alert severity levels.
    //
    enum class AlertSeverity {
        INFO,
        WARNING,
        CRITICAL
    };

    //
    // Convert AlertSeverity to string.
    //
    [[nodiscard]] auto to_string(AlertSeverity severity) -> std::string;

    //
    // Convert string to AlertSeverity.
    //
    [[nodiscard]] auto parse_severity(std::string const& str) -> std::optional<AlertSeverity>;

    //
    // Alert record.
    //
    struct Alert {
        std::int64_t id{0};
        AlertSeverity severity{AlertSeverity::INFO};
        std::string category;
        std::optional<FilePath> path;
        std::string summary;
        std::optional<std::string> details;
        std::string source;
        bool acknowledged{false};
    };

    //
    // Filter criteria for alert queries.
    //
    struct AlertFilter {
        std::optional<AlertSeverity> severity;
        std::optional<bool> acknowledged;
        std::optional<std::string> category;
        std::optional<std::int64_t> since_id;  // Alerts with ID >= this
    };

    //
    // Storage interface for alerts.
    //
    class AlertStore {
    public:
        explicit AlertStore(Database& database) : m_db(database) {}

        //
        // Insert a new alert.
        //
        // Postconditions:
        //   - On success: alert is inserted and its id is returned
        //   - On failure: returns error message
        //
        [[nodiscard]] auto insert(Alert const& alert)
            -> std::expected<std::int64_t, std::string>;

        //
        // Find alert by id.
        //
        [[nodiscard]] auto find_by_id(std::int64_t alert_id)
            -> std::expected<std::optional<Alert>, std::string>;

        //
        // Get recent alerts (most recent first).
        //
        [[nodiscard]] auto get_recent(int limit = 100)
            -> std::expected<std::vector<Alert>, std::string>;

        //
        // Get unacknowledged alerts.
        //
        [[nodiscard]] auto get_unacknowledged()
            -> std::expected<std::vector<Alert>, std::string>;

        //
        // Get filtered alerts with pagination.
        //
        // Returns alerts matching the filter criteria, ordered by ID descending (most recent first).
        // Use limit/offset for pagination.
        //
        [[nodiscard]] auto get_filtered(
            AlertFilter const& filter,
            int limit = 100,
            int offset = 0
        ) -> std::expected<std::vector<Alert>, std::string>;

        //
        // Acknowledge an alert by id.
        //
        [[nodiscard]] auto acknowledge(std::int64_t alert_id)
            -> std::expected<void, std::string>;

        //
        // Un-acknowledge an alert by id.
        //
        [[nodiscard]] auto unacknowledge(std::int64_t alert_id)
            -> std::expected<void, std::string>;

    private:
        Database& m_db;
    };

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_STORAGE_ALERT_STORE_H
