# systemd Integration Plan for Vigilant Canine

## Context

All three detection phases (file integrity, log analysis, audit subsystem) are complete and the core daemon is production-ready. However, it currently lacks systemd integration, requiring manual execution and lacking distribution packaging support.

This plan adds systemd service files, installation targets, and documentation to enable:
- Automatic daemon startup at boot
- Proper privilege management via capabilities
- Security hardening through systemd sandboxing
- Clean lifecycle management (start/stop/reload)
- Distribution-agnostic installation

The daemon already integrates with systemd (journal logging, signal handling), so this work primarily involves creating service files and build system integration.

## Recommended Approach

### Design Decisions

**Service Type: Type=simple**
- Daemon doesn't currently call `sd_notify()`, so Type=notify would hang
- Type=simple is robust and appropriate for the current architecture
- Can upgrade to Type=notify later if needed (requires adding sd_notify calls)

**Security Hardening: Aggressive**
- Apply comprehensive systemd sandboxing (ProtectSystem, ProtectHome, PrivateTmp, etc.)
- Restrict capabilities to only CAP_DAC_READ_SEARCH and CAP_AUDIT_READ
- Enable SystemCallFilter to block privileged syscalls while allowing fanotify/audit
- Use NoNewPrivileges, LockPersonality, RestrictSUIDSGID for defense-in-depth

**Installation: Distribution-agnostic**
- Use pkg-config to detect systemd paths (works on Fedora and Debian/Ubuntu)
- Support both manual installation and future packaging (RPM/DEB)
- tmpfiles.d for automatic directory creation

**API Daemon: Deferred**
- Socket activation deferred until vigilant-canined-api is implemented
- Current plan focuses only on the core daemon

## Implementation Steps

### 1. Create systemd Directory and Service Files

**File: `systemd/vigilant-canined.service`**
```ini
[Unit]
Description=Vigilant Canine Host Intrusion Detection System
Documentation=https://github.com/tony/vigilant-canine
After=network.target auditd.service systemd-journald.service
Wants=auditd.service

[Service]
Type=simple
ExecStart=/usr/bin/vigilant-canined --config /etc/vigilant-canine/config.toml
Restart=on-failure
RestartSec=10s
TimeoutStartSec=30
TimeoutStopSec=30

# User/Group (run as root for capabilities)
User=root
Group=root

# Capabilities
AmbientCapabilities=CAP_DAC_READ_SEARCH CAP_AUDIT_READ
CapabilityBoundingSet=CAP_DAC_READ_SEARCH CAP_AUDIT_READ

# Security Hardening
NoNewPrivileges=yes
PrivateTmp=yes
ProtectSystem=strict
ProtectHome=yes
ProtectKernelTunables=yes
ProtectKernelModules=yes
ProtectKernelLogs=no
ProtectControlGroups=yes
RestrictRealtime=yes
RestrictSUIDSGID=yes
LockPersonality=yes
RestrictNamespaces=yes
RestrictAddressFamilies=AF_UNIX AF_NETLINK

# Filesystem access
ReadWritePaths=/var/lib/vigilant-canine
ReadOnlyPaths=/etc/vigilant-canine

# System call filtering
SystemCallFilter=@system-service
SystemCallFilter=~@privileged @resources
SystemCallFilter=fanotify_init fanotify_mark inotify_init inotify_add_watch

# Memory protection
MemoryDenyWriteExecute=yes

# Logging
StandardOutput=journal
StandardError=journal
SyslogIdentifier=vigilant-canined

[Install]
WantedBy=multi-user.target
```

**Key features:**
- `Wants=auditd.service` (not Requires) for graceful degradation if audit unavailable
- Explicit ReadWritePaths for database, ReadOnlyPaths for config
- SystemCallFilter allows fanotify/inotify/audit syscalls while blocking most privileged operations
- ProtectKernelLogs=no because daemon needs journal access

**File: `systemd/vigilant-canine.conf` (tmpfiles.d)**
```
# Vigilant Canine runtime directories
# See tmpfiles.d(5)

# State directory (database, baselines)
d /var/lib/vigilant-canine 0750 root root -

# Configuration directory
d /etc/vigilant-canine 0755 root root -
```

**File: `systemd/README.md`**
```markdown
# systemd Integration

## Manual Installation

After building the project:

```bash
# Install to /usr (requires root)
sudo cmake --install build/gcc-release --prefix /usr

# Create runtime directories
sudo systemd-tmpfiles --create /usr/lib/tmpfiles.d/vigilant-canine.conf

# Install and customize configuration
sudo cp /etc/vigilant-canine/config.toml.example /etc/vigilant-canine/config.toml
sudo $EDITOR /etc/vigilant-canine/config.toml

# Enable and start service
sudo systemctl daemon-reload
sudo systemctl enable --now vigilant-canined.service

# Verify operation
sudo systemctl status vigilant-canined.service
sudo journalctl -u vigilant-canined -f
```

## Packaged Installation

When installed via package manager (RPM/DEB), files are installed automatically.
Enable the service with:

```bash
sudo systemctl enable --now vigilant-canined.service
```

## Service Management

```bash
# Check status
sudo systemctl status vigilant-canined.service

# View logs
sudo journalctl -u vigilant-canined -f

# Reload configuration (SIGHUP)
sudo systemctl kill -s HUP vigilant-canined.service

# Restart service
sudo systemctl restart vigilant-canined.service

# Stop service
sudo systemctl stop vigilant-canined.service

# Disable autostart
sudo systemctl disable vigilant-canined.service
```

## Uninstallation

```bash
sudo systemctl disable --now vigilant-canined.service
sudo rm /usr/lib/systemd/system/vigilant-canined.service
sudo rm /usr/lib/tmpfiles.d/vigilant-canine.conf
sudo rm /usr/bin/vigilant-canined
sudo rm -rf /etc/vigilant-canine /var/lib/vigilant-canine
```

## Troubleshooting

### Service fails to start

Check journal for errors:
```bash
sudo journalctl -u vigilant-canined -xe
```

Common issues:
- Missing capabilities (needs CAP_DAC_READ_SEARCH and CAP_AUDIT_READ)
- Invalid configuration file syntax
- Database directory not writable

### Verify security sandboxing

```bash
systemd-analyze security vigilant-canined.service
```

This shows the security posture and any potential improvements.
```

### 2. Update CMakeLists.txt

**Location:** Lines 136-149

Replace the current install section with:

```cmake
# Installation
include(GNUInstallDirs)

# Binary
install(TARGETS vigilant-canined
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Configuration example
install(FILES config/vigilant-canine.toml.example
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/vigilant-canine
    RENAME config.toml.example
)

# systemd service files
# Use pkg-config to detect systemd paths (works on Fedora/Debian/Ubuntu)
pkg_get_variable(SYSTEMD_SYSTEM_UNIT_DIR systemd systemdsystemunitdir)
pkg_get_variable(TMPFILES_DIR systemd tmpfilesdir)

# Fallback for packaging (DESTDIR set by rpmbuild/dpkg-buildpackage)
if(NOT SYSTEMD_SYSTEM_UNIT_DIR)
    set(SYSTEMD_SYSTEM_UNIT_DIR /usr/lib/systemd/system)
endif()
if(NOT TMPFILES_DIR)
    set(TMPFILES_DIR /usr/lib/tmpfiles.d)
endif()

install(FILES systemd/vigilant-canined.service
    DESTINATION ${SYSTEMD_SYSTEM_UNIT_DIR}
)

install(FILES systemd/vigilant-canine.conf
    DESTINATION ${TMPFILES_DIR}
)

# Documentation
install(FILES
    README.md
    docs/architecture.md
    docs/configuration.md
    docs/troubleshooting.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)
```

**Key changes:**
- Use GNUInstallDirs for standard FHS paths
- pkg-config detection for systemd paths (distribution-agnostic)
- Fallback to FHS standard paths for packaging
- Install config example separately (don't overwrite user configs)
- Install documentation to standard location

### 3. Update README.md

**Location:** After line 126 ("# systemd integration coming soon")

Replace the "systemd integration coming soon" section with:

```markdown
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

## Manual Execution (Development/Testing)

For development or testing without systemd:

```bash
# Run directly (for testing)
sudo ./build/gcc-debug/vigilant-canined --config /etc/vigilant-canine/config.toml

# Or run in foreground with debug logging (if --log-level flag exists)
sudo ./build/gcc-debug/vigilant-canined --config /etc/vigilant-canine/config.toml
```
```

### 4. Update docs/troubleshooting.md

Add a new section at the end:

```markdown
## systemd Integration

### Service Management

**Check service status:**
```bash
sudo systemctl status vigilant-canined.service
```

**View logs:**
```bash
# Follow logs in real-time
sudo journalctl -u vigilant-canined -f

# View recent logs with errors
sudo journalctl -u vigilant-canined -xe

# View logs since boot
sudo journalctl -u vigilant-canined -b
```

**Reload configuration without restart:**
```bash
sudo systemctl kill -s HUP vigilant-canined.service
```

Check journal to verify reload succeeded.

### Common Issues

**Service fails to start:**

Check journal for specific error:
```bash
sudo journalctl -u vigilant-canined -xe
```

Common causes:
- **Missing capabilities**: Service needs `CAP_DAC_READ_SEARCH` and `CAP_AUDIT_READ`
- **Invalid config**: Check `/etc/vigilant-canine/config.toml` syntax (TOML format)
- **Database permissions**: `/var/lib/vigilant-canine/` must be writable by root
- **Missing directories**: Run `sudo systemd-tmpfiles --create /usr/lib/tmpfiles.d/vigilant-canine.conf`

**Audit subsystem monitoring disabled:**

If logs show "Audit monitoring unavailable":
```bash
# Check auditd status
sudo systemctl status auditd

# Enable and start
sudo systemctl enable --now auditd

# Restart vigilant-canined
sudo systemctl restart vigilant-canined.service
```

The daemon gracefully degrades if audit is unavailable (file and log monitoring continue).

**Verify security hardening:**

```bash
systemd-analyze security vigilant-canined.service
```

Should show high security score with CAP_DAC_READ_SEARCH and CAP_AUDIT_READ as only ambient capabilities.

**Service restarts repeatedly:**

```bash
# Check for crash loop
sudo journalctl -u vigilant-canined -n 50

# Disable auto-restart temporarily for debugging
sudo systemctl set-property vigilant-canined.service Restart=no
sudo systemctl restart vigilant-canined.service
```

Check for configuration errors or resource exhaustion.
```

## Critical Files

The following files need modification or creation:

**New files:**
- `systemd/vigilant-canined.service` (systemd service unit)
- `systemd/vigilant-canine.conf` (tmpfiles.d configuration)
- `systemd/README.md` (installation and usage guide)

**Modified files:**
- `CMakeLists.txt` (lines 136-149: replace install targets)
- `README.md` (line 126: replace "coming soon" with installation instructions)
- `docs/troubleshooting.md` (append systemd troubleshooting section)

## Verification

After implementation, verify the integration works:

### Build and Install
```bash
# Clean build
mkdir -p build/gcc-release
cd build/gcc-release
cmake -DCMAKE_BUILD_TYPE=Release ../..
cmake --build . -j$(nproc)

# Install
sudo cmake --install . --prefix /usr
```

### Verify File Installation
```bash
# Check files installed correctly
ls -la /usr/bin/vigilant-canined
ls -la /usr/lib/systemd/system/vigilant-canined.service
ls -la /usr/lib/tmpfiles.d/vigilant-canine.conf
ls -la /etc/vigilant-canine/config.toml.example
ls -la /usr/share/doc/vigilant-canine/
```

### Create Directories and Configure
```bash
# Create runtime directories
sudo systemd-tmpfiles --create /usr/lib/tmpfiles.d/vigilant-canine.conf

# Verify directory creation and permissions
ls -lad /var/lib/vigilant-canine  # Should be drwxr-x--- root root
ls -lad /etc/vigilant-canine       # Should be drwxr-xr-x root root

# Install config
sudo cp /etc/vigilant-canine/config.toml.example /etc/vigilant-canine/config.toml
```

### Start and Verify Service
```bash
# Reload systemd
sudo systemctl daemon-reload

# Start service
sudo systemctl start vigilant-canined.service

# Check status
sudo systemctl status vigilant-canined.service

# Should show "active (running)"
```

### Verify Operation
```bash
# Check journal logs
sudo journalctl -u vigilant-canined -n 50

# Should see "Daemon initialized" and "Daemon running" messages

# Verify database created
sudo ls -la /var/lib/vigilant-canine/vc.db

# Verify capabilities (should only show CAP_DAC_READ_SEARCH and CAP_AUDIT_READ)
sudo grep Cap /proc/$(pgrep vigilant-canined)/status
```

### Test Signal Handling
```bash
# Test config reload (SIGHUP)
sudo systemctl kill -s HUP vigilant-canined.service
sudo journalctl -u vigilant-canined -n 20
# Should show "Reloading configuration"

# Test graceful stop
sudo systemctl stop vigilant-canined.service
sudo journalctl -u vigilant-canined -n 10
# Should show "Daemon stopped"

# Test restart
sudo systemctl restart vigilant-canined.service
sudo systemctl status vigilant-canined.service
```

### Test Autostart
```bash
# Enable service
sudo systemctl enable vigilant-canined.service

# Verify enabled
systemctl is-enabled vigilant-canined.service
# Should output "enabled"

# Optional: Reboot and verify service starts automatically
```

### Security Verification
```bash
# Analyze security posture
systemd-analyze security vigilant-canined.service

# Should show:
# - Overall exposure level: 1.5 or lower (good security)
# - PrivateTmp, ProtectSystem, ProtectHome enabled
# - Limited capabilities (only CAP_DAC_READ_SEARCH, CAP_AUDIT_READ)
```

### Test on Multiple Distributions (if available)

**Fedora 40+:**
```bash
# Should use /usr/lib/systemd/system
# SELinux should not block operation (check audit.log if issues)
```

**Ubuntu 24.04+:**
```bash
# Should use /lib/systemd/system (via pkg-config)
# AppArmor should not block operation
```

## Success Criteria

- [ ] All systemd files created and properly formatted
- [ ] CMakeLists.txt install targets work without errors
- [ ] Service starts cleanly and shows "active (running)"
- [ ] All monitoring subsystems operational (fanotify, journal, audit)
- [ ] Config reload (SIGHUP) works without service restart
- [ ] Graceful shutdown completes within TimeoutStopSec
- [ ] Database writes succeed with ProtectSystem=strict
- [ ] systemd-analyze security shows appropriate hardening
- [ ] Documentation complete and accurate
- [ ] Service autostart on boot works

## Future Work (Deferred)

These items are explicitly deferred until API daemon implementation:

- Socket activation for API daemon (vigilant-canined-api.socket)
- API daemon service unit (vigilant-canined-api.service)
- Runtime directory `/run/vigilant-canine` for socket files
- Separate user-level notification client service (if needed - DbusNotifier currently embedded)
- RPM/DEB packaging specs (`.spec` and `debian/` directory)
- sd_notify() support for Type=notify (requires daemon code changes)
