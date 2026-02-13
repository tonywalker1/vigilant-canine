//
// vigilant-canine - Scanner Tests
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <scanner/scanner.h>

#include <baseline/strategy.h>
#include <events/event_bus.h>
#include <storage/baseline_store.h>
#include <storage/database.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace vigilant_canine {

    // Test strategy with no exclusions for easier testing
    class TestStrategy : public BaselineStrategy {
    public:
        auto get_monitor_paths() const -> MonitorPaths override {
            MonitorPaths paths;
            paths.critical = {"/usr"};
            paths.config = {"/etc"};
            // Empty exclude list for testing
            paths.exclude = {};
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

    class ScannerTest : public ::testing::Test {
    protected:
        void SetUp() override {
            // Create temporary database
            m_db_path = std::filesystem::temp_directory_path() / "vc_scanner_test.sqlite";

            if (std::filesystem::exists(m_db_path)) {
                std::filesystem::remove(m_db_path);
            }

            auto db_result = Database::open(m_db_path);
            ASSERT_TRUE(db_result.has_value()) << db_result.error();
            m_db = std::make_unique<Database>(std::move(*db_result));

            m_store = std::make_unique<BaselineStore>(*m_db);
            m_strategy = std::make_unique<TestStrategy>();
            m_event_bus = std::make_unique<EventBus>();

            // Create test directory in current directory (not in exclude list)
            m_test_dir = std::filesystem::current_path() / "test_scanner_files";
            std::filesystem::create_directories(m_test_dir);
        }

        void TearDown() override {
            m_store.reset();
            m_strategy.reset();

            if (std::filesystem::exists(m_db_path)) {
                std::filesystem::remove(m_db_path);
            }

            if (std::filesystem::exists(m_test_dir)) {
                std::filesystem::remove_all(m_test_dir);
            }
        }

        void create_test_file(std::filesystem::path const& name, std::string const& content) {
            auto path = m_test_dir / name;
            std::ofstream file{path};
            file << content;
        }

        std::filesystem::path m_db_path;
        std::filesystem::path m_test_dir;
        std::unique_ptr<Database> m_db;
        std::unique_ptr<BaselineStore> m_store;
        std::unique_ptr<BaselineStrategy> m_strategy;
        std::unique_ptr<EventBus> m_event_bus;
    };

    TEST_F(ScannerTest, ScanSingleFile) {
        create_test_file("test.txt", "Hello, World!");

        Scanner scanner{*m_store, *m_strategy, *m_event_bus};

        auto file_path = FilePath{m_test_dir / "test.txt"};
        auto result = scanner.scan_file(file_path);

        ASSERT_TRUE(result.has_value()) << result.error();

        // Verify baseline was created
        auto baseline = m_store->find_by_path(file_path, std::nullopt);
        ASSERT_TRUE(baseline.has_value());
        ASSERT_TRUE(baseline->has_value());

        auto const& bl = baseline->value();
        EXPECT_EQ(*bl.path, m_test_dir / "test.txt");
        EXPECT_FALSE((*bl.hash_value).empty());
        EXPECT_EQ(bl.size, 13);  // "Hello, World!" is 13 bytes
    }

    TEST_F(ScannerTest, ScanDirectory) {
        create_test_file("file1.txt", "Content 1");
        create_test_file("file2.txt", "Content 2");

        // Create subdirectory with file
        std::filesystem::create_directory(m_test_dir / "subdir");
        create_test_file("subdir/file3.txt", "Content 3");

        Scanner scanner{*m_store, *m_strategy, *m_event_bus};

        auto result = scanner.scan_directory(m_test_dir);

        ASSERT_TRUE(result.has_value()) << result.error();

        auto const& stats = *result;

        // Debug output
        std::cout << "Files scanned: " << stats.files_scanned << "\n";
        std::cout << "Files skipped: " << stats.files_skipped << "\n";
        std::cout << "Errors: " << stats.errors << "\n";

        EXPECT_EQ(stats.files_scanned, 3);
        EXPECT_EQ(stats.errors, 0);
    }

    TEST_F(ScannerTest, VerifyUnchangedFile) {
        create_test_file("test.txt", "Original content");

        Scanner scanner{*m_store, *m_strategy, *m_event_bus};

        // Create baseline
        auto file_path = FilePath{m_test_dir / "test.txt"};
        auto scan_result = scanner.scan_file(file_path);
        ASSERT_TRUE(scan_result.has_value());

        // Verify (should find no changes)
        auto verify_result = scanner.verify_baselines(m_test_dir);
        ASSERT_TRUE(verify_result.has_value());

        auto const& changes = *verify_result;
        EXPECT_EQ(changes.size(), 0);
    }

    TEST_F(ScannerTest, DetectModifiedFile) {
        create_test_file("test.txt", "Original content");

        Scanner scanner{*m_store, *m_strategy, *m_event_bus};

        // Create baseline
        auto file_path = FilePath{m_test_dir / "test.txt"};
        auto scan_result = scanner.scan_file(file_path);
        ASSERT_TRUE(scan_result.has_value());

        // Modify file
        std::ofstream file{m_test_dir / "test.txt"};
        file << "Modified content";
        file.close();

        // Verify (should detect change)
        auto verify_result = scanner.verify_baselines(m_test_dir);
        ASSERT_TRUE(verify_result.has_value());

        auto const& changes = *verify_result;
        ASSERT_EQ(changes.size(), 1);
        EXPECT_EQ(*changes[0].path, m_test_dir / "test.txt");
        EXPECT_EQ(changes[0].change_type, "modified");
    }

    TEST_F(ScannerTest, DetectNewFile) {
        Scanner scanner{*m_store, *m_strategy, *m_event_bus};

        // Create file after scanner initialization (no baseline)
        create_test_file("new_file.txt", "New content");

        // Verify (should detect new file)
        auto verify_result = scanner.verify_baselines(m_test_dir);
        ASSERT_TRUE(verify_result.has_value());

        auto const& changes = *verify_result;
        ASSERT_EQ(changes.size(), 1);
        EXPECT_EQ(*changes[0].path, m_test_dir / "new_file.txt");
        EXPECT_EQ(changes[0].change_type, "new");
    }

    TEST_F(ScannerTest, PublishesEventsOnScan) {
        // Subscribe to events
        int event_count = 0;
        auto sub = m_event_bus->subscribe([&](Event const& event) {
            event_count++;
            // Verify event has correct source
            EXPECT_EQ(event.source, "scanner");
        });

        create_test_file("file1.txt", "Content 1");

        Scanner scanner{*m_store, *m_strategy, *m_event_bus};
        auto result = scanner.scan_directory(m_test_dir);

        ASSERT_TRUE(result.has_value());

        // Should have published ScanCompletedEvent
        EXPECT_GE(event_count, 1);
    }

    TEST_F(ScannerTest, PublishesFileModifiedEvent) {
        create_test_file("test.txt", "Original content");

        // Track events
        std::vector<std::string> event_types;
        auto sub = m_event_bus->subscribe([&](Event const& event) {
            event_types.push_back(std::string{event_type_name(event.data)});
        });

        Scanner scanner{*m_store, *m_strategy, *m_event_bus};

        // Create baseline
        auto file_path = FilePath{m_test_dir / "test.txt"};
        auto scan_result = scanner.scan_file(file_path);
        ASSERT_TRUE(scan_result.has_value());

        // Modify file
        std::ofstream file{m_test_dir / "test.txt"};
        file << "Modified content";
        file.close();

        // Verify - should publish FileModifiedEvent
        auto verify_result = scanner.verify_baselines(m_test_dir);
        ASSERT_TRUE(verify_result.has_value());

        // Should have FileModifiedEvent in the event types
        auto it = std::find(event_types.begin(), event_types.end(), "FileModified");
        EXPECT_NE(it, event_types.end()) << "FileModifiedEvent should have been published";
    }

}  // namespace vigilant_canine
