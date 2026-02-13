# Troubleshooting Guide

Common issues and solutions for Vigilant Canine.

## Build Issues

### "Could not find BLAKE3"

**Symptom:**
```
CMake Error: Could not find BLAKE3 (missing: BLAKE3_LIBRARIES BLAKE3_INCLUDE_DIRS)
```

**Solution:**

Install the BLAKE3 development package:

```bash
# Fedora/RHEL
sudo dnf install blake3-devel

# Debian/Ubuntu (if available)
sudo apt install libblake3-dev
```

If BLAKE3 isn't available in your distribution's repositories, you may need to build it from source or wait for Phase 2 which will vendor the dependency.

### "C++23 features not available"

**Symptom:**
```
error: 'expected' is not a member of 'std'
```

**Solution:**

The project requires C++23 support. Update your compiler:

- GCC 15 or later
- Clang 18 or later

```bash
# Fedora
sudo dnf install gcc-c++

# Check version
g++ --version  # Should be 15.x or higher
```

### Build artifacts in source tree

**Symptom:**

CMakeFiles/, Makefiles, or *.o files appear in src/, tests/, or project root.

**Solution:**

Always build in the designated build directory structure:

```bash
# Clean up first
git clean -fdx  # WARNING: removes all untracked files

# Build correctly
mkdir -p build/gcc-debug
cd build/gcc-debug
cmake -DCMAKE_BUILD_TYPE=Debug ../..
cmake --build . -j$(nproc)
```

**Never run `cmake .` from the project root.**

## Runtime Issues

### "Permission denied" when starting daemon

**Symptom:**
```
Failed to initialize fanotify: Permission denied
```

**Solution:**

The daemon requires elevated privileges to monitor file access events:

```bash
# Run with sudo
sudo ./build/gcc-debug/vigilant-canined --config /etc/vigilant-canine/config.toml
```

For production use, run via systemd which handles privileges correctly.

### "Failed to open database"

**Symptom:**
```
Failed to open database: unable to open database file
```

**Solution:**

Ensure the database directory exists and is writable:

```bash
# Check configured path
grep db_path /etc/vigilant-canine/config.toml

# Create directory
sudo mkdir -p /var/lib/vigilant-canine
sudo chmod 750 /var/lib/vigilant-canine
```

### No desktop notifications appearing

**Symptom:**

Daemon runs but no notifications show up when files change.

**Diagnosis:**

Check if D-Bus alerts are enabled:

```bash
grep dbus /etc/vigilant-canine/config.toml
# Should show: dbus = true
```

Check systemd journal for alert deliveries:

```bash
sudo journalctl -u vigilant-canined -f
# Trigger test alert by modifying /etc/hosts
echo "# test" | sudo tee -a /etc/hosts
# Watch for alert messages
```

**Solutions:**

1. Ensure you're running a desktop environment with notification support
2. Check that D-Bus notifications are enabled in config
3. Verify the notification daemon is running:
   ```bash
   ps aux | grep notification
   ```

### "Package manager not detected"

**Symptom:**
```
Warning: Could not detect package manager, baseline may be incomplete
```

**Solution:**

The daemon supports rpm and dpkg. Ensure one is installed:

```bash
# Check for rpm
rpm --version

# Check for dpkg
dpkg --version
```

If using a non-mainstream distribution, you may need to wait for additional package manager support or manually specify files to monitor in the configuration.

## Performance Issues

### High CPU usage during scan

**Symptom:**

Daemon consumes significant CPU during baseline scanning.

**Explanation:**

Initial baseline scans hash all monitored files (potentially hundreds of thousands). This is expected and only happens:
- On first run
- On boot (if `scan.on_boot = true`)
- On scheduled intervals (default: daily)

**Mitigation:**

```toml
[scan]
schedule = "weekly"  # Reduce scan frequency
on_boot = false      # Skip boot-time scan
```

Reduce monitored paths:

```toml
[monitor.system]
paths = ["/etc", "/boot"]  # Monitor only critical paths
```

### Excessive alert spam

**Symptom:**

Constant notifications for the same files.

**Diagnosis:**

Check journal logs to identify which files are triggering alerts:

```bash
sudo journalctl -u vigilant-canined -n 100 | grep "File modified"
```

**Solution:**

Add frequently-changing files to the exclude list:

```toml
[monitor.system]
exclude = [
    "/etc/resolv.conf",    # Changes with network
    "/etc/machine-id",
    "/etc/hostname",
    "/var/log/*",          # Log files change constantly
]
```

## Logging and Debugging

### Enable debug logging

Increase verbosity to diagnose issues:

```toml
[daemon]
log_level = "debug"  # or "trace" for maximum verbosity
```

Restart daemon and check logs:

```bash
sudo systemctl restart vigilant-canined
sudo journalctl -u vigilant-canined -f
```

### View alerts history

Check systemd journal for all alerts:

```bash
# All alerts
sudo journalctl -u vigilant-canined | grep "ALERT"

# Alerts from today
sudo journalctl -u vigilant-canined --since today | grep "ALERT"

# Follow live
sudo journalctl -u vigilant-canined -f
```

### Test configuration validity

Validate config without starting daemon:

```bash
# Assuming --validate flag exists (check --help)
sudo vigilant-canined --config /etc/vigilant-canine/config.toml --dry-run
```

## Getting Help

If you've tried the solutions above and still have issues:

1. **Check existing issues**: [GitHub Issues](https://github.com/tonywalker1/vigilant-canine/issues)
2. **Gather diagnostic info**:
   ```bash
   # System info
   uname -a

   # Compiler version
   g++ --version

   # Daemon version
   vigilant-canined --version

   # Config (sanitize sensitive paths)
   cat /etc/vigilant-canine/config.toml

   # Recent logs
   sudo journalctl -u vigilant-canined -n 100 --no-pager
   ```
3. **Open an issue** with the diagnostic info

## Common False Positives

Some file modifications are normal and expected:

### Package Manager Updates

**Symptom:**

Alerts when updating packages via dnf/apt.

**Explanation:**

This is working as intended - the daemon detects package changes and re-verifies against package manager metadata.

**Action:**

No action needed. The daemon will automatically update baselines for package-managed files.

### System Updates

**Symptom:**

Many alerts after kernel or systemd updates.

**Explanation:**

System updates modify critical files, which triggers alerts. The daemon then verifies the changes against the package manager.

**Action:**

Review the alerts to ensure they correspond to the updates you applied. Baselines update automatically.

### Configuration File Edits

**Symptom:**

Alert when editing /etc/fstab or other config files.

**Explanation:**

Intended behavior - any modification to monitored files triggers an alert.

**Action:**

This is expected. The alerts inform you of your own changes, providing an audit trail.
