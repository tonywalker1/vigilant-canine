//
// vigilant-canine - Baseline Strategy Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <baseline/strategy.h>

#include <array>
#include <cstdio>
#include <format>
#include <memory>

namespace vigilant_canine {

    namespace {

        //
        // Execute a command and capture its output.
        //
        auto exec_command(std::string const& command)
            -> std::expected<std::string, std::string> {

            std::array<char, 128> buffer{};
            std::string result;

            // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);

            if (!pipe) {
                return std::unexpected(std::format("Failed to execute: {}", command));
            }

            while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
                result += buffer.data();
            }

            return result;
        }

        //
        // Query RPM for the package owning a file.
        //
        auto query_rpm_owner(std::filesystem::path const& path)
            -> std::optional<std::string> {

            auto cmd = std::format("rpm -qf --queryformat '%{{NAME}}' '{}' 2>/dev/null",
                                   path.string());
            auto result = exec_command(cmd);

            if (!result || result->empty() || result->find("not owned") != std::string::npos) {
                return std::nullopt;
            }

            return *result;
        }

        //
        // Query dpkg for the package owning a file.
        //
        auto query_dpkg_owner(std::filesystem::path const& path)
            -> std::optional<std::string> {

            auto cmd = std::format("dpkg -S '{}' 2>/dev/null | cut -d: -f1", path.string());
            auto result = exec_command(cmd);

            if (!result || result->empty()) {
                return std::nullopt;
            }

            // Remove trailing whitespace
            auto pkg = *result;
            while (!pkg.empty() && std::isspace(pkg.back())) {
                pkg.pop_back();
            }

            return pkg.empty() ? std::nullopt : std::make_optional(pkg);
        }

    }  // anonymous namespace

    //
    // TraditionalStrategy implementation
    //

    auto TraditionalStrategy::get_monitor_paths() const -> MonitorPaths {
        MonitorPaths paths;

        // Critical system directories
        paths.critical = {
            "/usr/bin",
            "/usr/sbin",
            "/usr/lib",
            "/usr/lib64",
            "/bin",      // Often symlink to /usr/bin
            "/sbin",     // Often symlink to /usr/sbin
            "/lib",      // Often symlink to /usr/lib
            "/lib64",    // Often symlink to /usr/lib64
        };

        // Configuration directories
        paths.config = {
            "/etc",
        };

        // Paths to exclude from monitoring
        paths.exclude = {
            "/var/log",
            "/var/cache",
            "/var/tmp",
            "/tmp",
            "/home",
            "/root",
            "/proc",
            "/sys",
            "/dev",
            "/run",
        };

        return paths;
    }

    auto TraditionalStrategy::get_file_source(std::filesystem::path const& path) const
        -> std::expected<std::optional<BaselineSource>, std::string> {

        // Try RPM first (Fedora, RHEL, openSUSE)
        if (auto pkg = query_rpm_owner(path)) {
            return BaselineSource{std::format("rpm:{}", *pkg)};
        }

        // Try dpkg (Debian, Ubuntu)
        if (auto pkg = query_dpkg_owner(path)) {
            return BaselineSource{std::format("deb:{}", *pkg)};
        }

        // File not owned by a package
        return std::nullopt;
    }

    //
    // OstreeStrategy implementation
    //

    auto OstreeStrategy::get_monitor_paths() const -> MonitorPaths {
        MonitorPaths paths;

        // On OSTree systems, the deployment is in /ostree/deploy/<osname>/deploy/<checksum>
        // But from the user perspective, everything is under /usr
        paths.critical = {
            "/usr",
        };

        // Configuration overlay (writable)
        paths.config = {
            "/etc",
            "/var",
        };

        // Paths to exclude
        paths.exclude = {
            "/var/log",
            "/var/cache",
            "/var/tmp",
            "/tmp",
            "/home",
            "/root",
            "/proc",
            "/sys",
            "/dev",
            "/run",
            "/ostree",  // Internal ostree data
        };

        return paths;
    }

    auto OstreeStrategy::get_file_source(std::filesystem::path const& path) const
        -> std::expected<std::optional<BaselineSource>, std::string> {

        // For OSTree systems, files in /usr are from the deployment
        if (path.string().starts_with("/usr/")) {
            auto deployment = get_deployment_id();
            if (deployment) {
                return BaselineSource{std::format("ostree:{}", *deployment)};
            }
        }

        // Files in /etc or /var might be in the overlay
        if (path.string().starts_with("/etc/") || path.string().starts_with("/var/")) {
            return BaselineSource{"ostree:overlay"};
        }

        return std::nullopt;
    }

    auto OstreeStrategy::get_deployment_id() const -> std::optional<std::string> {
        // Get current deployment checksum from ostree
        auto result = exec_command("ostree admin status --print-current-deployment 2>/dev/null");

        if (!result || result->empty()) {
            return std::nullopt;
        }

        // Parse deployment ID from output
        // Format is typically: osname checksum.0
        auto output = *result;

        // Remove trailing whitespace
        while (!output.empty() && std::isspace(output.back())) {
            output.pop_back();
        }

        return output.empty() ? std::nullopt : std::make_optional(output);
    }

    //
    // BtrfsSnapshotStrategy implementation
    //

    auto BtrfsSnapshotStrategy::get_monitor_paths() const -> MonitorPaths {
        MonitorPaths paths;

        // Similar to traditional, but aware of snapshots
        paths.critical = {
            "/usr",
            "/bin",
            "/sbin",
            "/lib",
            "/lib64",
        };

        paths.config = {
            "/etc",
        };

        paths.exclude = {
            "/var/log",
            "/var/cache",
            "/var/tmp",
            "/tmp",
            "/home",
            "/root",
            "/proc",
            "/sys",
            "/dev",
            "/run",
            "/.snapshots",  // Snapper snapshots directory
        };

        return paths;
    }

    auto BtrfsSnapshotStrategy::get_file_source(std::filesystem::path const& path) const
        -> std::expected<std::optional<BaselineSource>, std::string> {

        // For Btrfs snapshot systems, try package manager first
        if (auto pkg = query_rpm_owner(path)) {
            return BaselineSource{std::format("rpm:{}", *pkg)};
        }

        // Could also query snapper for snapshot information
        // For now, just indicate files are from the current snapshot
        return BaselineSource{"snapshot:current"};
    }

    //
    // Factory function
    //

    auto create_baseline_strategy(DistroType type)
        -> std::unique_ptr<BaselineStrategy> {

        switch (type) {
        case DistroType::traditional:
            return std::make_unique<TraditionalStrategy>();
        case DistroType::ostree:
            return std::make_unique<OstreeStrategy>();
        case DistroType::btrfs_snapshot:
            return std::make_unique<BtrfsSnapshotStrategy>();
        }

        // Should never reach here, but return traditional as fallback
        return std::make_unique<TraditionalStrategy>();
    }

}  // namespace vigilant_canine
