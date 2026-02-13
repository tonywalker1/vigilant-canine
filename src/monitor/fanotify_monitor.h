//
// vigilant-canine - Fanotify Filesystem Monitor
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_MONITOR_FANOTIFY_MONITOR_H
#define VIGILANT_CANINE_MONITOR_FANOTIFY_MONITOR_H

#include <baseline/strategy.h>
#include <events/event_bus.h>
#include <storage/baseline_store.h>

#include <atomic>
#include <expected>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace vigilant_canine {

    //
    // Fanotify-based real-time filesystem monitor.
    //
    // Uses Linux fanotify API to monitor filesystem events in real-time.
    // When files are modified, compares against baselines and publishes events.
    //
    // Requires CAP_DAC_READ_SEARCH or root privileges.
    //
    class FanotifyMonitor {
    public:
        FanotifyMonitor(BaselineStore& store,
                        BaselineStrategy& strategy,
                        EventBus& event_bus,
                        HashAlgorithm algorithm = HashAlgorithm::blake3);

        ~FanotifyMonitor();

        // Non-copyable, movable
        FanotifyMonitor(FanotifyMonitor const&) = delete;
        FanotifyMonitor& operator=(FanotifyMonitor const&) = delete;
        FanotifyMonitor(FanotifyMonitor&&) noexcept;
        FanotifyMonitor& operator=(FanotifyMonitor&&) noexcept;

        //
        // Initialize fanotify and mark paths for monitoring.
        //
        // Uses the baseline strategy to determine which paths to monitor.
        //
        // Preconditions:
        //   - Process must have CAP_DAC_READ_SEARCH or be root
        //
        // Postconditions:
        //   - On success: fanotify fd is initialized and paths are marked
        //   - On failure: returns error message
        //
        [[nodiscard]] auto initialize() -> std::expected<void, std::string>;

        //
        // Start monitoring in background thread.
        //
        // Spawns a thread that polls fanotify for events and publishes
        // file modification events to the event bus.
        //
        // Preconditions:
        //   - initialize() must have been called successfully
        //
        [[nodiscard]] auto start() -> std::expected<void, std::string>;

        //
        // Stop monitoring and join background thread.
        //
        void stop();

        //
        // Check if monitor is currently running.
        //
        [[nodiscard]] auto is_running() const -> bool {
            return m_running.load();
        }

    private:
        //
        // Background thread function that polls fanotify events.
        //
        void monitor_loop();

        //
        // Handle a single fanotify event.
        //
        void handle_event(int fd, std::filesystem::path const& path);

        BaselineStore& m_store;
        BaselineStrategy& m_strategy;
        EventBus& m_event_bus;
        HashAlgorithm m_algorithm;

        int m_fanotify_fd{-1};
        std::thread m_monitor_thread;
        std::atomic<bool> m_running{false};
        std::atomic<bool> m_should_stop{false};
    };

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_MONITOR_FANOTIFY_MONITOR_H
