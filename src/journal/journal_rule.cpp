//
// vigilant-canine - Journal Rule Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <journal/journal_rule.h>

#include <algorithm>

namespace vigilant_canine {

    auto matches_field(JournalFieldMatch const& match,
                       JournalEntry const& entry) -> bool {
        // Get field value from entry
        std::string field_value;

        if (match.field_name == journal_fields::MESSAGE) {
            field_value = entry.message;
        } else if (match.field_name == journal_fields::SYSLOG_IDENTIFIER) {
            field_value = entry.syslog_identifier;
        } else if (match.field_name == journal_fields::SYSTEMD_UNIT) {
            field_value = entry.systemd_unit;
        } else if (match.field_name == journal_fields::COMM) {
            field_value = entry.comm;
        } else if (match.field_name == journal_fields::EXE) {
            field_value = entry.exe;
        } else {
            // Check raw_fields for other fields
            auto it = entry.raw_fields.find(match.field_name);
            if (it != entry.raw_fields.end()) {
                field_value = it->second;
            }
        }

        // Perform match based on type
        bool result = false;
        switch (match.match_type) {
        case JournalMatchType::exact:
            result = (field_value == match.pattern);
            break;

        case JournalMatchType::contains:
            result = (field_value.find(match.pattern) != std::string::npos);
            break;

        case JournalMatchType::starts_with:
            result = (field_value.starts_with(match.pattern));
            break;

        case JournalMatchType::regex:
            if (match.compiled_regex) {
                result = std::regex_search(field_value, *match.compiled_regex);
            }
            break;
        }

        // Apply negation if requested
        return match.negate ? !result : result;
    }

    auto matches_rule(JournalRule const& rule,
                      JournalEntry const& entry) -> bool {
        if (!rule.enabled) {
            return false;
        }

        // All field matches must succeed (AND)
        return std::all_of(rule.field_matches.begin(), rule.field_matches.end(),
            [&](auto const& match) { return matches_field(match, entry); });
    }

    auto get_default_rules() -> std::vector<JournalRule> {
        std::vector<JournalRule> rules;

        // Rule 1: SSH authentication failures
        rules.push_back(JournalRule{
            .name = "ssh_auth_failure",
            .description = "SSH authentication failures",
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
            .severity = EventSeverity::warning,
            .enabled = true
        });

        // Rule 2: SSH invalid user
        rules.push_back(JournalRule{
            .name = "ssh_invalid_user",
            .description = "SSH invalid user attempts",
            .field_matches = {
                JournalFieldMatch{
                    .field_name = journal_fields::SYSLOG_IDENTIFIER,
                    .pattern = "sshd",
                    .match_type = JournalMatchType::exact
                },
                JournalFieldMatch{
                    .field_name = journal_fields::MESSAGE,
                    .pattern = "Invalid user",
                    .match_type = JournalMatchType::contains
                }
            },
            .action = JournalRuleAction::auth_failure,
            .severity = EventSeverity::warning,
            .enabled = true
        });

        // Rule 3: Sudo authentication failures
        rules.push_back(JournalRule{
            .name = "sudo_auth_failure",
            .description = "Sudo authentication failures",
            .field_matches = {
                JournalFieldMatch{
                    .field_name = journal_fields::SYSLOG_IDENTIFIER,
                    .pattern = "sudo",
                    .match_type = JournalMatchType::exact
                },
                JournalFieldMatch{
                    .field_name = journal_fields::MESSAGE,
                    .pattern = "authentication failure",
                    .match_type = JournalMatchType::contains
                }
            },
            .action = JournalRuleAction::auth_failure,
            .severity = EventSeverity::warning,
            .enabled = true
        });

        // Rule 4: Successful sudo commands
        rules.push_back(JournalRule{
            .name = "sudo_command",
            .description = "Successful sudo privilege escalation",
            .field_matches = {
                JournalFieldMatch{
                    .field_name = journal_fields::SYSLOG_IDENTIFIER,
                    .pattern = "sudo",
                    .match_type = JournalMatchType::exact
                },
                JournalFieldMatch{
                    .field_name = journal_fields::MESSAGE,
                    .pattern = "COMMAND=",
                    .match_type = JournalMatchType::contains
                }
            },
            .action = JournalRuleAction::privilege_escalation,
            .severity = EventSeverity::info,
            .enabled = true
        });

        // Rule 5: Su sessions
        rules.push_back(JournalRule{
            .name = "su_session",
            .description = "Su privilege escalation",
            .field_matches = {
                JournalFieldMatch{
                    .field_name = journal_fields::SYSLOG_IDENTIFIER,
                    .pattern = "su",
                    .match_type = JournalMatchType::exact
                },
                JournalFieldMatch{
                    .field_name = journal_fields::MESSAGE,
                    .pattern = "session opened",
                    .match_type = JournalMatchType::contains
                }
            },
            .action = JournalRuleAction::privilege_escalation,
            .severity = EventSeverity::info,
            .enabled = true
        });

        // Rule 6: Service failures
        rules.push_back(JournalRule{
            .name = "service_failed",
            .description = "Systemd service failures",
            .field_matches = {
                JournalFieldMatch{
                    .field_name = journal_fields::MESSAGE,
                    .pattern = "Failed to start",
                    .match_type = JournalMatchType::contains
                }
            },
            .action = JournalRuleAction::service_state,
            .severity = EventSeverity::warning,
            .enabled = true
        });

        // Rule 7: Kernel segfaults
        rules.push_back(JournalRule{
            .name = "kernel_segfault",
            .description = "Kernel segmentation faults",
            .field_matches = {
                JournalFieldMatch{
                    .field_name = journal_fields::SYSLOG_IDENTIFIER,
                    .pattern = "kernel",
                    .match_type = JournalMatchType::exact
                },
                JournalFieldMatch{
                    .field_name = journal_fields::MESSAGE,
                    .pattern = "segfault",
                    .match_type = JournalMatchType::contains
                }
            },
            .action = JournalRuleAction::suspicious_log,
            .severity = EventSeverity::warning,
            .enabled = true
        });

        // Rule 8: PAM authentication failures
        rules.push_back(JournalRule{
            .name = "pam_auth_failure",
            .description = "PAM authentication failures",
            .field_matches = {
                JournalFieldMatch{
                    .field_name = journal_fields::MESSAGE,
                    .pattern = "pam_unix.*authentication failure",
                    .match_type = JournalMatchType::regex,
                    .compiled_regex = std::regex("pam_unix.*authentication failure")
                }
            },
            .action = JournalRuleAction::auth_failure,
            .severity = EventSeverity::warning,
            .enabled = true
        });

        // Rule 9: Polkit authentication
        rules.push_back(JournalRule{
            .name = "polkit_auth",
            .description = "Polkit authentication requests",
            .field_matches = {
                JournalFieldMatch{
                    .field_name = journal_fields::SYSLOG_IDENTIFIER,
                    .pattern = "polkitd",
                    .match_type = JournalMatchType::exact
                },
                JournalFieldMatch{
                    .field_name = journal_fields::MESSAGE,
                    .pattern = "Registered Authentication Agent",
                    .match_type = JournalMatchType::contains
                }
            },
            .action = JournalRuleAction::privilege_escalation,
            .severity = EventSeverity::info,
            .enabled = true
        });

        // Rule 10: Pkexec execution
        rules.push_back(JournalRule{
            .name = "pkexec_command",
            .description = "Pkexec privilege escalation",
            .field_matches = {
                JournalFieldMatch{
                    .field_name = journal_fields::COMM,
                    .pattern = "pkexec",
                    .match_type = JournalMatchType::exact
                }
            },
            .action = JournalRuleAction::privilege_escalation,
            .severity = EventSeverity::info,
            .enabled = true
        });

        return rules;
    }

}  // namespace vigilant_canine
