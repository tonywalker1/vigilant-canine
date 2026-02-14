//
// vigilant-canine - Configuration Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <config/config.h>

#include <core/hash.h>
#include <hinder/exception/exception.h>

#include <toml++/toml.hpp>

#include <format>
#include <fstream>

namespace vigilant_canine {

    HINDER_DEFINE_EXCEPTION(config_error, hinder::generic_error);

    namespace {

        //
        // Extract optional value from TOML table with default.
        //
        template<typename T>
        auto get_or(toml::table const& table, std::string_view key, T default_value) -> T {
            if (auto opt = table[key].value<T>()) {
                return *opt;
            }
            return default_value;
        }

        //
        // Extract required value from TOML table.
        //
        template<typename T>
        auto get_required(toml::table const& table, std::string_view key) -> T {
            auto opt = table[key].value<T>();
            HINDER_EXPECTS(opt.has_value(), config_error)
                .message("Missing required config key: {}", key);
            return *opt;
        }

        //
        // Parse DaemonConfig section.
        //
        auto parse_daemon(toml::table const& root) -> DaemonConfig {
            DaemonConfig cfg;

            if (auto daemon = root["daemon"].as_table()) {
                cfg.log_level = get_or(*daemon, "log_level", cfg.log_level);
                cfg.db_path = get_or(*daemon, "db_path", cfg.db_path.string());
                cfg.worker_threads = get_or(*daemon, "worker_threads", cfg.worker_threads);
            }

            return cfg;
        }

        //
        // Parse HashConfig section.
        //
        auto parse_hash(toml::table const& root) -> HashConfig {
            HashConfig cfg;

            if (auto hash = root["hash"].as_table()) {
                auto alg_str = get_or(*hash, "algorithm", std::string{"blake3"});
                auto alg_result = string_to_algorithm(alg_str);
                HINDER_EXPECTS(alg_result.has_value(), config_error)
                    .message("Invalid hash algorithm: {}", alg_str);
                cfg.algorithm = *alg_result;
            }

            return cfg;
        }

        //
        // Parse string array from TOML.
        //
        auto parse_path_array(toml::array const* arr) -> std::vector<std::filesystem::path> {
            std::vector<std::filesystem::path> paths;
            if (!arr) return paths;

            for (auto const& elem : *arr) {
                if (auto str = elem.value<std::string>()) {
                    paths.emplace_back(*str);
                }
            }
            return paths;
        }

        //
        // Parse MonitorConfig section.
        //
        auto parse_monitor(toml::table const& root) -> MonitorConfig {
            MonitorConfig cfg;

            // monitor.system
            if (auto system = root["monitor"]["system"].as_table()) {
                cfg.system.paths = parse_path_array((*system)["paths"].as_array());
                cfg.system.exclude = parse_path_array((*system)["exclude"].as_array());
            }

            // monitor.flatpak
            if (auto flatpak = root["monitor"]["flatpak"].as_table()) {
                cfg.flatpak.enabled = get_or(*flatpak, "enabled", cfg.flatpak.enabled);
                cfg.flatpak.system = get_or(*flatpak, "system", cfg.flatpak.system);
                cfg.flatpak.user = get_or(*flatpak, "user", cfg.flatpak.user);
            }

            // monitor.ostree
            if (auto ostree = root["monitor"]["ostree"].as_table()) {
                cfg.ostree.enabled = get_or(*ostree, "enabled", cfg.ostree.enabled);
                cfg.ostree.verify_deployments = get_or(*ostree, "verify_deployments", cfg.ostree.verify_deployments);
                cfg.ostree.monitor_object_store = get_or(*ostree, "monitor_object_store", cfg.ostree.monitor_object_store);
            }

            // monitor.home
            if (auto home = root["monitor"]["home"].as_table()) {
                cfg.home.enabled = get_or(*home, "enabled", cfg.home.enabled);
                cfg.home.paths = parse_path_array((*home)["paths"].as_array());
                cfg.home.exclude = parse_path_array((*home)["exclude"].as_array());
            }

            return cfg;
        }

        //
        // Parse AlertConfig section.
        //
        auto parse_alerts(toml::table const& root) -> AlertConfig {
            AlertConfig cfg;

            if (auto alerts = root["alerts"].as_table()) {
                cfg.journal = get_or(*alerts, "journal", cfg.journal);
                cfg.dbus = get_or(*alerts, "dbus", cfg.dbus);
                cfg.socket = get_or(*alerts, "socket", cfg.socket);
            }

            return cfg;
        }

        //
        // Parse ScanConfig section.
        //
        auto parse_scan(toml::table const& root) -> ScanConfig {
            ScanConfig cfg;

            if (auto scan = root["scan"].as_table()) {
                cfg.schedule = get_or(*scan, "schedule", cfg.schedule);
                cfg.on_boot = get_or(*scan, "on_boot", cfg.on_boot);
            }

            return cfg;
        }

        //
        // Parse string array from TOML.
        //
        auto parse_string_array(toml::array const* arr) -> std::vector<std::string> {
            std::vector<std::string> strings;
            if (!arr) return strings;

            for (auto const& elem : *arr) {
                if (auto str = elem.value<std::string>()) {
                    strings.emplace_back(*str);
                }
            }
            return strings;
        }

        //
        // Parse Phase 2: JournalConfig section.
        //
        auto parse_journal(toml::table const& root) -> JournalConfig {
            JournalConfig cfg;

            if (auto journal = root["journal"].as_table()) {
                cfg.enabled = get_or(*journal, "enabled", cfg.enabled);

                // max_priority as integer
                if (auto priority = (*journal)["max_priority"].value<std::int64_t>()) {
                    cfg.max_priority = static_cast<std::uint8_t>(*priority);
                }

                cfg.exclude_units = parse_string_array((*journal)["exclude_units"].as_array());
                cfg.exclude_identifiers = parse_string_array((*journal)["exclude_identifiers"].as_array());

                // Parse journal rules array
                if (auto rules_arr = (*journal)["rules"].as_array()) {
                    for (auto const& rule_elem : *rules_arr) {
                        auto rule_table = rule_elem.as_table();
                        if (!rule_table) continue;

                        JournalRuleConfig rule;
                        rule.name = get_or(*rule_table, "name", std::string{});
                        rule.description = get_or(*rule_table, "description", std::string{});
                        rule.action = get_or(*rule_table, "action", rule.action);
                        rule.severity = get_or(*rule_table, "severity", rule.severity);
                        rule.enabled = get_or(*rule_table, "enabled", rule.enabled);

                        // Parse field matches
                        if (auto matches_arr = (*rule_table)["match"].as_array()) {
                            for (auto const& match_elem : *matches_arr) {
                                auto match_table = match_elem.as_table();
                                if (!match_table) continue;

                                JournalFieldMatchConfig match;
                                match.field = get_or(*match_table, "field", std::string{});
                                match.pattern = get_or(*match_table, "pattern", std::string{});
                                match.type = get_or(*match_table, "type", match.type);
                                match.negate = get_or(*match_table, "negate", match.negate);

                                rule.match.push_back(match);
                            }
                        }

                        cfg.rules.push_back(rule);
                    }
                }
            }

            return cfg;
        }

        //
        // Parse Phase 2: CorrelationConfig section.
        //
        auto parse_correlation(toml::table const& root) -> CorrelationConfig {
            CorrelationConfig cfg;

            if (auto correlation = root["correlation"].as_table()) {
                cfg.enabled = get_or(*correlation, "enabled", cfg.enabled);

                if (auto window = (*correlation)["window_seconds"].value<std::int64_t>()) {
                    cfg.window_seconds = static_cast<std::uint32_t>(*window);
                }

                // Parse correlation rules array
                if (auto rules_arr = (*correlation)["rules"].as_array()) {
                    for (auto const& rule_elem : *rules_arr) {
                        auto rule_table = rule_elem.as_table();
                        if (!rule_table) continue;

                        CorrelationRuleConfig rule;
                        rule.name = get_or(*rule_table, "name", std::string{});
                        rule.event_match = get_or(*rule_table, "event_match", std::string{});

                        if (auto thresh = (*rule_table)["threshold"].value<std::int64_t>()) {
                            rule.threshold = static_cast<std::uint32_t>(*thresh);
                        }
                        if (auto window = (*rule_table)["window_seconds"].value<std::int64_t>()) {
                            rule.window_seconds = static_cast<std::uint32_t>(*window);
                        }

                        rule.escalated_severity = get_or(*rule_table, "escalated_severity",
                                                          rule.escalated_severity);

                        cfg.rules.push_back(rule);
                    }
                }
            }

            return cfg;
        }

        //
        // Parse Phase 3: AuditConfig section.
        //
        auto parse_audit(toml::table const& root) -> AuditConfig {
            AuditConfig cfg;

            if (auto audit = root["audit"].as_table()) {
                cfg.enabled = get_or(*audit, "enabled", cfg.enabled);
                cfg.sanitize_command_lines = get_or(*audit, "sanitize_command_lines", cfg.sanitize_command_lines);

                cfg.exclude_comms = parse_string_array((*audit)["exclude_comms"].as_array());

                // Parse exclude_uids as integer array
                if (auto uids_arr = (*audit)["exclude_uids"].as_array()) {
                    for (auto const& uid_elem : *uids_arr) {
                        if (auto uid = uid_elem.value<std::int64_t>()) {
                            cfg.exclude_uids.push_back(static_cast<std::uint32_t>(*uid));
                        }
                    }
                }

                // Parse audit rules array
                if (auto rules_arr = (*audit)["rules"].as_array()) {
                    for (auto const& rule_elem : *rules_arr) {
                        auto rule_table = rule_elem.as_table();
                        if (!rule_table) continue;

                        AuditRuleConfig rule;
                        rule.name = get_or(*rule_table, "name", std::string{});
                        rule.description = get_or(*rule_table, "description", std::string{});
                        rule.action = get_or(*rule_table, "action", rule.action);
                        rule.severity = get_or(*rule_table, "severity", rule.severity);
                        rule.enabled = get_or(*rule_table, "enabled", rule.enabled);

                        if (auto syscall = (*rule_table)["syscall_filter"].value<std::int64_t>()) {
                            rule.syscall_filter = static_cast<std::uint32_t>(*syscall);
                        }

                        // Parse field matches
                        if (auto matches_arr = (*rule_table)["match"].as_array()) {
                            for (auto const& match_elem : *matches_arr) {
                                auto match_table = match_elem.as_table();
                                if (!match_table) continue;

                                AuditFieldMatchConfig match;
                                match.field = get_or(*match_table, "field", std::string{});
                                match.pattern = get_or(*match_table, "pattern", std::string{});
                                match.type = get_or(*match_table, "type", match.type);
                                match.negate = get_or(*match_table, "negate", match.negate);

                                rule.match.push_back(match);
                            }
                        }

                        cfg.rules.push_back(rule);
                    }
                }
            }

            return cfg;
        }

        //
        // Parse HomeMonitoringPolicy section.
        //
        auto parse_home_policy(toml::table const& root) -> HomeMonitoringPolicy {
            HomeMonitoringPolicy cfg;

            if (auto policy = root["policy"].as_table()) {
                if (auto home = (*policy)["home"].as_table()) {
                    cfg.monitor_users = parse_string_array((*home)["monitor_users"].as_array());
                    cfg.monitor_groups = parse_string_array((*home)["monitor_groups"].as_array());
                    cfg.allow_user_opt_out = get_or(*home, "allow_user_opt_out", cfg.allow_user_opt_out);
                    cfg.mandatory_paths = parse_string_array((*home)["mandatory_paths"].as_array());
                }
            }

            return cfg;
        }

    }  // anonymous namespace

    auto load_config(std::filesystem::path const& path)
        -> std::expected<Config, std::string> {

        try {
            auto toml = toml::parse_file(path.string());

            Config cfg;
            cfg.daemon = parse_daemon(toml);
            cfg.hash = parse_hash(toml);
            cfg.monitor = parse_monitor(toml);
            cfg.alerts = parse_alerts(toml);
            cfg.scan = parse_scan(toml);
            cfg.journal = parse_journal(toml);            // Phase 2
            cfg.correlation = parse_correlation(toml);    // Phase 2
            cfg.audit = parse_audit(toml);                // Phase 3
            cfg.home_policy = parse_home_policy(toml);    // Home monitoring policy

            return cfg;
        }
        catch (toml::parse_error const& e) {
            return std::unexpected(std::format("TOML parse error: {}", e.description()));
        }
        catch (config_error const& e) {
            return std::unexpected(std::format("Config error: {}", e.what()));
        }
        catch (std::exception const& e) {
            return std::unexpected(std::format("Unexpected error loading config: {}", e.what()));
        }
    }

    auto load_config_or_default(std::filesystem::path const& path)
        -> std::expected<Config, std::string> {

        if (!std::filesystem::exists(path)) {
            // Return default configuration
            return Config{};
        }

        return load_config(path);
    }

    auto merge_configs(
        Config const& system_config,
        HomeMonitoringPolicy const& policy,
        std::optional<Config> const& user_config,
        std::filesystem::path const& home_dir
    ) -> Config {

        Config merged = system_config;

        // Store the policy in the merged config
        merged.home_policy = policy;

        // If no user config, return system config with policy
        if (!user_config.has_value()) {
            return merged;
        }

        auto const& user = user_config.value();

        // Merge home monitoring configuration
        // Policy settings override user preferences
        merged.monitor.home.enabled = user.monitor.home.enabled;

        // Merge paths: start with user paths
        std::vector<std::filesystem::path> merged_paths;

        for (auto const& path : user.monitor.home.paths) {
            // Convert relative paths to absolute
            if (path.is_relative()) {
                merged_paths.push_back(home_dir / path);
            } else {
                merged_paths.push_back(path);
            }
        }

        // Add mandatory paths from policy if not already present
        for (auto const& mandatory : policy.mandatory_paths) {
            std::filesystem::path abs_mandatory = mandatory;
            if (abs_mandatory.is_relative()) {
                abs_mandatory = home_dir / mandatory;
            }

            // Check if already in merged_paths
            if (std::ranges::find(merged_paths, abs_mandatory) == merged_paths.end()) {
                merged_paths.push_back(abs_mandatory);
            }
        }

        merged.monitor.home.paths = std::move(merged_paths);

        // Merge exclusion patterns
        std::vector<std::filesystem::path> merged_exclude;

        for (auto const& excl : user.monitor.home.exclude) {
            // Convert relative paths to absolute
            if (excl.is_relative()) {
                merged_exclude.push_back(home_dir / excl);
            } else {
                merged_exclude.push_back(excl);
            }
        }

        // Remove any mandatory paths from exclusions
        std::vector<std::filesystem::path> filtered_exclude;
        for (auto const& excl : merged_exclude) {
            bool is_mandatory = false;

            for (auto const& mandatory : policy.mandatory_paths) {
                std::filesystem::path abs_mandatory = mandatory;
                if (abs_mandatory.is_relative()) {
                    abs_mandatory = home_dir / mandatory;
                }

                // Check if exclusion matches or is under a mandatory path
                if (excl == abs_mandatory || excl.string().starts_with(abs_mandatory.string())) {
                    is_mandatory = true;
                    break;
                }
            }

            if (!is_mandatory) {
                filtered_exclude.push_back(excl);
            }
        }

        merged.monitor.home.exclude = std::move(filtered_exclude);

        // Hash algorithm: user preference if set, otherwise system default
        if (user.hash.algorithm != HashAlgorithm::blake3) {
            merged.hash.algorithm = user.hash.algorithm;
        }

        // Alert preferences: merge user preferences
        merged.alerts.journal = user.alerts.journal;
        merged.alerts.dbus = user.alerts.dbus;
        merged.alerts.socket = user.alerts.socket;

        return merged;
    }

}  // namespace vigilant_canine
