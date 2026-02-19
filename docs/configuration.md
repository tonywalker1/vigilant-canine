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

### `[monitor.flatpak]` - Flatpak Monitoring (Planned)

**Status:** Configuration defined but not yet implemented. Will be added in a future release.

```toml
[monitor.flatpak]
enabled = false  # Not yet functional
system = true
user = false
```

Planned functionality:
- Monitor system-wide Flatpak installations for tampering
- Detect unauthorized modifications to Flatpak apps
- Track per-user Flatpak installations

### `[monitor.ostree]` - OSTree Monitoring (Planned)

**Status:** Configuration defined but not yet implemented. Will be added in a future release.

```toml
[monitor.ostree]
enabled = false  # Not yet functional
verify_deployments = true
monitor_object_store = true
```

Planned functionality (for OSTree-based distributions like Fedora Silverblue):
- Verify OSTree deployment checksums on boot
- Monitor the OSTree object store for integrity
- Detect unauthorized changes to bootable deployments

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

### `[retention]` - Database Retention

```toml
[retention]
enabled = true
interval_hours = 24
alert_days = 90
audit_event_days = 30
journal_event_days = 30
scan_days = 90
```

- **enabled**: Enable automatic database cleanup
  - Prunes old records to prevent unbounded database growth
  - Default: `true`

- **interval_hours**: Cleanup frequency in hours
  - How often the daemon runs retention cleanup
  - Cleanup also runs once at daemon startup
  - Default: `24` (daily)

- **alert_days**: Alert retention period in days
  - Alerts older than this are deleted
  - `0` means keep forever (not recommended)
  - Default: `90` (security incidents referenced longer for investigation)

- **audit_event_days**: Audit event retention period in days
  - Audit subsystem events older than this are deleted
  - High-volume forensic data
  - Default: `30` (standard log retention)

- **journal_event_days**: Journal event retention period in days
  - Journal log events older than this are deleted
  - High-volume log data
  - Default: `30` (matches audit events)

- **scan_days**: Scan history retention period in days
  - Scan records older than this are deleted
  - Audit trail of baseline verifications
  - Default: `90` (useful for debugging)

**Important Notes:**
- The `baselines` table is never pruned - it contains reference data
- The `schema_version` table is never pruned - it tracks database migrations
- Retention cleanup failures are logged as warnings but are non-fatal
- Database continues to work even if cleanup fails (it just grows larger)
- To disable retention entirely, set `enabled = false`

**Disk Space Warning:** Disabling retention or using very long retention periods can cause the database to grow large over time. Monitor disk usage if you customize these settings.

### `[journal]` - Journal Monitoring (Phase 2)

```toml
[journal]
enabled = true
max_priority = 6
exclude_units = []
exclude_identifiers = ["vigilant-canined"]
```

- **enabled**: Enable systemd journal monitoring
  - Monitors system logs for suspicious activity
  - Default: `true`

- **max_priority**: Maximum journal priority to process
  - 0 = emerg, 1 = alert, 2 = crit, 3 = err, 4 = warning, 5 = notice, 6 = info, 7 = debug
  - Lower numbers = higher priority (only urgent messages)
  - Higher numbers = more messages (including informational)
  - Default: `6` (info and above)

- **exclude_units**: systemd units to exclude from monitoring
  - Use for noisy services that generate many log entries
  - Example: `["NetworkManager.service", "systemd-resolved.service"]`
  - Default: `[]`

- **exclude_identifiers**: syslog identifiers to exclude
  - The daemon automatically excludes itself to prevent self-monitoring loops
  - Default: `["vigilant-canined"]`

**Built-in Rules:** The daemon includes 10 default rules covering:
- SSH authentication failures
- sudo authentication failures
- su authentication failures
- Login authentication failures
- Generic privilege escalation
- Service failures
- Service crashes
- Kernel errors
- Out-of-memory (OOM) events
- Suspicious systemd operations

Custom rules can be added via `[[journal.rules]]` sections (see example config for syntax).

### `[correlation]` - Event Correlation (Phase 2)

```toml
[correlation]
enabled = true
window_seconds = 300

[[correlation.rules]]
name = "repeated_auth_failure"
event_match = "auth_failure"
threshold = 5
window_seconds = 60
escalated_severity = "critical"
```

- **enabled**: Enable event correlation engine
  - Aggregates events over time windows to detect attack patterns
  - Default: `true`

- **window_seconds**: Default correlation window
  - How long to retain events for correlation
  - Events older than this are purged
  - Default: `300` (5 minutes)

**Correlation Rules:**

Each `[[correlation.rules]]` entry defines threshold-based alerting:

- **name**: Unique identifier for this rule
- **event_match**: Event category or rule name to match
  - Examples: `"auth_failure"`, `"ssh_auth_failure"`, `"file_modified"`
- **threshold**: Number of matching events to trigger escalation
- **window_seconds**: Time window for counting events (specific to this rule)
- **escalated_severity**: Severity when threshold is exceeded
  - Options: `"info"`, `"warning"`, `"critical"`

**Example:** Detect brute-force SSH attacks by escalating to critical when 5 auth failures occur within 60 seconds.

### `[audit]` - Linux Audit Subsystem Monitoring (Phase 3)

```toml
[audit]
enabled = true
sanitize_command_lines = true
exclude_comms = []
exclude_uids = []
```

Monitor security events from the Linux audit subsystem for deeper visibility into process execution, privilege changes, network connections, and file access attempts.

- **enabled**: Enable Linux audit subsystem integration
  - Requires libaudit and libauparse
  - Provides process execution tracking with command-line arguments
  - Tracks privilege escalation and network connections
  - Default: `true`

- **sanitize_command_lines**: Remove sensitive data from command lines before logging
  - Prevents credential exposure in logs (passwords, tokens, API keys)
  - Sanitizes common patterns: `--password=X`, `-p X`, etc.
  - Always recommended for security
  - Default: `true`

- **exclude_comms**: Command names to exclude from audit monitoring
  - Use for noisy system processes
  - Example: `["systemd-resolve", "dbus-daemon"]`
  - Default: `[]`

- **exclude_uids**: User IDs to exclude from audit monitoring
  - Use to ignore specific system users
  - Example: `[0]` to exclude root
  - Default: `[]`

**Built-in Audit Rules:** The daemon includes 10 default rules covering:
- Privilege escalation detection (setuid/setgid execution)
- Unauthorized file access attempts
- Suspicious network connections
- Kernel module loading
- Process execution patterns
- Failed access to sensitive files
- Credential file access attempts
- System call anomalies
- User context changes
- Network listener creation

**Event Types:**
- `ProcessExecution` - New process started with full command line
- `NetworkConnection` - Outbound network connection or listener
- `FailedAccess` - Denied file or resource access attempt
- `PrivilegeChange` - UID/GID change or capability modification

**Multi-record Correlation:** The audit monitor automatically correlates related audit records (SYSCALL + EXECVE + CWD + PATH) within 100ms windows to provide complete event context.

Custom rules can be added via `[[audit.rules]]` sections (see Custom Detection Rules below).

## Custom Detection Rules

Vigilant Canine supports user-defined detection rules for both journal log analysis and audit subsystem events. Custom rules are defined in the config file and merged with built-in defaults.

### Journal Rule Syntax

Journal rules match against systemd journal fields using various match types:

```toml
[[journal.rules]]
name = "docker_container_failed"           # Unique rule identifier
description = "Docker container failed"    # Human-readable description
enabled = true                             # Enable/disable this rule
action = "service_state"                   # Action type (see below)
severity = "warning"                       # Severity level (info/warning/critical)

[[journal.rules.match]]
field = "_SYSTEMD_UNIT"                    # Journal field to match
pattern = "docker"                         # Pattern to search for
type = "contains"                          # Match type (see below)
negate = false                             # Invert match (true = does NOT match)
```

**Match Types:**
- `exact` - Exact string match (case-sensitive)
- `contains` - Substring match
- `regex` - Regular expression match (uses std::regex syntax)
- `starts_with` - Prefix match

**Actions (determines event category):**
- `auth_failure` - Authentication/authorization failure
- `privilege_escalation` - Attempt to gain elevated privileges
- `service_state` - Service state change (start/stop/fail)
- `suspicious_log` - Generic suspicious log entry

**Severities:**
- `info` - Informational, low priority
- `warning` - Potential issue, deserves attention
- `critical` - Serious issue, immediate attention required

**Multiple Match Conditions:**

A rule can have multiple `[[journal.rules.match]]` sections - ALL conditions must match (AND logic):

```toml
[[journal.rules]]
name = "ssh_failed_root"
description = "Failed SSH login as root"
enabled = true
action = "auth_failure"
severity = "critical"

[[journal.rules.match]]
field = "SYSLOG_IDENTIFIER"
pattern = "sshd"
type = "exact"

[[journal.rules.match]]
field = "MESSAGE"
pattern = "Failed password for root"
type = "contains"
```

**Common Journal Fields:**
- `MESSAGE` - The main log message text
- `PRIORITY` - Numeric priority (0-7)
- `SYSLOG_IDENTIFIER` - Program name
- `_SYSTEMD_UNIT` - systemd unit name
- `_COMM` - Command name
- `_PID` - Process ID

See `man systemd.journal-fields` for complete field reference.

### Audit Rule Syntax

Audit rules match against Linux audit event fields:

```toml
[[audit.rules]]
name = "secrets_directory_access"          # Unique rule identifier
description = "Access to secrets dir"      # Human-readable description
enabled = true                             # Enable/disable this rule
action = "failed_access"                   # Action type (see below)
severity = "warning"                       # Severity level
syscall_filter = 0                         # Syscall number (0 = any)

[[audit.rules.match]]
field = "name"                             # Audit field to match
pattern = "/opt/secrets"                   # Pattern to search for
type = "starts_with"                       # Match type
negate = false                             # Invert match
```

**Match Types:**
- `exact` - Exact string match
- `contains` - Substring match
- `regex` - Regular expression match
- `starts_with` - Prefix match
- `numeric_eq` - Numeric equality (field == value)
- `numeric_gt` - Numeric greater-than (field > value)
- `numeric_lt` - Numeric less-than (field < value)

**Actions (determines event type):**
- `process_execution` - Process/program execution
- `network_connection` - Network socket connection
- `failed_access` - Access denied to file/resource
- `privilege_change` - UID/GID/capability change
- `suspicious_syscall` - Unusual system call pattern

**Common Audit Fields:**
- `name` - File path
- `comm` - Command name
- `exe` - Executable path
- `uid` - User ID (numeric)
- `auid` - Audit user ID (login UID)
- `success` - Success/failure flag (`yes`/`no`)
- `syscall` - System call number

**Syscall Filtering:**

Set `syscall_filter` to a specific syscall number to only match that syscall:

```toml
[[audit.rules]]
name = "execve_monitoring"
description = "Monitor all process executions"
syscall_filter = 59  # execve syscall on x86_64
# ...
```

Use `syscall_filter = 0` (default) to match any syscall.

### Rule Ordering and Precedence

- Custom rules are evaluated AFTER built-in default rules
- Rules are evaluated in the order they appear in the config file
- First matching rule determines the action and severity
- Use `enabled = false` to disable specific rules without removing them

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

[journal]
enabled = true

[correlation]
enabled = true

[audit]
enabled = true
sanitize_command_lines = true
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

[journal]
enabled = true
max_priority = 6  # Monitor all info-level and above

[correlation]
enabled = true

# Escalate repeated SSH failures quickly
[[correlation.rules]]
name = "ssh_brute_force"
event_match = "ssh_auth_failure"
threshold = 5
window_seconds = 60
escalated_severity = "critical"

[audit]
enabled = true
sanitize_command_lines = true
# Exclude noisy system processes
exclude_comms = ["systemd-resolve"]
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

[journal]
enabled = true
max_priority = 4  # Only warnings and errors (less noise)
exclude_units = ["cron.service"]  # Exclude noisy services

[correlation]
enabled = true

# Detect service failure patterns
[[correlation.rules]]
name = "service_failures"
event_match = "service_failure"
threshold = 3
window_seconds = 300
escalated_severity = "critical"

[audit]
enabled = true
sanitize_command_lines = true
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
