# Installation Guide

This guide covers all methods for installing Vigilant Canine on Linux systems.

## Table of Contents

- [Package Installation (Recommended)](#package-installation-recommended)
  - [Fedora/RHEL/CentOS](#fedorarhelcentos)
  - [Ubuntu/Debian](#ubuntudebian)
- [Building from Source](#building-from-source)
  - [Dependencies](#dependencies)
  - [Build Steps](#build-steps)
  - [Installation](#installation)
- [Post-Installation Configuration](#post-installation-configuration)
- [Optional Components](#optional-components)
- [Verification](#verification)
- [Uninstallation](#uninstallation)

## Package Installation (Recommended)

Installing from distribution packages is the recommended method. Packages handle dependencies automatically and integrate cleanly with your system's package manager.

### Fedora/RHEL/CentOS

#### From COPR Repository

```bash
# Add COPR repository (once available)
sudo dnf copr enable yourusername/vigilant-canine

# Install core daemon
sudo dnf install vigilant-canine

# Optional: Install API daemon
sudo dnf install vigilant-canine-api
```

#### From Binary RPM

Download the appropriate RPM from the [releases page](https://github.com/tony/vigilant-canine/releases).

```bash
# Install core daemon
sudo dnf install vigilant-canine-0.1.0-1.fc40.x86_64.rpm

# Optional: Install API daemon
sudo dnf install vigilant-canine-api-0.1.0-1.fc40.x86_64.rpm
```

The package installs:
- `/usr/bin/vigilant-canined` - Core monitoring daemon
- `/usr/lib/systemd/system/vigilant-canined.service` - systemd service unit
- `/usr/lib/tmpfiles.d/vigilant-canine.conf` - Runtime directory configuration
- `/etc/vigilant-canine/config.toml.example` - Example configuration

### Ubuntu/Debian

#### From PPA

```bash
# Add PPA (once available)
sudo add-apt-repository ppa:yourusername/vigilant-canine
sudo apt update

# Install core daemon
sudo apt install vigilant-canine

# Optional: Install API daemon
sudo apt install vigilant-canine-api
```

#### From Binary DEB

Download the appropriate DEB from the [releases page](https://github.com/tony/vigilant-canine/releases).

```bash
# Install core daemon
sudo apt install ./vigilant-canine_0.1.0-1_amd64.deb

# Optional: Install API daemon
sudo apt install ./vigilant-canine-api_0.1.0-1_amd64.deb
```

The package installs:
- `/usr/bin/vigilant-canined` - Core monitoring daemon
- `/usr/lib/systemd/system/vigilant-canined.service` - systemd service unit
- `/usr/lib/tmpfiles.d/vigilant-canine.conf` - Runtime directory configuration
- `/etc/vigilant-canine/config.toml.example` - Example configuration

## Building from Source

Building from source is recommended for development or if packages are not available for your distribution.

### Dependencies

#### Build Requirements

**Fedora/RHEL:**
```bash
sudo dnf install gcc-c++ cmake pkg-config \
                 sqlite-devel systemd-devel \
                 openssl-devel blake3-devel \
                 audit-libs-devel hinder-devel
```

**Ubuntu/Debian:**
```bash
sudo apt install g++ cmake pkg-config \
                 libsqlite3-dev libsystemd-dev \
                 libssl-dev libblake3-dev \
                 libaudit-dev libhinder-dev
```

#### Runtime Dependencies

The following libraries are required at runtime (automatically installed by packages):
- SQLite3
- systemd (libsystemd)
- OpenSSL (libcrypto)
- BLAKE3
- libaudit
- libauparse
- Hinder

### Build Steps

```bash
# Clone repository
git clone https://github.com/tony/vigilant-canine.git
cd vigilant-canine

# Create build directory
mkdir -p build/gcc-release
cd build/gcc-release

# Configure
cmake -DCMAKE_BUILD_TYPE=Release \
      -DVC_BUILD_TESTS=ON \
      ../..

# Build
cmake --build . -j$(nproc)

# Run tests (optional)
ctest --output-on-failure
```

### Installation

```bash
# Install to /usr (requires root)
sudo cmake --install . --prefix /usr

# Create runtime directories
sudo systemd-tmpfiles --create /usr/lib/tmpfiles.d/vigilant-canine.conf

# Copy and customize configuration
sudo cp /etc/vigilant-canine/config.toml.example \
        /etc/vigilant-canine/config.toml
sudo $EDITOR /etc/vigilant-canine/config.toml
```

## Post-Installation Configuration

After installing via package or from source:

### 1. Configure the Daemon

```bash
# Create configuration from example
sudo cp /etc/vigilant-canine/config.toml.example \
        /etc/vigilant-canine/config.toml

# Edit configuration
sudo $EDITOR /etc/vigilant-canine/config.toml
```

See [Configuration Guide](configuration.md) for all available options.

### 2. Enable and Start the Service

```bash
# Reload systemd
sudo systemctl daemon-reload

# Enable on boot
sudo systemctl enable vigilant-canined.service

# Start now
sudo systemctl start vigilant-canined.service

# Check status
sudo systemctl status vigilant-canined.service

# View logs
sudo journalctl -u vigilant-canined -f
```

### 3. Initial Baseline Scan

On first run, the daemon automatically creates a baseline of package-managed files. This may take several minutes depending on your system. Monitor progress:

```bash
# Watch logs during initial scan
sudo journalctl -u vigilant-canined -f

# Check database for baseline entries
sqlite3 /var/lib/vigilant-canine/vc.db \
    "SELECT COUNT(*) FROM baselines;"
```

## Optional Components

### API Daemon

The API daemon provides a REST interface for querying alerts and events:

```bash
# Enable and start
sudo systemctl enable --now vigilant-canined-api.service

# Test endpoint
curl --unix-socket /run/vigilant-canine/api.sock \
    http://localhost/api/v1/health
```

See [API Documentation](api.md) for complete endpoint reference.

### Desktop Notifications

Desktop notifications are sent automatically via D-Bus. No additional configuration required for desktop users.

### User Home Directory Monitoring

To enable monitoring for user-installed software:

```bash
# Create user configuration directory
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

See [User Monitoring Guide](user-monitoring.md) for details.

## Verification

### Check Service Status

```bash
# Service running
sudo systemctl is-active vigilant-canined

# Service enabled on boot
sudo systemctl is-enabled vigilant-canined

# View recent logs
sudo journalctl -u vigilant-canined -n 50
```

### Verify File Monitoring

```bash
# Trigger a test alert by modifying a monitored file
# (This assumes /etc/hosts is monitored - check your config)
echo "# Test modification" | sudo tee -a /etc/hosts

# Check for alert in logs
sudo journalctl -u vigilant-canined | grep -i alert

# Remove test modification
sudo sed -i '/# Test modification/d' /etc/hosts
```

### Check Database

```bash
# View database schema
sqlite3 /var/lib/vigilant-canine/vc.db ".schema"

# Count baseline entries
sqlite3 /var/lib/vigilant-canine/vc.db \
    "SELECT COUNT(*) FROM baselines;"

# View recent alerts
sqlite3 /var/lib/vigilant-canine/vc.db \
    "SELECT timestamp, severity, message FROM alerts ORDER BY timestamp DESC LIMIT 5;"
```

## Uninstallation

### Package Installation

**Fedora/RHEL:**
```bash
# Stop and disable service
sudo systemctl stop vigilant-canined.service
sudo systemctl disable vigilant-canined.service

# Remove package (keeps config and data)
sudo dnf remove vigilant-canine

# Purge all data (optional)
sudo rm -rf /var/lib/vigilant-canine
sudo rm -rf /etc/vigilant-canine
```

**Ubuntu/Debian:**
```bash
# Stop and disable service
sudo systemctl stop vigilant-canined.service
sudo systemctl disable vigilant-canined.service

# Remove package (keeps config and data)
sudo apt remove vigilant-canine

# Purge package and all data
sudo apt purge vigilant-canine
```

### Source Installation

```bash
# Stop and disable service
sudo systemctl stop vigilant-canined.service
sudo systemctl disable vigilant-canined.service

# Remove installed files
sudo rm -f /usr/bin/vigilant-canined
sudo rm -f /usr/bin/vigilant-canined-api
sudo rm -f /usr/lib/systemd/system/vigilant-canined.service
sudo rm -f /usr/lib/systemd/system/vigilant-canined-api.service
sudo rm -f /usr/lib/tmpfiles.d/vigilant-canine.conf

# Remove configuration and data
sudo rm -rf /etc/vigilant-canine
sudo rm -rf /var/lib/vigilant-canine

# Reload systemd
sudo systemctl daemon-reload
```

## Troubleshooting

See [Troubleshooting Guide](troubleshooting.md) for common issues and solutions.

### Common Issues

**Service fails to start:**
```bash
# Check detailed logs
sudo journalctl -u vigilant-canined -xe

# Verify configuration
sudo vigilant-canined --config /etc/vigilant-canine/config.toml --validate

# Check permissions
ls -la /var/lib/vigilant-canine
ls -la /etc/vigilant-canine
```

**High CPU usage during initial scan:**
This is normal. The daemon is building the initial baseline. Wait for completion or adjust scan settings in configuration.

**No alerts appearing:**
Check that monitoring is enabled in configuration and that the daemon has appropriate permissions (requires root or CAP_DAC_READ_SEARCH + CAP_AUDIT_READ).

## Next Steps

- [Configure monitoring policies](configuration.md)
- [Set up user home directory monitoring](user-monitoring.md)
- [Enable API daemon](api.md)
- [Review architecture](architecture.md)
