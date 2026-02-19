//
// vigilant-canine - Alert Store Tests
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <storage/database.h>
#include <storage/alert_store.h>

#include <gtest/gtest.h>

#include <filesystem>

namespace vigilant_canine {

    class AlertStoreTest : public ::testing::Test {
    protected:
        void SetUp() override {
            // Create temporary database path
            m_db_path = std::filesystem::temp_directory_path() / "vc_alert_test.sqlite";

            // Remove if exists from previous run
            if (std::filesystem::exists(m_db_path)) {
                std::filesystem::remove(m_db_path);
            }

            // Open database
            auto db_result = Database::open(m_db_path);
            ASSERT_TRUE(db_result.has_value());
            m_db = std::make_unique<Database>(std::move(*db_result));

            // Create store
            m_store = std::make_unique<AlertStore>(*m_db);
        }

        void TearDown() override {
            m_store.reset();
            m_db.reset();

            // Clean up
            if (std::filesystem::exists(m_db_path)) {
                std::filesystem::remove(m_db_path);
            }
        }

        void insert_alert_with_age(int days_ago, const std::string& summary) {
            // Insert alert with backdated timestamp
            auto stmt_result = m_db->prepare(R"(
                INSERT INTO alerts (severity, category, summary, source, created_at)
                VALUES ('warning', 'test_category', ?, 'test_source', datetime('now', '-' || ? || ' days'))
            )");

            ASSERT_TRUE(stmt_result.has_value());

            sqlite3_stmt* stmt = *stmt_result;
            sqlite3_bind_text(stmt, 1, summary.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 2, days_ago);

            int rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            ASSERT_EQ(rc, SQLITE_DONE);
        }

        int count_alerts() {
            auto stmt_result = m_db->prepare("SELECT COUNT(*) FROM alerts");
            EXPECT_TRUE(stmt_result.has_value());

            sqlite3_stmt* stmt = *stmt_result;
            int count = 0;

            if (sqlite3_step(stmt) == SQLITE_ROW) {
                count = sqlite3_column_int(stmt, 0);
            }

            sqlite3_finalize(stmt);
            return count;
        }

        std::filesystem::path m_db_path;
        std::unique_ptr<Database> m_db;
        std::unique_ptr<AlertStore> m_store;
    };

    TEST_F(AlertStoreTest, InsertAlert) {
        Alert alert;
        alert.severity = AlertSeverity::WARNING;
        alert.category = "test";
        alert.summary = "Test alert";
        alert.source = "test_source";

        auto result = m_store->insert(alert);

        ASSERT_TRUE(result.has_value());
        EXPECT_GT(*result, 0);
    }

    TEST_F(AlertStoreTest, PruneOldAlerts) {
        // Insert alerts at different ages
        insert_alert_with_age(30, "Old alert");
        insert_alert_with_age(100, "Very old alert");
        insert_alert_with_age(50, "Medium alert");

        EXPECT_EQ(count_alerts(), 3);

        // Prune alerts older than 90 days
        auto prune_result = m_store->prune_old_alerts(90);

        ASSERT_TRUE(prune_result.has_value())
            << "Prune failed: " << prune_result.error();

        // Should have only the 30-day and 50-day old alerts
        EXPECT_EQ(count_alerts(), 2);
    }

    TEST_F(AlertStoreTest, PruneOldAlertsWithDifferentRetention) {
        // Insert alerts
        insert_alert_with_age(10, "alert1");
        insert_alert_with_age(20, "alert2");
        insert_alert_with_age(40, "alert3");
        insert_alert_with_age(60, "alert4");

        EXPECT_EQ(count_alerts(), 4);

        // Prune alerts older than 30 days
        auto prune_result = m_store->prune_old_alerts(30);
        ASSERT_TRUE(prune_result.has_value());

        // Should keep alerts from last 30 days (10 and 20 day old)
        EXPECT_EQ(count_alerts(), 2);
    }

    TEST_F(AlertStoreTest, PruneOldAlertsZeroDays) {
        // Insert alerts
        insert_alert_with_age(1, "alert1");
        insert_alert_with_age(5, "alert2");

        EXPECT_EQ(count_alerts(), 2);

        // Prune with 0 days should prune everything
        auto prune_result = m_store->prune_old_alerts(0);
        ASSERT_TRUE(prune_result.has_value());

        EXPECT_EQ(count_alerts(), 0);
    }

    TEST_F(AlertStoreTest, PruneNoAlertsToRemove) {
        // Insert only recent alerts
        insert_alert_with_age(5, "alert1");
        insert_alert_with_age(10, "alert2");

        EXPECT_EQ(count_alerts(), 2);

        // Prune alerts older than 90 days
        auto prune_result = m_store->prune_old_alerts(90);
        ASSERT_TRUE(prune_result.has_value());

        // All alerts should remain
        EXPECT_EQ(count_alerts(), 2);
    }

}  // namespace vigilant_canine
