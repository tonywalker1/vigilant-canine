//
// vigilant-canine - Audit Rule Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <audit/audit_rule.h>

#include <charconv>
#include <pwd.h>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace vigilant_canine {

    namespace {

        //
        // Get field value from audit event accumulator.
        //
        auto get_field_value(AuditEventAccumulator const& event,
                             std::string const& field_name)
            -> std::optional<std::string> {
            // Check syscall record fields
            if (event.syscall.has_value()) {
                auto const& syscall = *event.syscall;
                if (field_name == "pid") {
                    return std::to_string(syscall.pid);
                }
                if (field_name == "ppid") {
                    return std::to_string(syscall.ppid);
                }
                if (field_name == "uid") {
                    return std::to_string(syscall.uid);
                }
                if (field_name == "euid") {
                    return std::to_string(syscall.euid);
                }
                if (field_name == "comm") {
                    return syscall.comm;
                }
                if (field_name == "exe") {
                    return syscall.exe;
                }
                if (field_name == "syscall") {
                    return std::to_string(syscall.syscall);
                }
                if (field_name == "success") {
                    return syscall.success;
                }
                if (field_name == "exit") {
                    return std::to_string(syscall.exit_code);
                }
            }

            // Check CWD
            if (event.cwd.has_value() && field_name == "cwd") {
                return event.cwd->cwd;
            }

            // Check EXECVE (command line)
            if (event.execve.has_value() && field_name == "cmdline") {
                // Join argv with spaces
                std::string cmdline;
                for (auto const& arg : event.execve->argv) {
                    if (!cmdline.empty()) {
                        cmdline += " ";
                    }
                    cmdline += arg;
                }
                return cmdline;
            }

            // Check PATH records
            if (field_name == "path" && !event.paths.empty()) {
                return event.paths[0].name;  // Return first path
            }

            // Check network fields
            if (event.network.has_value()) {
                auto const& net = *event.network;
                if (field_name == "saddr") {
                    return net.local_addr;
                }
                if (field_name == "daddr") {
                    return net.remote_addr;
                }
                if (field_name == "sport") {
                    return std::to_string(net.local_port);
                }
                if (field_name == "dport") {
                    return std::to_string(net.remote_port);
                }
                if (field_name == "protocol") {
                    return net.protocol;
                }
            }

            // Check raw fields
            auto it = event.raw_fields.find(field_name);
            if (it != event.raw_fields.end()) {
                return it->second;
            }

            return std::nullopt;
        }

        //
        // Parse numeric value from string.
        //
        auto parse_numeric(std::string const& value) -> std::optional<std::int64_t> {
            std::int64_t result{};
            auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
            if (ec == std::errc{}) {
                return result;
            }
            return std::nullopt;
        }

    }  // namespace

    auto matches_field(AuditFieldMatch const& match,
                       AuditEventAccumulator const& event)
        -> bool {
        auto field_value = get_field_value(event, match.field_name);
        if (!field_value) {
            return match.negate;  // Field not present
        }

        bool result = false;

        switch (match.match_type) {
        case AuditMatchType::exact:
            result = (*field_value == match.pattern);
            break;

        case AuditMatchType::contains:
            result = (field_value->find(match.pattern) != std::string::npos);
            break;

        case AuditMatchType::regex:
            if (match.compiled_regex) {
                result = std::regex_search(*field_value, *match.compiled_regex);
            }
            break;

        case AuditMatchType::starts_with:
            result = (field_value->starts_with(match.pattern));
            break;

        case AuditMatchType::numeric_eq:
        case AuditMatchType::numeric_gt:
        case AuditMatchType::numeric_lt: {
            auto field_num = parse_numeric(*field_value);
            auto pattern_num = parse_numeric(match.pattern);
            if (field_num && pattern_num) {
                if (match.match_type == AuditMatchType::numeric_eq) {
                    result = (*field_num == *pattern_num);
                } else if (match.match_type == AuditMatchType::numeric_gt) {
                    result = (*field_num > *pattern_num);
                } else {
                    result = (*field_num < *pattern_num);
                }
            }
            break;
        }
        }

        return match.negate ? !result : result;
    }

    auto matches_rule(AuditRule const& rule,
                      AuditEventAccumulator const& event)
        -> bool {
        if (!rule.enabled) {
            return false;
        }

        // Check syscall filter
        if (rule.syscall_filter != 0) {
            if (!event.syscall.has_value() ||
                event.syscall->syscall != rule.syscall_filter) {
                return false;
            }
        }

        // All field matches must succeed (AND logic)
        for (auto const& match : rule.field_matches) {
            if (!matches_field(match, event)) {
                return false;
            }
        }

        return true;
    }

    auto get_default_audit_rules() -> std::vector<AuditRule> {
        std::vector<AuditRule> rules;

        // 1. Compiler/interpreter execution
        rules.push_back({
            .name = "compiler_execution",
            .description = "Detect execution of compilers and interpreters",
            .field_matches = {
                {
                    .field_name = "comm",
                    .pattern = "gcc|g\\+\\+|clang|python|perl|bash|sh",
                    .match_type = AuditMatchType::regex,
                    .negate = false,
                    .compiled_regex = std::regex("gcc|g\\+\\+|clang|python|perl|bash|sh"),
                }
            },
            .action = AuditRuleAction::process_execution,
            .severity = EventSeverity::info,
            .enabled = true,
        });

        // 2. Privileged command execution
        rules.push_back({
            .name = "privileged_command",
            .description = "Detect privileged command execution",
            .field_matches = {
                {
                    .field_name = "comm",
                    .pattern = "sudo|su|pkexec|doas",
                    .match_type = AuditMatchType::regex,
                    .negate = false,
                    .compiled_regex = std::regex("sudo|su|pkexec|doas"),
                }
            },
            .action = AuditRuleAction::privilege_change,
            .severity = EventSeverity::warning,
            .enabled = true,
        });

        // 3. Shell spawning by unexpected parents
        rules.push_back({
            .name = "suspicious_shell",
            .description = "Detect shells spawned by unusual parent processes",
            .field_matches = {
                {
                    .field_name = "comm",
                    .pattern = "bash|sh|zsh|fish",
                    .match_type = AuditMatchType::regex,
                    .negate = false,
                    .compiled_regex = std::regex("bash|sh|zsh|fish"),
                }
                // Additional parent process filtering would require correlation
            },
            .action = AuditRuleAction::suspicious_syscall,
            .severity = EventSeverity::warning,
            .enabled = false,  // Disabled by default - too noisy
        });

        // 4. Sensitive file access
        rules.push_back({
            .name = "sensitive_file_access",
            .description = "Detect access to sensitive system files",
            .field_matches = {
                {
                    .field_name = "path",
                    .pattern = "/etc/shadow|/etc/sudoers|/etc/passwd",
                    .match_type = AuditMatchType::regex,
                    .negate = false,
                    .compiled_regex = std::regex("/etc/shadow|/etc/sudoers|/etc/passwd"),
                }
            },
            .action = AuditRuleAction::process_execution,
            .severity = EventSeverity::warning,
            .enabled = true,
        });

        // 5. Failed file access (permission denied)
        rules.push_back({
            .name = "failed_access",
            .description = "Detect failed file access attempts (EACCES/EPERM)",
            .field_matches = {
                {
                    .field_name = "success",
                    .pattern = "no",
                    .match_type = AuditMatchType::exact,
                    .negate = false,
                },
                {
                    .field_name = "exit",
                    .pattern = "-13|-1",  // -EACCES or -EPERM
                    .match_type = AuditMatchType::regex,
                    .negate = false,
                    .compiled_regex = std::regex("-13|-1"),
                }
            },
            .action = AuditRuleAction::failed_access,
            .severity = EventSeverity::info,
            .enabled = true,
        });

        // 6. Network connections by root processes
        rules.push_back({
            .name = "root_network_connection",
            .description = "Detect network connections initiated by root",
            .field_matches = {
                {
                    .field_name = "uid",
                    .pattern = "0",
                    .match_type = AuditMatchType::numeric_eq,
                    .negate = false,
                }
            },
            .action = AuditRuleAction::network_connection,
            .severity = EventSeverity::warning,
            .enabled = false,  // Disabled by default - can be noisy
        });

        // 7. Setuid/setgid binary execution
        rules.push_back({
            .name = "setuid_execution",
            .description = "Detect execution of setuid/setgid binaries",
            .field_matches = {
                {
                    .field_name = "uid",
                    .pattern = "euid",  // Will compare uid != euid
                    .match_type = AuditMatchType::exact,
                    .negate = true,  // uid != euid means setuid
                }
            },
            .action = AuditRuleAction::privilege_change,
            .severity = EventSeverity::warning,
            .enabled = true,
        });

        // 8. Privilege escalation syscalls
        rules.push_back({
            .name = "privilege_escalation",
            .description = "Detect privilege escalation syscalls",
            .field_matches = {
                {
                    .field_name = "syscall",
                    .pattern = "105|106|117",  // setuid, setgid, setresuid
                    .match_type = AuditMatchType::regex,
                    .negate = false,
                    .compiled_regex = std::regex("105|106|117"),
                }
            },
            .action = AuditRuleAction::privilege_change,
            .severity = EventSeverity::warning,
            .enabled = true,
        });

        // 9. Kernel module loading
        rules.push_back({
            .name = "kernel_module_load",
            .description = "Detect kernel module loading",
            .field_matches = {
                {
                    .field_name = "syscall",
                    .pattern = "175|313",  // init_module, finit_module
                    .match_type = AuditMatchType::regex,
                    .negate = false,
                    .compiled_regex = std::regex("175|313"),
                }
            },
            .action = AuditRuleAction::suspicious_syscall,
            .severity = EventSeverity::critical,
            .enabled = true,
        });

        // 10. User management commands
        rules.push_back({
            .name = "user_management",
            .description = "Detect user management commands",
            .field_matches = {
                {
                    .field_name = "comm",
                    .pattern = "useradd|usermod|userdel|passwd|groupadd|groupmod|groupdel",
                    .match_type = AuditMatchType::regex,
                    .negate = false,
                    .compiled_regex = std::regex(
                        "useradd|usermod|userdel|passwd|groupadd|groupmod|groupdel"
                    ),
                }
            },
            .action = AuditRuleAction::process_execution,
            .severity = EventSeverity::warning,
            .enabled = true,
        });

        return rules;
    }

}  // namespace vigilant_canine
