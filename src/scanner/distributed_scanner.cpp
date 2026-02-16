//
// vigilant-canine - Distributed Baseline Scanner Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <scanner/distributed_scanner.h>

#include <core/types.h>
#include <events/event.h>

#include <algorithm>
#include <format>

namespace vigilant_canine {

    auto DistributedScanner::start() -> std::expected<void, std::string> {
        if (m_running.load()) {
            return std::unexpected("Distributed scanner already running");
        }

        m_should_stop.store(false);
        m_running.store(true);

        try {
            m_thread = std::thread(&DistributedScanner::scanner_loop, this);
        } catch (std::exception const& e) {
            m_running.store(false);
            return std::unexpected(std::format("Failed to start scanner thread: {}", e.what()));
        }

        return {};
    }

    void DistributedScanner::stop() {
        if (!m_running.load()) {
            return;
        }

        m_should_stop.store(true);
        m_cv.notify_all();  // Wake up thread

        if (m_thread.joinable()) {
            m_thread.join();
        }

        m_running.store(false);
    }

    void DistributedScanner::update_config(ScanConfig config) {
        std::lock_guard lock(m_config_mutex);
        m_config = config;
    }

    void DistributedScanner::scanner_loop() {
        while (!m_should_stop.load()) {
            // Collect files to verify
            auto files = collect_file_list();

            if (files.empty()) {
                // No files to verify - sleep and retry
                std::unique_lock lock(m_cv_mutex);
                m_cv.wait_for(lock, std::chrono::minutes(10),
                              [this] { return m_should_stop.load(); });
                continue;
            }

            // Determine batch size
            std::size_t batch_size;
            {
                std::lock_guard lock(m_config_mutex);
                if (m_config.batch_size > 0) {
                    batch_size = m_config.batch_size;
                } else {
                    // Auto-calculate: ~1 batch per minute
                    auto batches_per_cycle = m_config.interval_hours * 60;
                    batch_size = std::max<std::size_t>(1, files.size() / batches_per_cycle);
                }
            }

            // Process files in batches
            std::size_t cursor = 0;
            while (cursor < files.size() && !m_should_stop.load()) {
                // Check power state
                auto power = m_power_monitor.read_power_state();

                // Check if we should pause due to low battery
                bool should_pause = false;
                {
                    std::lock_guard lock(m_config_mutex);
                    if (m_config.adaptive_pacing &&
                        power.source == PowerSource::battery &&
                        power.battery_percent < m_config.battery_pause_threshold) {
                        should_pause = true;
                    }
                }

                if (should_pause) {
                    // Wait 60s before re-checking battery
                    std::unique_lock lock(m_cv_mutex);
                    m_cv.wait_for(lock, std::chrono::seconds(60),
                                  [this] { return m_should_stop.load(); });
                    continue;
                }

                // Process batch
                auto batch_count = std::min(batch_size, files.size() - cursor);
                process_batch(files, cursor, batch_count);
                cursor += batch_count;

                // Sleep between batches
                auto sleep_duration = compute_sleep_duration(files.size(), batch_size, power);
                if (sleep_duration.count() > 0) {
                    std::unique_lock lock(m_cv_mutex);
                    m_cv.wait_for(lock, sleep_duration,
                                  [this] { return m_should_stop.load(); });
                }
            }

            // Publish scan completed event
            ScanCompletedEvent completed_event{
                std::filesystem::path{m_strategy.get_deployment_id().value_or("")},
                files.size(),
                0,  // changes_detected (would need to track from process_batch)
                std::chrono::milliseconds(0)  // duration_ms (would need to track)
            };
            m_event_bus.publish(Event{completed_event, EventSeverity::info, "distributed_scanner"});

            // Sleep before next full cycle
            std::unique_lock lock(m_cv_mutex);
            auto interval = std::chrono::hours(m_config.interval_hours);
            m_cv.wait_for(lock, interval, [this] { return m_should_stop.load(); });
        }
    }

    auto DistributedScanner::collect_file_list() -> std::vector<std::filesystem::path> {
        std::vector<std::filesystem::path> files;

        // Walk all critical paths from strategy
        auto monitor_paths = m_strategy.get_monitor_paths();
        for (auto const& path : monitor_paths.critical) {
            std::error_code ec;
            auto iter = std::filesystem::recursive_directory_iterator(
                path,
                std::filesystem::directory_options::skip_permission_denied,
                ec);

            if (ec) {
                continue;
            }

            for (auto const& entry : iter) {
                if (entry.is_regular_file(ec)) {
                    files.push_back(entry.path());
                }
            }
        }

        return files;
    }

    auto DistributedScanner::compute_sleep_duration(
        std::size_t total_files,
        std::size_t batch_size,
        PowerState const& power
    ) const -> std::chrono::milliseconds {

        if (total_files == 0 || batch_size == 0) {
            return std::chrono::milliseconds(0);
        }

        std::lock_guard lock(m_config_mutex);

        // Base sleep duration to spread work evenly over interval
        auto interval_ms = m_config.interval_hours * 60 * 60 * 1000;
        auto num_batches = (total_files + batch_size - 1) / batch_size;
        auto base_sleep_ms = interval_ms / std::max<std::size_t>(1, num_batches);

        // Apply slowdown factor on battery power
        if (m_config.adaptive_pacing && power.source == PowerSource::battery) {
            base_sleep_ms = static_cast<std::uint64_t>(
                base_sleep_ms * m_config.battery_slowdown_factor);
        }

        return std::chrono::milliseconds(base_sleep_ms);
    }

    auto DistributedScanner::process_batch(
        std::vector<std::filesystem::path> const& files,
        std::size_t start,
        std::size_t count
    ) -> std::size_t {

        std::size_t verified = 0;

        for (std::size_t i = 0; i < count && (start + i) < files.size(); ++i) {
            if (m_should_stop.load()) {
                break;
            }

            auto const& file_path = files[start + i];
            FilePath path{file_path};

            // Verify the file using Scanner::verify_file()
            auto result = m_scanner.verify_file(path);
            if (result) {
                verified++;
            }
        }

        return verified;
    }

}  // namespace vigilant_canine
