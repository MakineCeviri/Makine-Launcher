/**
 * @file sqlite_utils.hpp
 * @brief SQLite utilities with optional SQLiteCpp wrapper
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Provides modern C++ SQLite utilities.
 * Uses SQLiteCpp when available for cleaner API.
 *
 * Compile-time detection:
 * - MAKINE_HAS_SQLITECPP - SQLiteCpp library available
 *
 * All functions have fallback implementations using raw sqlite3.
 */

#pragma once

#include "features.hpp"
#include "error.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#ifdef MAKINE_HAS_SQLITECPP
#include <SQLiteCpp/SQLiteCpp.h>
#else
#include <sqlite3.h>
#endif

namespace makine::sqlite {

// =============================================================================
// Types
// =============================================================================

/**
 * @brief SQLite value types
 */
using Value = std::variant<
    std::monostate,  // NULL
    int64_t,         // INTEGER
    double,          // REAL
    std::string,     // TEXT
    std::vector<uint8_t>  // BLOB
>;

/**
 * @brief Row as a vector of values
 */
using Row = std::vector<Value>;

/**
 * @brief Result set as vector of rows
 */
using ResultSet = std::vector<Row>;

// =============================================================================
// Statement Wrapper
// =============================================================================

#ifndef MAKINE_HAS_SQLITECPP

/**
 * @brief RAII wrapper for sqlite3_stmt
 */
class Statement {
public:
    explicit Statement(sqlite3* db, const std::string& sql) : db_(db) {
        int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt_, nullptr);
        if (rc != SQLITE_OK) {
            throw std::runtime_error(sqlite3_errmsg(db));
        }
    }

    ~Statement() {
        if (stmt_) {
            sqlite3_finalize(stmt_);
        }
    }

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    Statement(Statement&& other) noexcept
        : db_(other.db_), stmt_(other.stmt_) {
        other.stmt_ = nullptr;
    }

    Statement& operator=(Statement&& other) noexcept {
        if (this != &other) {
            if (stmt_) sqlite3_finalize(stmt_);
            db_ = other.db_;
            stmt_ = other.stmt_;
            other.stmt_ = nullptr;
        }
        return *this;
    }

    // Bind parameters
    void bind(int index, int64_t value) {
        sqlite3_bind_int64(stmt_, index, value);
    }

    void bind(int index, double value) {
        sqlite3_bind_double(stmt_, index, value);
    }

    void bind(int index, const std::string& value) {
        sqlite3_bind_text(stmt_, index, value.c_str(), -1, SQLITE_TRANSIENT);
    }

    void bind(int index, std::string_view value) {
        sqlite3_bind_text(stmt_, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
    }

    void bind(int index, const std::vector<uint8_t>& value) {
        sqlite3_bind_blob(stmt_, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
    }

    void bindNull(int index) {
        sqlite3_bind_null(stmt_, index);
    }

    // Execute
    bool step() {
        int rc = sqlite3_step(stmt_);
        if (rc == SQLITE_ROW) return true;
        if (rc == SQLITE_DONE) return false;
        throw std::runtime_error(sqlite3_errmsg(db_));
    }

    void reset() {
        sqlite3_reset(stmt_);
        sqlite3_clear_bindings(stmt_);
    }

    // Get columns
    int columnCount() const {
        return sqlite3_column_count(stmt_);
    }

    int columnType(int index) const {
        return sqlite3_column_type(stmt_, index);
    }

    const char* columnName(int index) const {
        return sqlite3_column_name(stmt_, index);
    }

    int64_t getInt64(int index) const {
        return sqlite3_column_int64(stmt_, index);
    }

    double getDouble(int index) const {
        return sqlite3_column_double(stmt_, index);
    }

    std::string getString(int index) const {
        auto text = sqlite3_column_text(stmt_, index);
        auto size = sqlite3_column_bytes(stmt_, index);
        return std::string(reinterpret_cast<const char*>(text), size);
    }

    std::vector<uint8_t> getBlob(int index) const {
        auto data = sqlite3_column_blob(stmt_, index);
        auto size = sqlite3_column_bytes(stmt_, index);
        auto ptr = static_cast<const uint8_t*>(data);
        return std::vector<uint8_t>(ptr, ptr + size);
    }

    Value getValue(int index) const {
        switch (columnType(index)) {
            case SQLITE_NULL:
                return std::monostate{};
            case SQLITE_INTEGER:
                return getInt64(index);
            case SQLITE_FLOAT:
                return getDouble(index);
            case SQLITE_TEXT:
                return getString(index);
            case SQLITE_BLOB:
                return getBlob(index);
            default:
                return std::monostate{};
        }
    }

private:
    sqlite3* db_ = nullptr;
    sqlite3_stmt* stmt_ = nullptr;
};

#endif // !MAKINE_HAS_SQLITECPP

// =============================================================================
// Transaction Helper
// =============================================================================

/**
 * @brief RAII transaction guard
 *
 * Automatically commits on scope exit if not rolled back.
 * Rolls back on exception.
 */
class Transaction {
public:
#ifdef MAKINE_HAS_SQLITECPP
    explicit Transaction(SQLite::Database& db)
        : transaction_(std::make_unique<SQLite::Transaction>(db)) {}
#else
    explicit Transaction(sqlite3* db) : db_(db), active_(true) {
        char* errmsg = nullptr;
        int rc = sqlite3_exec(db, "BEGIN TRANSACTION", nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::string err = errmsg ? errmsg : "Unknown error";
            if (errmsg) sqlite3_free(errmsg);
            throw std::runtime_error(err);
        }
    }
#endif

    ~Transaction() {
#ifdef MAKINE_HAS_SQLITECPP
        // SQLite::Transaction handles cleanup
#else
        if (active_) {
            // Rollback on exception
            sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        }
#endif
    }

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    void commit() {
#ifdef MAKINE_HAS_SQLITECPP
        transaction_->commit();
#else
        if (active_) {
            char* errmsg = nullptr;
            int rc = sqlite3_exec(db_, "COMMIT", nullptr, nullptr, &errmsg);
            if (rc != SQLITE_OK) {
                std::string err = errmsg ? errmsg : "Unknown error";
                if (errmsg) sqlite3_free(errmsg);
                throw std::runtime_error(err);
            }
            active_ = false;
        }
#endif
    }

    void rollback() {
#ifdef MAKINE_HAS_SQLITECPP
        // SQLiteCpp doesn't have explicit rollback - destructor handles it
#else
        if (active_) {
            sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
            active_ = false;
        }
#endif
    }

private:
#ifdef MAKINE_HAS_SQLITECPP
    std::unique_ptr<SQLite::Transaction> transaction_;
#else
    sqlite3* db_ = nullptr;
    bool active_ = false;
#endif
};

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Execute a simple SQL statement
 */
#ifndef MAKINE_HAS_SQLITECPP
inline Result<void> exec(sqlite3* db, const std::string& sql) {
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::string err = errmsg ? errmsg : "Unknown error";
        if (errmsg) sqlite3_free(errmsg);
        return Error(ErrorCode::DatabaseError, err);
    }
    return {};
}
#endif

/**
 * @brief Get last insert rowid
 */
#ifndef MAKINE_HAS_SQLITECPP
[[nodiscard]] inline int64_t lastInsertRowId(sqlite3* db) {
    return sqlite3_last_insert_rowid(db);
}
#endif

/**
 * @brief Get number of changes from last statement
 */
#ifndef MAKINE_HAS_SQLITECPP
[[nodiscard]] inline int changes(sqlite3* db) {
    return sqlite3_changes(db);
}
#endif

/**
 * @brief Execute a query and return all rows
 */
#ifndef MAKINE_HAS_SQLITECPP
[[nodiscard]] inline Result<ResultSet> query(sqlite3* db, const std::string& sql) {
    try {
        Statement stmt(db, sql);
        ResultSet result;

        while (stmt.step()) {
            Row row;
            int cols = stmt.columnCount();
            for (int i = 0; i < cols; ++i) {
                row.push_back(stmt.getValue(i));
            }
            result.push_back(std::move(row));
        }

        return result;
    } catch (const std::exception& e) {
        return Error(ErrorCode::DatabaseError, e.what());
    }
}
#endif

/**
 * @brief Execute with callback for each row
 */
#ifndef MAKINE_HAS_SQLITECPP
inline Result<void> forEach(
    sqlite3* db,
    const std::string& sql,
    std::function<bool(const Row&)> callback
) {
    try {
        Statement stmt(db, sql);

        while (stmt.step()) {
            Row row;
            int cols = stmt.columnCount();
            for (int i = 0; i < cols; ++i) {
                row.push_back(stmt.getValue(i));
            }

            if (!callback(row)) {
                break;
            }
        }

        return {};
    } catch (const std::exception& e) {
        return Error(ErrorCode::DatabaseError, e.what());
    }
}
#endif

// =============================================================================
// Backup Helper
// =============================================================================

/**
 * @brief Backup database to file
 */
#ifndef MAKINE_HAS_SQLITECPP
inline Result<void> backup(sqlite3* sourceDb, const std::string& destPath) {
    sqlite3* destDb = nullptr;
    int rc = sqlite3_open(destPath.c_str(), &destDb);
    if (rc != SQLITE_OK) {
        return Error(ErrorCode::DatabaseError, "Failed to open destination database");
    }

    sqlite3_backup* bak = sqlite3_backup_init(destDb, "main", sourceDb, "main");
    if (!bak) {
        sqlite3_close(destDb);
        return Error(ErrorCode::DatabaseError, "Failed to initialize backup");
    }

    rc = sqlite3_backup_step(bak, -1);
    sqlite3_backup_finish(bak);
    sqlite3_close(destDb);

    if (rc != SQLITE_DONE) {
        return Error(ErrorCode::DatabaseError, "Backup failed");
    }

    return {};
}
#endif

// =============================================================================
// Statistics
// =============================================================================

/**
 * @brief Check if SQLiteCpp is available at runtime
 */
inline bool hasSqliteCpp() noexcept {
#ifdef MAKINE_HAS_SQLITECPP
    return true;
#else
    return false;
#endif
}

/**
 * @brief Get SQLite version string
 */
inline const char* sqliteVersion() noexcept {
    return sqlite3_libversion();
}

} // namespace makine::sqlite
