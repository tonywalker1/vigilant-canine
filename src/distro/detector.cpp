//
// vigilant-canine - Distribution Type Detection Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <distro/detector.h>

#include <sys/vfs.h>
#include <linux/magic.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <format>
#include <string_view>

namespace vigilant_canine {

    namespace {

        //
        // Check if a command exists in PATH.
        //
        auto command_exists(std::string_view command) -> bool {
            auto path_env = std::getenv("PATH");
            if (!path_env) {
                return false;
            }

            std::string path_str{path_env};
            std::size_t start = 0;

            while (start < path_str.size()) {
                auto end = path_str.find(':', start);
                if (end == std::string::npos) {
                    end = path_str.size();
                }

                std::filesystem::path bin_path{path_str.substr(start, end - start)};
                bin_path /= command;

                if (std::filesystem::exists(bin_path) &&
                    access(bin_path.c_str(), X_OK) == 0) {
                    return true;
                }

                start = end + 1;
            }

            return false;
        }

        //
        // Check if root filesystem is on Btrfs.
        //
        auto is_root_on_btrfs() -> bool {
            struct statfs fs_stat {};
            if (statfs("/", &fs_stat) != 0) {
                return false;
            }
            return fs_stat.f_type == BTRFS_SUPER_MAGIC;
        }

        //
        // Parse a key=value line from os-release.
        //
        auto parse_os_release_line(std::string const& line)
            -> std::pair<std::string, std::string> {

            auto eq_pos = line.find('=');
            if (eq_pos == std::string::npos) {
                return {"", ""};
            }

            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);

            // Remove quotes from value
            if (value.size() >= 2 &&
                ((value.front() == '"' && value.back() == '"') ||
                 (value.front() == '\'' && value.back() == '\''))) {
                value = value.substr(1, value.size() - 2);
            }

            return {key, value};
        }

    }  // anonymous namespace

    auto is_ostree_system() -> bool {
        // Check if /ostree directory exists
        if (!std::filesystem::exists("/ostree")) {
            return false;
        }

        // Check if ostree command is available
        return command_exists("ostree");
    }

    auto is_btrfs_snapshot_system() -> bool {
        // Must be on btrfs
        if (!is_root_on_btrfs()) {
            return false;
        }

        // Check for snapshot tools (snapper or transactional-update)
        return command_exists("snapper") || command_exists("transactional-update");
    }

    auto parse_os_release() -> std::expected<DistroInfo, std::string> {
        std::filesystem::path os_release_path = "/etc/os-release";

        // Fallback to /usr/lib/os-release if /etc doesn't exist
        if (!std::filesystem::exists(os_release_path)) {
            os_release_path = "/usr/lib/os-release";
        }

        std::ifstream file{os_release_path};
        if (!file) {
            return std::unexpected(std::format("Failed to open {}", os_release_path.string()));
        }

        DistroInfo info;
        info.type = DistroType::traditional;  // Will be determined by caller

        std::string line;
        while (std::getline(file, line)) {
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') {
                continue;
            }

            auto [key, value] = parse_os_release_line(line);

            if (key == "NAME") {
                info.name = value;
            } else if (key == "VERSION_ID") {
                info.version = value;
            } else if (key == "VARIANT" || key == "VARIANT_ID") {
                info.variant = value;
            }
        }

        if (info.name.empty()) {
            return std::unexpected("Failed to parse NAME from os-release");
        }

        return info;
    }

    auto detect_distro() -> std::expected<DistroInfo, std::string> {
        // Start by parsing os-release
        auto info_result = parse_os_release();
        if (!info_result) {
            return info_result;
        }

        DistroInfo info = *info_result;

        // Determine distro type
        if (is_ostree_system()) {
            info.type = DistroType::ostree;
        } else if (is_btrfs_snapshot_system()) {
            info.type = DistroType::btrfs_snapshot;
        } else {
            info.type = DistroType::traditional;
        }

        return info;
    }

    auto to_string(DistroType type) -> std::string {
        switch (type) {
            case DistroType::traditional:
                return "traditional";
            case DistroType::ostree:
                return "ostree";
            case DistroType::btrfs_snapshot:
                return "btrfs_snapshot";
        }
        return "unknown";
    }

}  // namespace vigilant_canine
