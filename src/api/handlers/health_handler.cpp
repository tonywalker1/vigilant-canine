// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tony Narlock

#include "health_handler.h"

namespace vigilant_canine::api {

std::string HealthHandler::handle_health() {
    return R"({"status":"ok"})";
}

} // namespace vigilant_canine::api
