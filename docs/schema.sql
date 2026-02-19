-- Vigilant Canine Database Schema
-- Schema version: 3 (Phase 3 - Audit subsystem)

-- Schema version tracking
CREATE TABLE IF NOT EXISTS schema_version (
    version     INTEGER PRIMARY KEY,
    applied_at  TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);

-- File integrity baselines
-- RETENTION: Never pruned (reference data needed for integrity checks)
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
    source      TEXT NOT NULL,           -- 'rpm', 'dpkg', 'manual', 'user:username'
    deployment  TEXT,                    -- Optional: tracks user-specific deployments
    created_at  TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
    updated_at  TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
    UNIQUE(path, deployment)
);

CREATE INDEX IF NOT EXISTS idx_baselines_path ON baselines(path);
CREATE INDEX IF NOT EXISTS idx_baselines_source ON baselines(source);

-- Security alerts
-- RETENTION: Default 90 days (configurable via retention.alert_days)
CREATE TABLE IF NOT EXISTS alerts (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    severity    TEXT NOT NULL,           -- 'info', 'warning', 'critical'
    category    TEXT NOT NULL,           -- 'file_integrity', 'suspicious_log', etc.
    path        TEXT,                    -- Optional: file path associated with alert
    summary     TEXT NOT NULL,
    details     TEXT,                    -- Optional: additional context (JSON)
    source      TEXT NOT NULL,           -- 'fanotify', 'journal', 'audit'
    acknowledged INTEGER NOT NULL DEFAULT 0,
    created_at  TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);

CREATE INDEX IF NOT EXISTS idx_alerts_severity ON alerts(severity);
CREATE INDEX IF NOT EXISTS idx_alerts_created ON alerts(created_at);
CREATE INDEX IF NOT EXISTS idx_alerts_path ON alerts(path);

-- Baseline scan history
-- RETENTION: Default 90 days (configurable via retention.scan_days)
CREATE TABLE IF NOT EXISTS scans (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    scan_type       TEXT NOT NULL,      -- 'boot', 'scheduled', 'manual'
    started_at      TEXT NOT NULL,
    finished_at     TEXT,
    files_checked   INTEGER DEFAULT 0,
    changes_found   INTEGER DEFAULT 0,
    status          TEXT NOT NULL DEFAULT 'running'  -- 'running', 'completed', 'failed'
);

-- Phase 2: systemd journal events
-- RETENTION: Default 30 days (configurable via retention.journal_event_days)
CREATE TABLE IF NOT EXISTS journal_events (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    rule_name   TEXT NOT NULL,           -- Rule that triggered this event
    message     TEXT NOT NULL,           -- Journal message content
    priority    INTEGER NOT NULL,        -- syslog priority (0=emerg, 7=debug)
    unit_name   TEXT,                    -- Optional: systemd unit name
    created_at  TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);

CREATE INDEX IF NOT EXISTS idx_journal_events_rule ON journal_events(rule_name);
CREATE INDEX IF NOT EXISTS idx_journal_events_created ON journal_events(created_at);

-- Phase 3: Linux audit subsystem events
-- RETENTION: Default 30 days (configurable via retention.audit_event_days)
CREATE TABLE IF NOT EXISTS audit_events (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    rule_name    TEXT NOT NULL,          -- Rule that triggered this event
    event_type   TEXT NOT NULL,          -- 'ProcessExecution', 'NetworkConnection', etc.
    pid          INTEGER,                -- Process ID
    uid          INTEGER,                -- User ID
    username     TEXT,                   -- Username (resolved from UID)
    exe_path     TEXT,                   -- Executable path
    command_line TEXT,                   -- Command line (sanitized)
    details      TEXT,                   -- Type-specific fields (JSON)
    created_at   TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);

CREATE INDEX IF NOT EXISTS idx_audit_events_rule ON audit_events(rule_name);
CREATE INDEX IF NOT EXISTS idx_audit_events_type ON audit_events(event_type);
CREATE INDEX IF NOT EXISTS idx_audit_events_created ON audit_events(created_at);
CREATE INDEX IF NOT EXISTS idx_audit_events_uid ON audit_events(uid);

-- Database Retention Policy
--
-- The daemon automatically prunes old records to prevent unbounded growth.
-- Cleanup runs:
--   1. Once at daemon startup
--   2. Periodically based on retention.interval_hours (default: 24 hours)
--
-- Default retention periods:
--   - alerts: 90 days (security incidents, referenced longer)
--   - audit_events: 30 days (high-volume forensic data)
--   - journal_events: 30 days (high-volume log data)
--   - scans: 90 days (audit trail for debugging)
--   - baselines: Never pruned (reference data)
--   - schema_version: Never pruned (migration tracking)
--
-- All retention periods are configurable via [retention] in config.toml.
-- Setting a retention period to 0 means "keep forever" for that table.
--
-- Cleanup failures are logged as warnings but are non-fatal.
-- The database continues to work even if cleanup fails (just grows larger).
