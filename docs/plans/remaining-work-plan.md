# Remaining Work Implementation Plan

## Context

All three detection phases (file integrity, log analysis, audit subsystem) are complete and operational. The core daemon, API daemon, systemd integration, and packaging infrastructure are all implemented. This plan addresses the remaining TODOs and unimplemented features identified in the codebase.

## Work Items

### 1. Custom Rule Merging (High Priority)

**Status:** 4 TODO comments in daemon.cpp
**Complexity:** Low
**Estimated Time:** 2-4 hours

**Problem:**
Currently, journal and audit monitors use only default rules. User-configured custom rules from the TOML config are loaded but never merged with defaults.

**Affected Code:**
- `src/daemon/daemon.cpp:151` - Journal rule initialization
- `src/daemon/daemon.cpp:205` - Audit rule initialization
- `src/daemon/daemon.cpp:436` - Journal rule hot-reload
- `src/daemon/daemon.cpp:467` - Audit rule hot-reload

**Solution:**

Implement `merge_journal_rules()` and `merge_audit_rules()` helper functions:

```cpp
namespace {
    auto merge_journal_rules(
        std::vector<JournalRule> defaults,
        std::vector<JournalRuleConfig> const& custom_configs
    ) -> std::vector<JournalRule> {
        // Convert custom configs to JournalRule
        for (auto const& config : custom_configs) {
            JournalRule rule;
            rule.name = config.name;
            rule.description = config.description;
            rule.enabled = config.enabled;
            // ... convert match conditions, severity, action
            defaults.push_back(std::move(rule));
        }
        return defaults;
    }

    auto merge_audit_rules(
        std::vector<AuditRule> defaults,
        std::vector<AuditRuleConfig> const& custom_configs
    ) -> std::vector<AuditRule> {
        // Similar implementation for audit rules
        // ...
    }
}
```

Then replace the 4 TODO locations:
```cpp
// OLD:
auto journal_rules = get_default_rules();
// TODO: Merge with m_config.journal.rules

// NEW:
auto journal_rules = merge_journal_rules(
    get_default_rules(),
    m_config.journal.rules
);
```

**Testing:**
1. Add custom journal rule to config.toml
2. Verify rule appears in rule list via API
3. Verify rule triggers correctly
4. Test hot-reload with SIGHUP
5. Add unit tests for merge functions

**Files to Modify:**
- `src/daemon/daemon.cpp` (add helper functions, update 4 call sites)
- `tests/daemon/daemon_test.cpp` (add merge function tests)

---

### 2. Extended Alert Filtering (Medium Priority)

**Status:** 1 TODO in alert_handler.cpp
**Complexity:** Low-Medium
**Estimated Time:** 3-6 hours

**Problem:**
API endpoint `/api/v1/alerts` only supports pagination (limit/offset). The plan specifies filtering by severity, acknowledged status, and category, but these aren't implemented.

**Affected Code:**
- `src/api/handlers/alert_handler.cpp:54`

**Solution:**

Update `AlertStore` to support filtering:

```cpp
// src/storage/alert_store.h
struct AlertFilter {
    std::optional<EventSeverity> severity;
    std::optional<bool> acknowledged;
    std::optional<std::string> category;
    std::optional<std::chrono::system_clock::time_point> since;
    std::optional<std::chrono::system_clock::time_point> until;
};

[[nodiscard]] auto get_filtered(
    AlertFilter const& filter,
    std::size_t limit,
    std::size_t offset
) -> std::expected<std::vector<Alert>, std::string>;
```

Implementation uses SQL WHERE clauses:
```sql
SELECT * FROM alerts
WHERE (severity = ? OR ? IS NULL)
  AND (acknowledged = ? OR ? IS NULL)
  AND (category = ? OR ? IS NULL)
  AND (timestamp >= ? OR ? IS NULL)
  AND (timestamp <= ? OR ? IS NULL)
ORDER BY timestamp DESC
LIMIT ? OFFSET ?
```

Update handler to parse query parameters:
```cpp
// src/api/handlers/alert_handler.cpp
auto handle_list_alerts(httplib::Request const& req, httplib::Response& res) {
    AlertFilter filter;

    // Parse severity=info|warning|critical
    if (req.has_param("severity")) {
        filter.severity = parse_severity(req.get_param_value("severity"));
    }

    // Parse acknowledged=true|false
    if (req.has_param("acknowledged")) {
        filter.acknowledged = parse_bool(req.get_param_value("acknowledged"));
    }

    // Parse category
    if (req.has_param("category")) {
        filter.category = req.get_param_value("category");
    }

    // Parse since/until timestamps (ISO 8601)
    // ...

    auto result = store_.get_filtered(filter, limit, offset);
    // ...
}
```

**Testing:**
1. Query with `?severity=critical` - verify only critical alerts returned
2. Query with `?acknowledged=false` - verify only unacknowledged alerts
3. Query with `?category=file_integrity` - verify category filtering
4. Query with `?since=2026-01-01T00:00:00Z` - verify time filtering
5. Combine multiple filters
6. Add integration tests for alert filtering

**Files to Modify:**
- `src/storage/alert_store.h` (add AlertFilter struct and get_filtered method)
- `src/storage/alert_store.cpp` (implement filtered queries)
- `src/api/handlers/alert_handler.cpp` (parse query params, use filter)
- `tests/storage/alert_store_test.cpp` (add filter tests)
- `tests/api/alert_handler_test.cpp` (add query param tests)
- `docs/api.md` (document query parameters)

---

### 3. Scanner Statistics Tracking (Low Priority)

**Status:** 1 TODO in scanner.cpp
**Complexity:** Low
**Estimated Time:** 1-2 hours

**Problem:**
Scanner increments `files_added` for all successfully scanned files. Should distinguish between new baselines and updates to existing ones.

**Affected Code:**
- `src/scanner/scanner.cpp:171`

**Solution:**

Modify `BaselineStore::store_baseline()` to return operation type:

```cpp
// src/storage/baseline_store.h
enum class StoreOperation { inserted, updated, unchanged };

[[nodiscard]] auto store_baseline(Baseline const& baseline)
    -> std::expected<StoreOperation, std::string>;
```

Update scanner to track separately:
```cpp
// src/scanner/scanner.cpp
auto result = m_baseline_store.store_baseline(baseline);
if (result) {
    stats.files_scanned++;
    if (*result == StoreOperation::inserted) {
        stats.files_added++;
    } else if (*result == StoreOperation::updated) {
        stats.files_updated++;  // New field
    }
} else {
    stats.errors++;
}
```

Update `ScanStats` structure:
```cpp
// src/scanner/scanner.h
struct ScanStats {
    std::size_t files_scanned{0};
    std::size_t files_added{0};
    std::size_t files_updated{0};    // New
    std::size_t files_unchanged{0};  // New
    std::size_t errors{0};
    std::chrono::milliseconds duration{0};
};
```

**Testing:**
1. Initial scan - verify files_added > 0, files_updated == 0
2. Rescan without changes - verify files_unchanged > 0
3. Modify file and rescan - verify files_updated > 0
4. Add unit tests for store_baseline return values

**Files to Modify:**
- `src/storage/baseline_store.h` (add StoreOperation enum)
- `src/storage/baseline_store.cpp` (return operation type)
- `src/scanner/scanner.h` (update ScanStats)
- `src/scanner/scanner.cpp` (track updates vs adds)
- `tests/storage/baseline_store_test.cpp` (test return values)
- `tests/scanner/scanner_test.cpp` (verify stats tracking)

---

### 4. Web Dashboard (Major Feature - Optional)

**Status:** ⏳ Planned but not implemented
**Complexity:** High
**Estimated Time:** 2-3 weeks

**Problem:**
No graphical interface for viewing alerts, baselines, and system status. Users must query API manually or read journalctl output.

**Requirements:**
- Static HTML/JS frontend (no server-side runtime)
- Served by existing API daemon (static file serving)
- Alert viewer with filtering and search
- Baseline browser
- Journal/audit event viewer
- Real-time updates (optional: WebSocket or polling)
- Responsive design (mobile-friendly)

**Technology Decisions:**

**Framework: Vanilla JS with Web Components (Recommended)**
- Zero build step for development
- No npm/webpack/bundler complexity
- Works offline once loaded
- Modern browsers support natively
- Lightweight (~50-100 KB total)

**Alternative: Preact + HTM**
- Slightly heavier (~150 KB)
- JSX-like syntax without build step
- Component reusability

**Avoid:**
- React/Vue/Angular (too heavy, requires build tooling)
- Server-side rendering (defeats static-asset goal)

**Directory Structure:**
```
web/
├── index.html           # Main entry point
├── css/
│   ├── main.css         # Base styles
│   └── components.css   # Component-specific styles
├── js/
│   ├── main.js          # App initialization
│   ├── api.js           # API client wrapper
│   ├── components/
│   │   ├── alert-list.js
│   │   ├── alert-detail.js
│   │   ├── baseline-browser.js
│   │   ├── event-viewer.js
│   │   └── nav-bar.js
│   └── utils/
│       ├── formatters.js    # Date/severity formatting
│       └── websocket.js     # WebSocket client (future)
└── assets/
    └── favicon.svg
```

**Core Features (MVP):**

1. **Alert Dashboard** (Priority 1)
   - List recent alerts with pagination
   - Filter by severity/acknowledged/category
   - Severity badge styling (color-coded)
   - Acknowledge/unacknowledge buttons
   - Alert detail view with full metadata

2. **Baseline Browser** (Priority 2)
   - Browse file baselines by source (rpm/dpkg/flatpak/user)
   - Search by path
   - Display hash, permissions, ownership
   - Show last verified timestamp

3. **Event Viewers** (Priority 3)
   - Journal events with rule matching
   - Audit events with process context
   - Time filtering

4. **System Status** (Priority 4)
   - Daemon health check
   - Last scan time
   - Unacknowledged alert count
   - Database statistics

**API Changes Required:**

API daemon needs static file serving:
```cpp
// src/api/http_server.cpp
void HttpServer::setup_routes() {
    // Existing API routes
    // ...

    // Static file serving (if web/ directory exists)
    server_.set_mount_point("/", "/usr/share/vigilant-canine/web");

    // Fallback to index.html for SPA routing
    server_.set_file_request_handler([](const httplib::Request& req, httplib::Response& res) {
        if (req.path.starts_with("/api/")) return;
        res.set_content(read_file("/usr/share/vigilant-canine/web/index.html"), "text/html");
    });
}
```

**Implementation Steps:**

**Phase 1: Alert Dashboard (Week 1)**
1. Create basic HTML structure with navigation
2. Implement API client wrapper (fetch-based)
3. Build alert-list component with pagination
4. Add severity filtering UI
5. Implement acknowledge/unacknowledge
6. Style with modern CSS (no framework needed)

**Phase 2: Detail Views (Week 2)**
7. Alert detail modal/page
8. Baseline browser component
9. Journal event viewer
10. Audit event viewer
11. Add search/filter UI

**Phase 3: Polish (Week 3)**
12. Responsive design (mobile layout)
13. Accessibility (ARIA labels, keyboard nav)
14. Error handling and loading states
15. Auto-refresh/polling (every 30s)
16. Local storage for preferences
17. Documentation and screenshots

**Testing:**
- Manual testing in Firefox/Chrome/Safari
- Test static file serving via API daemon
- Verify all API endpoints work through UI
- Test on mobile viewport sizes
- Check accessibility with screen reader

**Files to Create:**
- `web/index.html`
- `web/css/*.css`
- `web/js/**/*.js`
- `src/api/http_server.cpp` (add static serving)
- `CMakeLists.txt` (install web assets)
- `docs/web-dashboard.md` (usage guide)

**Installation:**
```cmake
# CMakeLists.txt
install(DIRECTORY web/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/vigilant-canine/web
    FILES_MATCHING
    PATTERN "*.html"
    PATTERN "*.css"
    PATTERN "*.js"
    PATTERN "*.svg"
)
```

**Deferred Features:**
- WebSocket real-time updates (polling is fine for MVP)
- User authentication (file permissions handle access control)
- Dark mode (nice-to-have)
- Export to CSV/JSON
- Alert graphs/charts
- Mobile app

---

## Implementation Order

**Recommended sequence:**

1. **Custom Rule Merging** (2-4 hours)
   - Unblocks user rule customization
   - Required for production deployments
   - Simple, low-risk change

2. **Scanner Statistics** (1-2 hours)
   - Quick win
   - Improves observability
   - No API changes needed

3. **Alert Filtering** (3-6 hours)
   - API enhancement
   - Useful even without web UI
   - Prepares for dashboard

4. **Web Dashboard** (2-3 weeks)
   - Large project
   - Can be done incrementally
   - Optional but high user value

**Total estimated time (excluding dashboard):** 6-12 hours
**Total estimated time (including dashboard):** 3-4 weeks

## Success Criteria

**For items 1-3:**
- [ ] All TODO comments removed from codebase
- [ ] Custom journal and audit rules work end-to-end
- [ ] Alert filtering parameters documented and tested
- [ ] Scanner statistics accurately track updates vs new files
- [ ] All existing tests pass
- [ ] New tests added for each feature
- [ ] No regressions in performance or memory usage

**For web dashboard:**
- [ ] Static assets served by API daemon
- [ ] All core features working (alerts, baselines, events)
- [ ] Responsive design works on mobile
- [ ] No JavaScript errors in console
- [ ] Documentation includes screenshots
- [ ] Installation via package manager works

## Notes

- Items 1-3 are bug fixes / technical debt and should be completed before v1.0 release
- Web dashboard is optional enhancement - core functionality works without it
- All work maintains backward compatibility with existing API
- No breaking changes to configuration format
