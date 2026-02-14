//
// vigilant-canine - User Manager Tests
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <user/user_manager.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include <unistd.h>

namespace vigilant_canine {

    TEST(UserManagerTest, DiscoverCurrentUser) {
        UserManager manager;
        auto result = manager.discover_users(1000);

        ASSERT_TRUE(result.has_value());
        auto const& users = result.value();

        // Should find at least the current user
        EXPECT_FALSE(users.empty());

        // Get current username
        char username[256];
        ASSERT_EQ(getlogin_r(username, sizeof(username)), 0);
        std::string current_user{username};

        // Check if current user is in the list
        bool found = false;
        for (auto const& user : users) {
            if (user.username == current_user) {
                found = true;
                EXPECT_GE(user.uid, 1000);
                EXPECT_FALSE(user.home_dir.empty());
                EXPECT_FALSE(user.shell.empty());
                EXPECT_FALSE(user.groups.empty());
                break;
            }
        }
        EXPECT_TRUE(found) << "Current user not found in discovered users";
    }

    TEST(UserManagerTest, FilterSystemUsers) {
        UserManager manager;
        auto result = manager.discover_users(1000);

        ASSERT_TRUE(result.has_value());
        auto const& users = result.value();

        // All discovered users should have UID >= 1000
        for (auto const& user : users) {
            EXPECT_GE(user.uid, 1000) << "User " << user.username << " has UID < 1000";
        }
    }

    TEST(UserManagerTest, ExcludeNologinShells) {
        UserManager manager;
        auto result = manager.discover_users(0);  // Don't filter by UID

        ASSERT_TRUE(result.has_value());
        auto const& users = result.value();

        // No user should have nologin or false shell
        for (auto const& user : users) {
            EXPECT_NE(user.shell, "/sbin/nologin");
            EXPECT_NE(user.shell, "/usr/sbin/nologin");
            EXPECT_NE(user.shell, "/bin/false");
            EXPECT_NE(user.shell, "/usr/bin/false");
        }
    }

    TEST(UserManagerTest, GetUserGroups) {
        UserManager manager;

        // Get groups for current user
        char username[256];
        ASSERT_EQ(getlogin_r(username, sizeof(username)), 0);

        auto result = manager.get_user_groups(username);
        ASSERT_TRUE(result.has_value());

        auto const& groups = result.value();
        EXPECT_FALSE(groups.empty()) << "Current user should belong to at least one group";
    }

    TEST(UserManagerTest, PolicyMonitorUsersForcesMonitoring) {
        UserManager manager;

        UserInfo user{
            .username = "testuser",
            .uid = 1001,
            .gid = 1001,
            .home_dir = "/home/testuser",
            .shell = "/bin/bash",
            .groups = {"testuser", "users"},
        };

        HomeMonitoringPolicy policy;
        policy.monitor_users = {"testuser"};
        policy.allow_user_opt_out = false;

        // User in monitor_users list with no opt-out: should be monitored
        bool should_monitor = manager.should_monitor_user(user, policy, false, false);
        EXPECT_TRUE(should_monitor);

        // Even if user config exists and is disabled, policy forces monitoring
        should_monitor = manager.should_monitor_user(user, policy, true, false);
        EXPECT_TRUE(should_monitor);
    }

    TEST(UserManagerTest, PolicyMonitorGroupsForcesMonitoring) {
        UserManager manager;

        UserInfo user{
            .username = "developer",
            .uid = 1002,
            .gid = 1002,
            .home_dir = "/home/developer",
            .shell = "/bin/bash",
            .groups = {"developer", "sudo", "users"},
        };

        HomeMonitoringPolicy policy;
        policy.monitor_groups = {"sudo"};
        policy.allow_user_opt_out = false;

        // User in monitored group: should be monitored
        bool should_monitor = manager.should_monitor_user(user, policy, false, false);
        EXPECT_TRUE(should_monitor);
    }

    TEST(UserManagerTest, PolicyAllowUserOptOutRespected) {
        UserManager manager;

        UserInfo user{
            .username = "optoutuser",
            .uid = 1003,
            .gid = 1003,
            .home_dir = "/home/optoutuser",
            .shell = "/bin/bash",
            .groups = {"optoutuser", "users"},
        };

        HomeMonitoringPolicy policy;
        policy.monitor_users = {"optoutuser"};
        policy.allow_user_opt_out = true;  // Allow opt-out

        // User can opt out if config exists and is disabled
        bool should_monitor = manager.should_monitor_user(user, policy, true, false);
        EXPECT_FALSE(should_monitor);

        // User can opt in if config exists and is enabled
        should_monitor = manager.should_monitor_user(user, policy, true, true);
        EXPECT_TRUE(should_monitor);

        // Without user config, policy forces monitoring
        should_monitor = manager.should_monitor_user(user, policy, false, false);
        EXPECT_TRUE(should_monitor);
    }

    TEST(UserManagerTest, LoadUserConfig) {
        UserManager manager;

        // Create temporary user config
        auto temp_home = std::filesystem::temp_directory_path() / "vc_test_home";
        std::filesystem::create_directories(temp_home / ".config" / "vigilant-canine");

        auto config_path = temp_home / ".config" / "vigilant-canine" / "config.toml";
        {
            std::ofstream file(config_path);
            file << R"(
[monitor.home]
enabled = true
paths = [".local/bin", ".cargo/bin"]
)";
        }

        UserInfo user{
            .username = "testuser",
            .uid = 1001,
            .gid = 1001,
            .home_dir = temp_home,
            .shell = "/bin/bash",
            .groups = {"testuser"},
        };

        auto result = manager.load_user_config(user);
        ASSERT_TRUE(result.has_value());

        auto const& opt_config = result.value();
        ASSERT_TRUE(opt_config.has_value());

        auto const& config = opt_config.value();
        EXPECT_TRUE(config.monitor.home.enabled);
        EXPECT_EQ(config.monitor.home.paths.size(), 2);

        // Cleanup
        std::filesystem::remove_all(temp_home);
    }

}  // namespace vigilant_canine
