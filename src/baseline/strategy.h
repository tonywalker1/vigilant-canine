//
// vigilant-canine - Baseline Strategy
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_BASELINE_STRATEGY_H
#define VIGILANT_CANINE_BASELINE_STRATEGY_H

#include <core/types.h>
#include <distro/detector.h>

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace vigilant_canine {

    //
    // Directories to monitor for file integrity.
    //
    struct MonitorPaths {
        std::vector<std::filesystem::path> critical;  // System binaries, libraries
        std::vector<std::filesystem::path> config;    // Configuration files
        std::vector<std::filesystem::path> exclude;   // Paths to skip (logs, caches)
    };

    //
    // Baseline source identifier.
    //
    // Indicates where a baseline came from:
    // - "rpm:<package>": File owned by RPM package
    // - "deb:<package>": File owned by Debian package
    // - "ostree:<deployment>": File in OSTree deployment
    // - "manual": Manually added baseline
    // - "scan": Full filesystem scan
    //
    using BaselineSource = StrongType<struct BaselineSourceTag, std::string>;

    //
    // Baseline strategy interface.
    //
    // Different distro types have different strategies for managing baselines:
    // - Traditional: Use package manager database (rpm -qf, dpkg -S)
    // - OSTree: Track deployment roots
    // - Btrfs snapshot: Track snapshots
    //
    class BaselineStrategy {
    public:
        virtual ~BaselineStrategy() = default;

        //
        // Get paths that should be monitored for this distro type.
        //
        [[nodiscard]] virtual auto get_monitor_paths() const -> MonitorPaths = 0;

        //
        // Determine the source of a file (package name, deployment, etc.).
        //
        // Preconditions:
        //   - path must be an absolute path
        //
        // Postconditions:
        //   - On success: returns source identifier
        //   - On failure: returns error message
        //   - Returns nullopt if file has no tracked source
        //
        [[nodiscard]] virtual auto get_file_source(std::filesystem::path const& path) const
            -> std::expected<std::optional<BaselineSource>, std::string> = 0;

        //
        // Get deployment identifier (for OSTree systems).
        //
        // Returns nullopt for non-OSTree systems.
        //
        [[nodiscard]] virtual auto get_deployment_id() const
            -> std::optional<std::string> = 0;
    };

    //
    // Strategy for traditional package manager-based distributions.
    //
    class TraditionalStrategy : public BaselineStrategy {
    public:
        [[nodiscard]] auto get_monitor_paths() const -> MonitorPaths override;

        [[nodiscard]] auto get_file_source(std::filesystem::path const& path) const
            -> std::expected<std::optional<BaselineSource>, std::string> override;

        [[nodiscard]] auto get_deployment_id() const -> std::optional<std::string> override {
            return std::nullopt;
        }
    };

    //
    // Strategy for OSTree-based distributions.
    //
    class OstreeStrategy : public BaselineStrategy {
    public:
        [[nodiscard]] auto get_monitor_paths() const -> MonitorPaths override;

        [[nodiscard]] auto get_file_source(std::filesystem::path const& path) const
            -> std::expected<std::optional<BaselineSource>, std::string> override;

        [[nodiscard]] auto get_deployment_id() const -> std::optional<std::string> override;
    };

    //
    // Strategy for Btrfs snapshot-based distributions.
    //
    class BtrfsSnapshotStrategy : public BaselineStrategy {
    public:
        [[nodiscard]] auto get_monitor_paths() const -> MonitorPaths override;

        [[nodiscard]] auto get_file_source(std::filesystem::path const& path) const
            -> std::expected<std::optional<BaselineSource>, std::string> override;

        [[nodiscard]] auto get_deployment_id() const -> std::optional<std::string> override {
            return std::nullopt;
        }
    };

    //
    // Create appropriate baseline strategy for the detected distro type.
    //
    [[nodiscard]] auto create_baseline_strategy(DistroType type)
        -> std::unique_ptr<BaselineStrategy>;

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_BASELINE_STRATEGY_H
