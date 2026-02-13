// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tony Narlock

#include "alert_handler.h"
#include "../serialization/json.h"

#include <optional>
#include <string>

namespace vigilant_canine::api {

AlertHandler::AlertHandler(AlertStore& store) : store_(store) {}

void AlertHandler::handle_list(const httplib::Request& req, httplib::Response& res) {
    // Parse query parameters
    int limit = 100; // default
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

    // For Phase 2, we'll implement basic filtering
    // TODO: Add severity, acknowledged, category filters in later phases
    auto result = store_.get_recent(limit + offset);

    if (!result) {
        res.status = 500;
        res.set_content(json::error_response("DATABASE_ERROR",
            result.error()), "application/json");
        return;
    }

    // Apply offset (simple approach for Phase 2)
    std::vector<Alert> alerts;
    auto& all_alerts = *result;
    if (offset < static_cast<int>(all_alerts.size())) {
        auto start = all_alerts.begin() + offset;
        auto end = all_alerts.begin() + std::min(offset + limit,
                                                  static_cast<int>(all_alerts.size()));
        alerts.assign(start, end);
    }

    // Serialize response
    std::string alerts_json = json::to_json(alerts);
    std::string response = json::paginated_response(alerts_json, "alerts",
                                                    all_alerts.size(), limit, offset);

    res.status = 200;
    res.set_content(response, "application/json");
}

void AlertHandler::handle_get(const httplib::Request& req, httplib::Response& res) {
    // Extract alert ID from path (cpp-httplib captures are in req.matches)
    if (req.matches.size() < 2) {
        res.status = 400;
        res.set_content(json::error_response("INVALID_REQUEST",
            "Missing alert ID"), "application/json");
        return;
    }

    std::int64_t alert_id;
    try {
        alert_id = std::stoll(req.matches[1].str());
    } catch (...) {
        res.status = 400;
        res.set_content(json::error_response("INVALID_PARAMETER",
            "Invalid alert ID"), "application/json");
        return;
    }

    auto result = store_.find_by_id(alert_id);

    if (!result) {
        res.status = 500;
        res.set_content(json::error_response("DATABASE_ERROR",
            result.error()), "application/json");
        return;
    }

    if (!result->has_value()) {
        res.status = 404;
        res.set_content(json::error_response("NOT_FOUND",
            "Alert not found"), "application/json");
        return;
    }

    std::string alert_json = json::to_json(**result);
    res.status = 200;
    res.set_content(alert_json, "application/json");
}

void AlertHandler::handle_acknowledge(const httplib::Request& req, httplib::Response& res) {
    // Extract alert ID from path (cpp-httplib captures are in req.matches)
    if (req.matches.size() < 2) {
        res.status = 400;
        res.set_content(json::error_response("INVALID_REQUEST",
            "Missing alert ID"), "application/json");
        return;
    }

    std::int64_t alert_id;
    try {
        alert_id = std::stoll(req.matches[1].str());
    } catch (...) {
        res.status = 400;
        res.set_content(json::error_response("INVALID_PARAMETER",
            "Invalid alert ID"), "application/json");
        return;
    }

    auto ack_result = store_.acknowledge(alert_id);

    if (!ack_result) {
        res.status = 500;
        res.set_content(json::error_response("DATABASE_ERROR",
            ack_result.error()), "application/json");
        return;
    }

    // Return updated alert
    auto result = store_.find_by_id(alert_id);

    if (!result || !result->has_value()) {
        res.status = 404;
        res.set_content(json::error_response("NOT_FOUND",
            "Alert not found after acknowledgment"), "application/json");
        return;
    }

    std::string alert_json = json::to_json(**result);
    res.status = 200;
    res.set_content(alert_json, "application/json");
}

void AlertHandler::handle_unacknowledge(const httplib::Request& req, httplib::Response& res) {
    // Extract alert ID from path (cpp-httplib captures are in req.matches)
    if (req.matches.size() < 2) {
        res.status = 400;
        res.set_content(json::error_response("INVALID_REQUEST",
            "Missing alert ID"), "application/json");
        return;
    }

    std::int64_t alert_id;
    try {
        alert_id = std::stoll(req.matches[1].str());
    } catch (...) {
        res.status = 400;
        res.set_content(json::error_response("INVALID_PARAMETER",
            "Invalid alert ID"), "application/json");
        return;
    }

    auto unack_result = store_.unacknowledge(alert_id);

    if (!unack_result) {
        res.status = 500;
        res.set_content(json::error_response("DATABASE_ERROR",
            unack_result.error()), "application/json");
        return;
    }

    // Return updated alert
    auto result = store_.find_by_id(alert_id);

    if (!result || !result->has_value()) {
        res.status = 404;
        res.set_content(json::error_response("NOT_FOUND",
            "Alert not found after un-acknowledgment"), "application/json");
        return;
    }

    std::string alert_json = json::to_json(**result);
    res.status = 200;
    res.set_content(alert_json, "application/json");
}

} // namespace vigilant_canine::api
