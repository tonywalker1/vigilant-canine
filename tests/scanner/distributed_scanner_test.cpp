//
// vigilant-canine - Distributed Scanner Tests
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <scanner/distributed_scanner.h>

#include <baseline/strategy.h>
#include <config/config.h>
#include <events/event_bus.h>
#include <power/power_monitor.h>
#include <scanner/scanner.h>
#include <storage/baseline_store.h>
#include <storage/database.h>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <thread>

namespace vigilant_canine {

    namespace {

        //
        // Mock strategy for testing.
        //
        class MockStrategy : public BaselineStrategy {
        public:
            auto get_deployment_id() const -> std::optional<std::string> override {
                return "test-deployment";
            }

            auto get_monitor_paths() const -> MonitorPaths override {
                MonitorPaths paths;
                paths.critical = test_paths;
                return paths;
            }

            auto get_file_source(std::filesystem::path const& /*path*/) const
                -> std::expected<std::optional<BaselineSource>, std::string> override {
                return BaselineSource{"test-source"};
            }

            std::vector<std::filesystem::path> test_paths;
        };

        //
        // Test fixture for DistributedScanner.
        //
        class DistributedScannerTest : public ::testing::Test {
        protected:
            void SetUp() override {
                // Create temp directory for test database
                temp_dir = std::filesystem::temp_directory_path() / "vc_dist_scanner_test";
                std::filesystem::create_directories(temp_dir);

                // Create database
                auto db_path = temp_dir / "test.db";
                auto db_result = Database::open(db_path);
                ASSERT_TRUE(db_result.has_value());
                db = std::make_unique<Database>(std::move(*db_result));

                store = std::make_unique<BaselineStore>(*db);

                // Create mock sysfs for power monitor
                sysfs_dir = temp_dir / "sysfs";
                std::filesystem::create_directories(sysfs_dir);
            }

            void TearDown() override {
                std::filesystem::remove_all(temp_dir);
            }

            std::filesystem::path temp_dir;
            std::filesystem::path sysfs_dir;
            std::unique_ptr<Database> db;
            std::unique_ptr<BaselineStore> store;
        };

    }  // anonymous namespace

    TEST_F(DistributedScannerTest, StartStop) {
        EventBus event_bus;
        MockStrategy strategy;
        Scanner scanner(*store, strategy, event_bus);
        PowerMonitor power_monitor(sysfs_dir);

        ScanConfig config;
        config.on_boot = false;
        config.interval_hours = 24;

        DistributedScanner dist_scanner(scanner, *store, strategy, event_bus,
                                        power_monitor, config);

        // Start scanner
        auto start_result = dist_scanner.start();
        ASSERT_TRUE(start_result.has_value());
        EXPECT_TRUE(dist_scanner.is_running());

        // Give it a moment to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Stop scanner
        dist_scanner.stop();
        EXPECT_FALSE(dist_scanner.is_running());
    }

    TEST_F(DistributedScannerTest, CannotStartTwice) {
        EventBus event_bus;
        MockStrategy strategy;
        Scanner scanner(*store, strategy, event_bus);
        PowerMonitor power_monitor(sysfs_dir);

        ScanConfig config;
        DistributedScanner dist_scanner(scanner, *store, strategy, event_bus,
                                        power_monitor, config);

        // Start once
        auto start1 = dist_scanner.start();
        ASSERT_TRUE(start1.has_value());

        // Try to start again - should fail
        auto start2 = dist_scanner.start();
        EXPECT_FALSE(start2.has_value());

        dist_scanner.stop();
    }

    TEST_F(DistributedScannerTest, UpdateConfig) {
        EventBus event_bus;
        MockStrategy strategy;
        Scanner scanner(*store, strategy, event_bus);
        PowerMonitor power_monitor(sysfs_dir);

        ScanConfig config;
        config.interval_hours = 24;

        DistributedScanner dist_scanner(scanner, *store, strategy, event_bus,
                                        power_monitor, config);

        // Update config while stopped - should not crash
        ScanConfig new_config;
        new_config.interval_hours = 12;
        dist_scanner.update_config(new_config);

        // Start and update again
        auto start_result = dist_scanner.start();
        ASSERT_TRUE(start_result.has_value());

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Update while running
        new_config.interval_hours = 6;
        dist_scanner.update_config(new_config);

        dist_scanner.stop();
    }

    TEST_F(DistributedScannerTest, StopInterrupts) {
        EventBus event_bus;
        MockStrategy strategy;
        Scanner scanner(*store, strategy, event_bus);
        PowerMonitor power_monitor(sysfs_dir);

        ScanConfig config;
        config.interval_hours = 24;

        DistributedScanner dist_scanner(scanner, *store, strategy, event_bus,
                                        power_monitor, config);

        auto start_result = dist_scanner.start();
        ASSERT_TRUE(start_result.has_value());

        // Stop should interrupt immediately, not wait for full cycle
        auto start_time = std::chrono::steady_clock::now();
        dist_scanner.stop();
        auto stop_time = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            stop_time - start_time);

        // Should stop within 500ms
        EXPECT_LT(elapsed.count(), 500);
    }

}  // namespace vigilant_canine
