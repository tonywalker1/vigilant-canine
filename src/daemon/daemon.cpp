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
        m_audit_event_store = std::make_unique<AuditEventStore>(*m_database);  // Phase 3

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
                                                                 *m_baseline_store,
                                                                 dispatch_config);

        // Phase 3: Audit monitor
        if (m_config.audit.enabled) {
            auto audit_rules = get_default_audit_rules();
            // TODO: Merge with m_config.audit.rules

            AuditMonitorConfig audit_config;
            audit_config.sanitize_command_lines = m_config.audit.sanitize_command_lines;
            audit_config.exclude_comms = m_config.audit.exclude_comms;
            audit_config.exclude_uids = m_config.audit.exclude_uids;

            m_audit_monitor = std::make_unique<AuditMonitor>(
                *m_event_bus,
                std::move(audit_rules),
                audit_config
            );

            auto audit_init_result = m_audit_monitor->initialize();
            if (!audit_init_result) {
                // Audit may not be available - log warning but continue
                sd_journal_print(LOG_WARNING,
                    "vigilant-canined: Audit subsystem unavailable: %s",
                    audit_init_result.error().c_str());
                m_audit_monitor.reset();  // Don't keep unusable monitor
            }
        }

        // Initialize user monitoring
        auto user_init_result = initialize_user_monitoring();
        if (!user_init_result) {
            sd_journal_print(LOG_WARNING,
                "vigilant-canined: User monitoring initialization failed: %s",
                user_init_result.error().c_str());
            // Continue anyway - user monitoring is optional
        }

        // Create power monitor
        m_power_monitor = std::make_unique<PowerMonitor>();

        // Create distributed scanner
        m_distributed_scanner = std::make_unique<DistributedScanner>(
            *m_scanner,
            *m_baseline_store,
            *m_strategy,
            *m_event_bus,
            *m_power_monitor,
            m_config.scan
        );

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

        // Phase 3: Start audit monitor
        if (m_audit_monitor) {
            auto audit_start_result = m_audit_monitor->start();
            if (!audit_start_result) {
                sd_journal_print(LOG_WARNING,
                    "vigilant-canined: Audit monitor start failed: %s",
                    audit_start_result.error().c_str());
            } else {
                sd_journal_print(LOG_INFO, "vigilant-canined: Audit monitor started");
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

            // Scan user baselines
            auto user_scan_result = scan_user_baselines();
            if (!user_scan_result) {
                sd_journal_print(LOG_WARNING,
                    "vigilant-canined: User baseline scan failed: %s",
                    user_scan_result.error().c_str());
            }
        }

        // Start distributed scanner
        auto dist_scanner_result = m_distributed_scanner->start();
        if (!dist_scanner_result) {
            sd_journal_print(LOG_WARNING,
                "vigilant-canined: Distributed scanner start failed: %s",
                dist_scanner_result.error().c_str());
        } else {
            sd_journal_print(LOG_INFO, "vigilant-canined: Distributed scanner started");
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
        if (m_distributed_scanner) {
            m_distributed_scanner->stop();
        }
        m_fanotify_monitor->stop();
        m_alert_dispatcher->stop();
        if (m_audit_monitor) {
            m_audit_monitor->stop();
        }

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

        // Update distributed scanner config
        if (m_distributed_scanner) {
            m_distributed_scanner->update_config(m_config.scan);
        }

        // Phase 3: Hot reload audit rules
        if (m_audit_monitor && m_config.audit.enabled) {
            auto audit_rules = get_default_audit_rules();
            // TODO: Merge with m_config.audit.rules
            m_audit_monitor->update_rules(std::move(audit_rules));
        }

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

    auto Daemon::initialize_user_monitoring() -> std::expected<void, std::string> {
        // Create user manager
        m_user_manager = std::make_unique<UserManager>();

        // Discover users with interactive shells (UID >= 1000)
        auto users_result = m_user_manager->discover_users();
        if (!users_result) {
            return std::unexpected(std::format("Failed to discover users: {}",
                                                users_result.error()));
        }

        auto const& all_users = users_result.value();
        sd_journal_print(LOG_INFO, "vigilant-canined: Discovered %zu users",
                         all_users.size());

        // For each user, check if monitoring applies
        for (auto const& user : all_users) {
            // Load user config if it exists
            auto user_config_result = m_user_manager->load_user_config(user);
            if (!user_config_result) {
                sd_journal_print(LOG_WARNING,
                    "vigilant-canined: Failed to load config for user %s: %s",
                    user.username.c_str(),
                    user_config_result.error().c_str());
                continue;
            }

            auto const& user_config_opt = user_config_result.value();
            bool user_config_exists = user_config_opt.has_value();
            bool user_config_enabled = user_config_exists
                ? user_config_opt.value().monitor.home.enabled
                : false;

            // Check if user should be monitored based on policy
            bool should_monitor = m_user_manager->should_monitor_user(
                user,
                m_config.home_policy,
                user_config_exists,
                user_config_enabled
            );

            if (!should_monitor) {
                continue;
            }

            // Merge configs for this user
            auto merged_config = merge_configs(
                m_config,
                m_config.home_policy,
                user_config_opt,
                user.home_dir
            );

            // Add user paths to fanotify monitor
            for (auto const& path : merged_config.monitor.home.paths) {
                if (std::filesystem::exists(path)) {
                    // Note: The fanotify monitor will handle adding paths
                    // For now, we just track the user for baseline scanning
                }
            }

            m_monitored_users.push_back(user);

            sd_journal_print(LOG_INFO,
                "vigilant-canined: User %s: monitoring enabled (%zu paths)",
                user.username.c_str(),
                merged_config.monitor.home.paths.size());
        }

        sd_journal_print(LOG_INFO, "vigilant-canined: Monitoring %zu users",
                         m_monitored_users.size());

        return {};
    }

    auto Daemon::scan_user_baselines() -> std::expected<void, std::string> {
        if (m_monitored_users.empty()) {
            return {};
        }

        sd_journal_print(LOG_INFO,
            "vigilant-canined: Scanning baselines for %zu monitored users",
            m_monitored_users.size());

        for (auto const& user : m_monitored_users) {
            // Load user config
            auto user_config_result = m_user_manager->load_user_config(user);
            if (!user_config_result || !user_config_result->has_value()) {
                continue;
            }

            auto const& user_config = user_config_result->value();

            // Merge configs
            auto merged_config = merge_configs(
                m_config,
                m_config.home_policy,
                user_config,
                user.home_dir
            );

            // Scan user paths
            std::string source = std::format("user:{}", user.username);

            auto scan_result = m_scanner->scan_user_paths(
                merged_config.monitor.home.paths,
                merged_config.monitor.home.exclude,
                source
            );

            if (scan_result) {
                sd_journal_print(LOG_INFO,
                    "vigilant-canined: User %s baseline: %zu files scanned, %zu added",
                    user.username.c_str(),
                    scan_result->files_scanned,
                    scan_result->files_added);
            } else {
                sd_journal_print(LOG_WARNING,
                    "vigilant-canined: Failed to scan baseline for user %s: %s",
                    user.username.c_str(),
                    scan_result.error().c_str());
            }
        }

        return {};
    }

}  // namespace vigilant_canine
