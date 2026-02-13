//
// vigilant-canine - Configuration Loading
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_CONFIG_CONFIG_H
#define VIGILANT_CANINE_CONFIG_CONFIG_H

#include <core/types.h>

#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace vigilant_canine {

    //
    // Daemon configuration loaded from TOML files.
    //
    struct DaemonConfig {
        std::string log_level{"info"};
        std::filesystem::path db_path{"/var/lib/vigilant-canine/vc.db"};
        std::size_t worker_threads{0};  // 0 = auto-detect
    };

    struct HashConfig {
        HashAlgorithm algorithm{HashAlgorithm::blake3};
    };

    struct MonitorSystemConfig {
        std::vector<std::filesystem::path> paths;
        std::vector<std::filesystem::path> exclude;
    };

    struct MonitorFlatpakConfig {
        bool enabled{true};
        bool system{true};
        bool user{false};
    };

    struct MonitorOstreeConfig {
        bool enabled{true};
        bool verify_deployments{true};
        bool monitor_object_store{true};
    };

    struct MonitorHomeConfig {
        bool enabled{false};
        std::vector<std::filesystem::path> paths;
        std::vector<std::filesystem::path> exclude;
    };

    struct MonitorConfig {
        MonitorSystemConfig system;
        MonitorFlatpakConfig flatpak;
        MonitorOstreeConfig ostree;
        MonitorHomeConfig home;
    };

    struct AlertConfig {
        bool journal{true};
        bool dbus{true};
        bool socket{true};
    };

    struct ScanConfig {
        std::string schedule{"daily"};
        bool on_boot{true};
    };

    //
    // Phase 2: Journal monitoring configuration.
    //
    struct JournalFieldMatchConfig {
        std::string field;
        std::string pattern;
        std::string type{"contains"};  // "exact", "contains", "regex", "starts_with"
        bool negate{false};
    };

    struct JournalRuleConfig {
        std::string name;
        std::string description;
        std::vector<JournalFieldMatchConfig> match;
        std::string action{"suspicious_log"};
        std::string severity{"warning"};
        bool enabled{true};
    };

    struct JournalConfig {
        bool enabled{true};
        std::uint8_t max_priority{6};  // LOG_INFO
        std::vector<std::string> exclude_units;
        std::vector<std::string> exclude_identifiers;
        std::vector<JournalRuleConfig> rules;
    };

    //
    // Phase 2: Event correlation configuration.
    //
    struct CorrelationRuleConfig {
        std::string name;
        std::string event_match;  // Rule name or event category
        std::uint32_t threshold{5};
        std::uint32_t window_seconds{60};
        std::string escalated_severity{"critical"};
    };

    struct CorrelationConfig {
        bool enabled{true};
        std::uint32_t window_seconds{300};
        std::vector<CorrelationRuleConfig> rules;
    };

    //
    // Top-level configuration structure.
    //
    struct Config {
        DaemonConfig daemon;
        HashConfig hash;
        MonitorConfig monitor;
        AlertConfig alerts;
        ScanConfig scan;
        JournalConfig journal;          // Phase 2
        CorrelationConfig correlation;  // Phase 2
    };

    //
    // Load configuration from a TOML file.
    //
    // Preconditions:
    //   - path must refer to a valid TOML file
    //
    // Postconditions:
    //   - On success: returns parsed and validated Config
    //   - On failure: returns error message describing the failure
    //
    [[nodiscard]] auto load_config(std::filesystem::path const& path)
        -> std::expected<Config, std::string>;

    //
    // Load configuration with defaults for missing values.
    //
    // If the file doesn't exist, returns default configuration.
    // If the file exists but has parse errors, returns error.
    //
    [[nodiscard]] auto load_config_or_default(std::filesystem::path const& path)
        -> std::expected<Config, std::string>;

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_CONFIG_CONFIG_H
