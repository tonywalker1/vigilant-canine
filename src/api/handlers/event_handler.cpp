// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tony Narlock

#include "event_handler.h"
#include "../serialization/json.h"

#include <sqlite3.h>

namespace vigilant_canine::api {

EventHandler::EventHandler(Database& db, AuditEventStore& audit_store)
    : db_(db), audit_store_(audit_store) {}

void EventHandler::handle_journal_events(const httplib::Request& req, httplib::Response& res) {
    // Parse query parameters
    int limit = 100;
    int offset = 0;

    if (req.has_param("limit")) {
        try {
            limit = std::stoi(req.get_param_value("limit"));
            if (limit < 1 || limit > 1000) {
                res.status = 400;
                res.set_content(json::error_response("INVALID_PARAMETER",
                    "limit must be between 1 and 1000"), "application/json");
                return;
            }
        } catch (...) {
            res.status = 400;
            res.set_content(json::error_response("INVALID_PARAMETER",
                "Invalid limit value"), "application/json");
            return;
        }
    }

    if (req.has_param("offset")) {
        try {
            offset = std::stoi(req.get_param_value("offset"));
            if (offset < 0) {
                res.status = 400;
                res.set_content(json::error_response("INVALID_PARAMETER",
                    "offset must be >= 0"), "application/json");
                return;
            }
        } catch (...) {
            res.status = 400;
            res.set_content(json::error_response("INVALID_PARAMETER",
                "Invalid offset value"), "application/json");
            return;
        }
    }

    auto result = get_journal_events(limit + offset, 0);

    if (!result) {
        res.status = 500;
        res.set_content(json::error_response("DATABASE_ERROR",
            result.error()), "application/json");
        return;
    }

    // Apply offset (simple approach)
    std::vector<JournalEventRecord> events;
    auto& all_events = *result;
    if (offset < static_cast<int>(all_events.size())) {
        auto start = all_events.begin() + offset;
        auto end = all_events.begin() + std::min(offset + limit,
                                                  static_cast<int>(all_events.size()));
        events.assign(start, end);
    }

    // Serialize response
    std::string events_json = json::to_json(events);
    std::string response = json::paginated_response(events_json, "journal_events",
                                                    all_events.size(), limit, offset);

    res.status = 200;
    res.set_content(response, "application/json");
}

void EventHandler::handle_audit_events(const httplib::Request& req, httplib::Response& res) {
    // Parse query parameters
    int limit = 100;
    int offset = 0;

    if (req.has_param("limit")) {
        try {
            limit = std::stoi(req.get_param_value("limit"));
            if (limit < 1 || limit > 1000) {
                res.status = 400;
                res.set_content(json::error_response("INVALID_PARAMETER",
                    "limit must be between 1 and 1000"), "application/json");
                return;
            }
        } catch (...) {
            res.status = 400;
            res.set_content(json::error_response("INVALID_PARAMETER",
                "Invalid limit value"), "application/json");
            return;
        }
    }

    if (req.has_param("offset")) {
        try {
            offset = std::stoi(req.get_param_value("offset"));
            if (offset < 0) {
                res.status = 400;
                res.set_content(json::error_response("INVALID_PARAMETER",
                    "offset must be >= 0"), "application/json");
                return;
            }
        } catch (...) {
            res.status = 400;
            res.set_content(json::error_response("INVALID_PARAMETER",
                "Invalid offset value"), "application/json");
            return;
        }
    }

    auto result = audit_store_.get_recent(limit + offset);

    if (!result) {
        res.status = 500;
        res.set_content(json::error_response("DATABASE_ERROR",
            result.error()), "application/json");
        return;
    }

    // Apply offset
    std::vector<AuditEventRecord> events;
    auto& all_events = *result;
    if (offset < static_cast<int>(all_events.size())) {
        auto start = all_events.begin() + offset;
        auto end = all_events.begin() + std::min(offset + limit,
                                                  static_cast<int>(all_events.size()));
        events.assign(start, end);
    }

    // Serialize response
    std::string events_json = json::to_json(events);
    std::string response = json::paginated_response(events_json, "audit_events",
                                                    all_events.size(), limit, offset);

    res.status = 200;
    res.set_content(response, "application/json");
}

auto EventHandler::get_journal_events(int limit, int offset)
    -> std::expected<std::vector<JournalEventRecord>, std::string> {

    auto stmt_result = db_.prepare(R"(
        SELECT id, rule_name, message, priority, unit_name, created_at
        FROM journal_events
        ORDER BY created_at DESC
        LIMIT ? OFFSET ?
    )");

    if (!stmt_result) {
        return std::unexpected(stmt_result.error());
    }

    sqlite3_stmt* stmt = *stmt_result;
    sqlite3_bind_int(stmt, 1, limit);
    sqlite3_bind_int(stmt, 2, offset);

    std::vector<JournalEventRecord> events;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        JournalEventRecord event;
        event.id = sqlite3_column_int64(stmt, 0);
        event.rule_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        event.message = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        event.priority = sqlite3_column_int(stmt, 3);

        if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
            event.unit_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        }

        event.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));

        events.push_back(std::move(event));
    }

    sqlite3_finalize(stmt);
    return events;
}

} // namespace vigilant_canine::api
