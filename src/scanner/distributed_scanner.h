//
// vigilant-canine - Distributed Baseline Scanner
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_SCANNER_DISTRIBUTED_SCANNER_H
#define VIGILANT_CANINE_SCANNER_DISTRIBUTED_SCANNER_H

#include <baseline/strategy.h>
#include <config/config.h>
#include <events/event_bus.h>
#include <power/power_monitor.h>
#include <scanner/scanner.h>
#include <storage/baseline_store.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <expected>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace vigilant_canine {

    //
    // Distributed baseline scanner with adaptive pacing.
    //
    // Spreads verification work evenly over a configurable interval (e.g., 24 hours)
    // and adapts pacing based on AC/battery power state.
    //
    class DistributedScanner {
    public:
        DistributedScanner(Scanner& scanner,
                           BaselineStore& store,
                           BaselineStrategy& strategy,
                           EventBus& event_bus,
                           PowerMonitor& power_monitor,
                           ScanConfig config)
            : m_scanner(scanner),
              m_store(store),
              m_strategy(strategy),
              m_event_bus(event_bus),
              m_power_monitor(power_monitor),
              m_config(config) {}

        //
        // Start the distributed scanner thread.
        //
        // Preconditions:
        //   - Scanner must not already be running
        //
        // Postconditions:
        //   - On success: background thread started
        //   - On failure: returns error message
        //
        [[nodiscard]] auto start() -> std::expected<void, std::string>;

        //
        // Stop the distributed scanner thread.
        //
        // Blocks until the scanner thread has exited.
        //
        void stop();

        //
        // Check if scanner is running.
        //
        [[nodiscard]] auto is_running() const -> bool {
            return m_running.load();
        }

        //
        // Update scanner configuration (for SIGHUP reload).
        //
        void update_config(ScanConfig config);

    private:
        //
        // Scanner thread main loop.
        //
        void scanner_loop();

        //
        // Collect list of files to verify from strategy.
        //
        auto collect_file_list() -> std::vector<std::filesystem::path>;

        //
        // Compute sleep duration between batches based on power state.
        //
        auto compute_sleep_duration(std::size_t total_files,
                                     std::size_t batch_size,
                                     PowerState const& power) const -> std::chrono::milliseconds;

        //
        // Process a batch of files for verification.
        //
        // Returns number of files successfully verified.
        //
        auto process_batch(std::vector<std::filesystem::path> const& files,
                           std::size_t start,
                           std::size_t count) -> std::size_t;

        Scanner& m_scanner;
        BaselineStore& m_store;
        BaselineStrategy& m_strategy;
        EventBus& m_event_bus;
        PowerMonitor& m_power_monitor;

        ScanConfig m_config;
        mutable std::mutex m_config_mutex;

        std::atomic<bool> m_running{false};
        std::atomic<bool> m_should_stop{false};
        std::thread m_thread;
        std::condition_variable m_cv;
        std::mutex m_cv_mutex;
    };

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_SCANNER_DISTRIBUTED_SCANNER_H
