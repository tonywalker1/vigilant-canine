//
// vigilant-canine - Filesystem Scanner
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_SCANNER_SCANNER_H
#define VIGILANT_CANINE_SCANNER_SCANNER_H

#include <baseline/strategy.h>
#include <core/types.h>
#include <events/event_bus.h>
#include <storage/baseline_store.h>

#include <expected>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace vigilant_canine {

    //
    // File metadata for baseline comparison.
    //
    struct FileMetadata {
        FilePath path;
        std::uint64_t size;
        std::uint32_t mode;
        std::uint32_t uid;
        std::uint32_t gid;
        std::int64_t mtime_ns;
    };

    //
    // Scan result statistics.
    //
    struct ScanStats {
        std::uint64_t files_scanned{0};
        std::uint64_t files_added{0};
        std::uint64_t files_updated{0};
        std::uint64_t files_unchanged{0};
        std::uint64_t files_skipped{0};
        std::uint64_t errors{0};
    };

    //
    // Change detected during verification.
    //
    struct FileChange {
        FilePath path;
        std::string change_type;  // "modified", "deleted", "new"
        std::optional<std::string> details;
    };

    //
    // Progress callback for long-running scans.
    //
    using ScanProgressCallback = std::function<void(FilePath const&, ScanStats const&)>;

    //
    // Filesystem scanner for baseline creation and verification.
    //
    class Scanner {
    public:
        Scanner(BaselineStore& store,
                BaselineStrategy& strategy,
                EventBus& event_bus,
                HashAlgorithm algorithm = HashAlgorithm::blake3)
            : m_store(store), m_strategy(strategy), m_event_bus(event_bus), m_algorithm(algorithm) {}

        //
        // Scan directory tree and create baselines.
        //
        // Scans all files in the given directory tree, calculates hashes,
        // and stores baselines in the database. Uses the baseline strategy
        // to determine which files to scan and their sources.
        //
        // Preconditions:
        //   - path must be a valid directory
        //
        // Postconditions:
        //   - On success: returns scan statistics
        //   - On failure: returns error message
        //
        [[nodiscard]] auto scan_directory(std::filesystem::path const& path,
                                           ScanProgressCallback const& progress = nullptr)
            -> std::expected<ScanStats, std::string>;

        //
        // Verify existing baselines against current filesystem state.
        //
        // Compares stored baselines with current file state and reports changes.
        //
        // Postconditions:
        //   - On success: returns list of detected changes
        //   - On failure: returns error message
        //
        [[nodiscard]] auto verify_baselines(std::filesystem::path const& path,
                                              ScanProgressCallback const& progress = nullptr)
            -> std::expected<std::vector<FileChange>, std::string>;

        //
        // Scan a single file and create/update baseline.
        //
        [[nodiscard]] auto scan_file(FilePath const& path)
            -> std::expected<void, std::string>;

        //
        // Scan directory tree for a specific user with custom source.
        //
        // Scans files in the given paths, using a custom source identifier
        // (e.g., "user:alice"). Supports exclusion patterns.
        //
        // Preconditions:
        //   - paths must be valid directories
        //   - source must be non-empty
        //
        // Postconditions:
        //   - On success: returns scan statistics
        //   - On failure: returns error message
        //
        [[nodiscard]] auto scan_user_paths(
            std::vector<std::filesystem::path> const& paths,
            std::vector<std::filesystem::path> const& exclude_patterns,
            std::string const& source,
            ScanProgressCallback const& progress = nullptr
        ) -> std::expected<ScanStats, std::string>;

        //
        // Set hash algorithm for scans.
        //
        void set_hash_algorithm(HashAlgorithm algorithm) {
            m_algorithm = algorithm;
        }

    private:
        //
        // Get file metadata.
        //
        [[nodiscard]] auto get_file_metadata(std::filesystem::path const& path)
            -> std::expected<FileMetadata, std::string>;

        //
        // Check if path should be excluded from scanning.
        //
        [[nodiscard]] auto should_exclude(std::filesystem::path const& path) const -> bool;

        BaselineStore& m_store;
        BaselineStrategy& m_strategy;
        EventBus& m_event_bus;
        HashAlgorithm m_algorithm;
    };

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_SCANNER_SCANNER_H
