//
// vigilant-canine - Database Connection
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_STORAGE_DATABASE_H
#define VIGILANT_CANINE_STORAGE_DATABASE_H

#include <sqlite3.h>

#include <expected>
#include <filesystem>
#include <memory>
#include <string>

namespace vigilant_canine {

    //
    // RAII wrapper for SQLite database connection.
    //
    // Manages connection lifecycle and ensures proper cleanup.
    // Thread-safety: NOT thread-safe. Use one Database per thread or add external locking.
    //
    class Database {
    public:
        //
        // Open or create database at the given path.
        //
        // Preconditions:
        //   - Parent directory of db_path must exist or be creatable
        //
        // Postconditions:
        //   - On success: database is open and schema is initialized/migrated
        //   - On failure: returns error message
        //
        [[nodiscard]] static auto open(std::filesystem::path const& db_path)
            -> std::expected<Database, std::string>;

        // Move-only type (SQLite connection is not copyable)
        Database(Database&& other) noexcept;
        Database& operator=(Database&& other) noexcept;
        Database(Database const&) = delete;
        Database& operator=(Database const&) = delete;

        ~Database();

        //
        // Execute a SQL statement (for DDL or statements without results).
        //
        // Preconditions:
        //   - sql must be valid SQLite SQL
        //
        [[nodiscard]] auto execute(std::string_view sql) -> std::expected<void, std::string>;

        //
        // Prepare a SQL statement for execution.
        //
        // Preconditions:
        //   - sql must be valid SQLite SQL
        //
        // Postconditions:
        //   - On success: returns prepared statement handle
        //   - Caller is responsible for calling sqlite3_finalize
        //
        [[nodiscard]] auto prepare(std::string_view sql)
            -> std::expected<sqlite3_stmt*, std::string>;

        //
        // Get the raw SQLite connection handle.
        //
        // Use with caution - direct access to the handle bypasses RAII safety.
        //
        [[nodiscard]] auto handle() -> sqlite3* { return m_db; }

        //
        // Get the last insert rowid.
        //
        [[nodiscard]] auto last_insert_rowid() const -> std::int64_t;

    private:
        explicit Database(sqlite3* db);

        //
        // Initialize schema (create tables if they don't exist).
        //
        [[nodiscard]] auto init_schema() -> std::expected<void, std::string>;

        //
        // Get current schema version from database.
        //
        [[nodiscard]] auto get_schema_version() -> std::expected<int, std::string>;

        //
        // Set schema version in database.
        //
        [[nodiscard]] auto set_schema_version(int version) -> std::expected<void, std::string>;

        sqlite3* m_db{nullptr};
    };

    //
    // Ensure database directory exists and has NOCOW attribute on Btrfs.
    //
    // Preconditions:
    //   - db_path is a file path (not a directory)
    //
    // Postconditions:
    //   - Parent directory exists
    //   - On Btrfs: directory has NOCOW attribute set
    //
    [[nodiscard]] auto ensure_database_directory(std::filesystem::path const& db_path)
        -> std::expected<void, std::string>;

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_STORAGE_DATABASE_H
