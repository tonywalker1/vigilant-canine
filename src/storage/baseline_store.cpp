//
// vigilant-canine - Baseline Storage Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <storage/baseline_store.h>

#include <format>

namespace vigilant_canine {

    auto BaselineStore::insert(Baseline const& baseline)
        -> std::expected<std::int64_t, std::string> {

        auto stmt_result = m_db.prepare(R"(
            INSERT INTO baselines (path, hash_alg, hash_value, size, mode, uid, gid, mtime_ns, source, deployment)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )");

        if (!stmt_result) {
            return std::unexpected(stmt_result.error());
        }

        sqlite3_stmt* stmt = *stmt_result;

        sqlite3_bind_text(stmt, 1, (*baseline.path).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, baseline.hash_alg.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, (*baseline.hash_value).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 4, baseline.size);
        sqlite3_bind_int(stmt, 5, static_cast<int>(baseline.mode));
        sqlite3_bind_int(stmt, 6, static_cast<int>(baseline.uid));
        sqlite3_bind_int(stmt, 7, static_cast<int>(baseline.gid));
        sqlite3_bind_int64(stmt, 8, baseline.mtime_ns);
        sqlite3_bind_text(stmt, 9, baseline.source.c_str(), -1, SQLITE_TRANSIENT);

        if (baseline.deployment) {
            sqlite3_bind_text(stmt, 10, baseline.deployment->c_str(), -1, SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_null(stmt, 10);
        }

        int result_code = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (result_code != SQLITE_DONE) {
            return std::unexpected(std::format("Failed to insert baseline: {}",
                                                sqlite3_errmsg(m_db.handle())));
        }

        return m_db.last_insert_rowid();
    }

    auto BaselineStore::update(Baseline const& baseline)
        -> std::expected<void, std::string> {

        auto stmt_result = m_db.prepare(R"(
            UPDATE baselines
            SET hash_alg = ?, hash_value = ?, size = ?, mode = ?, uid = ?, gid = ?, mtime_ns = ?,
                source = ?, updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
            WHERE path = ? AND (deployment IS ? OR (deployment IS NULL AND ? IS NULL))
        )");

        if (!stmt_result) {
            return std::unexpected(stmt_result.error());
        }

        sqlite3_stmt* stmt = *stmt_result;

        sqlite3_bind_text(stmt, 1, baseline.hash_alg.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, (*baseline.hash_value).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, baseline.size);
        sqlite3_bind_int(stmt, 4, static_cast<int>(baseline.mode));
        sqlite3_bind_int(stmt, 5, static_cast<int>(baseline.uid));
        sqlite3_bind_int(stmt, 6, static_cast<int>(baseline.gid));
        sqlite3_bind_int64(stmt, 7, baseline.mtime_ns);
        sqlite3_bind_text(stmt, 8, baseline.source.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 9, (*baseline.path).c_str(), -1, SQLITE_TRANSIENT);

        if (baseline.deployment) {
            sqlite3_bind_text(stmt, 10, baseline.deployment->c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 11, baseline.deployment->c_str(), -1, SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_null(stmt, 10);
            sqlite3_bind_null(stmt, 11);
        }

        int result_code = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (result_code != SQLITE_DONE) {
            return std::unexpected(std::format("Failed to update baseline: {}",
                                                sqlite3_errmsg(m_db.handle())));
        }

        return {};
    }

    auto BaselineStore::find_by_path(FilePath const& path,
                                      std::optional<std::string> const& deployment)
        -> std::expected<std::optional<Baseline>, std::string> {

        auto stmt_result = m_db.prepare(R"(
            SELECT id, path, hash_alg, hash_value, size, mode, uid, gid, mtime_ns, source, deployment
            FROM baselines
            WHERE path = ? AND (deployment IS ? OR (deployment IS NULL AND ? IS NULL))
        )");

        if (!stmt_result) {
            return std::unexpected(stmt_result.error());
        }

        sqlite3_stmt* stmt = *stmt_result;

        sqlite3_bind_text(stmt, 1, (*path).c_str(), -1, SQLITE_TRANSIENT);

        if (deployment) {
            sqlite3_bind_text(stmt, 2, deployment->c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, deployment->c_str(), -1, SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_null(stmt, 2);
            sqlite3_bind_null(stmt, 3);
        }

        int result_code = sqlite3_step(stmt);

        if (result_code == SQLITE_DONE) {
            // No record found
            sqlite3_finalize(stmt);
            return std::nullopt;
        }

        if (result_code != SQLITE_ROW) {
            sqlite3_finalize(stmt);
            return std::unexpected(std::format("Failed to query baseline: {}",
                                                sqlite3_errmsg(m_db.handle())));
        }

        Baseline baseline;
        baseline.id = sqlite3_column_int64(stmt, 0);
        baseline.path = FilePath{
            reinterpret_cast<char const*>(sqlite3_column_text(stmt, 1))};
        baseline.hash_alg = reinterpret_cast<char const*>(sqlite3_column_text(stmt, 2));
        baseline.hash_value = HashValue{
            reinterpret_cast<char const*>(sqlite3_column_text(stmt, 3))};
        baseline.size = sqlite3_column_int64(stmt, 4);
        baseline.mode = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 5));
        baseline.uid = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 6));
        baseline.gid = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 7));
        baseline.mtime_ns = sqlite3_column_int64(stmt, 8);
        baseline.source = reinterpret_cast<char const*>(sqlite3_column_text(stmt, 9));

        if (sqlite3_column_type(stmt, 10) != SQLITE_NULL) {
            baseline.deployment = reinterpret_cast<char const*>(sqlite3_column_text(stmt, 10));
        }

        sqlite3_finalize(stmt);
        return baseline;
    }

    auto BaselineStore::delete_by_path(FilePath const& path,
                                        std::optional<std::string> const& deployment)
        -> std::expected<void, std::string> {

        auto stmt_result = m_db.prepare(R"(
            DELETE FROM baselines
            WHERE path = ? AND (deployment IS ? OR (deployment IS NULL AND ? IS NULL))
        )");

        if (!stmt_result) {
            return std::unexpected(stmt_result.error());
        }

        sqlite3_stmt* stmt = *stmt_result;

        sqlite3_bind_text(stmt, 1, (*path).c_str(), -1, SQLITE_TRANSIENT);

        if (deployment) {
            sqlite3_bind_text(stmt, 2, deployment->c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, deployment->c_str(), -1, SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_null(stmt, 2);
            sqlite3_bind_null(stmt, 3);
        }

        int result_code = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (result_code != SQLITE_DONE) {
            return std::unexpected(std::format("Failed to delete baseline: {}",
                                                sqlite3_errmsg(m_db.handle())));
        }

        return {};
    }

    auto BaselineStore::find_by_source(std::string const& source)
        -> std::expected<std::vector<Baseline>, std::string> {

        auto stmt_result = m_db.prepare(R"(
            SELECT id, path, hash_alg, hash_value, size, mode, uid, gid, mtime_ns, source, deployment
            FROM baselines
            WHERE source = ?
        )");

        if (!stmt_result) {
            return std::unexpected(stmt_result.error());
        }

        sqlite3_stmt* stmt = *stmt_result;
        sqlite3_bind_text(stmt, 1, source.c_str(), -1, SQLITE_TRANSIENT);

        std::vector<Baseline> baselines;

        while (true) {
            int result_code = sqlite3_step(stmt);

            if (result_code == SQLITE_DONE) {
                break;
            }

            if (result_code != SQLITE_ROW) {
                sqlite3_finalize(stmt);
                return std::unexpected(std::format("Failed to query baselines: {}",
                                                    sqlite3_errmsg(m_db.handle())));
            }

            Baseline baseline;
            baseline.id = sqlite3_column_int64(stmt, 0);
            baseline.path = FilePath{
                reinterpret_cast<char const*>(sqlite3_column_text(stmt, 1))};
            baseline.hash_alg = reinterpret_cast<char const*>(sqlite3_column_text(stmt, 2));
            baseline.hash_value = HashValue{
                reinterpret_cast<char const*>(sqlite3_column_text(stmt, 3))};
            baseline.size = sqlite3_column_int64(stmt, 4);
            baseline.mode = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 5));
            baseline.uid = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 6));
            baseline.gid = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 7));
            baseline.mtime_ns = sqlite3_column_int64(stmt, 8);
            baseline.source = reinterpret_cast<char const*>(sqlite3_column_text(stmt, 9));

            if (sqlite3_column_type(stmt, 10) != SQLITE_NULL) {
                baseline.deployment = reinterpret_cast<char const*>(sqlite3_column_text(stmt, 10));
            }

            baselines.push_back(std::move(baseline));
        }

        sqlite3_finalize(stmt);
        return baselines;
    }

}  // namespace vigilant_canine
