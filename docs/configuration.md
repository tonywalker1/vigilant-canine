# Configuration Guide

Vigilant Canine uses TOML format for configuration. The default configuration file location is `/etc/vigilant-canine/config.toml`.

## File Location

The daemon searches for configuration in the following order:

1. Path specified via `--config` command-line flag
2. `/etc/vigilant-canine/config.toml`
3. Built-in defaults (if no config file exists)

## Configuration Sections

### `[daemon]` - Core Daemon Settings

```toml
[daemon]
log_level = "info"
db_path = "/var/lib/vigilant-canine/vc.db"
worker_threads = 0
```

- **log_level**: Logging verbosity. Options: `trace`, `debug`, `info`, `warn`, `error`
  - Use `debug` or `trace` for troubleshooting
  - Default: `info`

- **db_path**: SQLite database location for baselines and alerts
  - Directory must exist and be writable by the daemon
  - Default: `/var/lib/vigilant-canine/vc.db`

- **worker_threads**: Number of worker threads for file scanning
  - `0` = auto-detect based on CPU cores
  - Default: `0`

### `[hash]` - Hash Algorithm Settings

```toml
[hash]
algorithm = "blake3"
```

- **algorithm**: Hash algorithm for file integrity verification
  - Currently only `blake3` is supported
  - BLAKE3 provides cryptographic-strength hashing with excellent performance
  - Default: `blake3`

### `[monitor.system]` - System File Monitoring

```toml
[monitor.system]
paths = ["/etc", "/boot", "/usr/local/bin"]
exclude = ["/etc/machine-id", "/etc/hostname"]
```

- **paths**: Additional system directories to monitor beyond package-managed files
  - Package-managed files are automatically included based on your distribution
  - Common additions: `/etc`, `/boot`, `/usr/local/bin`, `/usr/local/sbin`
  - Default: See example config

- **exclude**: Patterns to exclude from monitoring
  - Supports glob patterns
  - Use for files that change frequently (machine-id, hostname, resolv.conf)
  - Temporary files should be excluded
  - Default: See example config

### `[monitor.flatpak]` - Flatpak Monitoring

```toml
[monitor.flatpak]
enabled = true
system = true
user = false
```

- **enabled**: Enable Flatpak application monitoring
  - Detects tampering with Flatpak installations
  - Default: `true`

- **system**: Monitor system-wide Flatpak installations (`/var/lib/flatpak`)
  - Default: `true`

- **user**: Monitor per-user Flatpak installations (`~/.local/share/flatpak`)
  - Can generate many alerts on multi-user systems
  - Default: `false`

### `[monitor.ostree]` - OSTree Monitoring

```toml
[monitor.ostree]
enabled = true
verify_deployments = true
monitor_object_store = true
```

Relevant for OSTree-based distributions like Fedora Silverblue, Fedora Kinoite, and other rpm-ostree systems.

- **enabled**: Enable OSTree deployment monitoring
  - Auto-disabled on non-OSTree systems
  - Default: `true`

- **verify_deployments**: Verify deployment checksums on boot
  - Ensures bootable deployments haven't been tampered with
  - Default: `true`

- **monitor_object_store**: Monitor the OSTree object store
  - Watches `/ostree/repo` for unauthorized changes
  - Default: `true`

### `[monitor.home]` - Home Directory Monitoring

```toml
[monitor.home]
enabled = false
paths = ["~/.ssh", "~/.gnupg"]
exclude = ["~/.cache", "~/.local/share/Trash"]
```

**Privacy Warning:** Home directory monitoring is disabled by default. Enable with caution.

- **enabled**: Enable monitoring of user home directories
  - **Disabled by default for privacy reasons**
  - Only enable if you understand the privacy implications
  - Default: `false`

- **paths**: Specific paths within home directories to monitor
  - Tilde (`~`) expansion is supported
  - Useful for protecting SSH keys, GPG keys, etc.
  - Only applies if `enabled = true`

- **exclude**: Patterns to exclude from home monitoring
  - Cache directories and trash should be excluded
  - Temporary files generate excessive alerts

### `[alerts]` - Alert Delivery

```toml
[alerts]
journal = true
dbus = true
socket = true
```

Multiple alert delivery methods can be enabled simultaneously.

- **journal**: Log alerts to systemd journal
  - Always recommended - provides persistent audit trail
  - View with: `journalctl -u vigilant-canined`
  - Default: `true`

- **dbus**: Send desktop notifications via D-Bus
  - Requires desktop environment with notification support
  - Alerts appear as freedesktop notifications
  - Safe to enable on headless systems (silently ignored)
  - Default: `true`

- **socket**: Make alerts available via Unix domain socket
  - Required for API daemon and web dashboard
  - Default: `true`

### `[scan]` - Baseline Scanning

```toml
[scan]
schedule = "daily"
on_boot = true
```

- **schedule**: Frequency of full baseline scans
  - Options: `"daily"`, `"weekly"`, `"monthly"`
  - Cron expressions not yet supported (planned)
  - Default: `"daily"`

- **on_boot**: Perform full scan on daemon startup
  - Detects offline tampering
  - Increases boot time slightly
  - Default: `true`

## Example Configurations

### Minimal Configuration (All Defaults)

```toml
# Empty file uses all defaults
# Equivalent to:
[daemon]
log_level = "info"

[alerts]
journal = true
dbus = true
socket = true
```

### High-Security Workstation

```toml
[daemon]
log_level = "info"

[monitor.system]
paths = ["/etc", "/boot", "/usr/local", "/opt"]
exclude = ["/etc/machine-id", "/etc/resolv.conf"]

[monitor.flatpak]
enabled = true
system = true
user = true  # Monitor per-user apps

[monitor.home]
enabled = true
paths = ["~/.ssh", "~/.gnupg", "~/.config/systemd"]
exclude = ["~/.cache", "~/.local/share/Trash"]

[scan]
schedule = "daily"
on_boot = true
```

### Server/Headless System

```toml
[daemon]
log_level = "warn"

[monitor.system]
paths = ["/etc", "/boot", "/srv"]
exclude = []

[monitor.flatpak]
enabled = false  # Not used on servers

[monitor.home]
enabled = false  # No user directories on server

[alerts]
journal = true
dbus = false  # No desktop environment
socket = true

[scan]
schedule = "daily"
on_boot = true
```

## Validation

The daemon validates configuration on startup and will refuse to start if errors are detected:

```bash
# Test configuration without starting daemon
sudo vigilant-canined --config /etc/vigilant-canine/config.toml --validate
```

Common validation errors:

- Invalid TOML syntax (missing quotes, unclosed brackets)
- Nonexistent paths for `db_path`
- Invalid log level
- Invalid schedule value

## Reloading Configuration

Send SIGHUP to reload configuration without restarting:

```bash
sudo systemctl reload vigilant-canined
# or
sudo killall -HUP vigilant-canined
```

Changes to monitoring paths and alert settings take effect immediately. Changes to `db_path` require a full restart.
