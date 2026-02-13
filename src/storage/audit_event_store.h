//
// vigilant-canine - Audit Event Storage
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_STORAGE_AUDIT_EVENT_STORE_H
#define VIGILANT_CANINE_STORAGE_AUDIT_EVENT_STORE_H

#include <storage/database.h>

#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace vigilant_canine {

    //
    // Audit event record.
    //
    struct AuditEventRecord {
        std::int64_t id{0};
        std::string rule_name;
        std::string event_type;
        std::optional<std::uint32_t> pid;
        std::optional<std::uint32_t> uid;
        std::optional<std::string> username;
        std::optional<std::string> exe_path;
        std::optional<std::string> command_line;
        std::optional<std::string> details;  // JSON for type-specific fields
        std::string created_at;
    };

    //
    // Storage interface for audit events.
    //
    class AuditEventStore {
    public:
        explicit AuditEventStore(Database& database) : m_db(database) {}

        //
        // Insert a new audit event.
        //
        // Postconditions:
        //   - On success: event is inserted and its id is returned
        //   - On failure: returns error message
        //
        [[nodiscard]] auto insert(AuditEventRecord const& event)
            -> std::expected<std::int64_t, std::string>;

        //
        // Get recent audit events (most recent first).
        //
        [[nodiscard]] auto get_recent(int limit = 100)
            -> std::expected<std::vector<AuditEventRecord>, std::string>;

        //
        // Get audit events by rule name.
        //
        [[nodiscard]] auto get_by_rule(std::string const& rule_name, int limit = 100)
            -> std::expected<std::vector<AuditEventRecord>, std::string>;

        //
        // Get audit events by event type.
        //
        [[nodiscard]] auto get_by_type(std::string const& event_type, int limit = 100)
            -> std::expected<std::vector<AuditEventRecord>, std::string>;

        //
        // Get audit events by UID.
        //
        [[nodiscard]] auto get_by_uid(std::uint32_t uid, int limit = 100)
            -> std::expected<std::vector<AuditEventRecord>, std::string>;

        //
        // Delete audit events older than specified days.
        //
        [[nodiscard]] auto prune_old_events(int days = 30)
            -> std::expected<void, std::string>;

    private:
        Database& m_db;
    };

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_STORAGE_AUDIT_EVENT_STORE_H
