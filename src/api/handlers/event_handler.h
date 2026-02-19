// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tony Narlock

#pragma once

#include <storage/audit_event_store.h>
#include <storage/journal_event_store.h>

#include <httplib.h>

namespace vigilant_canine::api {

/// Event endpoint handlers
class EventHandler {
public:
    EventHandler(JournalEventStore& journal_store, AuditEventStore& audit_store);

    /// Handle GET /api/v1/journal-events - list journal events with filtering
    void handle_journal_events(const httplib::Request& req, httplib::Response& res);

    /// Handle GET /api/v1/audit-events - list audit events with filtering
    void handle_audit_events(const httplib::Request& req, httplib::Response& res);

private:
    JournalEventStore& journal_store_;
    AuditEventStore& audit_store_;
};

} // namespace vigilant_canine::api
