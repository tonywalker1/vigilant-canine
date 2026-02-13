// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tony Narlock

#pragma once

#include <storage/audit_event_store.h>
#include <storage/database.h>

#include <httplib.h>

namespace vigilant_canine::api {

// Journal event record (simplified for API)
struct JournalEventRecord {
    std::int64_t id{0};
    std::string rule_name;
    std::string message;
    int priority{0};
    std::string unit_name;
    std::string created_at;
};

/// Event endpoint handlers
class EventHandler {
public:
    EventHandler(Database& db, AuditEventStore& audit_store);

    /// Handle GET /api/v1/journal-events - list journal events with filtering
    void handle_journal_events(const httplib::Request& req, httplib::Response& res);

    /// Handle GET /api/v1/audit-events - list audit events with filtering
    void handle_audit_events(const httplib::Request& req, httplib::Response& res);

private:
    Database& db_;
    AuditEventStore& audit_store_;

    // Helper to query journal events
    auto get_journal_events(int limit, int offset)
        -> std::expected<std::vector<JournalEventRecord>, std::string>;
};

} // namespace vigilant_canine::api
