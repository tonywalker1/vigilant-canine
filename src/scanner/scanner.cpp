//
// vigilant-canine - Filesystem Scanner Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <scanner/scanner.h>

#include <core/hash.h>

#include <sys/stat.h>

#include <cstring>
#include <format>

namespace vigilant_canine {

    auto Scanner::get_file_metadata(std::filesystem::path const& path)
        -> std::expected<FileMetadata, std::string> {

        struct stat st {};
        if (lstat(path.c_str(), &st) != 0) {
            return std::unexpected(std::format("Failed to stat {}: {}",
                                                path.string(), strerror(errno)));
        }

        // Only handle regular files for now
        if (!S_ISREG(st.st_mode)) {
            return std::unexpected("Not a regular file");
        }

        FileMetadata metadata;
        metadata.path = FilePath{path};
        metadata.size = static_cast<std::uint64_t>(st.st_size);
        metadata.mode = st.st_mode;
        metadata.uid = st.st_uid;
        metadata.gid = st.st_gid;

        // Convert mtime to nanoseconds
        metadata.mtime_ns = static_cast<std::int64_t>(st.st_mtim.tv_sec) * 1'000'000'000LL +
                             st.st_mtim.tv_nsec;

        return metadata;
    }

    auto Scanner::should_exclude(std::filesystem::path const& path) const -> bool {
        auto monitor_paths = m_strategy.get_monitor_paths();

        // Check if path is in exclude list
        for (auto const& exclude : monitor_paths.exclude) {
            // Check if path starts with exclude path
            auto exclude_str = exclude.string();
            auto path_str = path.string();

            if (path_str.starts_with(exclude_str)) {
                return true;
            }
        }

        return false;
    }

    auto Scanner::scan_file(FilePath const& path)
        -> std::expected<StoreOperation, std::string> {

        // Get file metadata
        auto metadata_result = get_file_metadata(*path);
        if (!metadata_result) {
            return std::unexpected(metadata_result.error());
        }

        auto const& metadata = *metadata_result;

        // Calculate hash
        auto hash_result = hash_file(path, m_algorithm);
        if (!hash_result) {
            return std::unexpected(hash_result.error());
        }

        // Get file source from strategy
        auto source_result = m_strategy.get_file_source(*path);
        if (!source_result) {
            return std::unexpected(source_result.error());
        }

        std::string source = "scan";  // Default source
        if (source_result->has_value()) {
            source = *(source_result->value());
        }

        // Check if baseline already exists
        auto deployment = m_strategy.get_deployment_id();
        auto existing = m_store.find_by_path(path, deployment);

        if (!existing) {
            return std::unexpected(existing.error());
        }

        Baseline baseline;
        baseline.path = path;
        baseline.hash_alg = std::string{algorithm_to_string(m_algorithm)};
        baseline.hash_value = *hash_result;
        baseline.size = static_cast<std::int64_t>(metadata.size);
        baseline.mode = metadata.mode;
        baseline.uid = metadata.uid;
        baseline.gid = metadata.gid;
        baseline.mtime_ns = metadata.mtime_ns;
        baseline.source = source;
        baseline.deployment = deployment;

        if (existing->has_value()) {
            // Check if baseline actually changed
            auto const& old_baseline = existing->value();
            if (old_baseline.hash_value == baseline.hash_value &&
                old_baseline.size == baseline.size &&
                old_baseline.mode == baseline.mode &&
                old_baseline.uid == baseline.uid &&
                old_baseline.gid == baseline.gid) {
                // No changes - skip database write
                return StoreOperation::unchanged;
            }

            // Update existing baseline
            auto update_result = m_store.update(baseline);
            if (!update_result) {
                return std::unexpected(update_result.error());
            }
            return StoreOperation::updated;
        } else {
            // Insert new baseline
            auto insert_result = m_store.insert(baseline);
            if (!insert_result) {
                return std::unexpected(insert_result.error());
            }
            return StoreOperation::inserted;
        }
    }

    auto Scanner::scan_directory(std::filesystem::path const& path,
                                  ScanProgressCallback const& progress)
        -> std::expected<ScanStats, std::string> {

        if (!std::filesystem::is_directory(path)) {
            return std::unexpected(std::format("{} is not a directory", path.string()));
        }

        auto start_time = std::chrono::system_clock::now();
        ScanStats stats;

        std::error_code ec;
        auto iterator = std::filesystem::recursive_directory_iterator(
            path,
            std::filesystem::directory_options::skip_permission_denied,
            ec);

        if (ec) {
            return std::unexpected(std::format("Failed to iterate directory: {}", ec.message()));
        }

        for (auto const& entry : iterator) {
            // Skip if excluded
            if (should_exclude(entry.path())) {
                stats.files_skipped++;
                continue;
            }

            // Only scan regular files
            if (!entry.is_regular_file(ec)) {
                if (ec) {
                    stats.errors++;
                }
                continue;
            }

            // Scan the file
            FilePath file_path{entry.path()};
            auto result = scan_file(file_path);

            if (result) {
                stats.files_scanned++;
                // Track operation type
                switch (*result) {
                    case StoreOperation::inserted:
                        stats.files_added++;
                        break;
                    case StoreOperation::updated:
                        stats.files_updated++;
                        break;
                    case StoreOperation::unchanged:
                        stats.files_unchanged++;
                        break;
                }
            } else {
                stats.errors++;
            }

            // Report progress
            if (progress) {
                progress(file_path, stats);
            }
        }

        // Publish scan completed event
        auto end_time = std::chrono::system_clock::now();
        ScanCompletedEvent scan_event{
            path,
            stats.files_scanned,
            stats.files_added,  // Changes detected (for now, same as files added)
            end_time - start_time
        };
        m_event_bus.publish(Event{scan_event, EventSeverity::info, "scanner"});

        return stats;
    }

    auto Scanner::verify_file(FilePath const& path)
        -> std::expected<std::optional<FileChange>, std::string> {

        // Check if baseline exists
        auto deployment = m_strategy.get_deployment_id();
        auto baseline_result = m_store.find_by_path(path, deployment);

        if (!baseline_result) {
            return std::unexpected(std::format("Failed to query baseline for {}", (*path).string()));
        }

        if (!baseline_result->has_value()) {
            // New file (no baseline)
            FileChange change;
            change.path = path;
            change.change_type = "new";

            // Publish FileCreatedEvent
            auto hash_result = hash_file(path, m_algorithm);
            if (hash_result) {
                FileCreatedEvent file_event{
                    path,
                    *hash_result,
                    std::nullopt
                };
                m_event_bus.publish(Event{file_event, EventSeverity::warning, "scanner"});
            }

            return change;
        }

        // Compare current state with baseline
        auto const& baseline = baseline_result->value();

        // Get current metadata
        auto metadata_result = get_file_metadata(*path);
        if (!metadata_result) {
            return std::unexpected(std::format("Failed to get metadata for {}", (*path).string()));
        }

        auto const& metadata = *metadata_result;

        // Quick check: size or mtime changed
        bool needs_hash_check = false;

        if (metadata.size != static_cast<std::uint64_t>(baseline.size)) {
            needs_hash_check = true;
        } else if (metadata.mtime_ns != baseline.mtime_ns) {
            needs_hash_check = true;
        }

        if (needs_hash_check) {
            // Calculate hash to verify
            auto hash_result = hash_file(path, m_algorithm);
            if (!hash_result) {
                return std::unexpected(std::format("Failed to hash file {}", (*path).string()));
            }

            if (*(*hash_result) != *(baseline.hash_value)) {
                FileChange change;
                change.path = path;
                change.change_type = "modified";
                change.details = std::format("Hash mismatch: expected {}, got {}",
                                               *(baseline.hash_value), *(*hash_result));

                // Publish FileModifiedEvent
                FileModifiedEvent mod_event{
                    path,
                    baseline.hash_value,
                    *hash_result,
                    *change.details
                };
                m_event_bus.publish(Event{mod_event, EventSeverity::critical, "scanner"});

                return change;
            }
        }

        // File unchanged
        return std::nullopt;
    }

    auto Scanner::verify_baselines(std::filesystem::path const& path,
                                     ScanProgressCallback const& progress)
        -> std::expected<std::vector<FileChange>, std::string> {

        std::vector<FileChange> changes;

        // For now, this is a simplified implementation
        // A full implementation would:
        // 1. Load all baselines for this path from database
        // 2. Walk filesystem and compare against baselines
        // 3. Detect: modified files, deleted files, new files

        std::error_code ec;
        auto iterator = std::filesystem::recursive_directory_iterator(
            path,
            std::filesystem::directory_options::skip_permission_denied,
            ec);

        if (ec) {
            return std::unexpected(std::format("Failed to iterate directory: {}", ec.message()));
        }

        ScanStats stats;

        for (auto const& entry : iterator) {
            if (should_exclude(entry.path())) {
                continue;
            }

            if (!entry.is_regular_file(ec)) {
                continue;
            }

            FilePath file_path{entry.path()};

            // Verify the file using the extracted method
            auto verify_result = verify_file(file_path);
            if (!verify_result) {
                // Error verifying file - skip it
                continue;
            }

            if (verify_result->has_value()) {
                // Change detected
                changes.push_back(**verify_result);
            }

            stats.files_scanned++;

            if (progress) {
                progress(file_path, stats);
            }
        }

        return changes;
    }

    auto Scanner::scan_user_paths(
        std::vector<std::filesystem::path> const& paths,
        std::vector<std::filesystem::path> const& exclude_patterns,
        std::string const& source,
        ScanProgressCallback const& progress
    ) -> std::expected<ScanStats, std::string> {

        if (source.empty()) {
            return std::unexpected("Source identifier cannot be empty");
        }

        ScanStats stats;

        for (auto const& path : paths) {
            // Skip if path doesn't exist (user might not have all directories)
            if (!std::filesystem::exists(path)) {
                continue;
            }

            // Skip if not a directory
            if (!std::filesystem::is_directory(path)) {
                continue;
            }

            std::error_code ec;
            auto iterator = std::filesystem::recursive_directory_iterator(
                path,
                std::filesystem::directory_options::skip_permission_denied,
                ec);

            if (ec) {
                // Skip directories we can't read
                continue;
            }

            for (auto const& entry : iterator) {
                // Check exclusions
                bool excluded = false;
                for (auto const& exclude : exclude_patterns) {
                    if (entry.path().string().starts_with(exclude.string())) {
                        excluded = true;
                        break;
                    }
                }

                if (excluded) {
                    stats.files_skipped++;
                    continue;
                }

                // Only scan regular files
                if (!entry.is_regular_file(ec)) {
                    if (ec) {
                        stats.errors++;
                    }
                    continue;
                }

                // Get file metadata
                FilePath file_path{entry.path()};
                auto metadata_result = get_file_metadata(*file_path);
                if (!metadata_result) {
                    stats.errors++;
                    continue;
                }

                auto const& metadata = *metadata_result;

                // Calculate hash
                auto hash_result = hash_file(file_path, m_algorithm);
                if (!hash_result) {
                    stats.errors++;
                    continue;
                }

                // Check if baseline already exists
                auto deployment = m_strategy.get_deployment_id();
                auto existing = m_store.find_by_path(file_path, deployment);

                if (!existing) {
                    stats.errors++;
                    continue;
                }

                Baseline baseline;
                baseline.path = file_path;
                baseline.hash_alg = std::string{algorithm_to_string(m_algorithm)};
                baseline.hash_value = *hash_result;
                baseline.size = static_cast<std::int64_t>(metadata.size);
                baseline.mode = metadata.mode;
                baseline.uid = metadata.uid;
                baseline.gid = metadata.gid;
                baseline.mtime_ns = metadata.mtime_ns;
                baseline.source = source;  // Use custom source
                baseline.deployment = deployment;

                if (existing->has_value()) {
                    // Update existing baseline
                    auto update_result = m_store.update(baseline);
                    if (!update_result) {
                        stats.errors++;
                        continue;
                    }
                    stats.files_updated++;
                } else {
                    // Insert new baseline
                    auto insert_result = m_store.insert(baseline);
                    if (!insert_result) {
                        stats.errors++;
                        continue;
                    }
                    stats.files_added++;
                }

                stats.files_scanned++;

                // Report progress
                if (progress) {
                    progress(file_path, stats);
                }
            }
        }

        return stats;
    }

}  // namespace vigilant_canine
