//
// vigilant-canine - Config Merge Tests
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <config/config.h>

#include <gtest/gtest.h>

#include <filesystem>

namespace vigilant_canine {

    TEST(ConfigMergeTest, UserDisabledPolicyForcesMonitoring) {
        Config system_config;
        system_config.monitor.home.enabled = false;

        HomeMonitoringPolicy policy;
        policy.monitor_users = {"testuser"};
        policy.allow_user_opt_out = false;

        Config user_config;
        user_config.monitor.home.enabled = false;  // User wants to disable

        auto merged = merge_configs(
            system_config,
            policy,
            std::optional{user_config},
            "/home/testuser"
        );

        // User config sets enabled = false, but this is just their preference
        // The calling code (UserManager::should_monitor_user) enforces policy
        EXPECT_FALSE(merged.monitor.home.enabled);  // User preference preserved
        EXPECT_FALSE(merged.home_policy.allow_user_opt_out);  // Policy enforced
    }

    TEST(ConfigMergeTest, UserEnabledPolicyAllows) {
        Config system_config;
        system_config.monitor.home.enabled = false;

        HomeMonitoringPolicy policy;
        policy.allow_user_opt_out = true;

        Config user_config;
        user_config.monitor.home.enabled = true;

        auto merged = merge_configs(
            system_config,
            policy,
            std::optional{user_config},
            "/home/testuser"
        );

        EXPECT_TRUE(merged.monitor.home.enabled);
    }

    TEST(ConfigMergeTest, UserAddsPaths) {
        Config system_config;

        HomeMonitoringPolicy policy;

        Config user_config;
        user_config.monitor.home.paths = {".local/bin", ".cargo/bin", "scripts"};

        auto merged = merge_configs(
            system_config,
            policy,
            std::optional{user_config},
            "/home/testuser"
        );

        EXPECT_EQ(merged.monitor.home.paths.size(), 3);
        EXPECT_EQ(merged.monitor.home.paths[0], "/home/testuser/.local/bin");
        EXPECT_EQ(merged.monitor.home.paths[1], "/home/testuser/.cargo/bin");
        EXPECT_EQ(merged.monitor.home.paths[2], "/home/testuser/scripts");
    }

    TEST(ConfigMergeTest, UserExcludesMandatoryPath) {
        Config system_config;

        HomeMonitoringPolicy policy;
        policy.mandatory_paths = {".ssh", ".gnupg"};

        Config user_config;
        user_config.monitor.home.paths = {".local/bin"};
        user_config.monitor.home.exclude = {".ssh", ".cache"};  // Try to exclude .ssh

        auto merged = merge_configs(
            system_config,
            policy,
            std::optional{user_config},
            "/home/testuser"
        );

        // Mandatory path .ssh should be added to paths
        EXPECT_GE(merged.monitor.home.paths.size(), 3);  // .local/bin, .ssh, .gnupg

        // .ssh should not be in exclude
        auto ssh_path = std::filesystem::path("/home/testuser/.ssh");
        EXPECT_EQ(
            std::ranges::find(merged.monitor.home.exclude, ssh_path),
            merged.monitor.home.exclude.end()
        );

        // .cache should still be in exclude
        auto cache_path = std::filesystem::path("/home/testuser/.cache");
        EXPECT_NE(
            std::ranges::find(merged.monitor.home.exclude, cache_path),
            merged.monitor.home.exclude.end()
        );
    }

    TEST(ConfigMergeTest, RelativePathsConvertedToAbsolute) {
        Config system_config;

        HomeMonitoringPolicy policy;

        Config user_config;
        user_config.monitor.home.paths = {".local/bin", "/absolute/path"};
        user_config.monitor.home.exclude = {".cache", "/absolute/exclude"};

        auto merged = merge_configs(
            system_config,
            policy,
            std::optional{user_config},
            "/home/testuser"
        );

        EXPECT_EQ(merged.monitor.home.paths[0], "/home/testuser/.local/bin");
        EXPECT_EQ(merged.monitor.home.paths[1], "/absolute/path");
        EXPECT_EQ(merged.monitor.home.exclude[0], "/home/testuser/.cache");
        EXPECT_EQ(merged.monitor.home.exclude[1], "/absolute/exclude");
    }

    TEST(ConfigMergeTest, UserConfigMissing) {
        Config system_config;
        system_config.monitor.home.enabled = false;

        HomeMonitoringPolicy policy;
        policy.mandatory_paths = {".ssh"};

        auto merged = merge_configs(
            system_config,
            policy,
            std::nullopt,
            "/home/testuser"
        );

        EXPECT_FALSE(merged.monitor.home.enabled);
        EXPECT_TRUE(merged.monitor.home.paths.empty());
        EXPECT_EQ(merged.home_policy.mandatory_paths.size(), 1);
    }

    TEST(ConfigMergeTest, EmptyUserConfig) {
        Config system_config;
        system_config.hash.algorithm = HashAlgorithm::blake3;

        HomeMonitoringPolicy policy;

        Config user_config;  // All defaults

        auto merged = merge_configs(
            system_config,
            policy,
            std::optional{user_config},
            "/home/testuser"
        );

        EXPECT_EQ(merged.hash.algorithm, HashAlgorithm::blake3);
        EXPECT_TRUE(merged.monitor.home.paths.empty());
    }

    TEST(ConfigMergeTest, PolicyWithNoMonitoring) {
        Config system_config;

        HomeMonitoringPolicy policy;  // No forced monitoring

        Config user_config;
        user_config.monitor.home.enabled = true;
        user_config.monitor.home.paths = {".local/bin"};

        auto merged = merge_configs(
            system_config,
            policy,
            std::optional{user_config},
            "/home/testuser"
        );

        EXPECT_TRUE(merged.monitor.home.enabled);
        EXPECT_EQ(merged.monitor.home.paths.size(), 1);
    }

    TEST(ConfigMergeTest, PolicyMandatoryPathsEnforced) {
        Config system_config;

        HomeMonitoringPolicy policy;
        policy.mandatory_paths = {".ssh", ".gnupg", ".config/systemd"};

        Config user_config;
        user_config.monitor.home.paths = {".local/bin"};

        auto merged = merge_configs(
            system_config,
            policy,
            std::optional{user_config},
            "/home/testuser"
        );

        // Should have user path + 3 mandatory paths
        EXPECT_EQ(merged.monitor.home.paths.size(), 4);

        // Check all mandatory paths are present
        auto ssh_path = std::filesystem::path("/home/testuser/.ssh");
        auto gnupg_path = std::filesystem::path("/home/testuser/.gnupg");
        auto systemd_path = std::filesystem::path("/home/testuser/.config/systemd");

        EXPECT_NE(std::ranges::find(merged.monitor.home.paths, ssh_path),
                  merged.monitor.home.paths.end());
        EXPECT_NE(std::ranges::find(merged.monitor.home.paths, gnupg_path),
                  merged.monitor.home.paths.end());
        EXPECT_NE(std::ranges::find(merged.monitor.home.paths, systemd_path),
                  merged.monitor.home.paths.end());
    }

    TEST(ConfigMergeTest, UserAlertPreferencesMerged) {
        Config system_config;
        system_config.alerts.journal = true;
        system_config.alerts.dbus = true;
        system_config.alerts.socket = true;

        HomeMonitoringPolicy policy;

        Config user_config;
        user_config.alerts.journal = true;
        user_config.alerts.dbus = false;  // User disables desktop notifications
        user_config.alerts.socket = true;

        auto merged = merge_configs(
            system_config,
            policy,
            std::optional{user_config},
            "/home/testuser"
        );

        EXPECT_TRUE(merged.alerts.journal);
        EXPECT_FALSE(merged.alerts.dbus);
        EXPECT_TRUE(merged.alerts.socket);
    }

}  // namespace vigilant_canine
