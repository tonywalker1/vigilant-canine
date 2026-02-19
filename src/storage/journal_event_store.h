//
// vigilant-canine - Journal Event Storage
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_STORAGE_JOURNAL_EVENT_STORE_H
#define VIGILANT_CANINE_STORAGE_JOURNAL_EVENT_STORE_H

#include <storage/database.h>

#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace vigilant_canine {

    //
    // Journal event record.
    //
    struct JournalEventRecord {
        std::int64_t id{0};
        std::string rule_name;
        std::string message;
        int priority{0};
        std::optional<std::string> unit_name;
        std::string created_at;
    };

    //
    // Storage interface for journal events.
    //
    class JournalEventStore {
    public:
        explicit JournalEventStore(Database& database) : m_db(database) {}

        //
        // Get recent journal events (most recent first).
        //
        [[nodiscard]] auto get_recent(int limit = 100)
            -> std::expected<std::vector<JournalEventRecord>, std::string>;

        //
        // Delete journal events older than specified days.
        //
        [[nodiscard]] auto prune_old_events(int days = 30)
            -> std::expected<void, std::string>;

    private:
        Database& m_db;
    };

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_STORAGE_JOURNAL_EVENT_STORE_H
