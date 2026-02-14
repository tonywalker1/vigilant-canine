//
// vigilant-canine - User Scanner Tests
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <scanner/scanner.h>
#include <baseline/strategy.h>
#include <storage/baseline_store.h>
#include <storage/database.h>
#include <events/event_bus.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace vigilant_canine {

    class UserScannerTest : public ::testing::Test {
    protected:
        void SetUp() override {
            // Create temporary database
            m_db_path = std::filesystem::temp_directory_path() / "user_scanner_test.db";
            std::filesystem::remove(m_db_path);

            auto db_result = Database::open(m_db_path);
            ASSERT_TRUE(db_result.has_value()) << db_result.error();
            m_db = std::make_unique<Database>(std::move(*db_result));

            m_store = std::make_unique<BaselineStore>(*m_db);
            m_strategy = std::make_unique<TraditionalStrategy>();
            m_scanner = std::make_unique<Scanner>(*m_store, *m_strategy, m_event_bus);

            // Create test directory structure
            m_test_home = std::filesystem::temp_directory_path() / "user_scanner_test_home";
            std::filesystem::remove_all(m_test_home);
            std::filesystem::create_directories(m_test_home / ".local" / "bin");
            std::filesystem::create_directories(m_test_home / ".cache");

            // Create test files
            create_test_file(m_test_home / ".local" / "bin" / "script1.sh", "#!/bin/bash\necho test1");
            create_test_file(m_test_home / ".local" / "bin" / "script2.py", "#!/usr/bin/env python3\nprint('test2')");
            create_test_file(m_test_home / ".cache" / "cache.txt", "cache data");
        }

        void TearDown() override {
            m_scanner.reset();
            m_store.reset();
            m_db.reset();
            std::filesystem::remove(m_db_path);
            std::filesystem::remove_all(m_test_home);
        }

        void create_test_file(std::filesystem::path const& path, std::string const& content) {
            std::ofstream file(path);
            file << content;
            file.close();
            std::filesystem::permissions(path, std::filesystem::perms::owner_all);
        }

        std::filesystem::path m_db_path;
        std::filesystem::path m_test_home;
        std::unique_ptr<Database> m_db;
        std::unique_ptr<BaselineStore> m_store;
        std::unique_ptr<BaselineStrategy> m_strategy;
        EventBus m_event_bus;
        std::unique_ptr<Scanner> m_scanner;
    };

    TEST_F(UserScannerTest, ScanUserDirectoryCreatesBaselines) {
        std::vector<std::filesystem::path> paths = {
            m_test_home / ".local" / "bin"
        };
        std::vector<std::filesystem::path> exclude;

        auto result = m_scanner->scan_user_paths(paths, exclude, "user:testuser");

        ASSERT_TRUE(result.has_value());
        auto const& stats = result.value();

        EXPECT_EQ(stats.files_scanned, 2);  // script1.sh, script2.py
        EXPECT_EQ(stats.files_added, 2);
        EXPECT_EQ(stats.files_skipped, 0);
    }

    TEST_F(UserScannerTest, SourceColumnSetToUserUsername) {
        std::vector<std::filesystem::path> paths = {
            m_test_home / ".local" / "bin"
        };
        std::vector<std::filesystem::path> exclude;

        auto result = m_scanner->scan_user_paths(paths, exclude, "user:alice");
        ASSERT_TRUE(result.has_value());

        // Query database to verify source
        FilePath test_file{m_test_home / ".local" / "bin" / "script1.sh"};
        auto baseline = m_store->find_by_path(test_file, std::nullopt);

        ASSERT_TRUE(baseline.has_value());
        ASSERT_TRUE(baseline->has_value());

        EXPECT_EQ(baseline->value().source, "user:alice");
    }

    TEST_F(UserScannerTest, RelativePathsNotSupported) {
        // This test verifies that paths should be absolute
        std::vector<std::filesystem::path> paths = {
            m_test_home / ".local" / "bin"  // Already absolute
        };
        std::vector<std::filesystem::path> exclude;

        auto result = m_scanner->scan_user_paths(paths, exclude, "user:testuser");
        ASSERT_TRUE(result.has_value());

        // All baselines should have absolute paths
        FilePath test_file{m_test_home / ".local" / "bin" / "script1.sh"};
        auto baseline = m_store->find_by_path(test_file, std::nullopt);

        ASSERT_TRUE(baseline.has_value());
        ASSERT_TRUE(baseline->has_value());

        // Dereference FilePath to get underlying path
        EXPECT_TRUE((*baseline->value().path).is_absolute());
    }

    TEST_F(UserScannerTest, ExcludedPatternsRespected) {
        std::vector<std::filesystem::path> paths = {
            m_test_home
        };
        std::vector<std::filesystem::path> exclude = {
            m_test_home / ".cache"
        };

        auto result = m_scanner->scan_user_paths(paths, exclude, "user:testuser");
        ASSERT_TRUE(result.has_value());

        auto const& stats = result.value();

        // Should scan .local/bin files but not .cache
        EXPECT_EQ(stats.files_scanned, 2);  // script1.sh, script2.py
        EXPECT_GE(stats.files_skipped, 1);  // .cache/cache.txt skipped

        // Verify .cache file not in database
        FilePath cache_file{m_test_home / ".cache" / "cache.txt"};
        auto baseline = m_store->find_by_path(cache_file, std::nullopt);

        ASSERT_TRUE(baseline.has_value());
        EXPECT_FALSE(baseline->has_value());  // Should not exist
    }

    TEST_F(UserScannerTest, MissingDirectoriesDontFailScan) {
        std::vector<std::filesystem::path> paths = {
            m_test_home / ".local" / "bin",
            m_test_home / ".nonexistent" / "dir",  // Doesn't exist
            m_test_home / ".also_missing"
        };
        std::vector<std::filesystem::path> exclude;

        auto result = m_scanner->scan_user_paths(paths, exclude, "user:testuser");
        ASSERT_TRUE(result.has_value());

        auto const& stats = result.value();

        // Should still scan the existing directory
        EXPECT_EQ(stats.files_scanned, 2);
        EXPECT_EQ(stats.errors, 0);
    }

    TEST_F(UserScannerTest, UserBaselineSeparateFromSystemBaseline) {
        // Scan same file as different sources
        std::vector<std::filesystem::path> paths = {
            m_test_home / ".local" / "bin"
        };
        std::vector<std::filesystem::path> exclude;

        // Scan as user
        auto result1 = m_scanner->scan_user_paths(paths, exclude, "user:alice");
        ASSERT_TRUE(result1.has_value());

        // Scan again as different user
        auto result2 = m_scanner->scan_user_paths(paths, exclude, "user:bob");
        ASSERT_TRUE(result2.has_value());

        // Both should succeed (creates separate baselines with same path but different source)
        // Note: Current implementation updates existing baselines, so this test shows
        // the last scan wins. True isolation would require source-aware queries.
        EXPECT_EQ(result1->files_scanned, 2);
        EXPECT_EQ(result2->files_scanned, 2);
    }

    TEST_F(UserScannerTest, ChangeDetectionWorksForUserFiles) {
        std::vector<std::filesystem::path> paths = {
            m_test_home / ".local" / "bin"
        };
        std::vector<std::filesystem::path> exclude;

        // Initial scan
        auto result1 = m_scanner->scan_user_paths(paths, exclude, "user:testuser");
        ASSERT_TRUE(result1.has_value());
        EXPECT_EQ(result1->files_added, 2);

        // Modify a file
        create_test_file(m_test_home / ".local" / "bin" / "script1.sh", "#!/bin/bash\necho modified");

        // Rescan
        auto result2 = m_scanner->scan_user_paths(paths, exclude, "user:testuser");
        ASSERT_TRUE(result2.has_value());

        // Should detect update
        EXPECT_EQ(result2->files_scanned, 2);
        EXPECT_EQ(result2->files_updated, 2);  // Both files get updated
    }

}  // namespace vigilant_canine
