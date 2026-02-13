//
// vigilant-canine - Event System Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <events/event.h>

namespace vigilant_canine {

    auto event_type_name(EventData const& data) -> std::string_view {
        return std::visit([](auto const& event) -> std::string_view {
            using T = std::decay_t<decltype(event)>;
            if constexpr (std::is_same_v<T, FileModifiedEvent>) {
                return "FileModified";
            } else if constexpr (std::is_same_v<T, FileCreatedEvent>) {
                return "FileCreated";
            } else if constexpr (std::is_same_v<T, FileDeletedEvent>) {
                return "FileDeleted";
            } else if constexpr (std::is_same_v<T, FilePermissionChangedEvent>) {
                return "FilePermissionChanged";
            } else if constexpr (std::is_same_v<T, ScanCompletedEvent>) {
                return "ScanCompleted";
            } else if constexpr (std::is_same_v<T, SystemStartupEvent>) {
                return "SystemStartup";
            } else if constexpr (std::is_same_v<T, AuthFailureEvent>) {
                return "AuthFailure";
            } else if constexpr (std::is_same_v<T, PrivilegeEscalationEvent>) {
                return "PrivilegeEscalation";
            } else if constexpr (std::is_same_v<T, ServiceStateEvent>) {
                return "ServiceState";
            } else if constexpr (std::is_same_v<T, SuspiciousLogEvent>) {
                return "SuspiciousLog";
            }
            return "Unknown";
        }, data);
    }

    auto severity_name(EventSeverity severity) -> std::string_view {
        switch (severity) {
        case EventSeverity::info:
            return "info";
        case EventSeverity::warning:
            return "warning";
        case EventSeverity::critical:
            return "critical";
        }
        return "unknown";
    }

}  // namespace vigilant_canine
