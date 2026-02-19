// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tony Narlock

#include "event_handler.h"
#include "../serialization/json.h"

namespace vigilant_canine::api {

EventHandler::EventHandler(JournalEventStore& journal_store, AuditEventStore& audit_store)
    : journal_store_(journal_store), audit_store_(audit_store) {}

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

    auto result = journal_store_.get_recent(limit + offset);

    if (!result) {
        res.status = 500;
        res.set_content(json::error_response("DATABASE_ERROR",
            result.error()), "application/json");
        return;
    }

    // Apply offset (simple approach)
    std::vector<vigilant_canine::JournalEventRecord> events;
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

} // namespace vigilant_canine::api
