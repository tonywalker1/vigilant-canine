# Vigilant Canine

Simple, host-level Intrusion Detection System (IDS) designed for average users.

# Introduction

The name "vigilant canine" is a respectful reference the dutiful, loyal, and protective family
dog. This software, like the family dog, should alert you to danger but not be intimidating or
imposing. Well, dogs do love attention, but this software should not require as much play, care,
and feeding.

There are several enterprise-grade Intrusion Detection/Prevention Systems (IDS/IPS) currently
available (OSSEC and Wahzu are popular examples). This project does not compete with these products
nor does it target their users. Instead, this project intends to provide a very simple to use, low
resource, and completely opensource solution for home users (or maybe small offices). The goal
is that this software is so simple that a Linux/BSD distribution could automatically ship this
software enabled and novice users could easily understand and use it.

# Goals

- Super simple to install, configure, understand, and use.
- Low resources.
- Meant for average users (from inexperienced Linux users to experienced users who just need to
  protect a few systems).
- Could be shipped enabled by distributions.

# Architecture

Vigilant Canine uses a client-server architecture with privilege separation:

- **vigilant-canined** — privileged daemon that monitors file integrity (fanotify) and system logs (journal)
- **vigilant-canined-api** — unprivileged API daemon serving a REST-like interface over a Unix socket
- **Web dashboard** — optional static-asset frontend for viewing alerts and system status
- **Desktop notifications** — optional D-Bus client for real-time alert delivery

See [docs/architecture.md](docs/architecture.md) for full details and rationale.

# Status

**v1.0 Feature-Complete:** Full-featured host IDS with file integrity, log analysis, and audit monitoring.

- ✅ **Phase 1**: File integrity monitoring via fanotify
- ✅ **Phase 1**: Package verification (rpm/dpkg)
- ✅ **Phase 1**: SQLite storage and TOML configuration
- ✅ **Phase 2**: Log analysis via systemd journal with custom rule support
- ✅ **Phase 2**: Event correlation engine with time-windowed aggregation
- ✅ **Phase 2**: Desktop notifications via D-Bus (freedesktop spec)
- ✅ **Phase 3**: Linux audit subsystem integration with custom rule support
- ✅ **Phase 3**: Process execution tracking and user attribution
- ✅ **Phase 3**: Multi-record event correlation with command-line sanitization
- ✅ **systemd integration**: Service files and installation support
- ✅ **API daemon**: REST API over Unix socket with advanced filtering
- ✅ **User home directory monitoring**: Opt-in monitoring for user-installed software with policy enforcement
- ✅ **Custom detection rules**: User-configurable journal and audit rules via TOML config
- ✅ **Enhanced observability**: Detailed scanner statistics and alert filtering
- ⏳ **Web dashboard**: Static frontend (optional, not required for core functionality)

# Installation

## From Package Repository (Recommended)

### Fedora/RHEL
```bash
# Add COPR repository (once available)
sudo dnf copr enable yourusername/vigilant-canine

# Install
sudo dnf install vigilant-canine
```

### Ubuntu/Debian
```bash
# Add PPA (once available)
sudo add-apt-repository ppa:yourusername/vigilant-canine
sudo apt update

# Install
sudo apt install vigilant-canine
```

## From Binary Package

Download the appropriate package for your distribution from the releases page.

### Fedora/RHEL
```bash
sudo dnf install vigilant-canine-*.rpm
```

### Ubuntu/Debian
```bash
sudo apt install ./vigilant-canine_*.deb
```

## From Source

See [Building from Source](#building-from-source) below for manual compilation and installation.

# Building from Source

## Dependencies

### Build Requirements
- C++23 compiler (GCC 15+ or Clang 18+)
- CMake 3.25+
- pkg-config

### Runtime Libraries
- SQLite3
- libsystemd (journal and D-Bus APIs)
- OpenSSL (libcrypto)
- BLAKE3 (libblake3)

### Fedora/RHEL
```bash
sudo dnf install gcc-c++ cmake sqlite-devel systemd-devel \
                 openssl-devel blake3-devel pkg-config
```

### Debian/Ubuntu
```bash
sudo apt install g++ cmake libsqlite3-dev libsystemd-dev \
                 libssl-dev libblake3-dev pkg-config
```

## Building

The project uses structured build directories to keep the source tree clean:

```bash
# Create and enter build directory
mkdir -p build/gcc-debug
cd build/gcc-debug

# Configure (Debug build)
cmake -DCMAKE_BUILD_TYPE=Debug ../..

# Or configure (Release build)
cmake -DCMAKE_BUILD_TYPE=Release ../..

# Build
cmake --build . -j$(nproc)

# Run tests
ctest --output-on-failure
```

## Configuration

Copy the example configuration and customize:

```bash
sudo mkdir -p /etc/vigilant-canine
sudo cp config/vigilant-canine.toml.example /etc/vigilant-canine/config.toml
sudo $EDITOR /etc/vigilant-canine/config.toml
```

See [config/vigilant-canine.toml.example](config/vigilant-canine.toml.example) for all available options.

## systemd Installation

After building:

```bash
# Install to /usr (requires root)
sudo cmake --install build/gcc-release --prefix /usr

# Create runtime directories
sudo systemd-tmpfiles --create /usr/lib/tmpfiles.d/vigilant-canine.conf

# Configure the daemon
sudo cp /etc/vigilant-canine/config.toml.example /etc/vigilant-canine/config.toml
sudo $EDITOR /etc/vigilant-canine/config.toml

# Enable and start
sudo systemctl daemon-reload
sudo systemctl enable --now vigilant-canined.service

# Verify
sudo systemctl status vigilant-canined.service
sudo journalctl -u vigilant-canined -f
```

See [systemd/README.md](systemd/README.md) for complete installation, management, and troubleshooting instructions.

## API Daemon (Optional)

The API daemon provides a RESTful HTTP interface for querying alerts, baselines, and events:

```bash
# Start with systemd
sudo systemctl enable --now vigilant-canined-api.service

# Or run manually for testing
./build/gcc-debug/vigilant-canined-api \
    -s /tmp/api.sock \
    -d /var/lib/vigilant-canine/vc.db

# Query health endpoint
curl --unix-socket /tmp/api.sock http://localhost/api/v1/health

# List recent alerts
curl --unix-socket /tmp/api.sock \
    'http://localhost/api/v1/alerts?limit=10'
```

See [docs/api.md](docs/api.md) for complete API documentation and endpoint details.

A testing script is provided for manual verification:
```bash
./scripts/test-api.sh
```

**Note:** The core monitoring daemon is feature-complete with file integrity monitoring, log analysis, and audit subsystem integration. The API daemon provides optional query capabilities for external tools and dashboards. A web dashboard frontend is planned for future releases.

## Custom Detection Rules

Vigilant Canine supports user-defined detection rules for both journal log analysis and audit subsystem events. Custom rules are defined in `/etc/vigilant-canine/config.toml` and are merged with the built-in default rules.

### Journal Rules

Add custom log pattern matching rules to detect application-specific events:

```toml
[[journal.rules]]
name = "docker_container_failed"
description = "Docker container failed to start"
enabled = true
action = "service_state"
severity = "warning"

[[journal.rules.match]]
field = "_SYSTEMD_UNIT"
pattern = "docker"
type = "contains"
negate = false
```

**Match types**: `exact`, `contains`, `regex`, `starts_with`
**Actions**: `auth_failure`, `privilege_escalation`, `service_state`, `suspicious_log`
**Severities**: `info`, `warning`, `critical`

### Audit Rules

Add custom syscall and file access detection rules:

```toml
[[audit.rules]]
name = "sensitive_dir_access"
description = "Access to sensitive directory"
enabled = true
action = "failed_access"
severity = "warning"
syscall_filter = 0  # 0 = no filter

[[audit.rules.match]]
field = "name"
pattern = "/opt/secrets"
type = "starts_with"
negate = false
```

**Match types**: `exact`, `contains`, `regex`, `starts_with`, `numeric_eq`, `numeric_gt`, `numeric_lt`
**Actions**: `process_execution`, `network_connection`, `failed_access`, `privilege_change`, `suspicious_syscall`

See `config/vigilant-canine.toml.example` for complete examples and `docs/configuration.md` for detailed rule syntax.

## Initial Setup

On first run, the daemon will:

1. Create the database at the configured path (default: `/var/lib/vigilant-canine/vc.db`)
2. Detect your distribution's package manager (rpm or dpkg)
3. Build an initial baseline of package-managed files
4. Begin monitoring for:
   - Unauthorized file modifications (fanotify)
   - Suspicious log patterns (systemd journal)
   - Process execution and privilege changes (Linux audit subsystem)

You'll receive desktop notifications for security events detected across all monitoring subsystems.

## User Home Directory Monitoring

Vigilant Canine can monitor user home directories for changes to user-installed software and
configuration files. This feature:

- **Opt-in by default**: Users must explicitly enable monitoring via `~/.config/vigilant-canine/config.toml`
- **Privacy-focused**: Users can exclude sensitive directories (Downloads, caches, etc.)
- **Policy enforcement**: Administrators can require monitoring for specific users or groups

### Enabling User Monitoring

To enable monitoring for your user account:

```bash
# Create configuration directory
mkdir -p ~/.config/vigilant-canine

# Copy example configuration
cp /usr/share/doc/vigilant-canine/user-config.toml.example \
   ~/.config/vigilant-canine/config.toml

# Edit and enable
$EDITOR ~/.config/vigilant-canine/config.toml
# Set: [monitor.home] enabled = true

# Restart daemon
sudo systemctl restart vigilant-canined.service
```

See [docs/user-monitoring.md](docs/user-monitoring.md) for complete documentation, including
policy configuration for administrators.

## Documentation

- [Configuration Guide](docs/configuration.md) - Detailed configuration options and examples
- [User Home Directory Monitoring](docs/user-monitoring.md) - User monitoring setup and policy enforcement
- [Architecture](docs/architecture.md) - System design and component responsibilities
- [API Documentation](docs/api.md) - REST API endpoints and usage examples
- [Troubleshooting](docs/troubleshooting.md) - Common issues and solutions

# Helping

See [CONTRIBUTING.md](CONTRIBUTING.md) for how to get involved.
