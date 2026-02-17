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

    // Build filter from query parameters
    AlertFilter filter;

    // Parse severity filter
    if (req.has_param("severity")) {
        std::string severity_str = req.get_param_value("severity");
        auto severity_opt = parse_severity(severity_str);
        if (!severity_opt) {
            res.status = 400;
            res.set_content(json::error_response("INVALID_PARAMETER",
                "severity must be 'INFO', 'WARNING', or 'CRITICAL'"), "application/json");
            return;
        }
        filter.severity = *severity_opt;
    }

    // Parse acknowledged filter
    if (req.has_param("acknowledged")) {
        std::string ack_str = req.get_param_value("acknowledged");
        if (ack_str == "true") {
            filter.acknowledged = true;
        } else if (ack_str == "false") {
            filter.acknowledged = false;
        } else {
            res.status = 400;
            res.set_content(json::error_response("INVALID_PARAMETER",
                "acknowledged must be 'true' or 'false'"), "application/json");
            return;
        }
    }

    // Parse category filter
    if (req.has_param("category")) {
        filter.category = req.get_param_value("category");
    }

    // Parse since_id filter
    if (req.has_param("since_id")) {
        try {
            filter.since_id = std::stoll(req.get_param_value("since_id"));
        } catch (...) {
            res.status = 400;
            res.set_content(json::error_response("INVALID_PARAMETER",
                "Invalid since_id value"), "application/json");
            return;
        }
    }

    // Query with filter
    auto result = store_.get_filtered(filter, limit, offset);

    if (!result) {
        res.status = 500;
        res.set_content(json::error_response("DATABASE_ERROR",
            result.error()), "application/json");
        return;
    }

    auto const& alerts = *result;

    // Serialize response (total is approximate for filtered results)
    std::string alerts_json = json::to_json(alerts);
    std::string response = json::paginated_response(alerts_json, "alerts",
                                                    alerts.size(), limit, offset);

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
