//
// vigilant-canine - Fanotify Monitor Tests
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <monitor/fanotify_monitor.h>

#include <baseline/strategy.h>
#include <events/event_bus.h>
#include <storage/baseline_store.h>
#include <storage/database.h>

#include <gtest/gtest.h>

#include <filesystem>

namespace vigilant_canine {

    // Test strategy with limited paths for testing
    class TestMonitorStrategy : public BaselineStrategy {
    public:
        auto get_monitor_paths() const -> MonitorPaths override {
            MonitorPaths paths;
            // Use current directory for testing
            paths.critical = {std::filesystem::current_path()};
            paths.config = {};
            paths.exclude = {"/tmp", "/var/tmp"};
            return paths;
        }

        auto get_file_source(std::filesystem::path const&) const
            -> std::expected<std::optional<BaselineSource>, std::string> override {
            return BaselineSource{"test"};
        }

        auto get_deployment_id() const -> std::optional<std::string> override {
            return std::nullopt;
        }
    };

    class FanotifyMonitorTest : public ::testing::Test {
    protected:
        void SetUp() override {
            m_db_path = std::filesystem::temp_directory_path() / "vc_fanotify_test.sqlite";

            if (std::filesystem::exists(m_db_path)) {
                std::filesystem::remove(m_db_path);
            }

            auto db_result = Database::open(m_db_path);
            ASSERT_TRUE(db_result.has_value());
            m_db = std::make_unique<Database>(std::move(*db_result));

            m_store = std::make_unique<BaselineStore>(*m_db);
            m_strategy = std::make_unique<TestMonitorStrategy>();
            m_event_bus = std::make_unique<EventBus>();
        }

        void TearDown() override {
            m_store.reset();
            m_strategy.reset();
            m_event_bus.reset();
            m_db.reset();

            if (std::filesystem::exists(m_db_path)) {
                std::filesystem::remove(m_db_path);
            }
        }

        std::filesystem::path m_db_path;
        std::unique_ptr<Database> m_db;
        std::unique_ptr<BaselineStore> m_store;
        std::unique_ptr<BaselineStrategy> m_strategy;
        std::unique_ptr<EventBus> m_event_bus;
    };

    TEST_F(FanotifyMonitorTest, ConstructAndDestroy) {
        // Just verify we can construct and destroy the monitor
        FanotifyMonitor monitor{*m_store, *m_strategy, *m_event_bus};

        EXPECT_FALSE(monitor.is_running());
    }

    TEST_F(FanotifyMonitorTest, InitializeWithoutPrivileges) {
        FanotifyMonitor monitor{*m_store, *m_strategy, *m_event_bus};

        // This will likely fail without root/capabilities, but should not crash
        auto result = monitor.initialize();

        if (!result) {
            // Expected failure without privileges
            std::cout << "Initialize failed (expected without privileges): "
                      << result.error() << "\n";
        } else {
            // If it succeeds (e.g., running as root), that's fine too
            std::cout << "Initialize succeeded (running with privileges)\n";
        }

        EXPECT_FALSE(monitor.is_running());
    }

    TEST_F(FanotifyMonitorTest, CannotStartWithoutInitialize) {
        FanotifyMonitor monitor{*m_store, *m_strategy, *m_event_bus};

        // Should fail because not initialized
        auto result = monitor.start();

        EXPECT_FALSE(result.has_value());
        if (!result) {
            EXPECT_NE(result.error().find("not initialized"), std::string::npos);
        }
    }

    TEST_F(FanotifyMonitorTest, MoveConstruction) {
        FanotifyMonitor monitor1{*m_store, *m_strategy, *m_event_bus};

        // Move construct
        FanotifyMonitor monitor2{std::move(monitor1)};

        EXPECT_FALSE(monitor2.is_running());
    }

}  // namespace vigilant_canine
