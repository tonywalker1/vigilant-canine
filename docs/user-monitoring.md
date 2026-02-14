# User Home Directory Monitoring

This document describes how to configure and use home directory monitoring in Vigilant Canine.

## Overview

Vigilant Canine can monitor user home directories in addition to system paths. This feature:

- **Protects user-installed software**: Monitor `~/.local/bin`, `~/.cargo/bin`, custom scripts
- **Monitors user Flatpak applications**: Track changes to `~/.local/share/flatpak/`
- **Respects privacy**: Opt-in by default, with configurable exclusions
- **Supports policy enforcement**: Administrators can require monitoring for specific users/groups

## Privacy Considerations

**Home directory monitoring is OPT-IN by default.** The daemon will NOT monitor any user's home
directory unless:

1. The user explicitly enables monitoring via `~/.config/vigilant-canine/config.toml`, OR
2. An administrator has configured a policy that requires monitoring for that user/group

Users can exclude sensitive directories (Downloads, caches, etc.) from monitoring.

## User Configuration

### Enabling Monitoring

To enable home directory monitoring for your user account:

1. Create the configuration directory:
   ```bash
   mkdir -p ~/.config/vigilant-canine
   ```

2. Create the configuration file:
   ```bash
   cp /usr/share/doc/vigilant-canine/user-config.toml.example \
      ~/.config/vigilant-canine/config.toml
   ```

3. Edit the configuration file and set `enabled = true` in the `[monitor.home]` section.

4. Customize the paths to monitor and exclude as needed.

5. Restart the daemon (or wait for it to detect the new configuration):
   ```bash
   sudo systemctl restart vigilant-canined.service
   ```

### Example User Configuration

```toml
[monitor.home]
# Enable home directory monitoring
enabled = true

# Paths to monitor (relative to $HOME)
paths = [
    ".local/bin",               # User-installed binaries
    ".cargo/bin",               # Rust toolchain binaries
    ".local/share/flatpak",     # User Flatpak applications
    "bin",                      # Custom scripts
    ".ssh",                     # SSH keys and config
    ".gnupg",                   # GPG keys
]

# Exclude sensitive/frequently-changing directories
exclude = [
    ".cache",
    ".local/share/Trash",
    "Downloads",
    ".mozilla/firefox/*/cache2",
]

[alerts]
# Receive desktop notifications for home directory changes
dbus = true
```

## Administrator Policy Configuration

Administrators can enforce home directory monitoring for specific users or groups via the
system configuration file.

### Policy Configuration

Add to `/etc/vigilant-canine/config.toml`:

```toml
[policy.home]
# Force monitoring for these users
monitor_users = ["admin", "developer"]

# Force monitoring for users in these groups
monitor_groups = ["sudo", "wheel"]

# Allow users to opt out (if true, users can disable via their config)
allow_user_opt_out = false

# Paths that MUST be monitored (users cannot exclude these)
mandatory_paths = [".ssh", ".gnupg"]
```

### Policy Evaluation Logic

The daemon evaluates monitoring requirements as follows:

1. **If the user is in `monitor_users` OR their group is in `monitor_groups`**:
   - **AND** `allow_user_opt_out = false`: Monitoring is **mandatory**
   - **AND** `allow_user_opt_out = true`: User can opt out via their config

2. **Otherwise**: User must opt-in via their config

### Policy Examples

#### Example 1: Mandatory monitoring for administrators

```toml
[policy.home]
monitor_groups = ["sudo", "wheel"]
allow_user_opt_out = false
mandatory_paths = [".ssh", ".gnupg"]
```

Result: All users in sudo/wheel groups **must** have .ssh and .gnupg monitored.
They cannot disable monitoring.

#### Example 2: Recommended monitoring with opt-out

```toml
[policy.home]
monitor_groups = ["developers"]
allow_user_opt_out = true
mandatory_paths = [".local/bin"]
```

Result: Developers are encouraged to enable monitoring (daemon will monitor by default),
but they can opt out. If they keep monitoring enabled, .local/bin is required.

#### Example 3: Mandatory paths only

```toml
[policy.home]
monitor_users = []
monitor_groups = []
mandatory_paths = [".ssh"]
```

Result: No forced monitoring, but users who do opt-in MUST include .ssh in their
monitored paths.

## How It Works

### User Discovery

On startup, the daemon:

1. Enumerates all users with UID >= 1000 and interactive shells
2. For each user, checks if monitoring applies (policy or user config)
3. Loads user configurations from `~/.config/vigilant-canine/config.toml`
4. Merges system, policy, and user configurations
5. Creates baselines for user files

### Baseline Storage

User file baselines are stored in the database with a `source` field:

- System files: `source = "system"`, `source = "rpm:package-name"`, etc.
- User files: `source = "user:username"` (e.g., `source = "user:alice"`)

This allows querying baselines per-user:

```sql
-- All baselines for user alice
SELECT * FROM baselines WHERE source = 'user:alice';

-- All user baselines
SELECT * FROM baselines WHERE source LIKE 'user:%';
```

### Alert Context

When a file in a user's home directory is modified, the alert includes the username:

```
Summary: User alice file modified: /home/alice/.local/bin/my-script.sh
Details: Old hash: abc123...
         New hash: def456...
         File content changed
         Username: alice
```

## Querying User Baselines

Via the API daemon:

```bash
# Get baselines for a specific user
curl --unix-socket /run/vigilant-canine/api.sock \
  'http://localhost/api/v1/baselines?source=user:alice&limit=10' | jq

# Get alerts for a specific user
curl --unix-socket /run/vigilant-canine/api.sock \
  'http://localhost/api/v1/alerts?limit=20' | \
  jq '.alerts[] | select(.details | contains("Username: alice"))'
```

## Troubleshooting

### Monitoring not working

1. **Check user configuration exists**:
   ```bash
   ls -la ~/.config/vigilant-canine/config.toml
   ```

2. **Verify enabled = true**:
   ```bash
   grep "enabled" ~/.config/vigilant-canine/config.toml
   ```

3. **Check daemon logs**:
   ```bash
   sudo journalctl -u vigilant-canined -n 50 | grep -i user
   ```

   Look for messages like:
   ```
   Discovered N users
   User alice: monitoring enabled (X paths)
   ```

4. **Verify baselines were created**:
   ```bash
   sudo sqlite3 /var/lib/vigilant-canine/vc.db \
     "SELECT COUNT(*) FROM baselines WHERE source='user:$(whoami)';"
   ```

### Permission errors

The daemon runs as root (or with CAP_DAC_READ_SEARCH), so it can read user home directories.
However:

- User config files must be readable by root
- Paths must exist (missing paths are skipped gracefully)
- Excluded patterns are respected

### Policy not enforced

1. **Check policy configuration**:
   ```bash
   sudo grep -A 10 "^\[policy.home\]" /etc/vigilant-canine/config.toml
   ```

2. **Verify user is in monitored groups**:
   ```bash
   groups $(whoami)
   ```

3. **Restart daemon after policy changes**:
   ```bash
   sudo systemctl restart vigilant-canined.service
   ```

## Security Considerations

### Read-Only Access

The daemon only **reads** user home directories. It does NOT:

- Write to user files
- Execute user programs
- Modify permissions

### Configuration Validation

User configuration files are validated before use:

- Invalid TOML syntax is rejected
- Invalid paths are skipped
- Mandatory paths from policy cannot be excluded

### Resource Limits

To prevent abuse:

- Maximum 100 monitored paths per user
- Maximum 50 monitored users (configurable)
- Excluded patterns prevent monitoring of caches, temporary files

## Related Documentation

- [Configuration Reference](configuration.md)
- [Architecture Overview](architecture.md)
- [API Documentation](api.md)
