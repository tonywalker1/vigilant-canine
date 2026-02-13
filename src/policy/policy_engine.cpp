//
// vigilant-canine - Policy Engine Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <policy/policy_engine.h>

#include <algorithm>
#include <fnmatch.h>

namespace vigilant_canine {

    PolicyEngine::PolicyEngine(PolicyConfig config) : m_config(std::move(config)) {}

    auto PolicyEngine::evaluate(Event const& event) const -> PolicyDecision {
        PolicyDecision decision;

        // Extract path from event if available
        auto path_opt = extract_path(event.data);

        // If we have a path, check path rules
        if (path_opt) {
            auto const& path = *path_opt;

            // Check each path rule in order
            for (auto const& rule : m_config.path_rules) {
                if (path_matches_pattern(path, rule.pattern)) {
                    decision.generate_alert = rule.alert;
                    decision.severity = rule.severity;
                    decision.reason = std::format("Matched path rule: {}", rule.pattern);
                    return decision;
                }
            }
        }

        // No matching rule - use defaults
        decision.generate_alert = m_config.alert_on_unknown;
        decision.severity = event_severity_to_alert_severity(event.severity);
        decision.reason = "No matching rule, using default policy";

        return decision;
    }

    auto PolicyEngine::path_matches_pattern(std::filesystem::path const& path,
                                              std::string const& pattern) const -> bool {
        // Use fnmatch for glob-style pattern matching
        return fnmatch(pattern.c_str(), path.c_str(), 0) == 0;
    }

    auto PolicyEngine::extract_path(EventData const& data) const
        -> std::optional<std::filesystem::path> {
        return std::visit([](auto const& event_data) -> std::optional<std::filesystem::path> {
            using T = std::decay_t<decltype(event_data)>;

            if constexpr (std::is_same_v<T, FileModifiedEvent>) {
                return *event_data.path;
            } else if constexpr (std::is_same_v<T, FileCreatedEvent>) {
                return *event_data.path;
            } else if constexpr (std::is_same_v<T, FileDeletedEvent>) {
                return *event_data.path;
            } else if constexpr (std::is_same_v<T, FilePermissionChangedEvent>) {
                return *event_data.path;
            } else {
                return std::nullopt;
            }
        }, data);
    }

    auto PolicyEngine::event_severity_to_alert_severity(EventSeverity severity) const
        -> AlertSeverity {
        switch (severity) {
            case EventSeverity::info:
                return AlertSeverity::INFO;
            case EventSeverity::warning:
                return AlertSeverity::WARNING;
            case EventSeverity::critical:
                return AlertSeverity::CRITICAL;
        }
        return AlertSeverity::INFO;
    }

    auto create_default_policy(DistroType distro_type) -> PolicyConfig {
        PolicyConfig config;

        // Critical: System binaries and libraries
        config.path_rules.push_back({"/usr/bin/*", AlertSeverity::CRITICAL, true});
        config.path_rules.push_back({"/usr/sbin/*", AlertSeverity::CRITICAL, true});
        config.path_rules.push_back({"/bin/*", AlertSeverity::CRITICAL, true});
        config.path_rules.push_back({"/sbin/*", AlertSeverity::CRITICAL, true});
        config.path_rules.push_back({"/usr/lib/*", AlertSeverity::CRITICAL, true});
        config.path_rules.push_back({"/usr/lib64/*", AlertSeverity::CRITICAL, true});
        config.path_rules.push_back({"/lib/*", AlertSeverity::CRITICAL, true});
        config.path_rules.push_back({"/lib64/*", AlertSeverity::CRITICAL, true});

        // Critical: System configuration
        config.path_rules.push_back({"/etc/*", AlertSeverity::CRITICAL, true});
        config.path_rules.push_back({"/boot/*", AlertSeverity::CRITICAL, true});

        // Warning: System state directories
        config.path_rules.push_back({"/var/lib/*", AlertSeverity::WARNING, true});
        config.path_rules.push_back({"/var/log/*", AlertSeverity::INFO, false});  // Don't alert on logs

        // Info: Temporary directories (low priority)
        config.path_rules.push_back({"/tmp/*", AlertSeverity::INFO, false});
        config.path_rules.push_back({"/var/tmp/*", AlertSeverity::INFO, false});
        config.path_rules.push_back({"/run/*", AlertSeverity::INFO, false});

        // Distro-specific rules
        if (distro_type == DistroType::ostree) {
            // OSTree deployments and object store
            config.path_rules.push_back({"/ostree/*", AlertSeverity::CRITICAL, true});
            config.path_rules.push_back({"/sysroot/ostree/*", AlertSeverity::CRITICAL, true});

            // OSTree overlay (user modifications)
            config.path_rules.push_back({"/var/home/*", AlertSeverity::WARNING, false});
        }

        if (distro_type == DistroType::btrfs_snapshot) {
            // Btrfs subvolumes
            config.path_rules.push_back({"/.snapshots/*", AlertSeverity::WARNING, true});
        }

        // Default: Alert on unknown paths with WARNING severity
        config.alert_on_unknown = true;
        config.default_severity = AlertSeverity::WARNING;

        return config;
    }

}  // namespace vigilant_canine
