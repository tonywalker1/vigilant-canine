// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tony Narlock

#pragma once

#include <storage/alert_store.h>

#include <httplib.h>

namespace vigilant_canine::api {

/// Alert endpoint handlers
class AlertHandler {
public:
    explicit AlertHandler(AlertStore& store);

    /// Handle GET /api/v1/alerts - list alerts with filtering and pagination
    void handle_list(const httplib::Request& req, httplib::Response& res);

    /// Handle GET /api/v1/alerts/{id} - get specific alert
    void handle_get(const httplib::Request& req, httplib::Response& res);

    /// Handle POST /api/v1/alerts/{id}/acknowledge - mark alert as acknowledged
    void handle_acknowledge(const httplib::Request& req, httplib::Response& res);

    /// Handle DELETE /api/v1/alerts/{id}/acknowledge - un-acknowledge alert
    void handle_unacknowledge(const httplib::Request& req, httplib::Response& res);

private:
    AlertStore& store_;
};

} // namespace vigilant_canine::api
