//
// vigilant-canine - Daemon Lifecycle Manager
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_DAEMON_DAEMON_H
#define VIGILANT_CANINE_DAEMON_DAEMON_H

#include <audit/audit_monitor.h>
#include <baseline/strategy.h>
#include <config/config.h>
#include <correlation/correlation_engine.h>
#include <dispatch/alert_dispatcher.h>
#include <distro/detector.h>
#include <events/event_bus.h>
#include <journal/journal_monitor.h>
#include <monitor/fanotify_monitor.h>
#include <notify/dbus_notifier.h>
#include <policy/policy_engine.h>
#include <scanner/scanner.h>
#include <storage/alert_store.h>
#include <storage/audit_event_store.h>
#include <storage/baseline_store.h>
#include <storage/database.h>
#include <user/user_manager.h>

#include <atomic>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>

namespace vigilant_canine {

    //
    // Main daemon class that manages the lifecycle of all components.
    //
    // Responsibilities:
    // - Load configuration
    // - Initialize storage (database)
    // - Create and wire components (EventBus, stores, monitors, dispatchers)
    // - Handle Unix signals (SIGTERM, SIGINT, SIGHUP)
    // - Run main event loop
    // - Graceful shutdown
    //
    class Daemon {
    public:
        explicit Daemon(std::filesystem::path config_path = "/etc/vigilant-canine/config.toml");
        ~Daemon();

        // Non-copyable, non-movable (manages global state)
        Daemon(Daemon const&) = delete;
        Daemon& operator=(Daemon const&) = delete;
        Daemon(Daemon&&) = delete;
        Daemon& operator=(Daemon&&) = delete;

        //
        // Initialize all components.
        //
        // Loads configuration, detects distribution, initializes storage,
        // creates baseline strategy, and wires all components together.
        //
        // Preconditions:
        //   - Config file must be readable (or use defaults if missing)
        //
        // Postconditions:
        //   - On success: all components initialized and ready to run
        //   - On failure: returns error message
        //
        [[nodiscard]] auto initialize() -> std::expected<void, std::string>;

        //
        // Run the daemon main loop.
        //
        // Starts monitors and dispatchers, sets up signal handlers,
        // and runs until a stop signal is received.
        //
        // This function blocks until the daemon is stopped.
        //
        // Preconditions:
        //   - initialize() must have been called successfully
        //
        [[nodiscard]] auto run() -> std::expected<void, std::string>;

        //
        // Request daemon shutdown.
        //
        // Sets the stop flag, which will cause run() to exit gracefully.
        // Can be called from signal handlers.
        //
        void stop();

        //
        // Reload configuration.
        //
        // Reloads config file and updates components as needed.
        // Called in response to SIGHUP.
        //
        auto reload_config() -> std::expected<void, std::string>;

    private:
        //
        // Setup Unix signal handlers.
        //
        void setup_signal_handlers();

        //
        // Create baseline strategy based on detected distro type.
        //
        [[nodiscard]] auto create_strategy(DistroInfo const& distro)
            -> std::unique_ptr<BaselineStrategy>;

        //
        // Initialize user monitoring.
        //
        // Discovers users, evaluates policy, loads user configs, and sets up
        // user path monitoring.
        //
        [[nodiscard]] auto initialize_user_monitoring()
            -> std::expected<void, std::string>;

        //
        // Scan user baselines.
        //
        // Creates initial baselines for monitored users.
        //
        [[nodiscard]] auto scan_user_baselines()
            -> std::expected<void, std::string>;

        std::filesystem::path m_config_path;
        Config m_config;
        DistroInfo m_distro;

        // Core storage
        std::unique_ptr<Database> m_database;
        std::unique_ptr<BaselineStore> m_baseline_store;
        std::unique_ptr<AlertStore> m_alert_store;

        // Event system
        std::unique_ptr<EventBus> m_event_bus;
        std::unique_ptr<PolicyEngine> m_policy_engine;

        // Components
        std::unique_ptr<BaselineStrategy> m_strategy;
        std::unique_ptr<Scanner> m_scanner;
        std::unique_ptr<FanotifyMonitor> m_fanotify_monitor;
        std::unique_ptr<AlertDispatcher> m_alert_dispatcher;

        // Phase 2 components
        std::unique_ptr<DbusNotifier> m_dbus_notifier;
        std::unique_ptr<JournalMonitor> m_journal_monitor;
        std::unique_ptr<CorrelationEngine> m_correlation_engine;

        // Phase 3 components
        std::unique_ptr<AuditMonitor> m_audit_monitor;
        std::unique_ptr<AuditEventStore> m_audit_event_store;

        // User monitoring
        std::unique_ptr<UserManager> m_user_manager;
        std::vector<UserInfo> m_monitored_users;

        // Lifecycle control
        std::atomic<bool> m_running{false};
        std::atomic<bool> m_should_stop{false};
        std::atomic<bool> m_should_reload{false};
    };

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_DAEMON_DAEMON_H
