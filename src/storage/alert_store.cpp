//
// vigilant-canine - Alert Storage Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <storage/alert_store.h>

#include <format>

namespace vigilant_canine {

    auto to_string(AlertSeverity severity) -> std::string {
        switch (severity) {
        case AlertSeverity::INFO:
            return "INFO";
        case AlertSeverity::WARNING:
            return "WARNING";
        case AlertSeverity::CRITICAL:
            return "CRITICAL";
        }
        return "UNKNOWN";
    }

    auto parse_severity(std::string const& str) -> std::optional<AlertSeverity> {
        if (str == "INFO") return AlertSeverity::INFO;
        if (str == "WARNING") return AlertSeverity::WARNING;
        if (str == "CRITICAL") return AlertSeverity::CRITICAL;
        return std::nullopt;
    }

    auto AlertStore::insert(Alert const& alert)
        -> std::expected<std::int64_t, std::string> {

        auto stmt_result = m_db.prepare(R"(
            INSERT INTO alerts (severity, category, path, summary, details, source, acknowledged)
            VALUES (?, ?, ?, ?, ?, ?, ?)
        )");

        if (!stmt_result) {
            return std::unexpected(stmt_result.error());
        }

        sqlite3_stmt* stmt = *stmt_result;

        sqlite3_bind_text(stmt, 1, to_string(alert.severity).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, alert.category.c_str(), -1, SQLITE_TRANSIENT);

        if (alert.path) {
            sqlite3_bind_text(stmt, 3, (*(*alert.path)).c_str(), -1, SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_null(stmt, 3);
        }

        sqlite3_bind_text(stmt, 4, alert.summary.c_str(), -1, SQLITE_TRANSIENT);

        if (alert.details) {
            sqlite3_bind_text(stmt, 5, alert.details->c_str(), -1, SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_null(stmt, 5);
        }

        sqlite3_bind_text(stmt, 6, alert.source.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 7, alert.acknowledged ? 1 : 0);

        int result_code = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (result_code != SQLITE_DONE) {
            return std::unexpected(std::format("Failed to insert alert: {}",
                                                sqlite3_errmsg(m_db.handle())));
        }

        return m_db.last_insert_rowid();
    }

    auto AlertStore::find_by_id(std::int64_t alert_id)
        -> std::expected<std::optional<Alert>, std::string> {

        auto stmt_result = m_db.prepare(R"(
            SELECT id, severity, category, path, summary, details, source, acknowledged
            FROM alerts
            WHERE id = ?
        )");

        if (!stmt_result) {
            return std::unexpected(stmt_result.error());
        }

        sqlite3_stmt* stmt = *stmt_result;
        sqlite3_bind_int64(stmt, 1, alert_id);

        int result_code = sqlite3_step(stmt);

        if (result_code == SQLITE_DONE) {
            // No record found
            sqlite3_finalize(stmt);
            return std::nullopt;
        }

        if (result_code != SQLITE_ROW) {
            sqlite3_finalize(stmt);
            return std::unexpected(std::format("Failed to query alert: {}",
                                                sqlite3_errmsg(m_db.handle())));
        }

        Alert alert;
        alert.id = sqlite3_column_int64(stmt, 0);

        auto severity_str = reinterpret_cast<char const*>(sqlite3_column_text(stmt, 1));
        if (auto sev = parse_severity(severity_str)) {
            alert.severity = *sev;
        }

        alert.category = reinterpret_cast<char const*>(sqlite3_column_text(stmt, 2));

        if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
            alert.path = FilePath{reinterpret_cast<char const*>(sqlite3_column_text(stmt, 3))};
        }

        alert.summary = reinterpret_cast<char const*>(sqlite3_column_text(stmt, 4));

        if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
            alert.details = reinterpret_cast<char const*>(sqlite3_column_text(stmt, 5));
        }

        alert.source = reinterpret_cast<char const*>(sqlite3_column_text(stmt, 6));
        alert.acknowledged = sqlite3_column_int(stmt, 7) != 0;

        sqlite3_finalize(stmt);
        return alert;
    }

    auto AlertStore::get_recent(int limit)
        -> std::expected<std::vector<Alert>, std::string> {

        auto stmt_result = m_db.prepare(R"(
            SELECT id, severity, category, path, summary, details, source, acknowledged
            FROM alerts
            ORDER BY created_at DESC
            LIMIT ?
        )");

        if (!stmt_result) {
            return std::unexpected(stmt_result.error());
        }

        sqlite3_stmt* stmt = *stmt_result;
        sqlite3_bind_int(stmt, 1, limit);

        std::vector<Alert> alerts;

        while (true) {
            int result_code = sqlite3_step(stmt);

            if (result_code == SQLITE_DONE) {
                break;
            }

            if (result_code != SQLITE_ROW) {
                sqlite3_finalize(stmt);
                return std::unexpected(std::format("Failed to query alerts: {}",
                                                    sqlite3_errmsg(m_db.handle())));
            }

            Alert alert;
            alert.id = sqlite3_column_int64(stmt, 0);

            auto severity_str = reinterpret_cast<char const*>(sqlite3_column_text(stmt, 1));
            if (auto sev = parse_severity(severity_str)) {
                alert.severity = *sev;
            }

            alert.category = reinterpret_cast<char const*>(sqlite3_column_text(stmt, 2));

            if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
                alert.path = FilePath{reinterpret_cast<char const*>(sqlite3_column_text(stmt, 3))};
            }

            alert.summary = reinterpret_cast<char const*>(sqlite3_column_text(stmt, 4));

            if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
                alert.details = reinterpret_cast<char const*>(sqlite3_column_text(stmt, 5));
            }

            alert.source = reinterpret_cast<char const*>(sqlite3_column_text(stmt, 6));
            alert.acknowledged = sqlite3_column_int(stmt, 7) != 0;

            alerts.push_back(std::move(alert));
        }

        sqlite3_finalize(stmt);
        return alerts;
    }

    auto AlertStore::get_unacknowledged()
        -> std::expected<std::vector<Alert>, std::string> {

        auto stmt_result = m_db.prepare(R"(
            SELECT id, severity, category, path, summary, details, source, acknowledged
            FROM alerts
            WHERE acknowledged = 0
            ORDER BY created_at DESC
        )");

        if (!stmt_result) {
            return std::unexpected(stmt_result.error());
        }

        sqlite3_stmt* stmt = *stmt_result;

        std::vector<Alert> alerts;

        while (true) {
            int result_code = sqlite3_step(stmt);

            if (result_code == SQLITE_DONE) {
                break;
            }

            if (result_code != SQLITE_ROW) {
                sqlite3_finalize(stmt);
                return std::unexpected(std::format("Failed to query alerts: {}",
                                                    sqlite3_errmsg(m_db.handle())));
            }

            Alert alert;
            alert.id = sqlite3_column_int64(stmt, 0);

            auto severity_str = reinterpret_cast<char const*>(sqlite3_column_text(stmt, 1));
            if (auto sev = parse_severity(severity_str)) {
                alert.severity = *sev;
            }

            alert.category = reinterpret_cast<char const*>(sqlite3_column_text(stmt, 2));

            if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
                alert.path = FilePath{reinterpret_cast<char const*>(sqlite3_column_text(stmt, 3))};
            }

            alert.summary = reinterpret_cast<char const*>(sqlite3_column_text(stmt, 4));

            if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
                alert.details = reinterpret_cast<char const*>(sqlite3_column_text(stmt, 5));
            }

            alert.source = reinterpret_cast<char const*>(sqlite3_column_text(stmt, 6));
            alert.acknowledged = sqlite3_column_int(stmt, 7) != 0;

            alerts.push_back(std::move(alert));
        }

        sqlite3_finalize(stmt);
        return alerts;
    }

    auto AlertStore::acknowledge(std::int64_t alert_id)
        -> std::expected<void, std::string> {

        auto stmt_result = m_db.prepare(R"(
            UPDATE alerts
            SET acknowledged = 1
            WHERE id = ?
        )");

        if (!stmt_result) {
            return std::unexpected(stmt_result.error());
        }

        sqlite3_stmt* stmt = *stmt_result;
        sqlite3_bind_int64(stmt, 1, alert_id);

        int result_code = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (result_code != SQLITE_DONE) {
            return std::unexpected(std::format("Failed to acknowledge alert: {}",
                                                sqlite3_errmsg(m_db.handle())));
        }

        return {};
    }

    auto AlertStore::unacknowledge(std::int64_t alert_id)
        -> std::expected<void, std::string> {

        auto stmt_result = m_db.prepare(R"(
            UPDATE alerts
            SET acknowledged = 0
            WHERE id = ?
        )");

        if (!stmt_result) {
            return std::unexpected(stmt_result.error());
        }

        sqlite3_stmt* stmt = *stmt_result;
        sqlite3_bind_int64(stmt, 1, alert_id);

        int result_code = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (result_code != SQLITE_DONE) {
            return std::unexpected(std::format("Failed to unacknowledge alert: {}",
                                                sqlite3_errmsg(m_db.handle())));
        }

        return {};
    }

}  // namespace vigilant_canine
