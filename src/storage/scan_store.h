//
// vigilant-canine - Scan Storage
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_STORAGE_SCAN_STORE_H
#define VIGILANT_CANINE_STORAGE_SCAN_STORE_H

#include <storage/database.h>

#include <expected>
#include <string>

namespace vigilant_canine {

    //
    // Storage interface for scan history.
    //
    class ScanStore {
    public:
        explicit ScanStore(Database& database) : m_db(database) {}

        //
        // Delete scans older than specified days.
        //
        [[nodiscard]] auto prune_old_scans(int days = 90)
            -> std::expected<void, std::string>;

    private:
        Database& m_db;
    };

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_STORAGE_SCAN_STORE_H
