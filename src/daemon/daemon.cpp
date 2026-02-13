//
// vigilant-canine - Daemon Lifecycle Manager Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <daemon/daemon.h>

#include <systemd/sd-journal.h>

#include <csignal>
#include <format>
#include <iostream>
#include <thread>

namespace vigilant_canine {

    // Global pointer for signal handlers
    static Daemon* g_daemon_instance = nullptr;

    // Signal handler functions
    extern "C" {
        static void signal_handler(int signum) {
            if (g_daemon_instance) {
                switch (signum) {
                    case SIGTERM:
                    case SIGINT:
                        sd_journal_print(LOG_INFO, "vigilant-canined: Received stop signal");
                        g_daemon_instance->stop();
                        break;
                    case SIGHUP:
                        sd_journal_print(LOG_INFO, "vigilant-canined: Received reload signal");
                        auto result = g_daemon_instance->reload_config();
                        if (!result) {
                            sd_journal_print(LOG_ERR,
                                "vigilant-canined: Config reload failed: %s",
                                result.error().c_str());
                        }
                        break;
                }
            }
        }
    }

    Daemon::Daemon(std::filesystem::path config_path)
        : m_config_path(std::move(config_path)) {
        // Only allow one daemon instance
        if (g_daemon_instance != nullptr) {
            throw std::runtime_error("Daemon instance already exists");
        }
        g_daemon_instance = this;
    }

    Daemon::~Daemon() {
        stop();
        g_daemon_instance = nullptr;
    }

    auto Daemon::initialize() -> std::expected<void, std::string> {
        sd_journal_print(LOG_INFO, "vigilant-canined: Initializing daemon");

        // Load configuration
        auto config_result = load_config_or_default(m_config_path);
        if (!config_result) {
            return std::unexpected(std::format("Failed to load config: {}",
                                                config_result.error()));
        }
        m_config = std::move(*config_result);

        // Detect distribution
        auto distro_result = detect_distro();
        if (!distro_result) {
            return std::unexpected(std::format("Failed to detect distro: {}",
                                                distro_result.error()));
        }
        m_distro = std::move(*distro_result);

        sd_journal_print(LOG_INFO, "vigilant-canined: Detected distro: %s %s (%s)",
                         m_distro.name.c_str(),
                         m_distro.version.c_str(),
                         to_string(m_distro.type).c_str());

        // Initialize database
        auto db_result = Database::open(m_config.daemon.db_path);
        if (!db_result) {
            return std::unexpected(std::format("Failed to open database: {}",
                                                db_result.error()));
        }
        m_database = std::make_unique<Database>(std::move(*db_result));

        // Create stores
        m_baseline_store = std::make_unique<BaselineStore>(*m_database);
        m_alert_store = std::make_unique<AlertStore>(*m_database);

        // Create event bus
        m_event_bus = std::make_unique<EventBus>();

        // Create policy engine with default policy for distro type
        auto policy_config = create_default_policy(m_distro.type);
        m_policy_engine = std::make_unique<PolicyEngine>(policy_config);

        // Create baseline strategy
        m_strategy = create_strategy(m_distro);

        // Create scanner
        m_scanner = std::make_unique<Scanner>(*m_baseline_store,
                                               *m_strategy,
                                               *m_event_bus,
                                               m_config.hash.algorithm);

        // Create fanotify monitor
        m_fanotify_monitor = std::make_unique<FanotifyMonitor>(*m_baseline_store,
                                                                 *m_strategy,
                                                                 *m_event_bus,
                                                                 m_config.hash.algorithm);

        // Create alert dispatcher
        AlertDispatcherConfig dispatch_config;
        dispatch_config.log_to_journal = m_config.alerts.journal;
        dispatch_config.send_dbus = m_config.alerts.dbus;

        m_alert_dispatcher = std::make_unique<AlertDispatcher>(*m_event_bus,
                                                                 *m_alert_store,
                                                                 dispatch_config);

        sd_journal_print(LOG_INFO, "vigilant-canined: Initialization complete");
        return {};
    }

    auto Daemon::run() -> std::expected<void, std::string> {
        if (!m_database) {
            return std::unexpected("Daemon not initialized");
        }

        sd_journal_print(LOG_INFO, "vigilant-canined: Starting daemon");

        // Setup signal handlers
        setup_signal_handlers();

        // Start alert dispatcher
        auto dispatch_result = m_alert_dispatcher->start();
        if (!dispatch_result) {
            return std::unexpected(std::format("Failed to start alert dispatcher: {}",
                                                dispatch_result.error()));
        }

        // Initialize and start fanotify monitor
        auto fanotify_init_result = m_fanotify_monitor->initialize();
        if (!fanotify_init_result) {
            sd_journal_print(LOG_WARNING,
                "vigilant-canined: Fanotify init failed (may need elevated privileges): %s",
                fanotify_init_result.error().c_str());
        } else {
            auto fanotify_start_result = m_fanotify_monitor->start();
            if (!fanotify_start_result) {
                sd_journal_print(LOG_WARNING,
                    "vigilant-canined: Fanotify start failed: %s",
                    fanotify_start_result.error().c_str());
            } else {
                sd_journal_print(LOG_INFO, "vigilant-canined: Fanotify monitor started");
            }
        }

        // Perform initial scan if configured
        if (m_config.scan.on_boot) {
            sd_journal_print(LOG_INFO, "vigilant-canined: Running initial scan");

            auto monitor_paths = m_strategy->get_monitor_paths();
            for (auto const& path : monitor_paths.critical) {
                if (std::filesystem::exists(path)) {
                    auto scan_result = m_scanner->scan_directory(path);
                    if (scan_result) {
                        sd_journal_print(LOG_INFO,
                            "vigilant-canined: Scanned %s: %zu files, %zu added",
                            path.c_str(),
                            scan_result->files_scanned,
                            scan_result->files_added);
                    }
                }
            }
        }

        m_running.store(true);
        sd_journal_print(LOG_INFO, "vigilant-canined: Daemon running");

        // Main loop - sleep and check for stop/reload signals
        while (!m_should_stop.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            if (m_should_reload.load()) {
                m_should_reload.store(false);
                auto reload_result = reload_config();
                if (!reload_result) {
                    sd_journal_print(LOG_ERR,
                        "vigilant-canined: Config reload failed: %s",
                        reload_result.error().c_str());
                }
            }
        }

        sd_journal_print(LOG_INFO, "vigilant-canined: Shutting down");

        // Stop components
        m_fanotify_monitor->stop();
        m_alert_dispatcher->stop();

        m_running.store(false);
        sd_journal_print(LOG_INFO, "vigilant-canined: Daemon stopped");

        return {};
    }

    void Daemon::stop() {
        m_should_stop.store(true);
    }

    auto Daemon::reload_config() -> std::expected<void, std::string> {
        sd_journal_print(LOG_INFO, "vigilant-canined: Reloading configuration");

        auto config_result = load_config_or_default(m_config_path);
        if (!config_result) {
            return std::unexpected(std::format("Failed to load config: {}",
                                                config_result.error()));
        }

        m_config = std::move(*config_result);

        // Update alert dispatcher config
        AlertDispatcherConfig dispatch_config;
        dispatch_config.log_to_journal = m_config.alerts.journal;
        dispatch_config.send_dbus = m_config.alerts.dbus;

        // Note: We can't easily update all components without restarting
        // For now, just update what we can
        sd_journal_print(LOG_INFO, "vigilant-canined: Configuration reloaded");

        return {};
    }

    void Daemon::setup_signal_handlers() {
        // Setup signal handlers for graceful shutdown and config reload
        std::signal(SIGTERM, signal_handler);
        std::signal(SIGINT, signal_handler);
        std::signal(SIGHUP, signal_handler);

        // Ignore SIGPIPE
        std::signal(SIGPIPE, SIG_IGN);
    }

    auto Daemon::create_strategy(DistroInfo const& distro)
        -> std::unique_ptr<BaselineStrategy> {

        switch (distro.type) {
            case DistroType::traditional:
                return std::make_unique<TraditionalStrategy>();
            case DistroType::ostree:
                return std::make_unique<OstreeStrategy>();
            case DistroType::btrfs_snapshot:
                // Future: BtrfsSnapshotStrategy
                // For now, fall back to traditional
                return std::make_unique<TraditionalStrategy>();
        }

        return std::make_unique<TraditionalStrategy>();
    }

}  // namespace vigilant_canine
