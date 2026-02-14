# Home Directory Monitoring Implementation Plan

## Context

The core daemon currently monitors only system paths (`/usr`, `/etc`, `/boot`, etc.) and requires all users to be equally trusted. Home directory monitoring was explicitly deferred from Phase 1 to avoid complexity, but is a natural extension that provides:

1. **User-installed software protection** - Monitor `~/.local/bin`, `~/.cargo/bin`, custom scripts
2. **User-level Flatpak monitoring** - `~/.local/share/flatpak/` applications
3. **Targeted monitoring** - Administrators can enforce monitoring for specific users/groups
4. **Opt-in for privacy** - Users explicitly enable monitoring via personal config

This feature completes the "simple IDS for average users" goal by extending protection to user-writable areas while respecting privacy through opt-in design.

## Architecture

### Configuration Hierarchy

Three configuration layers with clear precedence:

1. **System daemon config** (`/etc/vigilant-canine/config.toml`) - Global defaults
2. **Policy config** (`/etc/vigilant-canine/policy.toml`) - Administrator overrides
3. **User config** (`~/.config/vigilant-canine/config.toml`) - Individual preferences

**Precedence rules:**
- Policy always wins (admin can force monitoring on/off)
- User config is constrained by policy (can add paths, cannot remove mandated ones)
- System config provides defaults when user config absent

### User Discovery

**At daemon startup:**
1. Enumerate users with interactive shells (`/etc/passwd`, UID >= 1000, shell != nologin)
2. For each user, check if monitoring applies:
   - User config exists and enabled: `~/.config/vigilant-canine/config.toml`
   - OR policy forces monitoring: `monitor_users = ["alice"]` or user in `monitor_groups`
3. Build effective config by merging system + policy + user
4. Create per-user baseline entries with `source = "user:<username>"`

**Runtime monitoring:**
- Single FanotifyMonitor instance watches all paths (system + user)
- Baselines table uses `source` column to differentiate: `"system"`, `"user:alice"`, `"user:bob"`
- Alerts include username context when relevant

### Policy Enforcement

**Policy config controls:**
```toml
[policy.home]
# Force monitoring for specific users (cannot opt out)
monitor_users = ["admin", "developer"]

# Force monitoring for users in these groups
monitor_groups = ["sudo", "wheel"]

# Prevent users from opting out (respects monitor_users/groups)
allow_user_opt_out = true  # false = force monitoring for listed users

# Minimum paths to monitor (users can add more, not remove these)
mandatory_paths = [".ssh", ".gnupg"]
```

**Evaluation logic:**
```
if user in monitor_users OR user_primary_group in monitor_groups:
    monitoring_enabled = true
    user_can_disable = allow_user_opt_out
else:
    monitoring_enabled = user_config.enabled (default false)
```

### User Configuration Format

**File:** `~/.config/vigilant-canine/config.toml`

```toml
[monitor.home]
# Opt-in to home directory monitoring
enabled = true

# Paths to monitor (relative to $HOME)
paths = [
    ".local/bin",
    ".cargo/bin",
    ".local/share/flatpak",
    "bin",
    "scripts",
]

# Exclude patterns (relative to $HOME)
exclude = [
    ".cache",
    ".local/share/Trash",
    "Downloads",
    ".mozilla/firefox/*/cache2",
]

# Hash algorithm (default inherits from system config)
# hash_algorithm = "blake3"

# Notification preferences (per-user)
[notifications]
# Desktop notifications for my alerts
enabled = true

# Minimum severity (info, warning, critical, emergency)
min_severity = "warning"
```

### Database Schema Changes

**No schema changes needed!** Existing `baselines` table already supports this:

```sql
-- Existing schema supports user monitoring via source column
CREATE TABLE baselines (
    -- ... existing columns ...
    source TEXT NOT NULL,  -- "system", "user:alice", "user:bob"
    -- ... rest of columns ...
);
```

**Query patterns:**
```sql
-- All baselines for user alice
SELECT * FROM baselines WHERE source = 'user:alice';

-- All user baselines
SELECT * FROM baselines WHERE source LIKE 'user:%';

-- System baselines only
SELECT * FROM baselines WHERE source = 'system';
```

## Implementation Steps

### Step 1: Policy Configuration Parsing (1 day)

**Files to create:**
- None (extend existing config.h/cpp)

**Files to modify:**
1. `src/config/config.h` - Add `PolicyConfig` struct:
```cpp
struct HomeMonitoringPolicy {
    std::vector<std::string> monitor_users;
    std::vector<std::string> monitor_groups;
    bool allow_user_opt_out{true};
    std::vector<std::string> mandatory_paths;
};

struct PolicyConfig {
    HomeMonitoringPolicy home;
};

struct Config {
    // ... existing fields ...
    PolicyConfig policy;
};
```

2. `src/config/config.cpp` - Parse `[policy.home]` section from `/etc/vigilant-canine/policy.toml`

3. `config/vigilant-canine.toml.example` - Add policy examples

**Tests:**
- `tests/config/policy_config_test.cpp` (6 tests)
  - Parse monitor_users list
  - Parse monitor_groups list
  - Parse mandatory_paths
  - Default values when section missing
  - Invalid group names rejected
  - Invalid user names rejected

**Acceptance:** Policy config loads, defaults correct, validation works

### Step 2: User Discovery and Enumeration (2 days)

**Files to create:**
1. `src/user/user_manager.h` - User discovery interface
2. `src/user/user_manager.cpp` - Implementation

```cpp
namespace vigilant_canine {

struct UserInfo {
    std::string username;
    uid_t uid;
    gid_t gid;
    std::filesystem::path home_dir;
    std::string shell;
    std::vector<std::string> groups;  // All groups user belongs to
};

class UserManager {
public:
    // Enumerate users with interactive shells (UID >= 1000, valid shell)
    [[nodiscard]] auto discover_users() -> std::expected<std::vector<UserInfo>, std::string>;

    // Get groups for a specific user
    [[nodiscard]] auto get_user_groups(std::string const& username)
        -> std::expected<std::vector<std::string>, std::string>;

    // Check if user should be monitored based on policy
    [[nodiscard]] auto should_monitor_user(
        UserInfo const& user,
        PolicyConfig const& policy,
        bool user_config_exists,
        bool user_config_enabled
    ) -> bool;

    // Load user config if exists
    [[nodiscard]] auto load_user_config(UserInfo const& user)
        -> std::expected<std::optional<Config>, std::string>;
};

}  // namespace vigilant_canine
```

**Implementation details:**
- Parse `/etc/passwd` for UID >= 1000 (configurable threshold)
- Exclude system users (nologin, false shells)
- Use `getgrouplist()` to enumerate user's groups
- Handle missing home directories gracefully

**Tests:**
- `tests/user/user_manager_test.cpp` (8 tests)
  - Discover current user
  - Filter system users (UID < 1000)
  - Exclude nologin shells
  - Get user groups
  - Policy: monitor_users forces monitoring
  - Policy: monitor_groups forces monitoring
  - Policy: allow_user_opt_out respected
  - Load user config (mock filesystem)

**Acceptance:** User discovery works, group membership correct, policy evaluation accurate

### Step 3: User Config Loading and Merging (2 days)

**Files to modify:**
1. `src/config/config.cpp` - Add user config loading

**New functions:**
```cpp
// Load user-specific config from ~/.config/vigilant-canine/config.toml
auto load_user_config(std::filesystem::path const& config_path)
    -> std::expected<Config, std::string>;

// Merge system + policy + user configs with precedence rules
auto merge_configs(
    Config const& system_config,
    PolicyConfig const& policy,
    std::optional<Config> const& user_config,
    UserInfo const& user
) -> Config;
```

**Merging rules:**
1. Start with system config defaults
2. If policy forces monitoring, override user preferences
3. Merge user paths (add to system paths, don't replace)
4. Apply mandatory_paths from policy (cannot be excluded)
5. Resolve relative paths to absolute (prepend home_dir)

**Tests:**
- `tests/config/config_merge_test.cpp` (10 tests)
  - User disabled, policy forces: monitoring enabled
  - User enabled, policy allows: monitoring enabled
  - User adds paths: paths merged
  - User excludes mandatory path: path still monitored
  - Relative paths converted to absolute
  - User config missing: use defaults
  - Empty user config: use system defaults
  - Policy with no monitoring: user can opt in
  - Policy mandatory_paths enforced
  - Conflicting hash_algorithm: policy wins

**Acceptance:** Config merging follows precedence, paths resolved correctly, policy enforced

### Step 4: Per-User Baseline Creation (2 days)

**Files to modify:**
1. `src/scanner/scanner.h` - Add username parameter
2. `src/scanner/scanner.cpp` - Support `source = "user:<username>"`

**Modified interface:**
```cpp
class Scanner {
public:
    // Existing: scan system paths
    [[nodiscard]] auto scan_paths(
        std::vector<std::filesystem::path> const& paths,
        std::string const& source  // "system"
    ) -> std::expected<ScanResult, std::string>;

    // New: scan user paths with username context
    [[nodiscard]] auto scan_user_paths(
        UserInfo const& user,
        std::vector<std::filesystem::path> const& paths,
        std::vector<std::string> const& exclude_patterns
    ) -> std::expected<ScanResult, std::string>;
};
```

**Implementation:**
- Use `source = "user:<username>"` in baseline entries
- Store absolute paths (not relative)
- Handle missing directories gracefully (user may not have ~/.local/bin)
- Skip excluded patterns (fnmatch on relative path)

**Tests:**
- `tests/scanner/user_scanner_test.cpp` (7 tests)
  - Scan user directory creates baselines
  - Source column set to "user:testuser"
  - Relative paths converted to absolute
  - Excluded patterns respected
  - Missing directories don't fail scan
  - User baseline separate from system baseline
  - Change detection works for user files

**Acceptance:** User baselines created with correct source, exclusions work, isolation correct

### Step 5: Daemon Integration (2 days)

**Files to modify:**
1. `src/daemon/daemon.h` - Add user monitoring components
2. `src/daemon/daemon.cpp` - Initialize user monitoring

**Changes to Daemon class:**
```cpp
class Daemon {
private:
    // New members
    std::unique_ptr<UserManager> m_user_manager;
    std::vector<UserInfo> m_monitored_users;

    // Modified initialization
    auto initialize_user_monitoring() -> std::expected<void, std::string>;
    auto scan_user_baselines() -> std::expected<void, std::string>;
};
```

**Initialization sequence:**
```
1. Load system config (existing)
2. Load policy config (new)
3. Discover users (new)
4. For each user:
   a. Check if monitoring applies (policy + user config)
   b. Load user config if exists
   c. Merge configs (system + policy + user)
   d. Add user paths to FanotifyMonitor
   e. Scan user baseline if needed
5. Start monitoring (existing)
```

**Tests:**
- `tests/daemon/user_monitoring_test.cpp` (6 tests)
  - User with config: monitoring enabled
  - User without config: monitoring disabled (unless policy)
  - Policy forces monitoring: works without user config
  - Multiple users monitored simultaneously
  - User paths added to fanotify
  - User baselines created on startup

**Acceptance:** Daemon discovers users, applies policy, monitors user paths, creates baselines

### Step 6: Alert Context Enhancement (1 day)

**Files to modify:**
1. `src/dispatch/alert_dispatcher.cpp` - Add username to alerts

**Enhancement:**
When file modification detected in user home directory, extract username from baseline source:

```cpp
// In alert creation:
if (baseline.source.starts_with("user:")) {
    auto username = baseline.source.substr(5);  // Extract "alice" from "user:alice"
    alert.details["username"] = username;
    alert.summary = std::format("User {} file modified: {}", username, path);
}
```

**Tests:**
- Extend `tests/dispatch/alert_dispatcher_test.cpp` (3 new tests)
  - User file modified: username in alert
  - System file modified: no username
  - User file created: username in alert

**Acceptance:** Alerts for user files include username context

### Step 7: Documentation and Examples (1 day)

**Files to create:**
1. `docs/user-monitoring.md` - Complete user guide

**Files to modify:**
1. `README.md` - Add user monitoring section
2. `docs/configuration.md` - Document policy and user config
3. `config/vigilant-canine.toml.example` - Add policy section
4. Create example user config: `config/user-config.toml.example`

**Documentation sections:**
- How to enable user monitoring (opt-in instructions)
- Policy enforcement examples (force monitoring for admins)
- Privacy considerations (what's monitored, what's excluded)
- Troubleshooting (permission errors, missing config)

**Acceptance:** Documentation complete, examples work, privacy explained

## Configuration Examples

### System Policy (Admin Enforced)

**File:** `/etc/vigilant-canine/policy.toml`

```toml
[policy.home]
# Force monitoring for administrators
monitor_users = ["admin", "root"]

# Force monitoring for privileged groups
monitor_groups = ["sudo", "wheel"]

# These users can disable monitoring if they want
allow_user_opt_out = true

# Always monitor SSH and GPG directories (cannot be excluded)
mandatory_paths = [".ssh", ".gnupg"]
```

### User Opt-In Configuration

**File:** `~/.config/vigilant-canine/config.toml`

```toml
[monitor.home]
# I want monitoring for my custom scripts
enabled = true

# Watch these directories
paths = [
    ".local/bin",        # User-installed binaries
    ".cargo/bin",        # Rust binaries
    "bin",               # Custom scripts
    ".local/share/flatpak",  # User Flatpak apps
]

# Don't monitor these (privacy)
exclude = [
    ".cache",
    ".local/share/Trash",
    "Downloads",
    ".mozilla/firefox/*/cache2",
    ".config/chromium/*/Cache",
]

[notifications]
# Show me warnings and above
enabled = true
min_severity = "warning"
```

## Critical Files

**New files:**
1. `src/user/user_manager.h` - User discovery interface
2. `src/user/user_manager.cpp` - Implementation
3. `tests/user/user_manager_test.cpp` - Unit tests
4. `tests/config/policy_config_test.cpp` - Policy parsing tests
5. `tests/config/config_merge_test.cpp` - Config merging tests
6. `tests/scanner/user_scanner_test.cpp` - User scanning tests
7. `tests/daemon/user_monitoring_test.cpp` - Integration tests
8. `docs/user-monitoring.md` - User guide
9. `config/user-config.toml.example` - Example user config
10. `config/policy.toml.example` - Example policy config

**Modified files:**
1. `src/config/config.h` - Add PolicyConfig, HomeMonitoringPolicy
2. `src/config/config.cpp` - Parse policy, load user configs, merge
3. `src/scanner/scanner.h` - Add scan_user_paths
4. `src/scanner/scanner.cpp` - Support user source
5. `src/daemon/daemon.h` - Add user monitoring members
6. `src/daemon/daemon.cpp` - Initialize user monitoring
7. `src/dispatch/alert_dispatcher.cpp` - Add username to alerts
8. `CMakeLists.txt` - Add user_manager.cpp to build
9. `tests/CMakeLists.txt` - Add user/ subdirectory
10. `README.md` - Document user monitoring
11. `docs/configuration.md` - Policy and user config docs
12. `config/vigilant-canine.toml.example` - Add policy section

## Security Considerations

### Privacy

**Opt-in by default:**
- No user monitoring without explicit config or policy
- Users control what's monitored (within policy constraints)
- Excluded patterns prevent monitoring sensitive areas

**Transparency:**
- Clear documentation of what's monitored
- Alerts show username context
- Users can query their baselines via API

### Privilege Separation

**Read-only access:**
- Daemon reads user configs as root (CAP_DAC_READ_SEARCH)
- No writing to user home directories
- Config files validated before use

**Policy enforcement:**
- Policy.toml writable only by root
- Users cannot override policy restrictions
- Mandatory paths cannot be excluded

### Resource Limits

**Per-user limits:**
- Maximum 100 paths per user (prevent DoS)
- Maximum 10MB baseline per user
- Skip users with inaccessible home directories

**System limits:**
- Maximum 50 monitored users (configurable)
- Total fanotify watches tracked
- Graceful degradation if limits exceeded

## Testing Strategy

### Unit Tests (~30 new tests)

1. **PolicyConfig parsing** (6 tests) - Load, validate, defaults
2. **UserManager** (8 tests) - Discovery, groups, policy evaluation
3. **Config merging** (10 tests) - Precedence, path resolution, policy enforcement
4. **User scanner** (7 tests) - Baselines, exclusions, source column

### Integration Tests (~10 tests)

1. **Daemon user monitoring** (6 tests) - Full initialization flow
2. **Alert dispatcher** (3 tests) - Username in alerts
3. **End-to-end** (1 test) - Create user config, scan, detect change

### Manual Testing

1. Create test user: `sudo useradd -m testuser`
2. Create user config as testuser
3. Restart daemon, verify user paths monitored
4. Modify file in `~testuser/.local/bin`
5. Verify alert includes username
6. Test policy enforcement (force monitoring)
7. Test opt-out behavior

**Target test count:** ~40 new tests, all passing

## Verification Steps

### 1. Build and Test
```bash
# Clean build
mkdir -p build/gcc-debug
cd build/gcc-debug
cmake -DCMAKE_BUILD_TYPE=Debug ../..
cmake --build . -j$(nproc)

# Run all tests
ctest --output-on-failure

# Verify new tests
ctest -R user -V
ctest -R policy -V
ctest -R merge -V
```

### 2. Create Test User
```bash
# Create user with home directory
sudo useradd -m -s /bin/bash testuser
sudo passwd testuser

# Create test directories
sudo -u testuser mkdir -p /home/testuser/.local/bin
sudo -u testuser mkdir -p /home/testuser/.cargo/bin

# Create test script
sudo -u testuser tee /home/testuser/.local/bin/test.sh <<'EOF'
#!/bin/bash
echo "Test script"
EOF
sudo -u testuser chmod +x /home/testuser/.local/bin/test.sh
```

### 3. Configure User Monitoring
```bash
# Create user config directory
sudo -u testuser mkdir -p /home/testuser/.config/vigilant-canine

# Create user config
sudo -u testuser tee /home/testuser/.config/vigilant-canine/config.toml <<'EOF'
[monitor.home]
enabled = true
paths = [".local/bin", ".cargo/bin"]
exclude = [".cache"]

[notifications]
enabled = true
min_severity = "warning"
EOF
```

### 4. Test Policy Enforcement
```bash
# Create policy config
sudo tee -a /etc/vigilant-canine/config.toml <<'EOF'

# Policy enforcement
[policy.home]
monitor_users = ["testuser"]
allow_user_opt_out = false
mandatory_paths = [".ssh"]
EOF
```

### 5. Restart Daemon and Verify
```bash
# Restart daemon
sudo systemctl restart vigilant-canined.service

# Check logs for user discovery
sudo journalctl -u vigilant-canined -n 50 | grep -i user

# Should see: "Discovered N users for monitoring"
# Should see: "User testuser: monitoring enabled"

# Verify baselines created
sudo sqlite3 /var/lib/vigilant-canine/vc.db \
  "SELECT COUNT(*) FROM baselines WHERE source='user:testuser';"
# Should show > 0

# List user baselines
sudo sqlite3 /var/lib/vigilant-canine/vc.db \
  "SELECT path FROM baselines WHERE source='user:testuser' LIMIT 5;"
```

### 6. Test Change Detection
```bash
# Modify test script
sudo -u testuser tee /home/testuser/.local/bin/test.sh <<'EOF'
#!/bin/bash
echo "Modified test script"
EOF

# Wait a few seconds
sleep 5

# Check for alert
sudo journalctl -u vigilant-canined -n 20 | grep -i modified

# Check database
sudo sqlite3 /var/lib/vigilant-canine/vc.db \
  "SELECT summary FROM alerts WHERE path LIKE '%testuser%' ORDER BY created_at DESC LIMIT 1;"
# Should show alert with username context
```

### 7. Test Exclusions
```bash
# Modify excluded directory (should NOT alert)
sudo -u testuser mkdir -p /home/testuser/.cache
sudo -u testuser touch /home/testuser/.cache/test.txt

# Modify monitored directory (should alert)
sudo -u testuser touch /home/testuser/.local/bin/new-script.sh

# Verify only monitored path alerted
sudo journalctl -u vigilant-canined -n 20
```

### 8. Test Multi-User
```bash
# Create second user
sudo useradd -m -s /bin/bash testuser2
sudo -u testuser2 mkdir -p /home/testuser2/.config/vigilant-canine
sudo -u testuser2 tee /home/testuser2/.config/vigilant-canine/config.toml <<'EOF'
[monitor.home]
enabled = true
paths = [".local/bin"]
EOF

# Restart daemon
sudo systemctl restart vigilant-canined.service

# Verify both users monitored
sudo sqlite3 /var/lib/vigilant-canine/vc.db \
  "SELECT DISTINCT source FROM baselines WHERE source LIKE 'user:%';"
# Should show: user:testuser, user:testuser2
```

### 9. Test API Access
```bash
# Query user baselines via API
curl --unix-socket /run/vigilant-canine/api.sock \
  'http://localhost/api/v1/baselines?source=user:testuser&limit=5' | jq

# Query user alerts
curl --unix-socket /run/vigilant-canine/api.sock \
  'http://localhost/api/v1/alerts?limit=10' | jq '.alerts[] | select(.details.username == "testuser")'
```

## Success Criteria

- [ ] All 40 new tests pass
- [ ] User discovery works (enumerates users with UID >= 1000)
- [ ] Policy config loads and enforces correctly
- [ ] User configs load and merge with precedence
- [ ] User baselines created with `source = "user:<username>"`
- [ ] File changes in user directories trigger alerts
- [ ] Alerts include username context
- [ ] Exclusion patterns work
- [ ] Multiple users monitored simultaneously
- [ ] Policy enforcement prevents opt-out when configured
- [ ] Mandatory paths cannot be excluded
- [ ] API returns user baselines correctly
- [ ] Documentation complete and accurate
- [ ] No performance regression (daemon startup < 10s with 10 users)

## Estimated Timeline

- **Step 1:** Policy config parsing - 1 day
- **Step 2:** User discovery - 2 days
- **Step 3:** Config merging - 2 days
- **Step 4:** Baseline creation - 2 days
- **Step 5:** Daemon integration - 2 days
- **Step 6:** Alert enhancement - 1 day
- **Step 7:** Documentation - 1 day

**Total: ~11 days** (2 weeks with testing/refinement)

## Future Enhancements (Not in Scope)

- Real-time user login/logout detection (inotify on /etc/passwd)
- Per-user notification daemon (not needed, DbusNotifier works)
- User-specific scan schedules
- User quarantine/remediation actions
- Integration with pam_exec for monitoring enforcement
