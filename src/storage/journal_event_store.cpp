//
// vigilant-canine - Journal Event Storage Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <storage/journal_event_store.h>

#include <format>

namespace vigilant_canine {

    auto JournalEventStore::get_recent(int limit)
        -> std::expected<std::vector<JournalEventRecord>, std::string> {

        auto stmt_result = m_db.prepare(R"(
            SELECT id, rule_name, message, priority, unit_name, created_at
            FROM journal_events
            ORDER BY created_at DESC
            LIMIT ?
        )");

        if (!stmt_result) {
            return std::unexpected(stmt_result.error());
        }

        sqlite3_stmt* stmt = *stmt_result;
        sqlite3_bind_int(stmt, 1, limit);

        std::vector<JournalEventRecord> events;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            JournalEventRecord event;
            event.id = sqlite3_column_int64(stmt, 0);

            const char* rule_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            event.rule_name = rule_name ? rule_name : "";

            const char* message = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            event.message = message ? message : "";

            event.priority = sqlite3_column_int(stmt, 3);

            const char* unit_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            if (unit_name) {
                event.unit_name = unit_name;
            }

            const char* created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            event.created_at = created_at ? created_at : "";

            events.push_back(std::move(event));
        }

        sqlite3_finalize(stmt);
        return events;
    }

    auto JournalEventStore::prune_old_events(int days)
        -> std::expected<void, std::string> {

        auto stmt_result = m_db.prepare(R"(
            DELETE FROM journal_events
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
            return std::unexpected(std::format("Failed to prune old journal events: {}",
                                                sqlite3_errmsg(m_db.handle())));
        }

        return {};
    }

}  // namespace vigilant_canine
