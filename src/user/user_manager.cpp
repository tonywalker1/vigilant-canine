//
// vigilant-canine - User Manager Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <user/user_manager.h>

#include <algorithm>
#include <cstring>
#include <format>
#include <fstream>
#include <sstream>

#include <grp.h>
#include <pwd.h>
#include <unistd.h>

namespace vigilant_canine {

    auto UserManager::is_interactive_shell(std::string const& shell) -> bool {
        // Non-interactive shells
        static constexpr std::array non_interactive = {
            "/sbin/nologin",
            "/usr/sbin/nologin",
            "/bin/false",
            "/usr/bin/false",
            "/bin/true",
            "/usr/bin/true",
        };

        for (auto const& non_shell : non_interactive) {
            if (shell == non_shell) {
                return false;
            }
        }

        return !shell.empty();
    }

    auto UserManager::discover_users(uid_t min_uid)
        -> std::expected<std::vector<UserInfo>, std::string> {

        std::vector<UserInfo> users;

        // Read /etc/passwd
        std::ifstream passwd_file("/etc/passwd");
        if (!passwd_file) {
            return std::unexpected("Failed to open /etc/passwd");
        }

        std::string line;
        while (std::getline(passwd_file, line)) {
            // Parse passwd entry: username:x:uid:gid:comment:home:shell
            std::istringstream iss(line);
            std::string username, x, uid_str, gid_str, comment, home, shell;

            if (!std::getline(iss, username, ':')) continue;
            if (!std::getline(iss, x, ':')) continue;
            if (!std::getline(iss, uid_str, ':')) continue;
            if (!std::getline(iss, gid_str, ':')) continue;
            if (!std::getline(iss, comment, ':')) continue;
            if (!std::getline(iss, home, ':')) continue;
            if (!std::getline(iss, shell)) continue;

            try {
                auto uid = static_cast<uid_t>(std::stoul(uid_str));
                auto gid = static_cast<gid_t>(std::stoul(gid_str));

                // Filter by UID threshold
                if (uid < min_uid) {
                    continue;
                }

                // Filter by shell
                if (!is_interactive_shell(shell)) {
                    continue;
                }

                // Get user's groups
                auto groups_result = get_user_groups(username);
                if (!groups_result.has_value()) {
                    // Skip users we can't get groups for
                    continue;
                }

                UserInfo user{
                    .username = username,
                    .uid = uid,
                    .gid = gid,
                    .home_dir = home,
                    .shell = shell,
                    .groups = std::move(groups_result.value()),
                };

                users.push_back(std::move(user));
            }
            catch (std::exception const&) {
                // Skip malformed entries
                continue;
            }
        }

        return users;
    }

    auto UserManager::get_user_groups(std::string const& username)
        -> std::expected<std::vector<std::string>, std::string> {

        // Get passwd entry for the user
        errno = 0;
        auto* pwd = getpwnam(username.c_str());
        if (pwd == nullptr) {
            if (errno != 0) {
                return std::unexpected(std::format("getpwnam failed: {}", strerror(errno)));
            }
            return std::unexpected(std::format("User not found: {}", username));
        }

        // Get supplementary groups
        int ngroups = 0;
        getgrouplist(username.c_str(), pwd->pw_gid, nullptr, &ngroups);

        if (ngroups <= 0) {
            // User has no groups (unusual but possible)
            return std::vector<std::string>{};
        }

        std::vector<gid_t> gids(static_cast<std::size_t>(ngroups));
        if (getgrouplist(username.c_str(), pwd->pw_gid, gids.data(), &ngroups) == -1) {
            return std::unexpected("getgrouplist failed");
        }

        // Convert GIDs to group names
        std::vector<std::string> groups;
        groups.reserve(static_cast<std::size_t>(ngroups));

        for (auto gid : gids) {
            errno = 0;
            auto* grp = getgrgid(gid);
            if (grp != nullptr) {
                groups.emplace_back(grp->gr_name);
            }
        }

        return groups;
    }

    auto UserManager::should_monitor_user(
        UserInfo const& user,
        HomeMonitoringPolicy const& policy,
        bool user_config_exists,
        bool user_config_enabled
    ) -> bool {

        // Check if user is explicitly listed
        bool in_monitor_users = std::ranges::find(
            policy.monitor_users,
            user.username
        ) != policy.monitor_users.end();

        // Check if user's primary group or any supplementary group is listed
        bool in_monitor_groups = false;
        for (auto const& group : user.groups) {
            if (std::ranges::find(policy.monitor_groups, group)
                != policy.monitor_groups.end()) {
                in_monitor_groups = true;
                break;
            }
        }

        // Policy forces monitoring
        if (in_monitor_users || in_monitor_groups) {
            // If allow_user_opt_out is true and user has config, respect their choice
            if (policy.allow_user_opt_out && user_config_exists) {
                return user_config_enabled;
            }
            // Otherwise, monitoring is forced
            return true;
        }

        // Policy doesn't force monitoring, check user config
        if (user_config_exists) {
            return user_config_enabled;
        }

        // No policy requirement, no user config: don't monitor
        return false;
    }

    auto UserManager::load_user_config(UserInfo const& user)
        -> std::expected<std::optional<Config>, std::string> {

        auto config_path = user.home_dir / ".config" / "vigilant-canine" / "config.toml";

        if (!std::filesystem::exists(config_path)) {
            return std::optional<Config>{std::nullopt};
        }

        auto config_result = load_config(config_path);
        if (!config_result.has_value()) {
            return std::unexpected(std::format(
                "Failed to load user config for {}: {}",
                user.username,
                config_result.error()
            ));
        }

        return std::optional<Config>{std::move(config_result.value())};
    }

}  // namespace vigilant_canine
