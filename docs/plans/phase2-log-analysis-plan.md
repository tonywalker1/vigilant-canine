# Plan: Phase 1 Audit + Phase 2 Proposal

## Context

The user wants to move to Phase 2 (log analysis). Before doing so, the Phase 1 plan
(`docs/plans/phase1-file-integrity-daemon.md`) must be audited against the actual code so it
can serve as a regeneration blueprint. Then Phase 2 needs a similarly detailed plan.

## Deliverables

1. **Edit Phase 1 plan in-place** — fix all discrepancies so the plan matches reality
2. **Remove Boost::system** — dead dependency, no source file uses it
3. **Write Phase 2 plan** — `docs/plans/phase2-log-analysis.md` in same detailed style
4. **Implement Phase 2** — following the plan's 11-step order

---

## Part 1: Phase 1 Plan Corrections

The Phase 1 plan has significant drift from the implemented code. These corrections are needed
to make the plan an accurate regeneration blueprint. Edit
`docs/plans/phase1-file-integrity-daemon.md` in-place (git history preserves the original).

### 1.1 Test Count (Critical)

- **Plan claims**: 210 tests
- **Actual**: 76 TEST/TEST_F macros across 11 test files
- **Breakdown**: hash(8), config(4), database(6), detector(5), strategy(7), scanner(7),
  event_bus(7), fanotify_monitor(4), alert_dispatcher(7), policy_engine(10), package_verifier(11)
- **Fix**: Update all references from "210" to "76"

### 1.2 Source Tree (Multiple Errors)

The plan's source tree listing has wrong paths, wrong filenames, and missing files.

| Plan Says | Actual | Issue |
|-----------|--------|-------|
| `monitor/scanner.h/cpp` | `scanner/scanner.h/cpp` | Wrong directory |
| (not listed) | `events/event.cpp` | Missing from plan |
| (not listed) | `storage/schema.h` | Missing from plan |
| (not listed) | `baseline/strategy.h/cpp` | Missing from plan tree (listed elsewhere) |
| `config/policy.h/cpp` | Does not exist | Listed as "deferred" but also in source tree |
| `config/paths.h/cpp` | Does not exist | Same |
| `distro/traditional.cpp` | Does not exist | Merged into `baseline/strategy.cpp` |
| `distro/ostree.cpp` | Does not exist | Merged into `baseline/strategy.cpp` |
| `distro/flatpak.cpp` | Does not exist | Not implemented |
| `monitor/watcher.h/cpp` | Does not exist | Not needed with EventBus |
| `dispatch/journal.h/cpp` | Does not exist | Integrated into `alert_dispatcher` |

**Corrected source tree:**
```
src/
    daemon/
        main.cpp
        daemon.h / daemon.cpp
    config/
        config.h / config.cpp
    core/
        hash.h / hash.cpp
        types.h
    baseline/
        strategy.h / strategy.cpp
    distro/
        detector.h / detector.cpp
    events/
        event.h / event.cpp
        event_bus.h / event_bus.cpp
    scanner/
        scanner.h / scanner.cpp
    monitor/
        fanotify_monitor.h / fanotify_monitor.cpp
    storage/
        database.h / database.cpp
        schema.h
        baseline_store.h / baseline_store.cpp
        alert_store.h / alert_store.cpp
    dispatch/
        alert_dispatcher.h / alert_dispatcher.cpp
    policy/
        policy_engine.h / policy_engine.cpp
    package/
        package_verifier.h / package_verifier.cpp
```

### 1.3 CMake Section (Significantly Wrong)

The plan's CMake section lists files that don't exist and uses wrong names.

**Key corrections:**
- Source file list must match the 15 actual .cpp files (see 1.2 above)
- `package_verifier.cpp` is NOT linked into the daemon — only compiled in tests
- BLAKE3 uses `pkg_check_modules(BLAKE3 REQUIRED libblake3)`, not vendored
- toml++ uses `FetchContent` (v3.4.0 from GitHub), not just "header-only"
- Hinder is conditionally linked (`if(EXISTS external/hinder/...)`)
- Boost::system IS linked but is UNUSED in source code (dead dependency)

### 1.4 Database Schema

- **Plan says**: "schema_version/scans deferred"
- **Actual**: ALL 4 tables (schema_version, baselines, alerts, scans) are implemented in
  `src/storage/schema.h` with `CURRENT_VERSION = 1`
- Schema DDL is in `schema.h` as `inline constexpr std::string_view` constants

### 1.5 Fanotify API Section

The plan shows a `FanotifySource` class using `boost::asio::io_context` and
`boost::asio::posix::stream_descriptor`. The actual implementation is `FanotifyMonitor` with a
simple `std::thread` + `poll()` loop (no Asio). The plan's fanotify section needs complete
replacement.

### 1.6 ostree/Flatpak Sections

The plan has full API specs for `distro/ostree.h` and `distro/flatpak.h` (OstreeDeployment,
FlatpakInstallation, etc.). These files don't exist. OSTree support is partially handled by
`OstreeStrategy` in `baseline/strategy.h`. The plan should mark these as aspirational/future
and remove them from the "implemented" source tree.

### 1.7 Alert Dispatch Section

Plan shows a `Dispatcher` class in `dispatch/dispatcher.h`. Actual is `AlertDispatcher` in
`dispatch/alert_dispatcher.h` with different API (has `start()/stop()` lifecycle, subscribes
to EventBus, owns an `EventSubscription`).

### 1.8 Minor Naming Differences

| Plan Name | Actual Name |
|-----------|-------------|
| `distro/detect.h` | `distro/detector.h` |
| `monitor/fanotify.h` | `monitor/fanotify_monitor.h` |
| `dispatch/dispatcher.h` | `dispatch/alert_dispatcher.h` |

### 1.9 Boost Dependency

Boost::system is `find_package(Boost REQUIRED COMPONENTS system)` and linked to the daemon,
but **no source file includes any Boost header**. **Action: remove it** from CMakeLists.txt
(lines 20 and 72). Re-add if Phase 3 needs Asio.

### 1.10 Package Verifier Not Linked to Daemon

`package_verifier.cpp` is only compiled in its test (`tests/package/CMakeLists.txt` includes
it via `../../src/package/package_verifier.cpp`). No daemon source file includes or references
`PackageVerifier`. It was built as a standalone utility (Step 11) but never wired into the
daemon runtime. **Action: note as "test-only utility" in plan; wiring into daemon deferred to
when actual package-update detection is implemented.**

---

## Part 2: Phase 2 — Log Analysis via systemd Journal

### Architectural Decision: No Asio Migration

The Phase 1 plan suggested migrating to Boost.Asio for Phase 2. **Recommendation: skip it.**

- FanotifyMonitor already demonstrates the pattern: dedicated thread + poll() loop + EventBus
- JournalMonitor can follow the identical pattern using `sd_journal_wait()`
- Journal event rates for home/small-office: tens/minute, not hundreds/second
- Synchronous EventBus handles this without contention
- Simple main loop remains appropriate
- Asio migration deferred to Phase 3 (auditd) if high-throughput netlink demands it

This also means the unused Boost::system dependency is **removed** from CMakeLists.txt
(user confirmed).

---

## Implementation Guide

### API-Level Implementation Details

**EventSubscription validity checking:**
```cpp
// ✅ Correct
if (m_subscription.is_valid()) {
    m_event_bus.unsubscribe(m_subscription);
}

// ❌ Wrong - no implicit bool conversion
if (m_subscription) {  // Compilation error
    m_event_bus.unsubscribe(m_subscription);
}
```

**Event timestamps:**
- Event timestamps are set at construction via `std::chrono::system_clock::now()`
- Timestamps are immutable after construction
- **Important for tests**: Create new Event objects for time-based tests, don't reuse

```cpp
// ❌ Wrong - both events have same timestamp
Event event{data, severity, "test"};
bus.publish(event);
std::this_thread::sleep_for(std::chrono::seconds(2));
bus.publish(event);  // Still has original timestamp!

// ✅ Correct - each event gets current timestamp
{
    Event event{data, severity, "test"};
    bus.publish(event);
}
std::this_thread::sleep_for(std::chrono::seconds(2));
{
    Event event{data, severity, "test"};  // New timestamp
    bus.publish(event);
}
```

**C++ standard library compatibility:**
- Use iterator-based algorithms from `<algorithm>`, NOT `std::ranges`
- C++23 ranges support is incomplete in current toolchain

```cpp
// ✅ Correct
std::all_of(vec.begin(), vec.end(), [](auto const& x) { return predicate(x); })

// ❌ May not compile
std::ranges::all_of(vec, [](auto const& x) { return predicate(x); })
```

### Key Type Locations

Quick reference for finding type definitions:

| Type | Header | Notes |
|------|--------|-------|
| `Alert` | `storage/alert_store.h` | Used by DbusNotifier |
| `Event` / `EventData` | `events/event.h` | Core event types |
| `EventSubscription` | `events/event_bus.h` | Subscription handle |
| `JournalEntry` | `journal/journal_fields.h` | Intermediate representation |
| `CorrelationRule` | `correlation/correlation_engine.h` | Time-window rules |

### Forward Declaration Pattern

When forward-declaring Phase 2 types, keep the namespace open:

```cpp
// ✅ Correct
#include <events/event.h>
#include <storage/alert_store.h>

namespace vigilant_canine {

    // Forward declaration
    class DbusNotifier;

    class AlertDispatcher {  // Same namespace continues
        // ...
        DbusNotifier* m_dbus_notifier{nullptr};
    };

}  // namespace vigilant_canine

// ❌ Wrong - closes and reopens namespace
namespace vigilant_canine {
    class DbusNotifier;
}  // Namespace closed

namespace vigilant_canine {  // Error-prone reopening
    class AlertDispatcher {
        // ...
    };
}
```

### Test CMakeLists.txt Pattern

Each Phase 2 test follows this dependency pattern:

```cmake
add_executable(my_component_test
    my_component_test.cpp
    ../../src/my/component.cpp
    # Include source dependencies:
    # - If using Database: ../../src/storage/database.cpp
    # - If using AlertStore: ../../src/storage/alert_store.cpp + database.cpp
    # - If using EventBus: ../../src/events/event_bus.cpp
    # - If using Event types: ../../src/events/event.cpp
)

target_link_libraries(my_component_test PRIVATE
    GTest::gtest
    GTest::gtest_main
    # Add libraries based on dependencies:
    # - If using Database/AlertStore: SQLite::SQLite3
    # - If using sd_journal: ${SYSTEMD_LIBRARIES}
    # - If using sd_bus: ${SYSTEMD_LIBRARIES}
)

# Hinder (required if using Database or any Hinder exceptions)
if(TARGET hinder::hinder)
    target_link_libraries(my_component_test PRIVATE hinder::hinder)
endif()

target_include_directories(my_component_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    # Add based on dependencies:
    # - If using systemd: ${SYSTEMD_INCLUDE_DIRS}
)

gtest_discover_tests(my_component_test)
```

**Dependency quick reference:**
- JournalMonitor tests: Need `${SYSTEMD_LIBRARIES}` (sd_journal)
- DbusNotifier tests: Need `SQLite::SQLite3` (Alert), `${SYSTEMD_LIBRARIES}` (sd_bus), `database.cpp`, `alert_store.cpp`, Hinder
- CorrelationEngine tests: Need `event.cpp`, `event_bus.cpp`

### Build Dependency Graph

**Important**: While tasks are numbered sequentially, build dependencies require some interleaving.

```
Task 9 (AlertDispatcher) → Requires daemon sources updated → Partial Task 12
                          ↓
                    Add Phase 2 .cpp files to daemon CMakeLists.txt
                          ↓
                    Continue with remaining tasks
                          ↓
                    Complete Task 12 (daemon wiring)
```

**Critical path adjustment:**
1. Complete Tasks 1-8 as specified
2. Update Task 9 (AlertDispatcher code)
3. **Before Task 9 compiles**: Add Phase 2 sources to daemon `CMakeLists.txt`:
   ```cmake
   add_executable(vigilant-canined
       # ... existing sources ...
       # Phase 2 components
       src/journal/journal_rule.cpp
       src/journal/journal_monitor.cpp
       src/correlation/correlation_engine.cpp
       src/notify/dbus_notifier.cpp
   )
   ```
4. Continue Tasks 10-12 as specified

This is necessary because `AlertDispatcher` now includes `<notify/dbus_notifier.h>` and calls
its methods, requiring the implementation to be linked into the daemon executable.

---

### Phase 2 Scope

**In scope:**
1. Journal monitor (sd_journal API, background thread)
2. New log-analysis event types (4 types added to EventData variant)
3. Journal rule/pattern matching engine with TOML configuration
4. Event correlation engine (time-windowed aggregation)
5. D-Bus desktop notification delivery (implementing the existing stub)
6. Schema migration (v1 → v2) for journal event tracking
7. Config additions for `[journal]` and `[correlation]` sections
8. Policy engine extension for log-based rules

**Deferred:**
- User config merging and per-user policy enforcement
- Home directory monitoring activation
- auditd integration
- Scheduled scan cron expressions
- SIGUSR1-triggered scans

### New Source Files

```
src/
    journal/
        journal_fields.h              # Journal field name constants + JournalEntry struct
        journal_rule.h / .cpp         # Rule definitions, matching logic, default rules
        journal_monitor.h / .cpp      # Background thread, sd_journal reader
    correlation/
        correlation_engine.h / .cpp   # Time-windowed event aggregation
    notify/
        dbus_notifier.h / .cpp        # D-Bus notification via sd_bus

tests/
    journal/
        journal_rule_test.cpp         # Rule matching (pure functions, no privileges)
        journal_monitor_test.cpp      # Lifecycle tests (graceful failure pattern)
        CMakeLists.txt
    correlation/
        correlation_engine_test.cpp   # Threshold, windowing, debounce
        CMakeLists.txt
    notify/
        dbus_notifier_test.cpp        # Graceful degradation when no session bus
        CMakeLists.txt
```

### Modified Existing Files

| File | Change |
|------|--------|
| `src/events/event.h` | Add 4 new event types to EventData variant |
| `src/events/event.cpp` | Add type name mappings |
| `src/config/config.h` | Add JournalConfig, CorrelationConfig structs |
| `src/config/config.cpp` | Parse `[journal]` and `[correlation]` TOML sections |
| `src/dispatch/alert_dispatcher.h/.cpp` | Accept DbusNotifier*, handle new event types |
| `src/policy/policy_engine.h/.cpp` | Add LogRule, category-based evaluation |
| `src/storage/schema.h` | Bump CURRENT_VERSION to 2, add journal_events DDL |
| `src/storage/database.cpp` | Schema migration v1→v2 |
| `src/daemon/daemon.h/.cpp` | Wire JournalMonitor, CorrelationEngine, DbusNotifier |
| `CMakeLists.txt` | Add 4 new .cpp files, remove Boost dep |
| `tests/CMakeLists.txt` | Add 3 new test subdirectories |

### New Event Types

```cpp
// In events/event.h — 4 new types added to EventData variant

struct AuthFailureEvent {
    std::string username;
    std::string service;                    // "sshd", "sudo", "login"
    std::optional<std::string> remote_host;
    std::string message;
};

struct PrivilegeEscalationEvent {
    std::string username;
    std::string target_user;                // e.g., "root"
    std::string method;                     // "sudo", "su", "pkexec"
    std::string command;
    std::string message;
};

struct ServiceStateEvent {
    std::string unit_name;                  // e.g., "sshd.service"
    std::string new_state;                  // "started", "stopped", "failed"
    std::optional<std::string> exit_code;
    std::string message;
};

struct SuspiciousLogEvent {
    std::string rule_name;                  // Rule that matched
    std::string unit_name;
    std::string message;
    std::uint8_t priority;                  // Journal PRIORITY (0-7)
};

// Updated variant
using EventData = std::variant<
    FileModifiedEvent, FileCreatedEvent, FileDeletedEvent,
    FilePermissionChangedEvent, ScanCompletedEvent, SystemStartupEvent,
    AuthFailureEvent, PrivilegeEscalationEvent,
    ServiceStateEvent, SuspiciousLogEvent
>;
```

### Journal Monitor Architecture

**JournalEntry** (intermediate representation in `journal_fields.h`):
```cpp
struct JournalEntry {
    std::string message;
    std::uint8_t priority{6};
    std::string syslog_identifier;       // e.g., "sshd"
    std::string systemd_unit;            // e.g., "sshd.service"
    std::optional<std::uint32_t> pid;
    std::optional<std::uint32_t> uid;
    std::string comm;
    std::string exe;
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> raw_fields;
};
```

**JournalRule** (in `journal_rule.h`):
```cpp
enum class JournalMatchType : std::uint8_t {
    exact, contains, regex, starts_with
};

struct JournalFieldMatch {
    std::string field_name;
    std::string pattern;
    JournalMatchType match_type{JournalMatchType::contains};
    bool negate{false};
    std::optional<std::regex> compiled_regex;
};

enum class JournalRuleAction : std::uint8_t {
    auth_failure, privilege_escalation, service_state, suspicious_log
};

struct JournalRule {
    std::string name;
    std::string description;
    std::vector<JournalFieldMatch> field_matches;  // ANDed
    JournalRuleAction action{JournalRuleAction::suspicious_log};
    EventSeverity severity{EventSeverity::warning};
    bool enabled{true};
};
```

Rules are evaluated in order; first match wins. Default rules (~10) cover: SSH auth failures,
invalid users, sudo failures, sudo success, su sessions, service failures, kernel segfaults,
PAM failures, polkit auth, pkexec execution.

**JournalMonitor** (follows FanotifyMonitor pattern):
```cpp
class JournalMonitor {
public:
    JournalMonitor(EventBus& event_bus, std::vector<JournalRule> rules,
                   JournalMonitorConfig config = {});
    [[nodiscard]] auto initialize() -> std::expected<void, std::string>;
    [[nodiscard]] auto start() -> std::expected<void, std::string>;
    void stop();
    [[nodiscard]] auto is_running() const -> bool;
    void update_rules(std::vector<JournalRule> rules);  // Hot reload (SIGHUP)

private:
    void monitor_loop();       // sd_journal_wait() + sd_journal_next() loop
    auto extract_fields(sd_journal*) -> JournalEntry;
    void evaluate_entry(JournalEntry const& entry);
    auto build_event(JournalEntry const&, JournalRule const&) -> Event;
    auto should_exclude(JournalEntry const&) const -> bool;
    // Self-monitoring loop prevention: excludes SYSLOG_IDENTIFIER=vigilant-canined
};
```

### Configuration Format

```toml
[journal]
enabled = true
max_priority = 6          # 0=emerg..7=debug; only process <= this

exclude_units = ["systemd-journald.service", "systemd-timesyncd.service"]
exclude_identifiers = ["systemd-tmpfiles"]

[[journal.rules]]
name = "my_custom_rule"
description = "Custom application errors"
severity = "warning"
action = "suspicious_log"

[[journal.rules.match]]
field = "SYSLOG_IDENTIFIER"
pattern = "my-app"
type = "exact"

[[journal.rules.match]]
field = "MESSAGE"
pattern = "CRITICAL ERROR"
type = "contains"

[correlation]
enabled = true
window_seconds = 300

[[correlation.rules]]
name = "ssh_brute_force"
event_match = "ssh_auth_failure"   # Matches journal rule name
threshold = 5
window_seconds = 60
escalated_severity = "critical"

[[correlation.rules]]
name = "rapid_file_changes"
event_match = "file_modified"       # Matches Phase 1 alert category
threshold = 20
window_seconds = 60
escalated_severity = "critical"
```

### Correlation Engine

```cpp
class CorrelationEngine {
public:
    CorrelationEngine(EventBus& event_bus, std::vector<CorrelationRule> rules);
    [[nodiscard]] auto start() -> std::expected<void, std::string>;
    void stop();
    void drain_escalated_events(EventBus& event_bus);  // Called from main loop
    void update_rules(std::vector<CorrelationRule> rules);

private:
    void handle_event(Event const& event);  // EventBus subscriber
    auto get_match_key(Event const& event) const -> std::string;
    // State: match_key -> vector<timestamp>, bounded to MAX_TRACKED_KEYS=1000
    // Debounce: rule_name -> last_fired_time
    // Pending events buffered, NOT published from handler (deadlock prevention)
};
```

**Critical design**: `drain_escalated_events()` is called from the daemon main loop, NOT from
within an EventBus handler. Publishing from a handler would deadlock (EventBus mutex is
non-recursive). Escalated events are buffered in `m_pending_events`.

### D-Bus Notifier

```cpp
class DbusNotifier {
public:
    [[nodiscard]] auto initialize() -> std::expected<void, std::string>;
    void notify(Alert const& alert);
    [[nodiscard]] auto is_available() const -> bool;
    // Uses sd_bus (already linked via libsystemd)
    // Opens user session bus; gracefully degrades on headless systems
    // Maps AlertSeverity → freedesktop urgency (INFO→low, WARNING→normal, CRITICAL→critical)
};
```

### Schema Changes

```sql
-- schema.h: CURRENT_VERSION bumped to 2

-- New table for journal event tracking
CREATE TABLE IF NOT EXISTS journal_events (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    rule_name   TEXT NOT NULL,
    message     TEXT NOT NULL,
    priority    INTEGER NOT NULL,
    unit_name   TEXT,
    created_at  TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);

CREATE INDEX IF NOT EXISTS idx_journal_events_rule ON journal_events(rule_name);
CREATE INDEX IF NOT EXISTS idx_journal_events_created ON journal_events(created_at);
```

Migration in `database.cpp`: detect version 1, execute new DDL, update to version 2.

### Daemon Main Loop Change

```cpp
while (!m_should_stop.load()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Phase 2: drain correlation escalated events
    if (m_correlation_engine) {
        m_correlation_engine->drain_escalated_events(*m_event_bus);
    }

    if (m_should_reload.load()) {
        m_should_reload.store(false);
        // ... existing reload + new rule hot-reload
    }
}
```

### Implementation Order (11 Steps)

**Note on ordering**: Tasks are listed sequentially but may require interleaving for build
dependencies (see Build Dependency Graph above). In particular, Task 7 completion requires
adding Phase 2 sources to daemon CMakeLists.txt (part of Task 10).

1. **Event types** — Add 4 types to event.h/cpp, extend event_bus tests
2. **Journal fields + rules** — journal_fields.h, journal_rule.h/cpp + tests (pure functions)
3. **Journal monitor** — journal_monitor.h/cpp + lifecycle tests
4. **Config parsing** — Add JournalConfig/CorrelationConfig to config.h/cpp + tests
5. **Correlation engine** — correlation_engine.h/cpp + tests (pure in-memory)
6. **D-Bus notifier** — dbus_notifier.h/cpp + tests (graceful degradation)
7. **Alert dispatcher updates** — Handle new event types, wire DbusNotifier
   - **Build requirement**: Add Phase 2 sources to daemon CMakeLists.txt before this compiles
8. **Schema migration** — Bump to v2, add journal_events table + migration tests
9. **Policy engine log rules** — Add LogRule, category-based evaluation + tests (optional)
10. **Daemon integration** — Wire all components, update CMake, main loop changes
11. **Documentation** — Phase 2 plan doc, update architecture.md, configuration.md

### Thread Safety Summary

- EventBus mutex serializes all handler calls
- JournalMonitor publishes from its thread → safe (EventBus handles serialization)
- CorrelationEngine handler runs under EventBus lock, takes its own m_state_mutex
- CorrelationEngine never publishes from handler (buffered → drained from main loop)
- JournalMonitor rule hot-reload uses separate m_rules_mutex

### Verification

1. Build: `cmake -B build/gcc-debug -DCMAKE_BUILD_TYPE=Debug && cmake --build build/gcc-debug`
2. Tests: `ctest --test-dir build/gcc-debug --output-on-failure`
3. Target: ~125-140 total tests (76 existing + ~50-65 new)
   - Actual achieved: ~126 tests (76 Phase 1 + 50 Phase 2)
4. Manual: Run daemon, trigger `sudo` / failed SSH → verify journal alert + D-Bus notification
5. Correlation: Generate 5+ SSH failures in 60s → verify escalated "critical" alert

### Implementation Results (2026-02-12)

**Completed:**
- ✅ Part 1: Phase 1 plan audit + Boost::system removal
- ✅ Steps 1-8: All core Phase 2 components implemented and tested
- ✅ Step 10: Schema migration v2 (journal_events table)
- ✅ Partial Step 10: Phase 2 sources integrated into daemon build
- ⏭️ Step 9: PolicyEngine log rules (deferred as non-critical)

**Test Results:**
- journal_rule_test: 10/10 passing ✅
- journal_monitor_test: 8/8 passing ✅
- correlation_engine_test: 8/8 passing ✅
- dbus_notifier_test: 7/7 passing ✅
- event_bus extensions: 4/4 passing ✅
- **Total new**: ~50 tests, all passing

**Status**: Phase 2 core functionality implemented. Remaining work: daemon.cpp wiring for
runtime initialization of Phase 2 components (JournalMonitor::start(), CorrelationEngine::start(),
DbusNotifier::initialize(), main loop drain_escalated_events() call).
