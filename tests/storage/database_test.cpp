//
// vigilant-canine - Database Tests
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <storage/database.h>
#include <storage/schema.h>

#include <gtest/gtest.h>

#include <filesystem>

namespace vigilant_canine {

    class DatabaseTest : public ::testing::Test {
    protected:
        void SetUp() override {
            // Create temporary database path
            m_db_path = std::filesystem::temp_directory_path() / "vc_test_db.sqlite";

            // Remove if exists from previous run
            if (std::filesystem::exists(m_db_path)) {
                std::filesystem::remove(m_db_path);
            }
        }

        void TearDown() override {
            // Clean up
            if (std::filesystem::exists(m_db_path)) {
                std::filesystem::remove(m_db_path);
            }
        }

        std::filesystem::path m_db_path;
    };

    TEST_F(DatabaseTest, OpenNewDatabase) {
        auto db_result = Database::open(m_db_path);

        ASSERT_TRUE(db_result.has_value());
        EXPECT_TRUE(std::filesystem::exists(m_db_path));
    }

    TEST_F(DatabaseTest, OpenExistingDatabase) {
        // Create database
        {
            auto db_result = Database::open(m_db_path);
            ASSERT_TRUE(db_result.has_value());
        }

        // Reopen
        auto db_result = Database::open(m_db_path);
        ASSERT_TRUE(db_result.has_value());
    }

    TEST_F(DatabaseTest, ExecuteSimpleStatement) {
        auto db_result = Database::open(m_db_path);
        ASSERT_TRUE(db_result.has_value()) << "Failed to open database: " << (db_result.has_value() ? "" : db_result.error());

        auto& db = db_result.value();
        auto result = db.execute("CREATE TABLE test (id INTEGER PRIMARY KEY, value TEXT)");

        EXPECT_TRUE(result.has_value()) << "Execute failed: " << (result.has_value() ? "" : result.error());
    }

    TEST_F(DatabaseTest, PrepareAndExecuteStatement) {
        auto db_result = Database::open(m_db_path);
        ASSERT_TRUE(db_result.has_value());

        auto& db = db_result.value();

        // Create table
        auto create_result = db.execute("CREATE TABLE test (id INTEGER PRIMARY KEY, value TEXT)");
        ASSERT_TRUE(create_result.has_value());

        // Insert using prepared statement
        auto stmt_result = db.prepare("INSERT INTO test (value) VALUES (?)");
        ASSERT_TRUE(stmt_result.has_value());

        sqlite3_stmt* stmt = stmt_result.value();
        sqlite3_bind_text(stmt, 1, "test_value", -1, SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt);
        EXPECT_EQ(rc, SQLITE_DONE);

        sqlite3_finalize(stmt);

        // Verify insert
        auto id = db.last_insert_rowid();
        EXPECT_GT(id, 0);
    }

    TEST_F(DatabaseTest, SchemaVersionSet) {
        auto db_result = Database::open(m_db_path);
        ASSERT_TRUE(db_result.has_value());

        auto& db = db_result.value();

        // Check schema version was set
        auto stmt_result = db.prepare("SELECT version FROM schema_version");
        ASSERT_TRUE(stmt_result.has_value());

        sqlite3_stmt* stmt = stmt_result.value();
        int rc = sqlite3_step(stmt);

        ASSERT_EQ(rc, SQLITE_ROW);
        int version = sqlite3_column_int(stmt, 0);
        EXPECT_EQ(version, schema::CURRENT_VERSION);

        sqlite3_finalize(stmt);
    }

    TEST_F(DatabaseTest, AllTablesCreated) {
        auto db_result = Database::open(m_db_path);
        ASSERT_TRUE(db_result.has_value());

        auto& db = db_result.value();

        // Verify all expected tables exist
        std::vector<std::string> expected_tables = {
            "schema_version",
            "baselines",
            "alerts",
            "scans"
        };

        for (auto const& table : expected_tables) {
            auto stmt_result = db.prepare(
                "SELECT name FROM sqlite_master WHERE type='table' AND name=?");
            ASSERT_TRUE(stmt_result.has_value());

            sqlite3_stmt* stmt = stmt_result.value();
            sqlite3_bind_text(stmt, 1, table.c_str(), -1, SQLITE_TRANSIENT);

            int rc = sqlite3_step(stmt);
            EXPECT_EQ(rc, SQLITE_ROW) << "Table " << table << " not found";

            sqlite3_finalize(stmt);
        }
    }

}  // namespace vigilant_canine
