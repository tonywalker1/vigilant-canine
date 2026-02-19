# COW-Friendly Database Handling Plan

## Current State

### Implementation Location
All nocow handling is currently in `src/storage/database.cpp`:
- `is_btrfs()` - Detects if filesystem is Btrfs (lines 31-37)
- `set_nocow_attribute()` - Sets nocow flag via ioctl (lines 42-59)
- `ensure_database_directory()` - Creates dir and sets nocow **only on Btrfs** (lines 63-86)

### Current Behavior
```cpp
if (is_btrfs(dir)) {
    set_nocow_attribute(dir);
    // Not a fatal error if this fails - log but continue
}
```

**Problem:** Only sets nocow on Btrfs. Other COW filesystems (ZFS, bcachefs, future systems) are not handled.

## Proposed Changes

### Philosophy
**Set nocow by default on all filesystems, let the kernel handle compatibility.**

Rationale:
1. The ioctl silently fails on non-COW filesystems (harmless)
2. Detecting all COW filesystems is difficult and fragile
3. Setting nocow is always beneficial for SQLite databases on COW filesystems
4. Simpler code, fewer branches, more robust

### Changes Required

#### 1. Remove Btrfs Detection (Simplify)

**File:** `src/storage/database.cpp`

**Before:**
```cpp
// Set NOCOW on Btrfs (for SQLite performance)
if (is_btrfs(dir)) {
    set_nocow_attribute(dir);
    // Not a fatal error if this fails - log but continue
}
```

**After:**
```cpp
// Set NOCOW for SQLite performance (works on all COW filesystems)
// Silently ignored on non-COW filesystems (harmless)
set_nocow_attribute(dir);
```

**Remove entirely:**
- `is_btrfs()` function (lines 31-37)
- `<linux/magic.h>` include (only used for BTRFS_SUPER_MAGIC)
- `<sys/vfs.h>` include (only used for statfs in is_btrfs)

#### 2. Update Comments and Documentation

**File:** `src/storage/database.cpp`

Change function comment from:
```cpp
// Set NOCOW attribute on a file or directory (Btrfs-specific).
```

To:
```cpp
// Set NOCOW attribute on a file or directory.
//
// This optimizes SQLite performance on copy-on-write filesystems
// (Btrfs, ZFS, bcachefs, etc.) by disabling COW for database files.
//
// The ioctl is silently ignored on non-COW filesystems (harmless).
// Failure to set nocow is not fatal - databases work fine with COW,
// just with some performance overhead.
```

**File:** `src/storage/database.h`

Update comment in `ensure_database_directory`:
```cpp
// Ensure database directory exists and has NOCOW attribute.
//
// Postconditions:
//   - Directory exists (created if needed)
//   - Directory has NOCOW attribute set (on COW filesystems)
//   - On error: returns error message
```

#### 3. Add Nocow to Database File Itself

**Current:** Only sets nocow on the directory.

**Problem:** On Btrfs, nocow is only inherited by *new* files. Existing database files don't get the attribute.

**Solution:** Also set nocow directly on the database file after opening.

**Location:** `Database::open()` after `sqlite3_open()`

```cpp
auto Database::open(std::filesystem::path const& db_path)
    -> std::expected<Database, std::string> {

    // Ensure directory exists and has NOCOW
    if (auto result = ensure_database_directory(db_path); !result) {
        return std::unexpected(result.error());
    }

    // Open database
    sqlite3* db = nullptr;
    int rc = sqlite3_open(db_path.c_str(), &db);
    if (rc != SQLITE_OK) {
        std::string error = sqlite3_errmsg(db);
        sqlite3_close(db);
        return std::unexpected(std::format("Failed to open database: {}", error));
    }

    // Set NOCOW on the database file itself (for existing files)
    // Directory nocow only affects new files, so we must also set it explicitly
    set_nocow_attribute(db_path);

    Database database{db};

    // ... rest of function
}
```

#### 4. Update tmpfiles.d Configuration

**File:** `systemd/vigilant-canine.conf`

**Before:**
```
# State directory (database, baselines)
d /var/lib/vigilant-canine 0750 root root -
```

**After:**
```
# State directory (database, baselines)
# h+C = NOCOW attribute for COW filesystems (Btrfs, ZFS, etc.)
d /var/lib/vigilant-canine 0750 root root - h+C
```

**Note:** The `h+C` attribute is ignored on non-COW filesystems (harmless).

#### 5. Update Documentation

**File:** `docs/architecture.md`

Add section under "Database Retention" or create new "Database Storage":

```markdown
## Database Storage Optimization

### Copy-on-Write (COW) Handling

SQLite databases perform poorly on copy-on-write filesystems due to excessive
fragmentation from small random writes. Vigilant Canine automatically sets the
NOCOW attribute on database directories and files.

**Supported filesystems:**
- Btrfs - via `FS_NOCOW_FL` flag
- ZFS - (TODO: verify ZFS behavior with this ioctl)
- bcachefs - (TODO: verify bcachefs behavior)
- Other COW filesystems - may work, may be silently ignored

**Implementation:**
- Directory `/var/lib/vigilant-canine` created with nocow via tmpfiles.d
- Runtime: nocow set on directory and database file via ioctl
- Failures are non-fatal (databases work fine with COW, just slower)

**When does it apply:**
- New database files inherit nocow from parent directory
- Existing database files have nocow set explicitly on open
- WAL and journal files inherit from directory
```

**File:** `docs/troubleshooting.md`

Add to Performance Issues section:

```markdown
### Database fragmentation on Btrfs/ZFS

**Symptom:**

Slow database queries or high I/O wait on COW filesystems.

**Diagnosis:**

Check if nocow is set:
```bash
lsattr /var/lib/vigilant-canine/vigilant-canine.db
# Should show: -------C-------- (C = nocow)
```

Check fragmentation (Btrfs):
```bash
sudo filefrag /var/lib/vigilant-canine/vigilant-canine.db
```

**Solution:**

The daemon automatically sets nocow, but if you have an existing database:

```bash
# Stop daemon
sudo systemctl stop vigilant-canined

# Set nocow on directory (affects new files)
sudo chattr +C /var/lib/vigilant-canine

# For existing database file, must recreate:
# Method 1: Defragment (Btrfs-specific, may not fully work with nocow)
sudo btrfs filesystem defragment /var/lib/vigilant-canine/vigilant-canine.db

# Method 2: Recreate database (safest)
cd /var/lib/vigilant-canine
sudo mv vigilant-canine.db vigilant-canine.db.old
sudo chattr +C .
sudo cp --reflink=never vigilant-canine.db.old vigilant-canine.db
sudo rm vigilant-canine.db.old

# Verify nocow is set
lsattr vigilant-canine.db  # Should show C flag

# Restart daemon
sudo systemctl start vigilant-canined
```
```

## Implementation Checklist

- [ ] Remove `is_btrfs()` function from database.cpp
- [ ] Remove unnecessary includes (`<linux/magic.h>`, `<sys/vfs.h>`)
- [ ] Update `ensure_database_directory()` to always set nocow
- [ ] Add nocow setting in `Database::open()` for the database file itself
- [ ] Update function comments in database.cpp
- [ ] Update function comments in database.h
- [ ] Update tmpfiles.d config with `h+C` attribute
- [ ] Add architecture.md section on COW handling
- [ ] Add troubleshooting.md section on database fragmentation
- [ ] Test on Btrfs (verify nocow is set)
- [ ] Test on ext4 (verify silently ignored, no errors)
- [ ] Test on ZFS if available (verify behavior)

## Testing

### Manual Verification

**On Btrfs:**
```bash
# Build and install
./build.sh -grt
sudo make install

# Check directory
lsattr /var/lib/vigilant-canine
# Should show: -------C-------- (C = nocow)

# Run daemon to create database
sudo systemctl start vigilant-canined

# Check database file
lsattr /var/lib/vigilant-canine/*.db
# Should show: -------C-------- (C = nocow)
```

**On ext4:**
```bash
# Same steps - should work without errors
# lsattr may show different flags, but no C (expected)
```

### Unit Tests

No new tests required - existing Database tests cover functionality.
The nocow setting is an optimization, not a requirement.

## Risks and Mitigations

**Risk:** Breaks on unknown filesystems
**Mitigation:** ioctl failures are already non-fatal, just logged

**Risk:** Performance regression if nocow doesn't work
**Mitigation:** Same as current behavior - databases work fine, just slower

**Risk:** Security concern with nocow (no COW snapshots)
**Mitigation:** Database integrity maintained by SQLite, not filesystem COW

## Alternative Considered: Keep Btrfs Detection

**Why rejected:**
- Fragile - must enumerate all COW filesystems
- ZFS, bcachefs, future filesystems would be missed
- More code, more complexity
- No benefit - ioctl is harmless on non-COW filesystems

## References

- Btrfs nocow: https://btrfs.readthedocs.io/en/latest/ch-nocow.html
- SQLite on Btrfs: https://www.sqlite.org/howtocorrupt.html#_filesystem_corruption
- FS_IOC_SETFLAGS: https://man7.org/linux/man-pages/man2/ioctl_iflags.2.html
