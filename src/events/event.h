//
// vigilant-canine - Event System
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_EVENTS_EVENT_H
#define VIGILANT_CANINE_EVENTS_EVENT_H

#include <core/types.h>

#include <chrono>
#include <optional>
#include <string>
#include <variant>

namespace vigilant_canine {

    //
    // Event severity levels.
    //
    enum class EventSeverity : std::uint8_t {
        info,       // Informational (baseline refresh, scan complete)
        warning,    // Suspicious but possibly benign (permission change)
        critical    // Likely compromise (binary modification, unexpected file)
    };

    //
    // File modification event.
    //
    struct FileModifiedEvent {
        FilePath path;
        HashValue old_hash;
        HashValue new_hash;
        std::string change_description;
    };

    //
    // New file detected event.
    //
    struct FileCreatedEvent {
        FilePath path;
        HashValue hash;
        std::optional<std::string> source;  // Package, deployment, etc.
    };

    //
    // File deleted event.
    //
    struct FileDeletedEvent {
        FilePath path;
        HashValue last_known_hash;
    };

    //
    // Permission change event.
    //
    struct FilePermissionChangedEvent {
        FilePath path;
        std::uint32_t old_mode;
        std::uint32_t new_mode;
    };

    //
    // Scan completed event.
    //
    struct ScanCompletedEvent {
        std::filesystem::path scan_path;
        std::uint64_t files_scanned;
        std::uint64_t changes_detected;
        std::chrono::system_clock::duration elapsed;
    };

    //
    // System startup event.
    //
    struct SystemStartupEvent {
        std::string distro_name;
        DistroType distro_type;
    };

    //
    // Authentication failure event (Phase 2).
    //
    struct AuthFailureEvent {
        std::string username;
        std::string service;                     // "sshd", "sudo", "login"
        std::optional<std::string> remote_host;
        std::string message;
    };

    //
    // Privilege escalation event (Phase 2).
    //
    struct PrivilegeEscalationEvent {
        std::string username;
        std::string target_user;                 // e.g., "root"
        std::string method;                      // "sudo", "su", "pkexec"
        std::string command;
        std::string message;
    };

    //
    // Service state change event (Phase 2).
    //
    struct ServiceStateEvent {
        std::string unit_name;                   // e.g., "sshd.service"
        std::string new_state;                   // "started", "stopped", "failed"
        std::optional<std::string> exit_code;
        std::string message;
    };

    //
    // Suspicious log entry event (Phase 2).
    //
    struct SuspiciousLogEvent {
        std::string rule_name;                   // Rule that matched
        std::string unit_name;
        std::string message;
        std::uint8_t priority;                   // Journal PRIORITY (0-7)
    };

    //
    // Event variant containing all possible event types.
    //
    using EventData = std::variant<
        FileModifiedEvent,
        FileCreatedEvent,
        FileDeletedEvent,
        FilePermissionChangedEvent,
        ScanCompletedEvent,
        SystemStartupEvent,
        AuthFailureEvent,
        PrivilegeEscalationEvent,
        ServiceStateEvent,
        SuspiciousLogEvent
    >;

    //
    // Event wrapper with metadata.
    //
    struct Event {
        EventData data;
        EventSeverity severity;
        std::chrono::system_clock::time_point timestamp;
        std::string source;  // e.g., "scanner", "fanotify", "daemon"

        Event(EventData d, EventSeverity sev, std::string src)
            : data(std::move(d)),
              severity(sev),
              timestamp(std::chrono::system_clock::now()),
              source(std::move(src)) {}
    };

    //
    // Get human-readable event type name.
    //
    [[nodiscard]] auto event_type_name(EventData const& data) -> std::string_view;

    //
    // Get human-readable severity name.
    //
    [[nodiscard]] auto severity_name(EventSeverity severity) -> std::string_view;

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_EVENTS_EVENT_H
