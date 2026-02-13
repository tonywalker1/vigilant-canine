//
// vigilant-canine - Audit Record Parsing Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <audit/audit_parsing.h>

#include <auparse.h>
#include <charconv>
#include <format>
#include <regex>

namespace vigilant_canine {

    namespace {

        //
        // Get string field from auparse state.
        //
        auto get_field_str(auparse_state_t* au, const char* field_name)
            -> std::optional<std::string> {
            if (auparse_find_field(au, field_name)) {
                const char* value = auparse_get_field_str(au);
                if (value) {
                    return std::string(value);
                }
            }
            return std::nullopt;
        }

        //
        // Get integer field from auparse state.
        //
        template<typename T>
        auto get_field_int(auparse_state_t* au, const char* field_name)
            -> std::optional<T> {
            if (auparse_find_field(au, field_name)) {
                const char* value = auparse_get_field_str(au);
                if (value) {
                    T result{};
                    auto str = std::string_view(value);
                    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), result);
                    if (ec == std::errc{}) {
                        return result;
                    }
                }
            }
            return std::nullopt;
        }

    }  // namespace

    auto parse_syscall_record(auparse_state_t* au)
        -> std::expected<SyscallRecord, std::string> {
        SyscallRecord record;

        // Extract audit ID
        record.audit_id = get_audit_id(au);

        // Extract PID
        if (auto pid = get_field_int<std::uint32_t>(au, "pid")) {
            record.pid = *pid;
        } else {
            return std::unexpected("Missing PID field");
        }

        // Extract PPID
        if (auto ppid = get_field_int<std::uint32_t>(au, "ppid")) {
            record.ppid = *ppid;
        }

        // Extract UIDs
        if (auto uid = get_field_int<std::uint32_t>(au, "uid")) {
            record.uid = *uid;
        }
        if (auto euid = get_field_int<std::uint32_t>(au, "euid")) {
            record.euid = *euid;
        }

        // Extract GIDs
        if (auto gid = get_field_int<std::uint32_t>(au, "gid")) {
            record.gid = *gid;
        }
        if (auto egid = get_field_int<std::uint32_t>(au, "egid")) {
            record.egid = *egid;
        }

        // Extract comm
        if (auto comm = get_field_str(au, "comm")) {
            record.comm = *comm;
        }

        // Extract exe
        if (auto exe = get_field_str(au, "exe")) {
            record.exe = *exe;
        }

        // Extract syscall number
        if (auto syscall = get_field_int<std::uint32_t>(au, "syscall")) {
            record.syscall = *syscall;
        }

        // Extract success
        if (auto success = get_field_str(au, "success")) {
            record.success = *success;
        }

        // Extract exit code
        if (auto exit_code = get_field_int<std::int32_t>(au, "exit")) {
            record.exit_code = *exit_code;
        }

        return record;
    }

    auto parse_execve_record(auparse_state_t* au)
        -> std::expected<ExecveRecord, std::string> {
        ExecveRecord record;

        record.audit_id = get_audit_id(au);

        // EXECVE records have argc and a0, a1, a2, ... fields
        auto argc = get_field_int<std::uint32_t>(au, "argc");
        if (!argc || *argc == 0) {
            return std::unexpected("Missing or zero argc");
        }

        // Extract arguments
        for (std::uint32_t i = 0; i < *argc; ++i) {
            auto field_name = std::format("a{}", i);
            if (auto arg = get_field_str(au, field_name.c_str())) {
                record.argv.push_back(*arg);
            }
        }

        return record;
    }

    auto parse_cwd_record(auparse_state_t* au)
        -> std::expected<CwdRecord, std::string> {
        CwdRecord record;

        record.audit_id = get_audit_id(au);

        if (auto cwd = get_field_str(au, "cwd")) {
            record.cwd = *cwd;
        } else {
            return std::unexpected("Missing cwd field");
        }

        return record;
    }

    auto parse_path_record(auparse_state_t* au)
        -> std::expected<PathRecord, std::string> {
        PathRecord record;

        record.audit_id = get_audit_id(au);

        if (auto name = get_field_str(au, "name")) {
            record.name = *name;
        } else {
            return std::unexpected("Missing name field");
        }

        if (auto nametype = get_field_str(au, "nametype")) {
            record.nametype = *nametype;
        }

        return record;
    }

    auto get_audit_id(auparse_state_t* au) -> std::uint64_t {
        const au_event_t* event = auparse_get_timestamp(au);
        if (event) {
            return event->serial;
        }
        return 0;
    }

    auto sanitize_command_line(std::string const& cmdline,
                                SanitizationConfig config)
        -> std::string {
        if (!config.enabled) {
            return cmdline;
        }

        std::string sanitized = cmdline;

        // Pattern 1: --password=secret, --password secret (must come before -p pattern)
        static const std::regex password_long_pattern(
            R"(--password[=\s]+[^\s]+)",
            std::regex::optimize
        );
        sanitized = std::regex_replace(sanitized, password_long_pattern, "--password=[REDACTED]");

        // Pattern 2: -p'password', -p"password", -ppassword (short form only)
        static const std::regex password_flag_pattern(
            R"(\s-p\s*['""]?[^\s'""]+['""]?)",
            std::regex::optimize
        );
        sanitized = std::regex_replace(sanitized, password_flag_pattern, " -p'[REDACTED]'");

        // Pattern 3: user:pass@ in URLs (after ://)
        static const std::regex url_userpass_pattern(
            R"(://([a-zA-Z0-9_-]+):([^@\s]+)@)",
            std::regex::optimize
        );
        sanitized = std::regex_replace(sanitized, url_userpass_pattern, "://$1:[REDACTED]@");

        // Pattern 4: Environment variables with sensitive names
        static const std::regex env_secret_pattern(
            R"(((?:SECRET|PASSWORD|TOKEN|KEY|APIKEY|AUTH)[A-Z_]*)=[^\s]+)",
            std::regex::optimize
        );
        sanitized = std::regex_replace(sanitized, env_secret_pattern, "$1=[REDACTED]");

        // Pattern 5: --token=value, --api-key=value
        static const std::regex token_pattern(
            R"(--(token|api-?key|auth-?key)[=\s]+[^\s]+)",
            std::regex::optimize | std::regex::icase
        );
        sanitized = std::regex_replace(sanitized, token_pattern, "--$1=[REDACTED]");

        return sanitized;
    }

    auto join_argv(std::vector<std::string> const& argv)
        -> std::string {
        std::string result;
        for (size_t i = 0; i < argv.size(); ++i) {
            if (i > 0) {
                result += " ";
            }
            // Quote arguments with spaces
            if (argv[i].find(' ') != std::string::npos) {
                result += "\"" + argv[i] + "\"";
            } else {
                result += argv[i];
            }
        }
        return result;
    }

    auto is_event_complete(AuditEventAccumulator const& event)
        -> bool {
        // Must have syscall record
        if (!event.syscall.has_value()) {
            return false;
        }

        // Must have either execve or at least one path
        return event.execve.has_value() || !event.paths.empty();
    }

}  // namespace vigilant_canine
