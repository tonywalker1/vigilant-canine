//
// vigilant-canine - Database Schema
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_STORAGE_SCHEMA_H
#define VIGILANT_CANINE_STORAGE_SCHEMA_H

#include <string_view>

namespace vigilant_canine::schema {

    // Current schema version
    inline constexpr int CURRENT_VERSION = 2;  // Phase 2: added journal_events

    // Schema version table
    inline constexpr std::string_view DDL_SCHEMA_VERSION = R"(
CREATE TABLE IF NOT EXISTS schema_version (
    version     INTEGER PRIMARY KEY,
    applied_at  TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
)
)";

    // File baselines table
    inline constexpr std::string_view DDL_BASELINES = R"(
CREATE TABLE IF NOT EXISTS baselines (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    path        TEXT NOT NULL,
    hash_alg    TEXT NOT NULL,
    hash_value  TEXT NOT NULL,
    size        INTEGER NOT NULL,
    mode        INTEGER NOT NULL,
    uid         INTEGER NOT NULL,
    gid         INTEGER NOT NULL,
    mtime_ns    INTEGER NOT NULL,
    source      TEXT NOT NULL,
    deployment  TEXT,
    created_at  TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
    updated_at  TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
    UNIQUE(path, deployment)
)
)";

    inline constexpr std::string_view DDL_BASELINES_IDX_PATH = R"(
CREATE INDEX IF NOT EXISTS idx_baselines_path ON baselines(path)
)";

    inline constexpr std::string_view DDL_BASELINES_IDX_SOURCE = R"(
CREATE INDEX IF NOT EXISTS idx_baselines_source ON baselines(source)
)";

    // Alerts table
    inline constexpr std::string_view DDL_ALERTS = R"(
CREATE TABLE IF NOT EXISTS alerts (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    severity    TEXT NOT NULL,
    category    TEXT NOT NULL,
    path        TEXT,
    summary     TEXT NOT NULL,
    details     TEXT,
    source      TEXT NOT NULL,
    acknowledged INTEGER NOT NULL DEFAULT 0,
    created_at  TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
)
)";

    inline constexpr std::string_view DDL_ALERTS_IDX_SEVERITY = R"(
CREATE INDEX IF NOT EXISTS idx_alerts_severity ON alerts(severity)
)";

    inline constexpr std::string_view DDL_ALERTS_IDX_CREATED = R"(
CREATE INDEX IF NOT EXISTS idx_alerts_created ON alerts(created_at)
)";

    inline constexpr std::string_view DDL_ALERTS_IDX_PATH = R"(
CREATE INDEX IF NOT EXISTS idx_alerts_path ON alerts(path)
)";

    // Scan history table
    inline constexpr std::string_view DDL_SCANS = R"(
CREATE TABLE IF NOT EXISTS scans (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    scan_type   TEXT NOT NULL,
    started_at  TEXT NOT NULL,
    finished_at TEXT,
    files_checked INTEGER DEFAULT 0,
    changes_found INTEGER DEFAULT 0,
    status      TEXT NOT NULL DEFAULT 'running'
)
)";

    // Phase 2: Journal events table
    inline constexpr std::string_view DDL_JOURNAL_EVENTS = R"(
CREATE TABLE IF NOT EXISTS journal_events (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    rule_name   TEXT NOT NULL,
    message     TEXT NOT NULL,
    priority    INTEGER NOT NULL,
    unit_name   TEXT,
    created_at  TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
)
)";

    inline constexpr std::string_view DDL_JOURNAL_EVENTS_IDX_RULE = R"(
CREATE INDEX IF NOT EXISTS idx_journal_events_rule ON journal_events(rule_name)
)";

    inline constexpr std::string_view DDL_JOURNAL_EVENTS_IDX_CREATED = R"(
CREATE INDEX IF NOT EXISTS idx_journal_events_created ON journal_events(created_at)
)";

}  // namespace vigilant_canine::schema

#endif  // VIGILANT_CANINE_STORAGE_SCHEMA_H
