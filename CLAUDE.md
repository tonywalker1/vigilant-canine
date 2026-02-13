# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Vigilant Canine is a host-level Intrusion Detection System (IDS) for home users and small offices. The system monitors file integrity and system logs to detect suspicious activity, alerting users through desktop notifications and an optional dashboard.

**Critical Design Constraint**: Simplicity for average Linux users trumps features. This is NOT enterprise IDS/IPS software. The core daemon must work with zero UI dependencies and could ship enabled-by-default in distributions.

## Architecture

Client-server architecture with strict privilege separation:

- **vigilant-canined** — Privileged daemon (root or CAP_DAC_READ_SEARCH + CAP_AUDIT_READ)
  - File integrity monitoring (fanotify, inotify fallback)
  - Log analysis (systemd journal)
  - Rule evaluation and alert dispatch
  - C++, managed by systemd

- **vigilant-canined-api** — Unprivileged API daemon
  - REST-like API over Unix domain socket
  - Reads from shared SQLite database
  - Optional static web UI asset serving
  - No network exposure by default

- **Web dashboard** (optional) — Static HTML/JS frontend
  - Alert viewer and log dashboard
  - Compiles to static assets, no server-side runtime

- **Desktop notifications** (optional) — D-Bus client
  - Listens for alert signals from core daemon
  - Delivers freedesktop notifications

**Key architectural principle**: Compromising the API daemon or web UI does NOT grant root access. The core daemon accepts only a narrow set of validated commands over its internal socket.

See `docs/architecture.md` for complete details on component responsibilities, technology rationale, privilege model, and phased detection roadmap.

### Phase 1 Architecture Decisions

**Event System**: Uses synchronous EventBus (pub/sub pattern) instead of Boost.Asio. This
simplified Phase 1 implementation while maintaining clean component boundaries. Asio deferred
to Phase 2 for log analysis work.

**Daemon Main Loop**: Simple `while (!should_stop) { sleep(1); }` loop with signal handlers.
The fanotify monitor and event bus handle actual work asynchronously. No complex thread pool
needed for file integrity monitoring workload.

**Package Verification**: On-demand verification via PackageVerifier class, not
fanotify-based. Cleaner separation and simpler implementation.

These decisions were validated through implementation - don't second-guess them in future work.

## Technology Stack

- **Language**: C++ (primary), static HTML/JS (web UI)
- **Build**: CMake
- **Storage**: SQLite (alerts, baselines, config)
- **API**: HTTP over Unix domain socket
- **Notifications**: D-Bus (freedesktop standard)
- **File monitoring**: fanotify (with inotify fallback)
- **Log analysis**: systemd journal API
- **Target**: Linux (BSD not ruled out but not a priority)

## Code Style

The project enforces strict formatting and linting:

- **Formatter**: clang-format (see `.clang-format`)
  - 100-character line limit
  - 4-space indentation
  - Attach braces (`BreakBeforeBraces: Attach`)
  - Left pointer alignment (`PointerAlignment: Left`)
  - No bin-packing of arguments/parameters

- **Linter**: clang-tidy (see `.clang-tidy`)
  - Comprehensive checks enabled (boost, bugprone, cert, cppcoreguidelines, hicpp, modernize, performance, readability)
  - Memory safety is critical for a security daemon

## Development Environment

A development container is available in `containers/vc-dev/`:

```bash
# Build the development container
cd containers/vc-dev
./build.sh

# Run the development container
./run.sh
```

The container provides a consistent Linux development environment with necessary tools and libraries for building the daemon.

## Security Considerations

This is security-critical infrastructure software:

- **Memory safety**: Strict clang-tidy rules, sanitizers in CI (ASAN/TSAN)
- **Privilege separation**: The API daemon and UI run unprivileged
- **No network exposure**: Unix socket by default (can be proxied later if needed)
- **Input validation**: The core daemon strictly validates commands from the API daemon
- **Audit trail**: All alerts logged to systemd journal (works with zero optional components)

## Development Phases

See `docs/architecture.md` for the phased detection roadmap:

1. **Phase 1** (current focus) — File integrity monitoring via fanotify
2. **Phase 2** — Log analysis via systemd journal
3. **Phase 3** — auditd integration for deeper visibility

Prevention (IPS) is a future goal, not immediate. Potential integration with fapolicyd for policy enforcement.

## License

GPLv3 — ensuring an open future for this software. See LICENSE for details.
