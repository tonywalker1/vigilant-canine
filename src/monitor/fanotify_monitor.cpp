//
// vigilant-canine - Fanotify Filesystem Monitor Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <monitor/fanotify_monitor.h>

#include <core/hash.h>

#include <sys/fanotify.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <linux/limits.h>

#include <cerrno>
#include <cstring>
#include <format>
#include <iostream>

namespace vigilant_canine {

    FanotifyMonitor::FanotifyMonitor(BaselineStore& store,
                                      BaselineStrategy& strategy,
                                      EventBus& event_bus,
                                      HashAlgorithm algorithm)
        : m_store(store), m_strategy(strategy), m_event_bus(event_bus), m_algorithm(algorithm) {}

    FanotifyMonitor::~FanotifyMonitor() {
        stop();
        if (m_fanotify_fd >= 0) {
            close(m_fanotify_fd);
        }
    }

    FanotifyMonitor::FanotifyMonitor(FanotifyMonitor&& other) noexcept
        : m_store(other.m_store),
          m_strategy(other.m_strategy),
          m_event_bus(other.m_event_bus),
          m_algorithm(other.m_algorithm),
          m_fanotify_fd(other.m_fanotify_fd) {
        other.m_fanotify_fd = -1;
        m_running.store(other.m_running.load());
        m_should_stop.store(other.m_should_stop.load());
    }

    FanotifyMonitor& FanotifyMonitor::operator=(FanotifyMonitor&& other) noexcept {
        if (this != &other) {
            stop();
            if (m_fanotify_fd >= 0) {
                close(m_fanotify_fd);
            }

            m_fanotify_fd = other.m_fanotify_fd;
            other.m_fanotify_fd = -1;
            m_running.store(other.m_running.load());
            m_should_stop.store(other.m_should_stop.load());
        }
        return *this;
    }

    auto FanotifyMonitor::initialize() -> std::expected<void, std::string> {
        // Initialize fanotify
        // FAN_CLASS_NOTIF: Notification only (no permission checking)
        // FAN_CLOEXEC: Close on exec
        // FAN_NONBLOCK: Non-blocking reads
        m_fanotify_fd = fanotify_init(FAN_CLASS_NOTIF | FAN_CLOEXEC | FAN_NONBLOCK, O_RDONLY);

        if (m_fanotify_fd < 0) {
            return std::unexpected(std::format("Failed to initialize fanotify: {}",
                                                strerror(errno)));
        }

        // Get paths to monitor from strategy
        auto paths = m_strategy.get_monitor_paths();

        // Mark critical paths for monitoring
        for (auto const& path : paths.critical) {
            if (!std::filesystem::exists(path)) {
                continue;  // Skip non-existent paths
            }

            // Monitor: file modifications and closes after write
            int ret = fanotify_mark(m_fanotify_fd, FAN_MARK_ADD | FAN_MARK_MOUNT,
                                     FAN_MODIFY | FAN_CLOSE_WRITE,
                                     AT_FDCWD, path.c_str());

            if (ret < 0) {
                return std::unexpected(std::format("Failed to mark path {}: {}",
                                                    path.string(), strerror(errno)));
            }
        }

        // Mark config paths for monitoring
        for (auto const& path : paths.config) {
            if (!std::filesystem::exists(path)) {
                continue;
            }

            int ret = fanotify_mark(m_fanotify_fd, FAN_MARK_ADD | FAN_MARK_MOUNT,
                                     FAN_MODIFY | FAN_CLOSE_WRITE,
                                     AT_FDCWD, path.c_str());

            if (ret < 0) {
                // Config paths are less critical - log but don't fail
                std::cerr << "Warning: Failed to mark config path " << path << ": "
                          << strerror(errno) << "\n";
            }
        }

        return {};
    }

    auto FanotifyMonitor::start() -> std::expected<void, std::string> {
        if (m_fanotify_fd < 0) {
            return std::unexpected("Monitor not initialized");
        }

        if (m_running.load()) {
            return std::unexpected("Monitor already running");
        }

        m_should_stop.store(false);
        m_running.store(true);

        m_monitor_thread = std::thread([this] { monitor_loop(); });

        return {};
    }

    void FanotifyMonitor::stop() {
        if (!m_running.load()) {
            return;
        }

        m_should_stop.store(true);

        if (m_monitor_thread.joinable()) {
            m_monitor_thread.join();
        }

        m_running.store(false);
    }

    void FanotifyMonitor::monitor_loop() {
        constexpr std::size_t event_buffer_size = 4096;
        alignas(struct fanotify_event_metadata) char buffer[event_buffer_size];

        while (!m_should_stop.load()) {
            // Poll with timeout so we can check m_should_stop periodically
            struct pollfd fds{};
            fds.fd = m_fanotify_fd;
            fds.events = POLLIN;

            int poll_ret = poll(&fds, 1, 1000);  // 1 second timeout

            if (poll_ret < 0) {
                if (errno == EINTR) {
                    continue;
                }
                std::cerr << "Poll error: " << strerror(errno) << "\n";
                break;
            }

            if (poll_ret == 0) {
                // Timeout - check if we should stop
                continue;
            }

            // Read events
            ssize_t length = read(m_fanotify_fd, buffer, event_buffer_size);

            if (length < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                }
                std::cerr << "Read error: " << strerror(errno) << "\n";
                break;
            }

            // Process events
            auto* metadata = reinterpret_cast<struct fanotify_event_metadata*>(buffer);

            while (FAN_EVENT_OK(metadata, length)) {
                if (metadata->vers != FANOTIFY_METADATA_VERSION) {
                    std::cerr << "Fanotify version mismatch\n";
                    break;
                }

                if (metadata->fd >= 0) {
                    // Get file path from fd
                    char path_buf[PATH_MAX];
                    std::snprintf(path_buf, sizeof(path_buf), "/proc/self/fd/%d", metadata->fd);

                    char real_path[PATH_MAX];
                    ssize_t len = readlink(path_buf, real_path, sizeof(real_path) - 1);

                    if (len > 0) {
                        real_path[len] = '\0';
                        handle_event(metadata->fd, std::filesystem::path{real_path});
                    }

                    close(metadata->fd);
                }

                metadata = FAN_EVENT_NEXT(metadata, length);
            }
        }
    }

    void FanotifyMonitor::handle_event(int /* fd */, std::filesystem::path const& path) {
        // Skip if path should be excluded
        auto monitor_paths = m_strategy.get_monitor_paths();
        for (auto const& exclude : monitor_paths.exclude) {
            if (path.string().starts_with(exclude.string())) {
                return;
            }
        }

        // Skip non-regular files
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec)) {
            return;
        }

        FilePath file_path{path};

        // Check if we have a baseline for this file
        auto deployment = m_strategy.get_deployment_id();
        auto baseline_result = m_store.find_by_path(file_path, deployment);

        if (!baseline_result || !baseline_result->has_value()) {
            // No baseline - new file detected
            auto hash_result = hash_file(file_path, m_algorithm);
            if (hash_result) {
                FileCreatedEvent event{
                    file_path,
                    *hash_result,
                    std::nullopt
                };
                m_event_bus.publish(Event{event, EventSeverity::warning, "fanotify"});
            }
            return;
        }

        auto const& baseline = baseline_result->value();

        // Calculate current hash
        auto hash_result = hash_file(file_path, m_algorithm);
        if (!hash_result) {
            return;
        }

        // Compare with baseline
        if (*(*hash_result) != *(baseline.hash_value)) {
            FileModifiedEvent event{
                file_path,
                baseline.hash_value,
                *hash_result,
                "File content modified"
            };
            m_event_bus.publish(Event{event, EventSeverity::critical, "fanotify"});
        }
    }

}  // namespace vigilant_canine
