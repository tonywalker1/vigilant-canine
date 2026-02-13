//
// vigilant-canine - Database Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <storage/database.h>
#include <storage/schema.h>

#include <hinder/exception/exception.h>

#include <sys/ioctl.h>
#include <sys/vfs.h>
#include <linux/fs.h>
#include <linux/magic.h>
#include <fcntl.h>
#include <unistd.h>

#include <format>

namespace vigilant_canine {

    HINDER_DEFINE_EXCEPTION(database_error, hinder::generic_error);

    namespace {

        //
        // Check if a filesystem is Btrfs.
        //
        auto is_btrfs(std::filesystem::path const& path) -> bool {
            struct statfs fs_stat {};
            if (statfs(path.c_str(), &fs_stat) != 0) {
                return false;
            }
            return fs_stat.f_type == BTRFS_SUPER_MAGIC;
        }

        //
        // Set NOCOW attribute on a file or directory (Btrfs-specific).
        //
        auto set_nocow_attribute(std::filesystem::path const& path) -> bool {
            int fd = open(path.c_str(), O_RDONLY);
            if (fd < 0) {
                return false;
            }

            int flags = 0;
            if (ioctl(fd, FS_IOC_GETFLAGS, &flags) < 0) {
                close(fd);
                return false;
            }

            flags |= FS_NOCOW_FL;
            bool success = ioctl(fd, FS_IOC_SETFLAGS, &flags) >= 0;

            close(fd);
            return success;
        }

    }  // anonymous namespace

    auto ensure_database_directory(std::filesystem::path const& db_path)
        -> std::expected<void, std::string> {

        auto dir = db_path.parent_path();

        try {
            // Create directory if it doesn't exist
            if (!std::filesystem::exists(dir)) {
                std::filesystem::create_directories(dir);

                // Set NOCOW on Btrfs (for SQLite performance)
                if (is_btrfs(dir)) {
                    set_nocow_attribute(dir);
                    // Not a fatal error if this fails - log but continue
                }
            }

            return {};
        }
        catch (std::filesystem::filesystem_error const& e) {
            return std::unexpected(
                std::format("Failed to create database directory: {}", e.what()));
        }
    }

    Database::Database(sqlite3* db) : m_db(db) {}

    Database::Database(Database&& other) noexcept : m_db(other.m_db) {
        other.m_db = nullptr;
    }

    Database& Database::operator=(Database&& other) noexcept {
        if (this != &other) {
            // Close current connection if any
            if (m_db) {
                sqlite3_close(m_db);
            }
            // Take ownership of other's connection
            m_db = other.m_db;
            other.m_db = nullptr;
        }
        return *this;
    }

    Database::~Database() {
        if (m_db) {
            sqlite3_close(m_db);
        }
    }

    auto Database::open(std::filesystem::path const& db_path)
        -> std::expected<Database, std::string> {

        // Ensure directory exists and has NOCOW on Btrfs
        if (auto result = ensure_database_directory(db_path); !result) {
            return std::unexpected(result.error());
        }

        // Open database
        sqlite3* db = nullptr;
        int rc = sqlite3_open(db_path.c_str(), &db);
        if (rc != SQLITE_OK) {
            std::string error = sqlite3_errmsg(db);
            sqlite3_close(db);
            return std::unexpected(std::format("Failed to open database: {}", error));
        }

        Database database{db};

        // Initialize schema
        if (auto result = database.init_schema(); !result) {
            return std::unexpected(result.error());
        }

        return database;
    }

    auto Database::execute(std::string_view sql) -> std::expected<void, std::string> {
        char* error_msg = nullptr;
        std::string sql_str{sql};  // Ensure null-termination
        int rc = sqlite3_exec(m_db, sql_str.c_str(), nullptr, nullptr, &error_msg);

        if (rc != SQLITE_OK) {
            std::string error{error_msg ? error_msg : "unknown error"};
            sqlite3_free(error_msg);
            return std::unexpected(std::format("SQL error: {}", error));
        }

        return {};
    }

    auto Database::prepare(std::string_view sql)
        -> std::expected<sqlite3_stmt*, std::string> {

        sqlite3_stmt* stmt = nullptr;
        std::string sql_str{sql};  // Ensure null-termination
        int rc = sqlite3_prepare_v2(m_db, sql_str.c_str(), -1, &stmt, nullptr);

        if (rc != SQLITE_OK) {
            std::string error = sqlite3_errmsg(m_db) ? sqlite3_errmsg(m_db) : "unknown error";
            return std::unexpected(std::format("Failed to prepare statement: {}", error));
        }

        return stmt;
    }

    auto Database::last_insert_rowid() const -> std::int64_t {
        return sqlite3_last_insert_rowid(m_db);
    }

    auto Database::init_schema() -> std::expected<void, std::string> {
        // Create schema_version table
        if (auto result = execute(schema::DDL_SCHEMA_VERSION); !result) {
            return result;
        }

        // Check current version
        auto version_result = get_schema_version();
        int current_version = version_result.value_or(0);

        // If version is 0, this is a new database
        if (current_version == 0) {
            // Create all tables
            if (auto result = execute(schema::DDL_BASELINES); !result) return result;
            if (auto result = execute(schema::DDL_BASELINES_IDX_PATH); !result) return result;
            if (auto result = execute(schema::DDL_BASELINES_IDX_SOURCE); !result) return result;

            if (auto result = execute(schema::DDL_ALERTS); !result) return result;
            if (auto result = execute(schema::DDL_ALERTS_IDX_SEVERITY); !result) return result;
            if (auto result = execute(schema::DDL_ALERTS_IDX_CREATED); !result) return result;
            if (auto result = execute(schema::DDL_ALERTS_IDX_PATH); !result) return result;

            if (auto result = execute(schema::DDL_SCANS); !result) return result;

            // Set schema version
            if (auto result = set_schema_version(schema::CURRENT_VERSION); !result) {
                return result;
            }
        }
        else if (current_version < schema::CURRENT_VERSION) {
            // Future: migration logic here
            return std::unexpected(std::format(
                "Schema migration not yet implemented (current: {}, required: {})",
                current_version, schema::CURRENT_VERSION));
        }
        else if (current_version > schema::CURRENT_VERSION) {
            return std::unexpected(std::format(
                "Database schema version {} is newer than supported version {}",
                current_version, schema::CURRENT_VERSION));
        }

        return {};
    }

    auto Database::get_schema_version() -> std::expected<int, std::string> {
        auto stmt_result = prepare("SELECT version FROM schema_version ORDER BY version DESC LIMIT 1");
        if (!stmt_result) {
            // Table might not exist yet
            return 0;
        }

        sqlite3_stmt* stmt = *stmt_result;
        int rc = sqlite3_step(stmt);

        int version = 0;
        if (rc == SQLITE_ROW) {
            version = sqlite3_column_int(stmt, 0);
        }

        sqlite3_finalize(stmt);
        return version;
    }

    auto Database::set_schema_version(int version) -> std::expected<void, std::string> {
        auto stmt_result = prepare("INSERT INTO schema_version (version) VALUES (?)");
        if (!stmt_result) {
            return std::unexpected(stmt_result.error());
        }

        sqlite3_stmt* stmt = *stmt_result;
        sqlite3_bind_int(stmt, 1, version);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            return std::unexpected(std::format("Failed to set schema version: {}",
                                                sqlite3_errmsg(m_db)));
        }

        return {};
    }

}  // namespace vigilant_canine
