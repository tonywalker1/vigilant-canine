# Database Retention Implementation Plan

## Context

The vigilant-canine daemon currently has no automatic database cleanup. Over time, the database grows unbounded as alerts, audit events, and journal events accumulate. This creates disk space issues and degrades query performance.

The `AuditEventStore::prune_old_events()` method exists but is never called. We need to:
1. Add similar pruning methods to other stores
2. Create a JournalEventStore class (journal events currently queried inline)
3. Add retention configuration
4. Integrate periodic cleanup into the daemon lifecycle

## Tables and Retention Policy

| Table | Retention Period | Rationale |
|---|---|---|
| `alerts` | 90 days | Security incidents, referenced longer for investigation |
| `audit_events` | 30 days | High-volume forensic data, standard log retention |
| `journal_events` | 30 days | High-volume log data, matches audit events |
| `scans` | 90 days | Audit trail of baseline verifications, useful for debugging |
| `baselines` | Never | Reference data, not pruned by age |
| `schema_version` | Never | Migration bookkeeping, never pruned |

## Implementation Steps

### 1. Create JournalEventStore Class

**Files to create:**
- `src/storage/journal_event_store.h`
- `src/storage/journal_event_store.cpp`

**Pattern:** Follow `AuditEventStore` exactly (see src/storage/audit_event_store.cpp:352-376)

**Methods:**
```cpp
class JournalEventStore {
    explicit JournalEventStore(Database& database);

    auto get_recent(int limit = 100)
        -> std::expected<std::vector<JournalEventRecord>, std::string>;

    auto prune_old_events(int days = 30)
        -> std::expected<void, std::string>;
};
```

**SQL pattern for pruning:**
```sql
DELETE FROM journal_events
WHERE created_at < datetime('now', '-' || ? || ' days')
```

### 2. Add Pruning Methods to Existing Stores

**AlertStore** (src/storage/alert_store.h and .cpp):
```cpp
auto prune_old_alerts(int days = 90)
    -> std::expected<void, std::string>;
```

**ScanStore** (new class, src/storage/scan_store.h and .cpp):
```cpp
class ScanStore {
    explicit ScanStore(Database& database);

    auto prune_old_scans(int days = 90)
        -> std::expected<void, std::string>;
};
```

All use the same SQL pattern: `DELETE FROM table WHERE created_at < datetime('now', '-X days')`

### 3. Add Retention Configuration

**File:** src/config/config.h

Add struct following pattern at lines 70-77 (ScanConfig):
```cpp
struct RetentionConfig {
    bool enabled{true};
    std::uint32_t interval_hours{24};
    std::uint32_t alert_days{90};
    std::uint32_t audit_event_days{30};
    std::uint32_t journal_event_days{30};
    std::uint32_t scan_days{90};
};
```

Add to Config struct (line 164):
```cpp
struct Config {
    // ... existing fields ...
    RetentionConfig retention;
};
```

**File:** src/config/config.cpp

Add `parse_retention()` helper following existing pattern, call from `load_config()`.

**File:** config/vigilant-canine.toml.example

Add section:
```toml
[retention]
# Automatic database cleanup
enabled = true

# Cleanup frequency (hours)
interval_hours = 24

# Retention periods (days) - 0 means keep forever
alert_days = 90              # Security alerts
audit_event_days = 30        # Audit subsystem events
journal_event_days = 30      # Journal log events
scan_days = 90               # Baseline scan history

# Note: baselines and schema_version tables are never pruned
```

### 4. Integrate Cleanup into Daemon

**File:** src/daemon/daemon.h

Add private members:
```cpp
std::unique_ptr<JournalEventStore> m_journal_event_store;
std::unique_ptr<ScanStore> m_scan_store;
std::chrono::system_clock::time_point m_last_retention_cleanup;
```

**File:** src/daemon/daemon.cpp

**Initialize stores** in `Daemon::initialize()` around line 260:
```cpp
m_journal_event_store = std::make_unique<JournalEventStore>(*m_database);
m_scan_store = std::make_unique<ScanStore>(*m_database);
m_last_retention_cleanup = std::chrono::system_clock::time_point{};  // Epoch
```

**Add cleanup method:**
```cpp
auto Daemon::run_retention_cleanup() -> void {
    if (!m_config.retention.enabled) {
        return;
    }

    sd_journal_print(LOG_INFO, "vigilant-canined: Running database retention cleanup");

    // Prune each table with configured retention period
    // Log warnings on failure but continue (non-fatal)

    if (m_config.retention.alert_days > 0) {
        auto result = m_alert_store->prune_old_alerts(m_config.retention.alert_days);
        if (!result) {
            sd_journal_print(LOG_WARNING,
                "vigilant-canined: Failed to prune alerts: %s",
                result.error().c_str());
        }
    }

    if (m_config.retention.audit_event_days > 0) {
        auto result = m_audit_event_store->prune_old_events(
            m_config.retention.audit_event_days);
        if (!result) {
            sd_journal_print(LOG_WARNING,
                "vigilant-canined: Failed to prune audit events: %s",
                result.error().c_str());
        }
    }

    if (m_config.retention.journal_event_days > 0) {
        auto result = m_journal_event_store->prune_old_events(
            m_config.retention.journal_event_days);
        if (!result) {
            sd_journal_print(LOG_WARNING,
                "vigilant-canined: Failed to prune journal events: %s",
                result.error().c_str());
        }
    }

    if (m_config.retention.scan_days > 0) {
        auto result = m_scan_store->prune_old_scans(m_config.retention.scan_days);
        if (!result) {
            sd_journal_print(LOG_WARNING,
                "vigilant-canined: Failed to prune scans: %s",
                result.error().c_str());
        }
    }

    m_last_retention_cleanup = std::chrono::system_clock::now();
    sd_journal_print(LOG_INFO, "vigilant-canined: Retention cleanup completed");
}
```

**Call cleanup** in `Daemon::run()`:

1. **At startup** (after distributed scanner starts, around line 525):
   ```cpp
   // Run retention cleanup on startup
   run_retention_cleanup();
   ```

2. **Periodically in main loop** (after drain_escalated_events, around line 536):
   ```cpp
   // Check if retention cleanup is due
   auto now = std::chrono::system_clock::now();
   auto elapsed = std::chrono::duration_cast<std::chrono::hours>(
       now - m_last_retention_cleanup);
   if (elapsed.count() >= m_config.retention.interval_hours) {
       run_retention_cleanup();
   }
   ```

### 5. Update API Daemon

**File:** src/api/handlers/event_handler.h

Change constructor and member:
```cpp
// Replace:
Database& db_;

// With:
JournalEventStore& journal_store_;
```

Remove private helper method `get_journal_events()`.

**File:** src/api/handlers/event_handler.cpp

- Update constructor parameter
- Replace inline SQL in `handle_journal_events()` with `journal_store_.get_recent()`
- Delete the `get_journal_events()` helper method (lines 148-186)

**File:** src/api/main.cpp

Create JournalEventStore and pass to EventHandler:
```cpp
auto journal_store = JournalEventStore{db};
auto event_handler = EventHandler{journal_store, audit_event_store};
```

### 6. Add Tests

**Create test files:**

- `tests/storage/journal_event_store_test.cpp` - Test get_recent() and prune_old_events()
- `tests/storage/scan_store_test.cpp` - Test prune_old_scans()
- Update `tests/storage/alert_store_test.cpp` - Add test for prune_old_alerts()

**Test pattern** (follow database_test.cpp):
1. Insert records with backdated timestamps using `datetime('now', '-X days')`
2. Call prune method
3. Query database to verify only recent records remain

### 7. Update Documentation

**File:** docs/configuration.md (create if missing)

Document retention configuration:
- Explain each retention period setting
- Note that 0 means "keep forever"
- Warn about disk space with retention disabled
- Explain cleanup runs at startup + periodic interval

**File:** docs/architecture.md

Add section on database retention:
- Cleanup timing (startup + periodic)
- Which tables are pruned vs. preserved forever
- Error handling (non-fatal warnings)

## Critical Files and Line References

- **Canonical prune pattern:** src/storage/audit_event_store.cpp:352-376
- **Config struct pattern:** src/config/config.h:70-77 (ScanConfig)
- **Main loop integration:** src/daemon/daemon.cpp:530-547
- **Store initialization:** src/daemon/daemon.cpp:256-260
- **Journal event inline SQL:** src/api/handlers/event_handler.cpp:148-186

## Verification Steps

1. **Build and unit test:**
   ```bash
   ./build.sh -gdt
   cd build/gcc-debug
   ctest --output-on-failure -R storage
   ```

2. **Test retention config parsing:**
   - Create test config with `[retention]` section
   - Load config and verify values parse correctly

3. **Integration test:**
   - Populate database with old alerts/events (use SQL with backdated timestamps)
   - Start daemon with retention enabled
   - Verify cleanup runs at startup (check journal logs)
   - Wait for periodic interval (or manually advance time in test)
   - Verify old records are deleted

4. **API test:**
   - Verify EventHandler still returns journal events correctly
   - Verify refactored code doesn't break API responses

5. **Production verification:**
   - Install on test system
   - Monitor database size over time
   - Check journal logs for cleanup success/failure messages
   - Verify systemd service restart triggers cleanup

## Trade-offs and Design Decisions

**Why JournalEventStore?**
- Consistency with AlertStore/AuditEventStore pattern
- Encapsulation (SQL in one place)
- Testability and maintainability
- Future-proofs for additional journal operations

**Why periodic cleanup in main loop?**
- Simple, low overhead (~0.01% CPU for daily cleanup)
- Matches existing pattern (drain_escalated_events)
- No thread synchronization complexity
- Observable via journal logs

**Why cleanup at startup AND periodically?**
- Startup: handles extended daemon shutdowns gracefully
- Periodic: normal steady-state operation
- No downside (quick operation, runs once at start)

**Why tiered retention periods?**
- Reflects actual use patterns (alerts referenced longer)
- Gives users sensible defaults with customization
- Balances disk space vs. forensic value

**Error handling:**
- All prune operations are non-fatal (log warnings, continue)
- Database continues to work even if cleanup fails
- Failure mode: database grows larger, but system keeps running
