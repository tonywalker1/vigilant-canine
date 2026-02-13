//
// vigilant-canine - Audit Field Definitions
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_AUDIT_FIELDS_H
#define VIGILANT_CANINE_AUDIT_FIELDS_H

#include <core/types.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace vigilant_canine {

    //
    // Standard audit field names from libaudit.
    //
    namespace audit_fields {
        // Process fields
        inline constexpr auto PID = "pid";
        inline constexpr auto PPID = "ppid";
        inline constexpr auto UID = "uid";
        inline constexpr auto EUID = "euid";
        inline constexpr auto GID = "gid";
        inline constexpr auto EGID = "egid";
        inline constexpr auto COMM = "comm";
        inline constexpr auto EXE = "exe";
        inline constexpr auto CWD = "cwd";

        // Syscall fields
        inline constexpr auto SYSCALL = "syscall";
        inline constexpr auto SUCCESS = "success";
        inline constexpr auto EXIT = "exit";

        // File/path fields
        inline constexpr auto NAME = "name";
        inline constexpr auto NAMETYPE = "nametype";

        // Network fields
        inline constexpr auto SADDR = "saddr";
        inline constexpr auto DADDR = "daddr";
        inline constexpr auto SPORT = "sport";
        inline constexpr auto DPORT = "dport";

        // Event metadata
        inline constexpr auto TYPE = "type";
        inline constexpr auto ARCH = "arch";
        inline constexpr auto AUID = "auid";
    }  // namespace audit_fields

    //
    // Syscall record from audit event.
    //
    struct SyscallRecord {
        std::uint64_t audit_id{0};
        std::uint32_t pid{0};
        std::uint32_t ppid{0};
        std::uint32_t uid{0};
        std::uint32_t euid{0};
        std::uint32_t gid{0};
        std::uint32_t egid{0};
        std::string comm;
        std::string exe;
        std::uint32_t syscall{0};
        std::string success{"yes"};
        std::int32_t exit_code{0};
    };

    //
    // EXECVE record (command-line arguments).
    //
    struct ExecveRecord {
        std::uint64_t audit_id{0};
        std::vector<std::string> argv;
    };

    //
    // CWD record (current working directory).
    //
    struct CwdRecord {
        std::uint64_t audit_id{0};
        std::string cwd;
    };

    //
    // PATH record (file/directory path).
    //
    struct PathRecord {
        std::uint64_t audit_id{0};
        std::string name;
        std::string nametype;  // "NORMAL", "CREATE", "DELETE", etc.
    };

    //
    // Network connection record.
    //
    struct NetworkRecord {
        std::uint64_t audit_id{0};
        std::string protocol;
        std::string local_addr;
        std::uint16_t local_port{0};
        std::string remote_addr;
        std::uint16_t remote_port{0};
    };

    //
    // Accumulated audit event across multiple records.
    //
    struct AuditEventAccumulator {
        std::uint64_t audit_id{0};
        std::chrono::steady_clock::time_point received;
        std::optional<SyscallRecord> syscall;
        std::optional<ExecveRecord> execve;
        std::optional<CwdRecord> cwd;
        std::vector<PathRecord> paths;
        std::optional<NetworkRecord> network;
        std::unordered_map<std::string, std::string> raw_fields;
    };

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_AUDIT_FIELDS_H
