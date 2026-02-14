//
// vigilant-canine - User Manager
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_USER_USER_MANAGER_H
#define VIGILANT_CANINE_USER_USER_MANAGER_H

#include <config/config.h>

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <sys/types.h>

namespace vigilant_canine {

    //
    // Information about a system user.
    //
    struct UserInfo {
        std::string username;
        uid_t uid;
        gid_t gid;
        std::filesystem::path home_dir;
        std::string shell;
        std::vector<std::string> groups;
    };

    //
    // Manages user discovery and monitoring policy evaluation.
    //
    class UserManager {
    public:
        //
        // Enumerate users with interactive shells (UID >= min_uid, valid shell).
        //
        // Preconditions:
        //   - /etc/passwd must be readable
        //
        // Postconditions:
        //   - On success: returns list of UserInfo for interactive users
        //   - On failure: returns error message
        //
        [[nodiscard]] auto discover_users(uid_t min_uid = 1000)
            -> std::expected<std::vector<UserInfo>, std::string>;

        //
        // Get all groups a user belongs to.
        //
        // Preconditions:
        //   - username must be valid
        //
        // Postconditions:
        //   - On success: returns list of group names
        //   - On failure: returns error message
        //
        [[nodiscard]] auto get_user_groups(std::string const& username)
            -> std::expected<std::vector<std::string>, std::string>;

        //
        // Check if user should be monitored based on policy.
        //
        // Evaluation logic:
        //   - If user in monitor_users OR user's primary group in monitor_groups:
        //     - monitoring_enabled = true
        //     - user_can_disable = allow_user_opt_out
        //   - Else:
        //     - monitoring_enabled = user_config_enabled (default false)
        //
        // Preconditions:
        //   - user must be valid UserInfo
        //   - policy must be valid HomeMonitoringPolicy
        //
        // Postconditions:
        //   - Returns true if user should be monitored
        //
        [[nodiscard]] auto should_monitor_user(
            UserInfo const& user,
            HomeMonitoringPolicy const& policy,
            bool user_config_exists,
            bool user_config_enabled
        ) -> bool;

        //
        // Load user config if it exists.
        //
        // Preconditions:
        //   - user must be valid UserInfo
        //
        // Postconditions:
        //   - On success: returns optional Config (nullopt if file doesn't exist)
        //   - On failure: returns error message (parse error)
        //
        [[nodiscard]] auto load_user_config(UserInfo const& user)
            -> std::expected<std::optional<Config>, std::string>;

    private:
        //
        // Check if shell is interactive (not nologin, false, etc.).
        //
        [[nodiscard]] auto is_interactive_shell(std::string const& shell) -> bool;
    };

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_USER_USER_MANAGER_H
