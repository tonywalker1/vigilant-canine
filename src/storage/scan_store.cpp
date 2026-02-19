//
// vigilant-canine - Scan Storage Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <storage/scan_store.h>

#include <format>

namespace vigilant_canine {

    auto ScanStore::prune_old_scans(int days)
        -> std::expected<void, std::string> {

        auto stmt_result = m_db.prepare(R"(
            DELETE FROM scans
            WHERE started_at < datetime('now', '-' || ? || ' days')
        )");

        if (!stmt_result) {
            return std::unexpected(stmt_result.error());
        }

        sqlite3_stmt* stmt = *stmt_result;
        sqlite3_bind_int(stmt, 1, days);

        int result_code = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (result_code != SQLITE_DONE) {
            return std::unexpected(std::format("Failed to prune old scans: {}",
                                                sqlite3_errmsg(m_db.handle())));
        }

        return {};
    }

}  // namespace vigilant_canine
