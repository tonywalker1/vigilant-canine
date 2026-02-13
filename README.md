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

**Phase 1 Complete:** File integrity monitoring daemon is functional with 210 passing tests.

- ✅ File integrity monitoring via fanotify
- ✅ Package verification (rpm/dpkg)
- ✅ Desktop notifications via D-Bus
- ✅ SQLite storage and TOML configuration
- ⏳ systemd integration (in progress)
- ⏳ API daemon and web dashboard (planned)
- ⏳ Log analysis via systemd journal (Phase 2)

# Getting Started

## Dependencies

### Build Requirements
- C++23 compiler (GCC 15+ or Clang 18+)
- CMake 3.25+
- pkg-config

### Runtime Libraries
- Boost (system component)
- SQLite3
- libsystemd
- OpenSSL (libcrypto)
- BLAKE3 (libblake3)

### Fedora/RHEL
```bash
sudo dnf install gcc-c++ cmake boost-devel sqlite-devel systemd-devel \
                 openssl-devel blake3-devel pkg-config
```

### Debian/Ubuntu
```bash
sudo apt install g++ cmake libboost-system-dev libsqlite3-dev \
                 libsystemd-dev libssl-dev libblake3-dev pkg-config
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

## Running

The daemon requires elevated privileges to monitor file access events:

```bash
# Run directly (for testing)
sudo ./build/gcc-debug/vigilant-canined --config /etc/vigilant-canine/config.toml

# Or run in foreground with debug logging
sudo ./build/gcc-debug/vigilant-canined --config /etc/vigilant-canine/config.toml --log-level debug

# systemd integration coming soon
```

**Note:** Phase 1 focuses on the core monitoring daemon. The API daemon and web dashboard are planned for later phases.

## Initial Setup

On first run, the daemon will:

1. Create the database at the configured path (default: `/var/lib/vigilant-canine/vc.db`)
2. Detect your distribution's package manager (rpm or dpkg)
3. Build an initial baseline of package-managed files
4. Begin monitoring for unauthorized changes

You'll receive desktop notifications for any detected file modifications.

## Documentation

- [Configuration Guide](docs/configuration.md) - Detailed configuration options and examples
- [Architecture](docs/architecture.md) - System design and component responsibilities
- [Troubleshooting](docs/troubleshooting.md) - Common issues and solutions

# Helping

See [CONTRIBUTING.md](CONTRIBUTING.md) for how to get involved.
