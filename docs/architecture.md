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
│  │ File integrity│ │ Log analysis│ │ auditd       │  │
│  │ (fanotify)   │ │ (journal)  │ │ (future)     │  │
│  └──────────────┘ └────────────┘ └──────────────┘  │
│  ┌────────────────────────────────────────────────┐ │
│  │ vigilant-canined (core daemon)                 │ │
│  │ Event bus, rule engine, alert dispatch          │ │
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

### vigilant-canined-api (API daemon)

An unprivileged process that provides the external API for UI clients. Communicates with the core daemon over an
internal Unix socket for commands; reads alert and state data from the shared SQLite database.

Responsibilities:
- Serve a REST-like API over a Unix domain socket (HTTP over UDS)
- Authenticate and authorize client requests
- Serve static web UI assets (optional)
- Query alerts, baselines, and configuration state

Using a Unix socket by default means:
- No network attack surface out of the box
- Access control via file permissions on the socket
- Can be proxied through nginx/caddy later if remote access is desired

### Web dashboard (optional)

A static-asset web frontend (HTML/JS) served by the API daemon. Provides a dashboard and log viewer for inspecting
alerts and system status. No server-side runtime required beyond the API daemon — the frontend compiles to static
files.

### Desktop notifications (optional)

A lightweight D-Bus client that listens for alert signals and delivers desktop notifications via the standard
freedesktop notification interface. Works with any desktop environment (KDE, GNOME, etc.).

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

## Detection Capabilities (Planned)

### Phase 1 — File Integrity Monitoring
- Baseline filesystem state (hashes, permissions, ownership) for critical paths
- Real-time change detection via fanotify
- Alerts on unauthorized modifications to system binaries, configuration files, etc.

### Phase 2 — Log Analysis
- Monitor systemd journal for suspicious patterns (failed auth, privilege escalation, etc.)
- Configurable rules with sane defaults

### Phase 3 — auditd Integration
- Consume Linux audit events for deeper visibility (process execution, file access by user, etc.)
- Potential future integration with fapolicyd for policy enforcement (moving toward IPS)

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
