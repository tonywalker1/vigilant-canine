//
// vigilant-canine - Audit Rule Tests
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <audit/audit_rule.h>

#include <gtest/gtest.h>

using namespace vigilant_canine;

TEST(AuditRuleTest, ExactMatchSuccess) {
    AuditFieldMatch match{
        .field_name = "comm",
        .pattern = "gcc",
        .match_type = AuditMatchType::exact,
        .negate = false,
    };

    AuditEventAccumulator event;
    event.syscall = SyscallRecord{.comm = "gcc"};

    EXPECT_TRUE(matches_field(match, event));
}

TEST(AuditRuleTest, ExactMatchFailure) {
    AuditFieldMatch match{
        .field_name = "comm",
        .pattern = "gcc",
        .match_type = AuditMatchType::exact,
        .negate = false,
    };

    AuditEventAccumulator event;
    event.syscall = SyscallRecord{.comm = "clang"};

    EXPECT_FALSE(matches_field(match, event));
}

TEST(AuditRuleTest, ContainsMatchSuccess) {
    AuditFieldMatch match{
        .field_name = "exe",
        .pattern = "python",
        .match_type = AuditMatchType::contains,
        .negate = false,
    };

    AuditEventAccumulator event;
    event.syscall = SyscallRecord{.exe = "/usr/bin/python3.11"};

    EXPECT_TRUE(matches_field(match, event));
}

TEST(AuditRuleTest, RegexMatchSuccess) {
    AuditFieldMatch match{
        .field_name = "comm",
        .pattern = "gcc|clang|g\\+\\+",
        .match_type = AuditMatchType::regex,
        .negate = false,
        .compiled_regex = std::regex("gcc|clang|g\\+\\+"),
    };

    AuditEventAccumulator event1;
    event1.syscall = SyscallRecord{.comm = "gcc"};
    EXPECT_TRUE(matches_field(match, event1));

    AuditEventAccumulator event2;
    event2.syscall = SyscallRecord{.comm = "clang"};
    EXPECT_TRUE(matches_field(match, event2));

    AuditEventAccumulator event3;
    event3.syscall = SyscallRecord{.comm = "g++"};
    EXPECT_TRUE(matches_field(match, event3));

    AuditEventAccumulator event4;
    event4.syscall = SyscallRecord{.comm = "rustc"};
    EXPECT_FALSE(matches_field(match, event4));
}

TEST(AuditRuleTest, NumericEqualMatch) {
    AuditFieldMatch match{
        .field_name = "uid",
        .pattern = "0",
        .match_type = AuditMatchType::numeric_eq,
        .negate = false,
    };

    AuditEventAccumulator event1;
    event1.syscall = SyscallRecord{.uid = 0};
    EXPECT_TRUE(matches_field(match, event1));

    AuditEventAccumulator event2;
    event2.syscall = SyscallRecord{.uid = 1000};
    EXPECT_FALSE(matches_field(match, event2));
}

TEST(AuditRuleTest, NumericGreaterThan) {
    AuditFieldMatch match{
        .field_name = "uid",
        .pattern = "1000",
        .match_type = AuditMatchType::numeric_gt,
        .negate = false,
    };

    AuditEventAccumulator event1;
    event1.syscall = SyscallRecord{.uid = 2000};
    EXPECT_TRUE(matches_field(match, event1));

    AuditEventAccumulator event2;
    event2.syscall = SyscallRecord{.uid = 500};
    EXPECT_FALSE(matches_field(match, event2));
}

TEST(AuditRuleTest, NegatedMatch) {
    AuditFieldMatch match{
        .field_name = "comm",
        .pattern = "systemd",
        .match_type = AuditMatchType::exact,
        .negate = true,  // Match if NOT systemd
    };

    AuditEventAccumulator event1;
    event1.syscall = SyscallRecord{.comm = "bash"};
    EXPECT_TRUE(matches_field(match, event1));

    AuditEventAccumulator event2;
    event2.syscall = SyscallRecord{.comm = "systemd"};
    EXPECT_FALSE(matches_field(match, event2));
}

TEST(AuditRuleTest, RuleWithMultipleFieldsAllMatch) {
    AuditRule rule{
        .name = "test_rule",
        .field_matches = {
            AuditFieldMatch{
                .field_name = "comm",
                .pattern = "gcc",
                .match_type = AuditMatchType::exact,
            },
            AuditFieldMatch{
                .field_name = "uid",
                .pattern = "0",
                .match_type = AuditMatchType::numeric_eq,
            },
        },
        .enabled = true,
    };

    AuditEventAccumulator event;
    event.syscall = SyscallRecord{.uid = 0, .comm = "gcc"};

    EXPECT_TRUE(matches_rule(rule, event));
}

TEST(AuditRuleTest, RuleWithMultipleFieldsPartialMatch) {
    AuditRule rule{
        .name = "test_rule",
        .field_matches = {
            AuditFieldMatch{
                .field_name = "comm",
                .pattern = "gcc",
                .match_type = AuditMatchType::exact,
            },
            AuditFieldMatch{
                .field_name = "uid",
                .pattern = "0",
                .match_type = AuditMatchType::numeric_eq,
            },
        },
        .enabled = true,
    };

    // Matches comm but not uid
    AuditEventAccumulator event;
    event.syscall = SyscallRecord{.uid = 1000, .comm = "gcc"};

    EXPECT_FALSE(matches_rule(rule, event));
}

TEST(AuditRuleTest, DisabledRuleNeverMatches) {
    AuditRule rule{
        .name = "test_rule",
        .field_matches = {
            AuditFieldMatch{
                .field_name = "comm",
                .pattern = "gcc",
                .match_type = AuditMatchType::exact,
            },
        },
        .enabled = false,  // Disabled
    };

    AuditEventAccumulator event;
    event.syscall = SyscallRecord{.comm = "gcc"};

    EXPECT_FALSE(matches_rule(rule, event));
}

TEST(AuditRuleTest, SyscallFilterMatch) {
    AuditRule rule{
        .name = "test_rule",
        .field_matches = {},
        .enabled = true,
        .syscall_filter = 105,  // setuid
    };

    AuditEventAccumulator event1;
    event1.syscall = SyscallRecord{.syscall = 105};
    EXPECT_TRUE(matches_rule(rule, event1));

    AuditEventAccumulator event2;
    event2.syscall = SyscallRecord{.syscall = 106};
    EXPECT_FALSE(matches_rule(rule, event2));
}

TEST(AuditRuleTest, DefaultRulesExist) {
    auto rules = get_default_audit_rules();

    EXPECT_FALSE(rules.empty());
    EXPECT_GE(rules.size(), 10);  // Should have at least 10 default rules

    // Check that compiler_execution rule exists
    auto it = std::find_if(rules.begin(), rules.end(), [](auto const& r) {
        return r.name == "compiler_execution";
    });
    EXPECT_NE(it, rules.end());

    // Check that privileged_command rule exists
    it = std::find_if(rules.begin(), rules.end(), [](auto const& r) {
        return r.name == "privileged_command";
    });
    EXPECT_NE(it, rules.end());
}

TEST(AuditRuleTest, DefaultRulesHaveValidRegex) {
    auto rules = get_default_audit_rules();

    for (auto const& rule : rules) {
        for (auto const& match : rule.field_matches) {
            if (match.match_type == AuditMatchType::regex) {
                // Should have compiled_regex
                EXPECT_TRUE(match.compiled_regex.has_value())
                    << "Rule: " << rule.name << " missing compiled regex";
            }
        }
    }
}
