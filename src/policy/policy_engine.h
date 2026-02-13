//
// vigilant-canine - Policy Engine
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_POLICY_POLICY_ENGINE_H
#define VIGILANT_CANINE_POLICY_POLICY_ENGINE_H

#include <config/config.h>
#include <events/event.h>
#include <storage/alert_store.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace vigilant_canine {

    //
    // Policy decision for an event.
    //
    struct PolicyDecision {
        bool generate_alert;                    // Should an alert be generated?
        AlertSeverity severity;                 // Final severity for the alert
        std::optional<std::string> reason;      // Optional reason for the decision
    };

    //
    // Path matching rule.
    //
    struct PathRule {
        std::string pattern;                    // Glob-like pattern (e.g., "/usr/bin/*")
        AlertSeverity severity;                 // Severity for matches
        bool alert;                             // Generate alert for matches?
    };

    //
    // Policy configuration.
    //
    struct PolicyConfig {
        std::vector<PathRule> path_rules;       // Path-based rules
        bool alert_on_unknown{true};            // Alert for files not matching any rule?
        AlertSeverity default_severity{AlertSeverity::WARNING};
    };

    //
    // Policy engine for evaluating events against configured rules.
    //
    // The policy engine determines:
    // 1. Whether an event should generate an alert
    // 2. The appropriate severity level for the alert
    // 3. Whether to suppress certain events based on configuration
    //
    // Rules are evaluated in order, with the first matching rule winning.
    //
    class PolicyEngine {
    public:
        explicit PolicyEngine(PolicyConfig config = {});

        //
        // Evaluate an event against the policy.
        //
        // Returns a PolicyDecision indicating whether to generate an alert
        // and at what severity level.
        //
        [[nodiscard]] auto evaluate(Event const& event) const -> PolicyDecision;

        //
        // Get current policy configuration.
        //
        [[nodiscard]] auto config() const -> PolicyConfig const& { return m_config; }

        //
        // Update policy configuration.
        //
        void set_config(PolicyConfig config) { m_config = std::move(config); }

    private:
        //
        // Check if a path matches a pattern.
        //
        [[nodiscard]] auto path_matches_pattern(std::filesystem::path const& path,
                                                 std::string const& pattern) const -> bool;

        //
        // Extract path from event data if available.
        //
        [[nodiscard]] auto extract_path(EventData const& data) const
            -> std::optional<std::filesystem::path>;

        //
        // Convert EventSeverity to AlertSeverity.
        //
        [[nodiscard]] auto event_severity_to_alert_severity(EventSeverity severity) const
            -> AlertSeverity;

        PolicyConfig m_config;
    };

    //
    // Create default policy configuration for a given distro type.
    //
    [[nodiscard]] auto create_default_policy(DistroType distro_type) -> PolicyConfig;

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_POLICY_POLICY_ENGINE_H
