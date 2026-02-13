// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tony Narlock

#pragma once

#include <storage/baseline_store.h>

#include <httplib.h>

namespace vigilant_canine::api {

/// Baseline endpoint handlers
class BaselineHandler {
public:
    explicit BaselineHandler(BaselineStore& store);

    /// Handle GET /api/v1/baselines - list baselines with filtering and pagination
    void handle_list(const httplib::Request& req, httplib::Response& res);

private:
    BaselineStore& store_;
};

} // namespace vigilant_canine::api
