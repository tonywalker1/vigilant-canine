//
// vigilant-canine - Journal Rule Definitions
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_JOURNAL_RULE_H
#define VIGILANT_CANINE_JOURNAL_RULE_H

#include <events/event.h>
#include <journal/journal_fields.h>

#include <optional>
#include <regex>
#include <string>
#include <vector>

namespace vigilant_canine {

    //
    // Match type for journal field matching.
    //
    enum class JournalMatchType : std::uint8_t {
        exact,
        contains,
        regex,
        starts_with
    };

    //
    // Field match specification.
    //
    struct JournalFieldMatch {
        std::string field_name;
        std::string pattern;
        JournalMatchType match_type{JournalMatchType::contains};
        bool negate{false};
        std::optional<std::regex> compiled_regex;  // Compiled if match_type == regex
    };

    //
    // Action to take when a rule matches.
    //
    enum class JournalRuleAction : std::uint8_t {
        auth_failure,
        privilege_escalation,
        service_state,
        suspicious_log
    };

    //
    // Journal rule definition.
    //
    struct JournalRule {
        std::string name;
        std::string description;
        std::vector<JournalFieldMatch> field_matches;  // ANDed together
        JournalRuleAction action{JournalRuleAction::suspicious_log};
        EventSeverity severity{EventSeverity::warning};
        bool enabled{true};
    };

    //
    // Check if a field match matches a journal entry.
    //
    [[nodiscard]] auto matches_field(JournalFieldMatch const& match,
                                      JournalEntry const& entry)
        -> bool;

    //
    // Check if a rule matches a journal entry.
    //
    [[nodiscard]] auto matches_rule(JournalRule const& rule,
                                     JournalEntry const& entry)
        -> bool;

    //
    // Get default journal rules for common log patterns.
    //
    [[nodiscard]] auto get_default_rules() -> std::vector<JournalRule>;

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_JOURNAL_RULE_H
