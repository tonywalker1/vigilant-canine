//
// vigilant-canine - Journal Field Definitions
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_JOURNAL_FIELDS_H
#define VIGILANT_CANINE_JOURNAL_FIELDS_H

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace vigilant_canine {

    //
    // Standard journal field names.
    //
    namespace journal_fields {
        inline constexpr auto MESSAGE = "MESSAGE";
        inline constexpr auto PRIORITY = "PRIORITY";
        inline constexpr auto SYSLOG_IDENTIFIER = "SYSLOG_IDENTIFIER";
        inline constexpr auto SYSTEMD_UNIT = "_SYSTEMD_UNIT";
        inline constexpr auto PID = "_PID";
        inline constexpr auto UID = "_UID";
        inline constexpr auto COMM = "_COMM";
        inline constexpr auto EXE = "_EXE";
    }  // namespace journal_fields

    //
    // Intermediate representation of a journal entry.
    // Extracted from sd_journal for rule matching.
    //
    struct JournalEntry {
        std::string message;
        std::uint8_t priority{6};                     // LOG_INFO default
        std::string syslog_identifier;
        std::string systemd_unit;
        std::optional<std::uint32_t> pid;
        std::optional<std::uint32_t> uid;
        std::string comm;
        std::string exe;
        std::chrono::system_clock::time_point timestamp;
        std::unordered_map<std::string, std::string> raw_fields;
    };

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_JOURNAL_FIELDS_H
