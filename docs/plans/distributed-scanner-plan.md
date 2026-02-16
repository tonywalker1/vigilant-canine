# Distributed Baseline Scanner with Adaptive Pacing

## Context

Baseline verification currently runs as a burst at daemon startup — all files hashed at once.
On laptops and low-power devices this causes sustained disk I/O, CPU load, and battery drain.
The `scan.schedule` config field is parsed but never acted upon — no periodic scanning exists.

This change spreads verification work evenly over a configurable interval (e.g., 24 hours) and
adapts pacing based on AC/battery power state.

## New Files

| File | Purpose |
|------|---------|
| `src/power/power_monitor.h` | PowerMonitor class — reads sysfs power state |
| `src/power/power_monitor.cpp` | Implementation |
| `src/scanner/distributed_scanner.h` | DistributedScanner class — threaded batch verifier |
| `src/scanner/distributed_scanner.cpp` | Implementation |
| `tests/power/CMakeLists.txt` | Test build for power module |
| `tests/power/power_monitor_test.cpp` | PowerMonitor tests (mock sysfs) |
| `tests/scanner/distributed_scanner_test.cpp` | DistributedScanner tests |

## Modified Files

| File | Change |
|------|--------|
| `src/config/config.h` | Replace `ScanConfig.schedule` with pacing fields |
| `src/config/config.cpp` | Parse new fields in `parse_scan()` |
| `src/scanner/scanner.h` | Add `verify_file()` public method |
| `src/scanner/scanner.cpp` | Extract single-file verification from `verify_baselines()` |
| `src/daemon/daemon.h` | Add `PowerMonitor` and `DistributedScanner` members |
| `src/daemon/daemon.cpp` | Create, start, stop, and reload the new components |
| `CMakeLists.txt` | Add new source files to `vigilant-canined` target |
| `tests/scanner/CMakeLists.txt` | Add `distributed_scanner_test` target |
| `tests/CMakeLists.txt` | Add `add_subdirectory(power)` |
| `config/vigilant-canine.toml.example` | Document new `[scan]` fields |

## Implementation Steps

### Step 1: Config Changes

Replace `ScanConfig` in `src/config/config.h:70-73`:

```cpp
struct ScanConfig {
    bool on_boot{true};
    std::uint32_t interval_hours{24};
    std::uint32_t batch_size{0};                // 0 = auto-calculate
    bool adaptive_pacing{true};
    double battery_slowdown_factor{2.0};
    std::uint8_t battery_pause_threshold{20};   // Pause below this %
};
```

Update `parse_scan()` in `src/config/config.cpp:147-156` to parse the new fields using the
existing `get_or()` pattern. Remove the `schedule` field entirely.

### Step 2: Scanner::verify_file()

Add to `src/scanner/scanner.h`:
```cpp
[[nodiscard]] auto verify_file(FilePath const& path)
    -> std::expected<std::optional<FileChange>, std::string>;
```

Extract the per-file logic from `verify_baselines()` (lines 228-314) into `verify_file()`.
Then have `verify_baselines()` call `verify_file()` in its loop. Pure refactor — no behavior
change. Existing scanner tests must still pass.

### Step 3: PowerMonitor

Stateless sysfs reader, no thread needed. Constructor takes `sysfs_path` for testability.

```cpp
class PowerMonitor {
public:
    explicit PowerMonitor(std::filesystem::path sysfs_path = "/sys/class/power_supply");
    [[nodiscard]] auto read_power_state() const -> PowerState;
private:
    std::filesystem::path m_sysfs_path;
};
```

`read_power_state()`:
- Iterates `m_sysfs_path/*/type` looking for `"Battery"` entries
- Reads `status` ("Charging"/"Discharging"/"Full"/"Not charging") and `capacity` (0-100)
- Charging/Full/Not charging → `PowerSource::ac`; Discharging → `PowerSource::battery`
- No battery found → `{ac, 100, false}`

Tests use temp directory with mock sysfs files (no root required):
- No power supply → AC/100%
- AC adapter only → AC
- Battery discharging at various levels
- Battery charging → reports AC (plugged in)
- Multiple power supplies

### Step 4: DistributedScanner

Follows existing monitor threading pattern (`start()`/`stop()`, `std::thread`, `atomic<bool>`).

```cpp
class DistributedScanner {
public:
    DistributedScanner(Scanner& scanner, BaselineStore& store,
                       BaselineStrategy& strategy, EventBus& event_bus,
                       PowerMonitor& power_monitor, ScanConfig config);
    [[nodiscard]] auto start() -> std::expected<void, std::string>;
    void stop();
    [[nodiscard]] auto is_running() const -> bool;
    void update_config(ScanConfig config);  // For SIGHUP
private:
    void scanner_loop();
    auto collect_file_list() -> std::vector<std::filesystem::path>;
    auto compute_sleep_duration(std::size_t total_files, std::size_t batch_size,
                                PowerState const& power) const -> std::chrono::milliseconds;
    auto process_batch(std::vector<std::filesystem::path> const& files,
                       std::size_t start, std::size_t count) -> std::size_t;
};
```

**Scanner loop logic:**
```
loop forever (until should_stop):
    files = collect_file_list()                    // Walk strategy paths
    batch_size = config.batch_size > 0
                     ? config.batch_size
                     : max(1, files.size() / (interval_hours * 60))   // ~1 batch/min
    cursor = 0
    while cursor < files.size() && !should_stop:
        power = power_monitor.read_power_state()
        if adaptive && battery && percent < pause_threshold:
            wait on CV for 60s (or until stop)     // Re-check battery
            continue
        process_batch(files, cursor, batch_size)
        cursor += batch_size
        sleep_ms = compute_sleep_duration(...)     // Apply slowdown factor on battery
        wait on CV for sleep_ms (or until stop)    // Interruptible via stop()
    publish ScanCompletedEvent (cycle complete)
```

Key details:
- Uses `std::condition_variable` for interruptible sleep (not `sleep_for`) so `stop()` is instant
- `m_config` protected by `m_config_mutex` for hot reload via `update_config()`
- `process_batch()` calls `Scanner::verify_file()` for each file in the batch
- No cursor persistence on restart — fresh cycle each daemon start

### Step 5: Daemon Integration

In `src/daemon/daemon.h`, add members:
```cpp
std::unique_ptr<PowerMonitor> m_power_monitor;
std::unique_ptr<DistributedScanner> m_distributed_scanner;
```

In `src/daemon/daemon.cpp`:
- `initialize()`: Create `PowerMonitor` and `DistributedScanner` after `Scanner`
- `run()`: Start distributed scanner after the boot scan block (line 239)
- Shutdown section (line 261): Stop distributed scanner before other components
- `reload_config()`: Call `m_distributed_scanner->update_config(m_config.scan)`

### Step 6: Build System

Root `CMakeLists.txt`: Add `src/power/power_monitor.cpp` and
`src/scanner/distributed_scanner.cpp` to `vigilant-canined` sources.

`tests/CMakeLists.txt`: Add `add_subdirectory(power)`.

`tests/power/CMakeLists.txt`: New test target linking `power_monitor.cpp` + GTest.

`tests/scanner/CMakeLists.txt`: Add `distributed_scanner_test` target linking scanner,
power, storage, baseline, events, and hash sources.

## Verification

1. Build: `mkdir -p build/gcc-debug && cd build/gcc-debug && cmake -DCMAKE_BUILD_TYPE=Debug ../.. && cmake --build . -j$(nproc)`
2. Run existing scanner tests — must pass unchanged (verify_file refactor is behavior-preserving)
3. Run new PowerMonitor tests — mock sysfs validation
4. Run new DistributedScanner tests — batch sizing, sleep computation, stop interruption
5. Manual: Run daemon, observe journal logs showing batch progress and power state
