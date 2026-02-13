// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tony Narlock

#pragma once

#include <storage/alert_store.h>
#include <storage/baseline_store.h>
#include <storage/audit_event_store.h>
#include <api/handlers/event_handler.h>

#include <string>
#include <vector>

namespace vigilant_canine::api::json {

/// Escape a string for JSON (handles quotes, backslashes, control characters)
std::string escape(const std::string& str);

/// Serialize an Alert to JSON
std::string to_json(const Alert& alert);

/// Serialize a vector of Alerts to JSON array
std::string to_json(const std::vector<Alert>& alerts);

/// Serialize a Baseline to JSON
std::string to_json(const Baseline& baseline);

/// Serialize a vector of Baselines to JSON array
std::string to_json(const std::vector<Baseline>& baselines);

/// Serialize a JournalEventRecord to JSON
std::string to_json(const JournalEventRecord& event);

/// Serialize a vector of JournalEventRecords to JSON array
std::string to_json(const std::vector<JournalEventRecord>& events);

/// Serialize an AuditEventRecord to JSON
std::string to_json(const AuditEventRecord& event);

/// Serialize a vector of AuditEventRecords to JSON array
std::string to_json(const std::vector<AuditEventRecord>& events);

/// Create a paginated response with total count
std::string paginated_response(const std::string& data_array, const std::string& key,
                               int total, int limit, int offset);

/// Create an error response
std::string error_response(const std::string& code, const std::string& message);

} // namespace vigilant_canine::api::json
