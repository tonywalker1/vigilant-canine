//
// vigilant-canine - Policy Config Tests
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <config/config.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace vigilant_canine {

    TEST(PolicyConfigTest, ParseMonitorUsers) {
        auto temp_path = std::filesystem::temp_directory_path() / "vc_test_policy_users.toml";
        {
            std::ofstream file(temp_path);
            file << R"(
[policy.home]
monitor_users = ["admin", "developer", "testuser"]
)";
        }

        auto cfg_result = load_config(temp_path);
        ASSERT_TRUE(cfg_result.has_value());

        auto const& cfg = cfg_result.value();
        EXPECT_EQ(cfg.home_policy.monitor_users.size(), 3);
        EXPECT_EQ(cfg.home_policy.monitor_users[0], "admin");
        EXPECT_EQ(cfg.home_policy.monitor_users[1], "developer");
        EXPECT_EQ(cfg.home_policy.monitor_users[2], "testuser");

        std::filesystem::remove(temp_path);
    }

    TEST(PolicyConfigTest, ParseMonitorGroups) {
        auto temp_path = std::filesystem::temp_directory_path() / "vc_test_policy_groups.toml";
        {
            std::ofstream file(temp_path);
            file << R"(
[policy.home]
monitor_groups = ["sudo", "wheel", "administrators"]
)";
        }

        auto cfg_result = load_config(temp_path);
        ASSERT_TRUE(cfg_result.has_value());

        auto const& cfg = cfg_result.value();
        EXPECT_EQ(cfg.home_policy.monitor_groups.size(), 3);
        EXPECT_EQ(cfg.home_policy.monitor_groups[0], "sudo");
        EXPECT_EQ(cfg.home_policy.monitor_groups[1], "wheel");
        EXPECT_EQ(cfg.home_policy.monitor_groups[2], "administrators");

        std::filesystem::remove(temp_path);
    }

    TEST(PolicyConfigTest, ParseMandatoryPaths) {
        auto temp_path = std::filesystem::temp_directory_path() / "vc_test_policy_paths.toml";
        {
            std::ofstream file(temp_path);
            file << R"(
[policy.home]
mandatory_paths = [".ssh", ".gnupg", ".config/systemd"]
)";
        }

        auto cfg_result = load_config(temp_path);
        ASSERT_TRUE(cfg_result.has_value());

        auto const& cfg = cfg_result.value();
        EXPECT_EQ(cfg.home_policy.mandatory_paths.size(), 3);
        EXPECT_EQ(cfg.home_policy.mandatory_paths[0], ".ssh");
        EXPECT_EQ(cfg.home_policy.mandatory_paths[1], ".gnupg");
        EXPECT_EQ(cfg.home_policy.mandatory_paths[2], ".config/systemd");

        std::filesystem::remove(temp_path);
    }

    TEST(PolicyConfigTest, DefaultValuesWhenSectionMissing) {
        auto temp_path = std::filesystem::temp_directory_path() / "vc_test_policy_default.toml";
        {
            std::ofstream file(temp_path);
            file << R"(
[daemon]
log_level = "info"
)";
        }

        auto cfg_result = load_config(temp_path);
        ASSERT_TRUE(cfg_result.has_value());

        auto const& cfg = cfg_result.value();
        EXPECT_TRUE(cfg.home_policy.monitor_users.empty());
        EXPECT_TRUE(cfg.home_policy.monitor_groups.empty());
        EXPECT_TRUE(cfg.home_policy.allow_user_opt_out);  // Default is true
        EXPECT_TRUE(cfg.home_policy.mandatory_paths.empty());

        std::filesystem::remove(temp_path);
    }

    TEST(PolicyConfigTest, ParseAllowUserOptOut) {
        auto temp_path = std::filesystem::temp_directory_path() / "vc_test_policy_optout.toml";
        {
            std::ofstream file(temp_path);
            file << R"(
[policy.home]
monitor_users = ["admin"]
allow_user_opt_out = false
)";
        }

        auto cfg_result = load_config(temp_path);
        ASSERT_TRUE(cfg_result.has_value());

        auto const& cfg = cfg_result.value();
        EXPECT_FALSE(cfg.home_policy.allow_user_opt_out);
        EXPECT_EQ(cfg.home_policy.monitor_users.size(), 1);

        std::filesystem::remove(temp_path);
    }

    TEST(PolicyConfigTest, CompletePolicy) {
        auto temp_path = std::filesystem::temp_directory_path() / "vc_test_policy_complete.toml";
        {
            std::ofstream file(temp_path);
            file << R"(
[policy.home]
monitor_users = ["admin", "developer"]
monitor_groups = ["sudo", "wheel"]
allow_user_opt_out = false
mandatory_paths = [".ssh", ".gnupg"]
)";
        }

        auto cfg_result = load_config(temp_path);
        ASSERT_TRUE(cfg_result.has_value());

        auto const& cfg = cfg_result.value();
        EXPECT_EQ(cfg.home_policy.monitor_users.size(), 2);
        EXPECT_EQ(cfg.home_policy.monitor_groups.size(), 2);
        EXPECT_FALSE(cfg.home_policy.allow_user_opt_out);
        EXPECT_EQ(cfg.home_policy.mandatory_paths.size(), 2);

        std::filesystem::remove(temp_path);
    }

}  // namespace vigilant_canine
