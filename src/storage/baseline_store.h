//
// vigilant-canine - Baseline Storage
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_STORAGE_BASELINE_STORE_H
#define VIGILANT_CANINE_STORAGE_BASELINE_STORE_H

#include <core/types.h>
#include <storage/database.h>

#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace vigilant_canine {

    //
    // Baseline record for a file.
    //
    struct Baseline {
        std::int64_t id{0};
        FilePath path;
        std::string hash_alg;
        HashValue hash_value;
        std::int64_t size{0};
        std::uint32_t mode{0};
        std::uint32_t uid{0};
        std::uint32_t gid{0};
        std::int64_t mtime_ns{0};
        std::string source;
        std::optional<std::string> deployment;
    };

    //
    // Storage interface for file baselines.
    //
    class BaselineStore {
    public:
        explicit BaselineStore(Database& db) : m_db(db) {}

        //
        // Insert a new baseline.
        //
        // Preconditions:
        //   - baseline.path must not already exist for the given deployment
        //
        // Postconditions:
        //   - On success: baseline is inserted and its id is returned
        //   - On failure: returns error message
        //
        [[nodiscard]] auto insert(Baseline const& baseline)
            -> std::expected<std::int64_t, std::string>;

        //
        // Update an existing baseline.
        //
        // Preconditions:
        //   - baseline.path and deployment must match an existing record
        //
        // Postconditions:
        //   - On success: baseline is updated
        //   - On failure: returns error message
        //
        [[nodiscard]] auto update(Baseline const& baseline)
            -> std::expected<void, std::string>;

        //
        // Find baseline by path and optional deployment.
        //
        [[nodiscard]] auto find_by_path(FilePath const& path,
                                         std::optional<std::string> const& deployment = std::nullopt)
            -> std::expected<std::optional<Baseline>, std::string>;

        //
        // Delete baseline by path and optional deployment.
        //
        [[nodiscard]] auto delete_by_path(FilePath const& path,
                                           std::optional<std::string> const& deployment = std::nullopt)
            -> std::expected<void, std::string>;

        //
        // Get all baselines for a given source.
        //
        [[nodiscard]] auto find_by_source(std::string const& source)
            -> std::expected<std::vector<Baseline>, std::string>;

    private:
        Database& m_db;
    };

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_STORAGE_BASELINE_STORE_H
