//
// vigilant-canine - Journal Event Store Tests
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <storage/database.h>
#include <storage/journal_event_store.h>

#include <gtest/gtest.h>

#include <filesystem>

namespace vigilant_canine {

    class JournalEventStoreTest : public ::testing::Test {
    protected:
        void SetUp() override {
            // Create temporary database path
            m_db_path = std::filesystem::temp_directory_path() / "vc_journal_test.sqlite";

            // Remove if exists from previous run
            if (std::filesystem::exists(m_db_path)) {
                std::filesystem::remove(m_db_path);
            }

            // Open database
            auto db_result = Database::open(m_db_path);
            ASSERT_TRUE(db_result.has_value());
            m_db = std::make_unique<Database>(std::move(*db_result));

            // Create store
            m_store = std::make_unique<JournalEventStore>(*m_db);
        }

        void TearDown() override {
            m_store.reset();
            m_db.reset();

            // Clean up
            if (std::filesystem::exists(m_db_path)) {
                std::filesystem::remove(m_db_path);
            }
        }

        void insert_journal_event_with_age(int days_ago, const std::string& rule_name) {
            // Insert event with backdated timestamp
            auto stmt_result = m_db->prepare(R"(
                INSERT INTO journal_events (rule_name, message, priority, created_at)
                VALUES (?, 'Test message', 6, datetime('now', '-' || ? || ' days'))
            )");

            ASSERT_TRUE(stmt_result.has_value());

            sqlite3_stmt* stmt = *stmt_result;
            sqlite3_bind_text(stmt, 1, rule_name.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 2, days_ago);

            int rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            ASSERT_EQ(rc, SQLITE_DONE);
        }

        int count_journal_events() {
            auto stmt_result = m_db->prepare("SELECT COUNT(*) FROM journal_events");
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
        std::unique_ptr<JournalEventStore> m_store;
    };

    TEST_F(JournalEventStoreTest, GetRecentEvents) {
        // Insert some events
        insert_journal_event_with_age(0, "rule1");
        insert_journal_event_with_age(1, "rule2");
        insert_journal_event_with_age(2, "rule3");

        // Get recent events
        auto result = m_store->get_recent(10);

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->size(), 3);

        // Most recent first
        EXPECT_EQ(result->at(0).rule_name, "rule1");
        EXPECT_EQ(result->at(1).rule_name, "rule2");
        EXPECT_EQ(result->at(2).rule_name, "rule3");
    }

    TEST_F(JournalEventStoreTest, GetRecentEventsWithLimit) {
        // Insert events
        for (int i = 0; i < 10; i++) {
            insert_journal_event_with_age(i, "rule" + std::to_string(i));
        }

        // Get recent events with limit
        auto result = m_store->get_recent(5);

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->size(), 5);
    }

    TEST_F(JournalEventStoreTest, PruneOldEvents) {
        // Insert events at different ages
        insert_journal_event_with_age(10, "old_rule");
        insert_journal_event_with_age(40, "very_old_rule");
        insert_journal_event_with_age(5, "recent_rule");

        EXPECT_EQ(count_journal_events(), 3);

        // Prune events older than 30 days
        auto prune_result = m_store->prune_old_events(30);

        ASSERT_TRUE(prune_result.has_value())
            << "Prune failed: " << prune_result.error();

        // Should have only the 10-day and 5-day old events
        EXPECT_EQ(count_journal_events(), 2);

        // Verify the correct events remain
        auto get_result = m_store->get_recent(10);
        ASSERT_TRUE(get_result.has_value());

        bool found_recent = false;
        bool found_old = false;
        bool found_very_old = false;

        for (const auto& event : *get_result) {
            if (event.rule_name == "recent_rule") found_recent = true;
            if (event.rule_name == "old_rule") found_old = true;
            if (event.rule_name == "very_old_rule") found_very_old = true;
        }

        EXPECT_TRUE(found_recent);
        EXPECT_TRUE(found_old);
        EXPECT_FALSE(found_very_old);
    }

    TEST_F(JournalEventStoreTest, PruneOldEventsZeroDays) {
        // Insert events
        insert_journal_event_with_age(10, "rule1");
        insert_journal_event_with_age(40, "rule2");

        EXPECT_EQ(count_journal_events(), 2);

        // Prune with 0 days should prune everything
        auto prune_result = m_store->prune_old_events(0);
        ASSERT_TRUE(prune_result.has_value());

        EXPECT_EQ(count_journal_events(), 0);
    }

}  // namespace vigilant_canine
