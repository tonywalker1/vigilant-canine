//
// vigilant-canine - Audit Rule Definitions
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_AUDIT_RULE_H
#define VIGILANT_CANINE_AUDIT_RULE_H

#include <audit/audit_fields.h>
#include <events/event.h>

#include <optional>
#include <regex>
#include <string>
#include <vector>

namespace vigilant_canine {

    //
    // Match type for audit field matching.
    //
    enum class AuditMatchType : std::uint8_t {
        exact,
        contains,
        regex,
        starts_with,
        numeric_eq,
        numeric_gt,
        numeric_lt
    };

    //
    // Field match specification.
    //
    struct AuditFieldMatch {
        std::string field_name;
        std::string pattern;
        AuditMatchType match_type{AuditMatchType::contains};
        bool negate{false};
        std::optional<std::regex> compiled_regex;  // Compiled if match_type == regex
    };

    //
    // Action to take when an audit rule matches.
    //
    enum class AuditRuleAction : std::uint8_t {
        process_execution,
        network_connection,
        failed_access,
        privilege_change,
        suspicious_syscall
    };

    //
    // Audit rule definition.
    //
    struct AuditRule {
        std::string name;
        std::string description;
        std::vector<AuditFieldMatch> field_matches;  // ANDed together
        AuditRuleAction action{AuditRuleAction::suspicious_syscall};
        EventSeverity severity{EventSeverity::warning};
        bool enabled{true};
        std::uint32_t syscall_filter{0};  // 0 = no filter, otherwise specific syscall number
    };

    //
    // Check if a field match matches an audit event accumulator.
    //
    [[nodiscard]] auto matches_field(AuditFieldMatch const& match,
                                      AuditEventAccumulator const& event)
        -> bool;

    //
    // Check if a rule matches an audit event accumulator.
    //
    [[nodiscard]] auto matches_rule(AuditRule const& rule,
                                     AuditEventAccumulator const& event)
        -> bool;

    //
    // Get default audit rules for common suspicious activities.
    //
    [[nodiscard]] auto get_default_audit_rules() -> std::vector<AuditRule>;

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_AUDIT_RULE_H
