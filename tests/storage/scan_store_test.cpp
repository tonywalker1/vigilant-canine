//
// vigilant-canine - Scan Store Tests
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <storage/database.h>
#include <storage/scan_store.h>

#include <gtest/gtest.h>

#include <filesystem>

namespace vigilant_canine {

    class ScanStoreTest : public ::testing::Test {
    protected:
        void SetUp() override {
            // Create temporary database path
            m_db_path = std::filesystem::temp_directory_path() / "vc_scan_test.sqlite";

            // Remove if exists from previous run
            if (std::filesystem::exists(m_db_path)) {
                std::filesystem::remove(m_db_path);
            }

            // Open database
            auto db_result = Database::open(m_db_path);
            ASSERT_TRUE(db_result.has_value());
            m_db = std::make_unique<Database>(std::move(*db_result));

            // Create store
            m_store = std::make_unique<ScanStore>(*m_db);
        }

        void TearDown() override {
            m_store.reset();
            m_db.reset();

            // Clean up
            if (std::filesystem::exists(m_db_path)) {
                std::filesystem::remove(m_db_path);
            }
        }

        void insert_scan_with_age(int days_ago, const std::string& scan_type) {
            // Insert scan with backdated timestamp
            auto stmt_result = m_db->prepare(R"(
                INSERT INTO scans (scan_type, started_at, finished_at, files_checked, changes_found, status)
                VALUES (?, datetime('now', '-' || ? || ' days'), datetime('now', '-' || ? || ' days'), 100, 0, 'completed')
            )");

            ASSERT_TRUE(stmt_result.has_value());

            sqlite3_stmt* stmt = *stmt_result;
            sqlite3_bind_text(stmt, 1, scan_type.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 2, days_ago);
            sqlite3_bind_int(stmt, 3, days_ago);

            int rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            ASSERT_EQ(rc, SQLITE_DONE);
        }

        int count_scans() {
            auto stmt_result = m_db->prepare("SELECT COUNT(*) FROM scans");
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
        std::unique_ptr<ScanStore> m_store;
    };

    TEST_F(ScanStoreTest, PruneOldScans) {
        // Insert scans at different ages
        insert_scan_with_age(30, "boot");
        insert_scan_with_age(100, "periodic");
        insert_scan_with_age(50, "manual");

        EXPECT_EQ(count_scans(), 3);

        // Prune scans older than 90 days
        auto prune_result = m_store->prune_old_scans(90);

        ASSERT_TRUE(prune_result.has_value())
            << "Prune failed: " << prune_result.error();

        // Should have only the 30-day and 50-day old scans
        EXPECT_EQ(count_scans(), 2);
    }

    TEST_F(ScanStoreTest, PruneOldScansWithDifferentRetention) {
        // Insert scans
        insert_scan_with_age(10, "scan1");
        insert_scan_with_age(20, "scan2");
        insert_scan_with_age(40, "scan3");
        insert_scan_with_age(60, "scan4");

        EXPECT_EQ(count_scans(), 4);

        // Prune scans older than 30 days
        auto prune_result = m_store->prune_old_scans(30);
        ASSERT_TRUE(prune_result.has_value());

        // Should keep scans from last 30 days (10 and 20 day old)
        EXPECT_EQ(count_scans(), 2);
    }

    TEST_F(ScanStoreTest, PruneOldScansZeroDays) {
        // Insert scans
        insert_scan_with_age(1, "scan1");
        insert_scan_with_age(5, "scan2");

        EXPECT_EQ(count_scans(), 2);

        // Prune with 0 days should prune everything
        auto prune_result = m_store->prune_old_scans(0);
        ASSERT_TRUE(prune_result.has_value());

        EXPECT_EQ(count_scans(), 0);
    }

    TEST_F(ScanStoreTest, PruneNoScansToRemove) {
        // Insert only recent scans
        insert_scan_with_age(5, "scan1");
        insert_scan_with_age(10, "scan2");

        EXPECT_EQ(count_scans(), 2);

        // Prune scans older than 90 days
        auto prune_result = m_store->prune_old_scans(90);
        ASSERT_TRUE(prune_result.has_value());

        // All scans should remain
        EXPECT_EQ(count_scans(), 2);
    }

}  // namespace vigilant_canine
