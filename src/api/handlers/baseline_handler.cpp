// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tony Narlock

#include "baseline_handler.h"
#include "../serialization/json.h"

namespace vigilant_canine::api {

BaselineHandler::BaselineHandler(BaselineStore& store) : store_(store) {}

void BaselineHandler::handle_list(const httplib::Request& req, httplib::Response& res) {
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

    // For Phase 2, we'll retrieve baselines by source filter if provided
    // Otherwise return empty (we don't have a "get all" method yet)
    std::vector<Baseline> baselines;

    if (req.has_param("source")) {
        std::string source = req.get_param_value("source");
        auto result = store_.find_by_source(source);

        if (!result) {
            res.status = 500;
            res.set_content(json::error_response("DATABASE_ERROR",
                result.error()), "application/json");
            return;
        }

        baselines = *result;
    }

    // Apply pagination
    std::vector<Baseline> paginated;
    int total = baselines.size();

    if (offset < total) {
        auto start = baselines.begin() + offset;
        auto end = baselines.begin() + std::min(offset + limit, total);
        paginated.assign(start, end);
    }

    // Serialize response
    std::string baselines_json = json::to_json(paginated);
    std::string response = json::paginated_response(baselines_json, "baselines",
                                                    total, limit, offset);

    res.status = 200;
    res.set_content(response, "application/json");
}

} // namespace vigilant_canine::api
