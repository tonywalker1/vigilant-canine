//
// vigilant-canine - Alert Dispatcher Tests
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <dispatch/alert_dispatcher.h>

#include <events/event_bus.h>
#include <storage/alert_store.h>
#include <storage/baseline_store.h>
#include <storage/database.h>

#include <gtest/gtest.h>

#include <filesystem>

namespace vigilant_canine {

    class AlertDispatcherTest : public ::testing::Test {
    protected:
        void SetUp() override {
            m_db_path = std::filesystem::temp_directory_path() / "vc_alert_dispatch_test.sqlite";

            if (std::filesystem::exists(m_db_path)) {
                std::filesystem::remove(m_db_path);
            }

            auto db_result = Database::open(m_db_path);
            ASSERT_TRUE(db_result.has_value());
            m_db = std::make_unique<Database>(std::move(*db_result));

            m_alert_store = std::make_unique<AlertStore>(*m_db);
            m_baseline_store = std::make_unique<BaselineStore>(*m_db);
            m_event_bus = std::make_unique<EventBus>();
        }

        void TearDown() override {
            m_alert_store.reset();
            m_baseline_store.reset();
            m_event_bus.reset();
            m_db.reset();

            if (std::filesystem::exists(m_db_path)) {
                std::filesystem::remove(m_db_path);
            }
        }

        std::filesystem::path m_db_path;
        std::unique_ptr<Database> m_db;
        std::unique_ptr<AlertStore> m_alert_store;
        std::unique_ptr<BaselineStore> m_baseline_store;
        std::unique_ptr<EventBus> m_event_bus;
    };

    TEST_F(AlertDispatcherTest, ConstructAndDestroy) {
        AlertDispatcher dispatcher{*m_event_bus, *m_alert_store, *m_baseline_store};
        EXPECT_FALSE(dispatcher.is_running());
    }

    TEST_F(AlertDispatcherTest, StartAndStop) {
        AlertDispatcher dispatcher{*m_event_bus, *m_alert_store, *m_baseline_store};

        auto result = dispatcher.start();
        ASSERT_TRUE(result.has_value());
        EXPECT_TRUE(dispatcher.is_running());

        dispatcher.stop();
        EXPECT_FALSE(dispatcher.is_running());
    }

    TEST_F(AlertDispatcherTest, CannotStartTwice) {
        AlertDispatcher dispatcher{*m_event_bus, *m_alert_store, *m_baseline_store};

        auto result1 = dispatcher.start();
        ASSERT_TRUE(result1.has_value());

        auto result2 = dispatcher.start();
        EXPECT_FALSE(result2.has_value());
        EXPECT_NE(result2.error().find("already running"), std::string::npos);
    }

    TEST_F(AlertDispatcherTest, DispatchesFileModifiedEvent) {
        // Disable journal logging for tests
        AlertDispatcherConfig config;
        config.log_to_journal = false;

        AlertDispatcher dispatcher{*m_event_bus, *m_alert_store, *m_baseline_store, config};
        auto start_result = dispatcher.start();
        ASSERT_TRUE(start_result.has_value());

        // Publish a file modified event
        FilePath path{std::filesystem::path{"/usr/bin/bash"}};
        HashValue old_hash{"sha256:abc123"};
        HashValue new_hash{"sha256:def456"};

        FileModifiedEvent event_data{
            path,
            old_hash,
            new_hash,
            "File content modified"
        };

        Event event{event_data, EventSeverity::critical, "test"};
        m_event_bus->publish(event);

        // Give dispatcher time to process
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Check that alert was stored
        auto alerts = m_alert_store->get_recent(10);
        ASSERT_TRUE(alerts.has_value());
        ASSERT_EQ(alerts->size(), 1);

        auto const& alert = alerts->at(0);
        EXPECT_EQ(alert.category, "file_modified");
        EXPECT_EQ(alert.severity, AlertSeverity::CRITICAL);
        EXPECT_EQ(alert.source, "test");
        ASSERT_TRUE(alert.path.has_value());
        EXPECT_EQ((**alert.path).string(), "/usr/bin/bash");
        EXPECT_NE(alert.summary.find("File modified"), std::string::npos);
        EXPECT_NE(alert.summary.find("/usr/bin/bash"), std::string::npos);
    }

    TEST_F(AlertDispatcherTest, DispatchesFileCreatedEvent) {
        AlertDispatcherConfig config;
        config.log_to_journal = false;

        AlertDispatcher dispatcher{*m_event_bus, *m_alert_store, *m_baseline_store, config};
        dispatcher.start();

        FilePath path{std::filesystem::path{"/tmp/newfile.txt"}};
        HashValue hash{"sha256:xyz789"};

        FileCreatedEvent event_data{
            path,
            hash,
            "test-package"
        };

        Event event{event_data, EventSeverity::warning, "scanner"};
        m_event_bus->publish(event);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        auto alerts = m_alert_store->get_recent(10);
        ASSERT_TRUE(alerts.has_value());
        ASSERT_EQ(alerts->size(), 1);

        auto const& alert = alerts->at(0);
        EXPECT_EQ(alert.category, "file_created");
        EXPECT_EQ(alert.severity, AlertSeverity::WARNING);
        EXPECT_EQ(alert.source, "scanner");
    }

    TEST_F(AlertDispatcherTest, DispatchesScanCompletedEvent) {
        AlertDispatcherConfig config;
        config.log_to_journal = false;

        AlertDispatcher dispatcher{*m_event_bus, *m_alert_store, *m_baseline_store, config};
        dispatcher.start();

        ScanCompletedEvent event_data{
            std::filesystem::path{"/usr"},
            1000,
            5,
            std::chrono::milliseconds{500}
        };

        Event event{event_data, EventSeverity::info, "scanner"};
        m_event_bus->publish(event);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        auto alerts = m_alert_store->get_recent(10);
        ASSERT_TRUE(alerts.has_value());
        ASSERT_EQ(alerts->size(), 1);

        auto const& alert = alerts->at(0);
        EXPECT_EQ(alert.category, "scan_completed");
        EXPECT_EQ(alert.severity, AlertSeverity::INFO);
        EXPECT_NE(alert.summary.find("Scan completed"), std::string::npos);
        ASSERT_TRUE(alert.details.has_value());
        EXPECT_NE(alert.details->find("1000"), std::string::npos);
        EXPECT_NE(alert.details->find("500ms"), std::string::npos);
    }

    TEST_F(AlertDispatcherTest, MoveConstruction) {
        AlertDispatcher dispatcher1{*m_event_bus, *m_alert_store, *m_baseline_store};
        dispatcher1.start();

        AlertDispatcher dispatcher2{std::move(dispatcher1)};
        EXPECT_TRUE(dispatcher2.is_running());
        EXPECT_FALSE(dispatcher1.is_running());
    }

}  // namespace vigilant_canine
