# Vigilant Canine — Architecture

## Overview

Vigilant Canine is a host-level Intrusion Detection System (IDS) for home users and small offices. It monitors file
integrity and system logs to detect suspicious activity, alerting users through desktop notifications and an optional
dashboard.

The system is designed around a client-server architecture with strict privilege separation and a well-defined API. The
core daemon runs with no UI dependencies, making it lightweight enough to ship enabled-by-default in Linux
distributions.

## Design Principles

- **Simplicity over features.** Average Linux users must be able to install, understand, and use this without security
  expertise.
- **Low resource footprint.** Minimal dependencies, minimal CPU/memory usage, no heavy runtimes.
- **Privilege separation.** The monitoring engine runs privileged; everything else runs unprivileged.
- **Distro-shippable.** The core daemon must work with zero UI dependencies and sane defaults. Optional components
  (web UI, notification client) are separate packages.
- **Linux-first.** Linux is the primary target. BSD compatibility is not ruled out but is not a development priority.

## Component Architecture

```
┌─────────────────────────────────────────────────────┐
│  UI Layer (optional, unprivileged)                  │
│  ┌───────────────┐  ┌────────────────────────────┐  │
│  │ Desktop notif  │  │ Web dashboard / log viewer │  │
│  │ (D-Bus client) │  │ (talks to API)             │  │
│  └───────────────┘  └────────────────────────────┘  │
├─────────────────────────────────────────────────────┤
│  API Layer (reduced privilege)                      │
│  ┌────────────────────────────────────────────────┐ │
│  │ vigilant-canined-api                           │ │
│  │ HTTP over Unix socket, auth, alert queries     │ │
│  └────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────┤
│  Engine Layer (privileged)                          │
│  ┌──────────────┐ ┌────────────┐ ┌──────────────┐  │
│  │ File integrity│ │ Log analysis│ │ Audit system │  │
│  │ (fanotify)   │ │ (journal)  │ │ (libaudit)   │  │
│  └──────────────┘ └────────────┘ └──────────────┘  │
│  ┌────────────────────────────────────────────────┐ │
│  │ vigilant-canined (core daemon)                 │ │
│  │ EventBus, CorrelationEngine, PolicyEngine,     │ │
│  │ AlertDispatcher, DbusNotifier, AuditMonitor    │ │
│  └────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────┤
│  Storage: SQLite (alerts, baselines, config state)  │
└─────────────────────────────────────────────────────┘
```

### vigilant-canined (core daemon)

The privileged engine that performs actual monitoring. Runs as root or with specific Linux capabilities
(`CAP_DAC_READ_SEARCH`, `CAP_AUDIT_READ`).

Responsibilities:
- File integrity monitoring via fanotify (with inotify as a fallback where needed)
- Log analysis via systemd journal
- Rule evaluation and event correlation
- Alert generation and dispatch (journal, D-Bus notifications, internal socket to API daemon)

Written in C++. Managed by systemd.

Key components:
- **FanotifyMonitor** — Real-time file access event monitoring (Phase 1)
- **JournalMonitor** — systemd journal rule-based log monitoring (Phase 2)
- **AuditMonitor** — Linux audit subsystem monitoring with multi-record correlation (Phase 3)
- **UserManager** — User discovery and home directory monitoring with policy enforcement
- **CorrelationEngine** — Time-windowed event aggregation and threshold detection (Phase 2)
- **EventBus** — Synchronous pub/sub event distribution with mutex serialization
- **PolicyEngine** — Rule evaluation and event filtering
- **AlertDispatcher** — Multi-channel alert routing (SQLite, journal, D-Bus)
- **DbusNotifier** — Desktop notification delivery via freedesktop specification (Phase 2)

### vigilant-canined-api (API daemon)

An unprivileged process that provides the external API for UI clients. Communicates with the core daemon over an
internal Unix socket for commands; reads alert and state data from the shared SQLite database.

Responsibilities:
- Serve a REST-like API over a Unix domain socket (HTTP over UDS)
- Authenticate and authorize client requests (future)
- Serve static web UI assets (optional, future)
- Query alerts, baselines, and configuration state
- Provide acknowledge/unacknowledge operations for alert triage

Using a Unix socket by default means:
- No network attack surface out of the box
- Access control via file permissions on the socket
- Can be proxied through nginx/caddy later if remote access is desired

**Implementation Details:**

Written in C++, using cpp-httplib for HTTP-over-Unix-socket support. The daemon:
- Opens the SQLite database in read-write mode (for acknowledge operations only)
- Provides RESTful JSON API endpoints (see docs/api.md)
- Uses manual JSON serialization (no external JSON library dependency)
- Implements pagination for large result sets (limit/offset parameters)
- Returns structured error responses with error codes and messages

**Security model:**
- Runs as unprivileged user `vigilant-canine`
- Unix socket created with 0660 permissions (owner and group only)
- No detection rule modification allowed (read-only access to baselines)
- Cannot trigger scans or modify monitoring behavior
- Database access limited to reads and alert acknowledgment updates

**API Endpoints:**
- `GET /api/v1/health` - Health check
- `GET /api/v1/alerts` - List alerts (paginated, filterable by acknowledged status)
- `GET /api/v1/alerts/{id}` - Get specific alert
- `POST /api/v1/alerts/{id}/acknowledge` - Mark alert as reviewed
- `DELETE /api/v1/alerts/{id}/acknowledge` - Remove acknowledgment
- `GET /api/v1/baselines` - List file integrity baselines (filterable by source)
- `GET /api/v1/journal-events` - List journal log events
- `GET /api/v1/audit-events` - List audit subsystem events

See `docs/api.md` for complete API documentation.

### Web dashboard (optional)

A static-asset web frontend (HTML/JS) served by the API daemon. Provides a dashboard and log viewer for inspecting
alerts and system status. No server-side runtime required beyond the API daemon — the frontend compiles to static
files.

### Desktop notifications (optional)

Desktop notifications are delivered via **DbusNotifier**, a component within the core daemon that sends freedesktop
notifications over the session D-Bus. The daemon gracefully degrades on headless systems (notifications are skipped if
no session bus is available). Works with any desktop environment (KDE, GNOME, etc.).

Notifications include alert severity mapping to urgency levels:
- INFO → low urgency
- WARNING → normal urgency
- CRITICAL → critical urgency

### User Home Directory Monitoring

Home directory monitoring is handled by the **UserManager** component, which provides opt-in monitoring of user-installed software and configuration files while respecting privacy.

**Design principles:**
- **Opt-in by default**: Users must explicitly enable monitoring via `~/.config/vigilant-canine/config.toml`
- **Policy enforcement**: Administrators can mandate monitoring for specific users/groups
- **Privacy-focused**: Users control which paths are monitored and can exclude sensitive directories
- **Per-user baselines**: User files are stored separately in the database (`source = "user:username"`)

**How it works:**
1. **User Discovery**: On startup, UserManager enumerates all users with UID >= 1000 and interactive shells
2. **Policy Evaluation**: For each user, checks if monitoring applies (admin policy or user opt-in)
3. **Config Loading**: Reads user configurations from `~/.config/vigilant-canine/config.toml`
4. **Config Merging**: Merges system, policy, and user configurations with precedence rules (policy > user > system)
5. **Baseline Creation**: Creates file integrity baselines for user files, stored with source attribution
6. **Monitoring**: FanotifyMonitor watches user paths just like system paths, but alerts include username context

**Policy enforcement:**
- Administrators can configure `/etc/vigilant-canine/config.toml` with `[policy.home]` settings
- Policy can require monitoring for specific users or groups (e.g., administrators, developers)
- Policy can define mandatory paths that cannot be excluded by users
- Users can opt out only if `allow_user_opt_out = true` in policy

**Alert context:**
Alerts for user files include username attribution, allowing administrators to identify which user's files triggered an alert.

See [docs/user-monitoring.md](user-monitoring.md) for complete configuration and usage details.

## Technology Decisions

| Decision             | Choice                     | Rationale                                                    |
|----------------------|----------------------------|--------------------------------------------------------------|
| Engine language      | C++                        | Deep existing expertise, mature Linux API bindings            |
|                      |                            | (fanotify, audit netlink, inotify), clang tooling already     |
|                      |                            | configured. Memory safety addressed via ASAN/TSAN in CI and  |
|                      |                            | strict clang-tidy rules.                                     |
| Build system         | CMake                      | Standard for C++ projects, already in place.                  |
| API transport        | HTTP over Unix domain socket | No network exposure by default, file-permission ACL,         |
|                      |                            | easily proxied for remote access later.                      |
| Storage              | SQLite                     | Single file, no separate service, C API, handles             |
|                      |                            | single-host IDS scale easily, distro-friendly.               |
| Web UI               | Static HTML/JS             | No server-side runtime, compiled to static assets,           |
|                      |                            | framework TBD (leaning toward something lightweight          |
|                      |                            | like Vue or Svelte).                                         |
| Notifications        | D-Bus (freedesktop)        | Desktop-environment-agnostic, standard on Linux.             |
| Primary target       | Linux                      | BSD not ruled out but not a development priority.             |

### Alternatives Considered

**Rust for the engine:** Stronger memory safety guarantees, relevant for a security-critical daemon. Decided against
because C++ expertise means faster progress toward a working system, and the clang sanitizer/linter safety net is
substantial. Could be revisited if the project grows contributors who prefer Rust.

**Qt/QML desktop UI:** Natural fit for a KDE/C++ developer, but pulls in Qt as a dependency and limits the UI to
desktop sessions. A web UI serves both desktop and headless use cases with less overall effort than maintaining
separate Qt and GTK frontends.

**TCP socket for the API:** Rejected for the default configuration because it adds unnecessary network attack surface
on a host-level security tool. Unix socket provides the same functionality with better security properties.

## Detection Capabilities

### Phase 1 — File Integrity Monitoring ✅ Complete
- Baseline filesystem state (hashes, permissions, ownership) for critical paths
- Real-time change detection via fanotify
- Alerts on unauthorized modifications to system binaries, configuration files, etc.
- Package manager integration (rpm/dpkg) for verification

**Components:** FanotifyMonitor, BaselineStore, Scanner, PackageVerifier

### Phase 2 — Log Analysis ✅ Complete
- Monitor systemd journal for suspicious patterns (failed auth, privilege escalation, service failures)
- Rule-based pattern matching (regex, exact, contains, starts_with)
- Time-windowed event correlation with threshold detection and debouncing
- Desktop notifications via D-Bus (freedesktop specification)
- Configurable rules with 10 sane defaults

**Components:** JournalMonitor, CorrelationEngine, DbusNotifier

**Default rules:**
- Authentication failures (ssh, sudo, su, login)
- Privilege escalation attempts
- Service failures and crashes
- Kernel errors and OOM events
- Suspicious systemd operations

### Phase 3 — auditd Integration ✅ Complete
- Consume Linux audit events for deeper visibility (process execution, network connections, failed access, privilege changes)
- Multi-record correlation (SYSCALL+EXECVE+CWD+PATH) with 100ms timeout windows
- Command-line sanitization to prevent credential exposure in logs
- 10 default audit rules for security monitoring
- Graceful degradation when audit subsystem unavailable
- Potential future integration with fapolicyd for policy enforcement (moving toward IPS)

**Components:** AuditMonitor, AuditRule engine, AuditEventStore

**Default rules:**
- Unauthorized file access attempts
- Process execution monitoring
- Network connection tracking
- Privilege escalation detection
- User context attribution

## Alert Flow

1. A detection module (file integrity, log analysis, etc.) generates an event.
2. The core daemon evaluates the event against rules and generates an alert if warranted.
3. The alert is:
   - Written to SQLite (for dashboard/API queries)
   - Logged to the systemd journal (always — works with zero optional components)
   - Dispatched via D-Bus signal (if the notification client is running)
   - Optionally sent via email (future, requires careful design to avoid complexity)

## Privilege Model

| Component              | Runs as         | Capabilities needed                        |
|------------------------|-----------------|---------------------------------------------|
| vigilant-canined       | root (or caps)  | CAP_DAC_READ_SEARCH, CAP_AUDIT_READ         |
| vigilant-canined-api   | unprivileged    | Read access to SQLite DB, socket to daemon   |
| Web UI                 | browser         | Talks to API via Unix socket (or proxy)      |
| Notification client    | user session    | D-Bus session bus access                     |

Compromising the API daemon or web UI does not grant root access. The core daemon accepts only a narrow set of
commands over its internal socket (e.g., rescan, reload config) and validates them strictly.
