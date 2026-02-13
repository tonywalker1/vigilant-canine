//
// vigilant-canine - Baseline Strategy Tests
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <baseline/strategy.h>

#include <gtest/gtest.h>

#include <filesystem>

namespace vigilant_canine {

    TEST(BaselineStrategyTest, CreateTraditionalStrategy) {
        auto strategy = create_baseline_strategy(DistroType::traditional);
        ASSERT_NE(strategy, nullptr);

        auto paths = strategy->get_monitor_paths();
        EXPECT_FALSE(paths.critical.empty());
        EXPECT_FALSE(paths.config.empty());
        EXPECT_FALSE(paths.exclude.empty());

        // Should not have deployment ID
        EXPECT_FALSE(strategy->get_deployment_id().has_value());
    }

    TEST(BaselineStrategyTest, CreateOstreeStrategy) {
        auto strategy = create_baseline_strategy(DistroType::ostree);
        ASSERT_NE(strategy, nullptr);

        auto paths = strategy->get_monitor_paths();
        EXPECT_FALSE(paths.critical.empty());
        EXPECT_FALSE(paths.config.empty());
        EXPECT_FALSE(paths.exclude.empty());
    }

    TEST(BaselineStrategyTest, CreateBtrfsSnapshotStrategy) {
        auto strategy = create_baseline_strategy(DistroType::btrfs_snapshot);
        ASSERT_NE(strategy, nullptr);

        auto paths = strategy->get_monitor_paths();
        EXPECT_FALSE(paths.critical.empty());
        EXPECT_FALSE(paths.config.empty());
        EXPECT_FALSE(paths.exclude.empty());

        // Should not have deployment ID
        EXPECT_FALSE(strategy->get_deployment_id().has_value());
    }

    TEST(BaselineStrategyTest, TraditionalGetFileSource) {
        auto strategy = create_baseline_strategy(DistroType::traditional);

        // Test with a known system file
        std::filesystem::path test_path = "/usr/bin/bash";

        if (std::filesystem::exists(test_path)) {
            auto result = strategy->get_file_source(test_path);
            ASSERT_TRUE(result.has_value());

            if (result->has_value()) {
                auto const& source = **result;
                std::cout << "Source of " << test_path << ": " << *source << "\n";

                // Should start with "rpm:" or "deb:" depending on distro
                EXPECT_TRUE((*source).starts_with("rpm:") || (*source).starts_with("deb:"));
            }
        } else {
            GTEST_SKIP() << "/usr/bin/bash not found";
        }
    }

    TEST(BaselineStrategyTest, TraditionalPathsIncludeUsrBin) {
        auto strategy = create_baseline_strategy(DistroType::traditional);
        auto paths = strategy->get_monitor_paths();

        // /usr/bin should be in critical paths
        bool found_usr_bin = false;
        for (auto const& path : paths.critical) {
            if (path == "/usr/bin") {
                found_usr_bin = true;
                break;
            }
        }
        EXPECT_TRUE(found_usr_bin);
    }

    TEST(BaselineStrategyTest, TraditionalPathsExcludeVarLog) {
        auto strategy = create_baseline_strategy(DistroType::traditional);
        auto paths = strategy->get_monitor_paths();

        // /var/log should be in exclude paths
        bool found_var_log = false;
        for (auto const& path : paths.exclude) {
            if (path == "/var/log") {
                found_var_log = true;
                break;
            }
        }
        EXPECT_TRUE(found_var_log);
    }

}  // namespace vigilant_canine
