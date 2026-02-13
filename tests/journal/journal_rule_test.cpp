//
// vigilant-canine - Journal Rule Tests
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <journal/journal_rule.h>

#include <gtest/gtest.h>

#include <algorithm>

namespace vigilant_canine {

    TEST(JournalRuleTest, ExactMatch) {
        JournalFieldMatch match{
            .field_name = journal_fields::SYSLOG_IDENTIFIER,
            .pattern = "sshd",
            .match_type = JournalMatchType::exact
        };

        JournalEntry entry{
            .syslog_identifier = "sshd"
        };

        EXPECT_TRUE(matches_field(match, entry));

        entry.syslog_identifier = "sshdaemon";
        EXPECT_FALSE(matches_field(match, entry));
    }

    TEST(JournalRuleTest, ContainsMatch) {
        JournalFieldMatch match{
            .field_name = journal_fields::MESSAGE,
            .pattern = "Failed password",
            .match_type = JournalMatchType::contains
        };

        JournalEntry entry{
            .message = "Failed password for invalid user admin from 10.0.0.1 port 22 ssh2"
        };

        EXPECT_TRUE(matches_field(match, entry));

        entry.message = "Accepted password for user from 10.0.0.1";
        EXPECT_FALSE(matches_field(match, entry));
    }

    TEST(JournalRuleTest, StartsWithMatch) {
        JournalFieldMatch match{
            .field_name = journal_fields::COMM,
            .pattern = "sudo",
            .match_type = JournalMatchType::starts_with
        };

        JournalEntry entry{
            .comm = "sudo"
        };

        EXPECT_TRUE(matches_field(match, entry));

        entry.comm = "sudoedit";
        EXPECT_TRUE(matches_field(match, entry));

        entry.comm = "my-sudo";
        EXPECT_FALSE(matches_field(match, entry));
    }

    TEST(JournalRuleTest, RegexMatch) {
        JournalFieldMatch match{
            .field_name = journal_fields::MESSAGE,
            .pattern = "pam_unix.*authentication failure",
            .match_type = JournalMatchType::regex,
            .compiled_regex = std::regex("pam_unix.*authentication failure")
        };

        JournalEntry entry{
            .message = "pam_unix(sshd:auth): authentication failure; logname= uid=0 euid=0 tty=ssh"
        };

        EXPECT_TRUE(matches_field(match, entry));

        entry.message = "pam_systemd: authentication success";
        EXPECT_FALSE(matches_field(match, entry));
    }

    TEST(JournalRuleTest, NegatedMatch) {
        JournalFieldMatch match{
            .field_name = journal_fields::SYSLOG_IDENTIFIER,
            .pattern = "systemd",
            .match_type = JournalMatchType::exact,
            .negate = true
        };

        JournalEntry entry{
            .syslog_identifier = "sshd"
        };

        EXPECT_TRUE(matches_field(match, entry));

        entry.syslog_identifier = "systemd";
        EXPECT_FALSE(matches_field(match, entry));
    }

    TEST(JournalRuleTest, RuleWithMultipleMatches) {
        JournalRule rule{
            .name = "ssh_auth_failure",
            .field_matches = {
                JournalFieldMatch{
                    .field_name = journal_fields::SYSLOG_IDENTIFIER,
                    .pattern = "sshd",
                    .match_type = JournalMatchType::exact
                },
                JournalFieldMatch{
                    .field_name = journal_fields::MESSAGE,
                    .pattern = "Failed password",
                    .match_type = JournalMatchType::contains
                }
            },
            .action = JournalRuleAction::auth_failure,
            .enabled = true
        };

        // Matches both conditions
        JournalEntry match_entry{
            .message = "Failed password for invalid user admin",
            .syslog_identifier = "sshd"
        };

        EXPECT_TRUE(matches_rule(rule, match_entry));

        // Matches first but not second
        JournalEntry partial_match{
            .message = "Accepted password",
            .syslog_identifier = "sshd"
        };

        EXPECT_FALSE(matches_rule(rule, partial_match));

        // Matches second but not first
        JournalEntry other_partial{
            .message = "Failed password for user",
            .syslog_identifier = "sudo"
        };

        EXPECT_FALSE(matches_rule(rule, other_partial));
    }

    TEST(JournalRuleTest, DisabledRule) {
        JournalRule rule{
            .name = "test_rule",
            .field_matches = {
                JournalFieldMatch{
                    .field_name = journal_fields::MESSAGE,
                    .pattern = "test",
                    .match_type = JournalMatchType::contains
                }
            },
            .enabled = false
        };

        JournalEntry entry{
            .message = "test message"
        };

        EXPECT_FALSE(matches_rule(rule, entry));
    }

    TEST(JournalRuleTest, DefaultRulesExist) {
        auto rules = get_default_rules();

        EXPECT_GE(rules.size(), 10);

        // Check that expected rules are present
        bool has_ssh_auth = std::any_of(rules.begin(), rules.end(),
            [](auto const& r) { return r.name == "ssh_auth_failure"; });
        bool has_sudo = std::any_of(rules.begin(), rules.end(),
            [](auto const& r) { return r.name == "sudo_command"; });
        bool has_segfault = std::any_of(rules.begin(), rules.end(),
            [](auto const& r) { return r.name == "kernel_segfault"; });

        EXPECT_TRUE(has_ssh_auth);
        EXPECT_TRUE(has_sudo);
        EXPECT_TRUE(has_segfault);
    }

    TEST(JournalRuleTest, RawFieldsMatch) {
        JournalFieldMatch match{
            .field_name = "_HOSTNAME",
            .pattern = "myserver",
            .match_type = JournalMatchType::exact
        };

        JournalEntry entry{
            .raw_fields = {{"_HOSTNAME", "myserver"}}
        };

        EXPECT_TRUE(matches_field(match, entry));

        entry.raw_fields = {{"_HOSTNAME", "otherserver"}};
        EXPECT_FALSE(matches_field(match, entry));
    }

    TEST(JournalRuleTest, PriorityField) {
        JournalEntry entry{
            .message = "Test message",
            .priority = 3  // LOG_ERR
        };

        EXPECT_EQ(entry.priority, 3);
    }

}  // namespace vigilant_canine
