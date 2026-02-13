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

}  // namespace vigilant_canine
