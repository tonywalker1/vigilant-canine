//
// vigilant-canine - Audit Record Parsing
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_AUDIT_PARSING_H
#define VIGILANT_CANINE_AUDIT_PARSING_H

#include <audit/audit_fields.h>

#include <auparse.h>

#include <chrono>
#include <expected>
#include <string>

namespace vigilant_canine {

    //
    // Configuration for command-line sanitization.
    //
    struct SanitizationConfig {
        bool enabled{true};
        // Patterns are hardcoded for security-critical use
    };

    //
    // Parse a SYSCALL record from auparse state.
    //
    [[nodiscard]] auto parse_syscall_record(auparse_state_t* au)
        -> std::expected<SyscallRecord, std::string>;

    //
    // Parse an EXECVE record from auparse state.
    //
    [[nodiscard]] auto parse_execve_record(auparse_state_t* au)
        -> std::expected<ExecveRecord, std::string>;

    //
    // Parse a CWD record from auparse state.
    //
    [[nodiscard]] auto parse_cwd_record(auparse_state_t* au)
        -> std::expected<CwdRecord, std::string>;

    //
    // Parse a PATH record from auparse state.
    //
    [[nodiscard]] auto parse_path_record(auparse_state_t* au)
        -> std::expected<PathRecord, std::string>;

    //
    // Extract audit ID from auparse state.
    //
    [[nodiscard]] auto get_audit_id(auparse_state_t* au) -> std::uint64_t;

    //
    // Sanitize command line by removing common password patterns.
    //
    // Patterns removed:
    // - -p'password', --password=secret
    // - -u user:pass@, --user=user:pass
    // - Environment variables with SECRET/PASSWORD/TOKEN in name
    //
    [[nodiscard]] auto sanitize_command_line(std::string const& cmdline,
                                               SanitizationConfig config = {})
        -> std::string;

    //
    // Join argv into a single command line string.
    //
    [[nodiscard]] auto join_argv(std::vector<std::string> const& argv)
        -> std::string;

    //
    // Check if an audit event accumulator is complete.
    //
    // Complete means: has syscall record and either execve or at least one path.
    //
    [[nodiscard]] auto is_event_complete(AuditEventAccumulator const& event)
        -> bool;

    //
    // Get timeout for incomplete audit events (milliseconds).
    //
    inline constexpr std::chrono::milliseconds INCOMPLETE_EVENT_TIMEOUT{100};

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_AUDIT_PARSING_H
