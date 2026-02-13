//
// vigilant-canine - Audit Event Storage Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <storage/audit_event_store.h>

#include <format>

namespace vigilant_canine {

    auto AuditEventStore::insert(AuditEventRecord const& event)
        -> std::expected<std::int64_t, std::string> {

        auto stmt_result = m_db.prepare(R"(
            INSERT INTO audit_events (rule_name, event_type, pid, uid, username, exe_path, command_line, details)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        )");

        if (!stmt_result) {
            return std::unexpected(stmt_result.error());
        }

        sqlite3_stmt* stmt = *stmt_result;

        sqlite3_bind_text(stmt, 1, event.rule_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, event.event_type.c_str(), -1, SQLITE_TRANSIENT);

        if (event.pid) {
            sqlite3_bind_int(stmt, 3, static_cast<int>(*event.pid));
        } else {
            sqlite3_bind_null(stmt, 3);
        }

        if (event.uid) {
            sqlite3_bind_int(stmt, 4, static_cast<int>(*event.uid));
        } else {
            sqlite3_bind_null(stmt, 4);
        }

        if (event.username) {
            sqlite3_bind_text(stmt, 5, event.username->c_str(), -1, SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_null(stmt, 5);
        }

        if (event.exe_path) {
            sqlite3_bind_text(stmt, 6, event.exe_path->c_str(), -1, SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_null(stmt, 6);
        }

        if (event.command_line) {
            sqlite3_bind_text(stmt, 7, event.command_line->c_str(), -1, SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_null(stmt, 7);
        }

        if (event.details) {
            sqlite3_bind_text(stmt, 8, event.details->c_str(), -1, SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_null(stmt, 8);
        }

        int result_code = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (result_code != SQLITE_DONE) {
            return std::unexpected(std::format("Failed to insert audit event: {}",
                                                sqlite3_errmsg(m_db.handle())));
        }

        return m_db.last_insert_rowid();
    }

    auto AuditEventStore::get_recent(int limit)
        -> std::expected<std::vector<AuditEventRecord>, std::string> {

        auto stmt_result = m_db.prepare(R"(
            SELECT id, rule_name, event_type, pid, uid, username, exe_path, command_line, details, created_at
            FROM audit_events
            ORDER BY created_at DESC
            LIMIT ?
        )");

        if (!stmt_result) {
            return std::unexpected(stmt_result.error());
        }

        sqlite3_stmt* stmt = *stmt_result;
        sqlite3_bind_int(stmt, 1, limit);

        std::vector<AuditEventRecord> events;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            AuditEventRecord event;
            event.id = sqlite3_column_int64(stmt, 0);

            const char* rule_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            event.rule_name = rule_name ? rule_name : "";

            const char* event_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            event.event_type = event_type ? event_type : "";

            if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
                event.pid = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 3));
            }

            if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
                event.uid = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 4));
            }

            const char* username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            if (username) {
                event.username = username;
            }

            const char* exe_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            if (exe_path) {
                event.exe_path = exe_path;
            }

            const char* command_line = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            if (command_line) {
                event.command_line = command_line;
            }

            const char* details = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
            if (details) {
                event.details = details;
            }

            const char* created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
            event.created_at = created_at ? created_at : "";

            events.push_back(std::move(event));
        }

        sqlite3_finalize(stmt);
        return events;
    }

    auto AuditEventStore::get_by_rule(std::string const& rule_name, int limit)
        -> std::expected<std::vector<AuditEventRecord>, std::string> {

        auto stmt_result = m_db.prepare(R"(
            SELECT id, rule_name, event_type, pid, uid, username, exe_path, command_line, details, created_at
            FROM audit_events
            WHERE rule_name = ?
            ORDER BY created_at DESC
            LIMIT ?
        )");

        if (!stmt_result) {
            return std::unexpected(stmt_result.error());
        }

        sqlite3_stmt* stmt = *stmt_result;
        sqlite3_bind_text(stmt, 1, rule_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, limit);

        std::vector<AuditEventRecord> events;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            AuditEventRecord event;
            event.id = sqlite3_column_int64(stmt, 0);

            const char* rn = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            event.rule_name = rn ? rn : "";

            const char* et = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            event.event_type = et ? et : "";

            if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
                event.pid = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 3));
            }

            if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
                event.uid = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 4));
            }

            const char* username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            if (username) {
                event.username = username;
            }

            const char* exe_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            if (exe_path) {
                event.exe_path = exe_path;
            }

            const char* command_line = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            if (command_line) {
                event.command_line = command_line;
            }

            const char* details = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
            if (details) {
                event.details = details;
            }

            const char* created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
            event.created_at = created_at ? created_at : "";

            events.push_back(std::move(event));
        }

        sqlite3_finalize(stmt);
        return events;
    }

    auto AuditEventStore::get_by_type(std::string const& event_type, int limit)
        -> std::expected<std::vector<AuditEventRecord>, std::string> {

        auto stmt_result = m_db.prepare(R"(
            SELECT id, rule_name, event_type, pid, uid, username, exe_path, command_line, details, created_at
            FROM audit_events
            WHERE event_type = ?
            ORDER BY created_at DESC
            LIMIT ?
        )");

        if (!stmt_result) {
            return std::unexpected(stmt_result.error());
        }

        sqlite3_stmt* stmt = *stmt_result;
        sqlite3_bind_text(stmt, 1, event_type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, limit);

        std::vector<AuditEventRecord> events;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            AuditEventRecord event;
            event.id = sqlite3_column_int64(stmt, 0);

            const char* rn = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            event.rule_name = rn ? rn : "";

            const char* et = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            event.event_type = et ? et : "";

            if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
                event.pid = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 3));
            }

            if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
                event.uid = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 4));
            }

            const char* username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            if (username) {
                event.username = username;
            }

            const char* exe_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            if (exe_path) {
                event.exe_path = exe_path;
            }

            const char* command_line = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            if (command_line) {
                event.command_line = command_line;
            }

            const char* details = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
            if (details) {
                event.details = details;
            }

            const char* created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
            event.created_at = created_at ? created_at : "";

            events.push_back(std::move(event));
        }

        sqlite3_finalize(stmt);
        return events;
    }

    auto AuditEventStore::get_by_uid(std::uint32_t uid, int limit)
        -> std::expected<std::vector<AuditEventRecord>, std::string> {

        auto stmt_result = m_db.prepare(R"(
            SELECT id, rule_name, event_type, pid, uid, username, exe_path, command_line, details, created_at
            FROM audit_events
            WHERE uid = ?
            ORDER BY created_at DESC
            LIMIT ?
        )");

        if (!stmt_result) {
            return std::unexpected(stmt_result.error());
        }

        sqlite3_stmt* stmt = *stmt_result;
        sqlite3_bind_int(stmt, 1, static_cast<int>(uid));
        sqlite3_bind_int(stmt, 2, limit);

        std::vector<AuditEventRecord> events;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            AuditEventRecord event;
            event.id = sqlite3_column_int64(stmt, 0);

            const char* rn = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            event.rule_name = rn ? rn : "";

            const char* et = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            event.event_type = et ? et : "";

            if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
                event.pid = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 3));
            }

            if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
                event.uid = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 4));
            }

            const char* username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            if (username) {
                event.username = username;
            }

            const char* exe_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            if (exe_path) {
                event.exe_path = exe_path;
            }

            const char* command_line = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            if (command_line) {
                event.command_line = command_line;
            }

            const char* details = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
            if (details) {
                event.details = details;
            }

            const char* created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
            event.created_at = created_at ? created_at : "";

            events.push_back(std::move(event));
        }

        sqlite3_finalize(stmt);
        return events;
    }

    auto AuditEventStore::prune_old_events(int days)
        -> std::expected<void, std::string> {

        auto stmt_result = m_db.prepare(R"(
            DELETE FROM audit_events
            WHERE created_at < datetime('now', '-' || ? || ' days')
        )");

        if (!stmt_result) {
            return std::unexpected(stmt_result.error());
        }

        sqlite3_stmt* stmt = *stmt_result;
        sqlite3_bind_int(stmt, 1, days);

        int result_code = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (result_code != SQLITE_DONE) {
            return std::unexpected(std::format("Failed to prune old audit events: {}",
                                                sqlite3_errmsg(m_db.handle())));
        }

        return {};
    }

}  // namespace vigilant_canine
