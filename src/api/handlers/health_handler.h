// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tony Narlock

#pragma once

#include <string>

namespace vigilant_canine::api {

/// Health check endpoint handlers
class HealthHandler {
public:
    /// Handle GET /api/v1/health - simple health check
    /// Returns {"status": "ok"}
    static std::string handle_health();
};

} // namespace vigilant_canine::api
