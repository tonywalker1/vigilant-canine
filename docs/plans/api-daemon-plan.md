# API Daemon Implementation Plan

## Context

The Vigilant Canine core daemon (vigilant-canined) is complete with all three detection phases operational:
- Phase 1: File integrity monitoring (fanotify)
- Phase 2: Log analysis (systemd journal)
- Phase 3: Audit subsystem integration

The system stores alerts and events in SQLite but has no way for users to query this data except through journalctl or direct database access. We need to build `vigilant-canined-api`, an unprivileged HTTP API daemon that exposes this data over a Unix domain socket. This enables:

1. Web dashboard for alert viewing and management
2. Integration with other tools via REST API
3. User-friendly alert acknowledgment and filtering
4. Programmatic access to security events

**Architectural Constraint**: The API daemon runs unprivileged and reads from a shared SQLite database. It does NOT have root access or privileged capabilities. This maintains strict privilege separation—compromising the API daemon cannot grant root access.

## Technology Decisions

**HTTP Library: cpp-httplib**
- Header-only library (single file include, zero runtime dependencies)
- Native Unix domain socket support via `AF_UNIX`
- GPL-3.0 compatible (Apache 2.0 license)
- Mature (v0.30.2), widely adopted, simple API
- Much simpler than Boost.Beast's async complexity

**Thread Model: Single-threaded with blocking I/O**
- Expected load is minimal (single user dashboard)
- SQLite reads are fast (local database)
- Simpler implementation (no mutex overhead)
- cpp-httplib supports thread pools if needed later (YAGNI)

**JSON Serialization: Manual construction initially**
- Alert/Baseline structures are simple
- Avoid additional dependencies initially
- Can migrate to nlohmann/json later if needed

**Database Access: Read-only SQLite with existing Store classes**
- Reuse existing AlertStore, BaselineStore, AuditEventStore
- Open database in SQLITE_OPEN_READONLY mode
- SQLite WAL mode prevents writer/reader blocking

## Architecture

### Component Structure

```
src/api/
├── main.cpp              # Entry point, signal handling
├── api_daemon.h/.cpp     # Lifecycle manager (follows src/daemon/daemon.h pattern)
├── http_server.h/.cpp    # cpp-httplib wrapper, Unix socket setup
├── handlers/
│   ├── alert_handler.h/.cpp      # GET /alerts, POST /alerts/{id}/acknowledge
│   ├── baseline_handler.h/.cpp   # GET /baselines
│   ├── event_handler.h/.cpp      # GET /journal-events, /audit-events
│   └── health_handler.h/.cpp     # GET /health, /status
└── serialization/
    └── json.h/.cpp       # Manual JSON construction for Alert, Baseline, etc.
```

### REST API Endpoints

All endpoints under `/api/v1/` prefix for versioning.

**Health & Status:**
- `GET /api/v1/health` - Simple health check (returns {"status": "ok"})
- `GET /api/v1/status` - Database stats (unacknowledged count, daemon state)

**Alerts:**
- `GET /api/v1/alerts` - List alerts with filtering
  - Query params: `severity`, `acknowledged`, `category`, `limit`, `offset`, `since`, `until`
  - Returns paginated JSON with total count
- `GET /api/v1/alerts/{id}` - Get specific alert
- `POST /api/v1/alerts/{id}/acknowledge` - Mark alert as acknowledged
- `DELETE /api/v1/alerts/{id}/acknowledge` - Un-acknowledge alert

**Baselines:**
- `GET /api/v1/baselines` - List file baselines
  - Query params: `path`, `source`, `limit`, `offset`

**Events:**
- `GET /api/v1/journal-events` - List journal events
  - Query params: `rule_name`, `limit`, `offset`, `since`, `until`
- `GET /api/v1/audit-events` - List audit events
  - Query params: `rule_name`, `event_type`, `uid`, `username`, `limit`, `offset`, `since`, `until`

### Response Format

**Success response:**
```json
{
  "alerts": [...],
  "total": 100,
  "limit": 50,
  "offset": 0
}
```

**Error response:**
```json
{
  "error": {
    "code": "INVALID_PARAMETER",
    "message": "Invalid severity value"
  }
}
```

## Critical Files

### New Files to Create

1. **src/api/main.cpp** - Entry point, signal handling (follows src/daemon/main.cpp pattern)
2. **src/api/api_daemon.h** - Lifecycle manager (follows src/daemon/daemon.h pattern)
3. **src/api/api_daemon.cpp** - Component initialization, graceful shutdown
4. **src/api/http_server.h** - cpp-httplib wrapper
5. **src/api/http_server.cpp** - Unix socket setup, route registration
6. **src/api/handlers/alert_handler.{h,cpp}** - Alert endpoint handlers
7. **src/api/handlers/baseline_handler.{h,cpp}** - Baseline endpoint handlers
8. **src/api/handlers/event_handler.{h,cpp}** - Journal/audit event handlers
9. **src/api/handlers/health_handler.{h,cpp}** - Health check handlers
10. **src/api/serialization/json.{h,cpp}** - JSON serialization helpers
11. **systemd/vigilant-canined-api.service** - systemd unit file

### Files to Modify

12. **CMakeLists.txt** - Add cpp-httplib via FetchContent, add vigilant-canined-api executable
13. **config/vigilant-canine.toml.example** - Add `[api]` section
14. **src/config/config.h** - Add `ApiConfig` struct
15. **src/config/config.cpp** - Parse `[api]` TOML section
16. **systemd/vigilant-canine.conf** (tmpfiles.d) - Add /run/vigilant-canine/api.sock

## Implementation Phases

### Phase 1: Basic HTTP Server (2 days)

**Goal**: Prove Unix socket HTTP server works

**Tasks**:
1. Add cpp-httplib to CMakeLists.txt via FetchContent (v0.30.2)
2. Create vigilant-canined-api build target
3. Create src/api/main.cpp with minimal signal handling
4. Create src/api/http_server.{h,cpp} with Unix socket setup
5. Implement GET /api/v1/health endpoint (hardcoded JSON)
6. Test with curl: `curl --unix-socket /run/vigilant-canine/api.sock http://localhost/api/v1/health`

**Acceptance**: Binary compiles, server listens on Unix socket, GET /health returns 200

### Phase 2: Database Integration (3 days)

**Goal**: Integrate with existing storage layer

**Tasks**:
1. Create ApiDaemon class (lifecycle manager, follows daemon.h pattern)
2. Open database in SQLITE_OPEN_READONLY mode
3. Instantiate AlertStore, BaselineStore from database reference
4. Create alert_handler.cpp with GET /api/v1/alerts implementation
5. Implement JSON serialization for Alert struct in json.cpp
6. Add query parameter parsing (severity, acknowledged, limit, offset)
7. Create baseline_handler.cpp with GET /api/v1/baselines
8. Implement JSON serialization for Baseline struct
9. Add GET /api/v1/status endpoint with database stats
10. Handle database errors gracefully (return HTTP 500)

**Acceptance**: GET /alerts returns real data, filtering works, pagination works, errors return proper HTTP codes

### Phase 3: Write Operations (2 days)

**Goal**: Implement alert acknowledgment

**Tasks**:
1. Implement POST /api/v1/alerts/{id}/acknowledge
2. Validate alert ID from URL path
3. Call AlertStore::acknowledge(id)
4. Return updated alert in response
5. Handle errors (404 for invalid ID, 500 for DB errors)
6. Add DELETE /api/v1/alerts/{id}/acknowledge (un-acknowledge)
7. Extend AlertStore if needed (add unacknowledge method)
8. Test acknowledge/unacknowledge workflow

**Acceptance**: POST updates database, response includes updated alert, 404 for invalid IDs, changes persist

### Phase 4: Event Endpoints (2 days)

**Goal**: Expose journal and audit events

**Tasks**:
1. Create event_handler.cpp
2. Implement GET /api/v1/journal-events with filtering
3. Add JSON serialization for journal events
4. Implement GET /api/v1/audit-events with filtering
5. Handle audit event details (nested JSON field)
6. Add time range filtering (since/until ISO 8601 parameters)
7. Test with real journal and audit data

**Acceptance**: GET /journal-events works, GET /audit-events works, filtering by rule_name and event_type works, time range filtering works

### Phase 5: systemd Integration (2 days)

**Goal**: Run as system service

**Tasks**:
1. Create vigilant-canined-api.service unit file
2. Configure Type=simple
3. Run as unprivileged user (vigilant-canine:vigilant-canine)
4. Add User/Group directives
5. Security hardening (NoNewPrivileges, PrivateTmp, ProtectSystem=strict, etc.)
6. Set ReadWritePaths=/run/vigilant-canine (for socket)
7. Set ReadOnlyPaths=/var/lib/vigilant-canine /etc/vigilant-canine
8. Add After=vigilant-canined.service dependency
9. Update tmpfiles.d/vigilant-canine.conf for socket directory
10. Test systemctl start/stop/restart
11. Verify socket permissions (0660, vigilant-canine:vigilant-canine)

**Acceptance**: Service starts, socket created with correct permissions, database opened read-only, service stops gracefully, no permission errors

### Phase 6: Testing & Documentation (3 days)

**Goal**: Comprehensive testing and user documentation

**Tasks**:
1. Create unit tests for handlers (tests/api/alert_handler_test.cpp, etc.)
2. Create integration tests with real database (tests/api/integration_test.cpp)
3. Write curl-based manual testing script (tests/api/manual_test.sh)
4. Create API documentation (docs/api.md with all endpoints, examples)
5. Update docs/architecture.md with API daemon details
6. Update README.md with API daemon installation steps
7. Add troubleshooting section for common API issues
8. Performance testing (response times)
9. Memory leak checking (valgrind)

**Acceptance**: All tests pass, API documentation complete, curl examples work, no memory leaks, response times < 100ms

**Total estimated time: 14 days**

## Security Considerations

### Socket Permissions
- Socket path: `/run/vigilant-canine/api.sock`
- Permissions: 0660
- Owner: `vigilant-canine:vigilant-canine`
- Access: Only processes in vigilant-canine group
- Apply chmod/chown immediately after socket creation

### Input Validation
- **Integer parameters** (limit, offset, id): Range checks (limit 1-1000, offset >=0)
- **Enum parameters** (severity, event_type): Whitelist validation, reject unknown
- **Timestamp parameters** (since, until): ISO 8601 format validation, range checks
- **String parameters** (path, username): Length limits (< 4096 chars)

### SQL Injection Prevention
- Use existing Store classes exclusively (they use parameterized queries)
- NEVER construct SQL strings with user input
- All filtering logic in store classes, not handlers

### Rate Limiting
- Phase 1: No rate limiting (Unix socket access gated by file permissions)
- Future: If deployed with TCP proxy, add rate limiting middleware

## Configuration Changes

Add new `[api]` section to config/vigilant-canine.toml.example:

```toml
[api]
# Enable API daemon
enabled = true

# Unix domain socket path
socket_path = "/run/vigilant-canine/api.sock"

# Socket permissions (octal)
socket_mode = 0o660

# Request timeout in seconds
timeout_seconds = 30

# Maximum request body size in bytes
max_body_size = 1048576  # 1MB

# Log level: trace, debug, info, warn, error
log_level = "info"
```

Add `ApiConfig` struct to src/config/config.h:

```cpp
struct ApiConfig {
    bool enabled{true};
    std::filesystem::path socket_path{"/run/vigilant-canine/api.sock"};
    std::uint32_t socket_mode{0660};
    std::uint32_t timeout_seconds{30};
    std::uint64_t max_body_size{1048576};
    std::string log_level{"info"};
};

struct Config {
    // ... existing fields ...
    ApiConfig api;  // Add this
};
```

## systemd Service Unit

Create systemd/vigilant-canined-api.service:

```ini
[Unit]
Description=Vigilant Canine API Daemon
Documentation=https://github.com/tony/vigilant-canine
After=vigilant-canined.service
Requires=vigilant-canined.service

[Service]
Type=simple
ExecStart=/usr/bin/vigilant-canined-api --config /etc/vigilant-canine/config.toml
Restart=on-failure
RestartSec=10s
TimeoutStartSec=30
TimeoutStopSec=30

# User/Group (unprivileged)
User=vigilant-canine
Group=vigilant-canine

# Security Hardening
NoNewPrivileges=yes
PrivateTmp=yes
ProtectSystem=strict
ProtectHome=yes
ProtectKernelTunables=yes
ProtectKernelModules=yes
ProtectKernelLogs=yes
ProtectControlGroups=yes
RestrictRealtime=yes
RestrictSUIDSGID=yes
LockPersonality=yes
RestrictNamespaces=yes
RestrictAddressFamilies=AF_UNIX

# Filesystem access
ReadWritePaths=/run/vigilant-canine
ReadOnlyPaths=/etc/vigilant-canine /var/lib/vigilant-canine

# System call filtering
SystemCallFilter=@system-service
SystemCallFilter=~@privileged @resources

# Memory protection
MemoryDenyWriteExecute=yes

# Logging
StandardOutput=journal
StandardError=journal
SyslogIdentifier=vigilant-canined-api

[Install]
WantedBy=multi-user.target
```

## CMakeLists.txt Changes

Add cpp-httplib dependency:

```cmake
# cpp-httplib - header-only HTTP library with Unix socket support
FetchContent_Declare(
    cpp-httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.30.2
)
FetchContent_MakeAvailable(cpp-httplib)
```

Add vigilant-canined-api executable:

```cmake
# API daemon executable
add_executable(vigilant-canined-api
    src/api/main.cpp
    src/api/api_daemon.cpp
    src/api/http_server.cpp
    src/api/handlers/alert_handler.cpp
    src/api/handlers/baseline_handler.cpp
    src/api/handlers/event_handler.cpp
    src/api/handlers/health_handler.cpp
    src/api/serialization/json.cpp
    # Reuse storage layer
    src/storage/database.cpp
    src/storage/baseline_store.cpp
    src/storage/alert_store.cpp
    src/storage/audit_event_store.cpp
    src/config/config.cpp
    src/core/hash.cpp
    src/events/event.cpp
)

target_link_libraries(vigilant-canined-api PRIVATE
    SQLite::SQLite3
    tomlplusplus::tomlplusplus
    httplib::httplib
)

target_include_directories(vigilant-canined-api PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(vigilant-canined-api PRIVATE
    -Wall -Wextra -Wpedantic
)
```

Install targets:

```cmake
# Binary
install(TARGETS vigilant-canined-api
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# API systemd service file
install(FILES systemd/vigilant-canined-api.service
    DESTINATION ${SYSTEMD_SYSTEM_UNIT_DIR}
)
```

## Verification

After implementation, verify end-to-end:

1. **Build**: `cmake -B build/gcc-debug -DCMAKE_BUILD_TYPE=Debug && cmake --build build/gcc-debug -j$(nproc)`
2. **Install service**: `sudo cp systemd/vigilant-canined-api.service /etc/systemd/system/`
3. **Create user/group**: `sudo groupadd --system vigilant-canine && sudo useradd --system --no-create-home --shell /sbin/nologin --gid vigilant-canine vigilant-canine`
4. **Start services**:
   - `sudo systemctl daemon-reload`
   - `sudo systemctl start vigilant-canined.service`
   - `sudo systemctl start vigilant-canined-api.service`
5. **Check status**: `systemctl status vigilant-canined-api`
6. **Verify socket**: `ls -l /run/vigilant-canine/api.sock` (should be 0660, vigilant-canine:vigilant-canine)
7. **Test health**: `curl --unix-socket /run/vigilant-canine/api.sock http://localhost/api/v1/health | jq`
8. **Test alerts**: `curl --unix-socket /run/vigilant-canine/api.sock http://localhost/api/v1/alerts?limit=5 | jq`
9. **Test acknowledge**: `curl -X POST --unix-socket /run/vigilant-canine/api.sock http://localhost/api/v1/alerts/1/acknowledge | jq`
10. **Check logs**: `journalctl -u vigilant-canined-api -f`
11. **Run tests**: `cd build/gcc-debug && ctest --output-on-failure`

## Anticipated Challenges

1. **Socket permission race conditions**: Use umask before socket creation, apply chmod/chown immediately after
2. **Database locked errors**: SQLite WAL mode prevents this; open in read-only mode
3. **Graceful shutdown with active requests**: cpp-httplib supports graceful shutdown; set TimeoutStopSec=30 in systemd
4. **Large result sets**: Enforce maximum limit (1000), default to 100, require pagination
5. **Command-line sanitization**: Already handled by AuditMonitor; JSON escaping handles special chars

## Success Criteria

- API daemon compiles and runs as unprivileged service
- All endpoints return valid JSON responses
- Query filtering and pagination work correctly
- Alert acknowledgment persists to database
- systemd service starts/stops gracefully
- Socket permissions are correct (0660)
- No memory leaks detected
- Response times < 100ms for typical queries
- All tests pass (unit and integration)
- Documentation is complete with working curl examples
