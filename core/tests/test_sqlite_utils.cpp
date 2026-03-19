/**
 * @file test_sqlite_utils.cpp
 * @brief Unit tests for sqlite_utils.hpp (raw SQLite C++ wrappers)
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Tests Value types, Statement, Transaction, and free functions
 * using an in-memory SQLite database.
 */

#include <gtest/gtest.h>
#include <sqlite3.h>
#include <makine/sqlite_utils.hpp>
#include <filesystem>
#include <string>
#include <variant>
#include <cstdint>
#include <vector>


namespace fs = std::filesystem;
using namespace makine;
using namespace makine::sqlite;

#ifndef MAKINE_HAS_SQLITECPP

// =============================================================================
// TEST FIXTURE -- in-memory SQLite database lifecycle
// =============================================================================

class SqliteUtilsTest : public ::testing::Test {
protected:
    sqlite3* db_ = nullptr;

    void SetUp() override {
        int rc = sqlite3_open(":memory:", &db_);
        ASSERT_EQ(rc, SQLITE_OK) << "Failed to open in-memory database";
    }

    void TearDown() override {
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
    }

    // Helper: create a simple test table
    void createTestTable() {
        auto result = exec(db_, "CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT, score REAL, data BLOB)");
        ASSERT_TRUE(result.has_value()) << "Failed to create test table";
    }
};

// =============================================================================
// Value Types
// =============================================================================

TEST_F(SqliteUtilsTest, ValueMonostateIsNull) {
    Value v = std::monostate{};
    EXPECT_TRUE(std::holds_alternative<std::monostate>(v));
    EXPECT_EQ(v.index(), 0u);
}

TEST_F(SqliteUtilsTest, ValueInt64) {
    Value v = int64_t(42);
    ASSERT_TRUE(std::holds_alternative<int64_t>(v));
    EXPECT_EQ(std::get<int64_t>(v), 42);
}

TEST_F(SqliteUtilsTest, ValueDouble) {
    Value v = 3.14;
    ASSERT_TRUE(std::holds_alternative<double>(v));
    EXPECT_DOUBLE_EQ(std::get<double>(v), 3.14);
}

TEST_F(SqliteUtilsTest, ValueString) {
    Value v = std::string("hello");
    ASSERT_TRUE(std::holds_alternative<std::string>(v));
    EXPECT_EQ(std::get<std::string>(v), "hello");
}

TEST_F(SqliteUtilsTest, ValueBlob) {
    std::vector<uint8_t> blob = {0xDE, 0xAD, 0xBE, 0xEF};
    Value v = blob;
    ASSERT_TRUE(std::holds_alternative<std::vector<uint8_t>>(v));
    EXPECT_EQ(std::get<std::vector<uint8_t>>(v), blob);
}

// =============================================================================
// Statement
// =============================================================================

TEST_F(SqliteUtilsTest, StatementPrepareValidSQL) {
    createTestTable();
    EXPECT_NO_THROW(Statement(db_, "SELECT * FROM test"));
}

TEST_F(SqliteUtilsTest, StatementPrepareInvalidSQLThrows) {
    EXPECT_THROW(Statement(db_, "INVALID SQL GIBBERISH"), std::runtime_error);
}

TEST_F(SqliteUtilsTest, StatementBindAndStep) {
    createTestTable();
    Statement insert(db_, "INSERT INTO test (id, name) VALUES (?, ?)");
    insert.bind(1, int64_t(1));
    insert.bind(2, std::string("alpha"));
    EXPECT_FALSE(insert.step());  // INSERT returns DONE, not ROW
}

TEST_F(SqliteUtilsTest, StatementColumnCount) {
    createTestTable();
    Statement stmt(db_, "SELECT id, name, score, data FROM test");
    EXPECT_EQ(stmt.columnCount(), 4);
}

TEST_F(SqliteUtilsTest, StatementGetString) {
    createTestTable();
    exec(db_, "INSERT INTO test (id, name) VALUES (1, 'world')");

    Statement stmt(db_, "SELECT name FROM test WHERE id = 1");
    ASSERT_TRUE(stmt.step());
    EXPECT_EQ(stmt.getString(0), "world");
}

TEST_F(SqliteUtilsTest, StatementGetInt64) {
    createTestTable();
    exec(db_, "INSERT INTO test (id, name) VALUES (99, 'x')");

    Statement stmt(db_, "SELECT id FROM test WHERE name = 'x'");
    ASSERT_TRUE(stmt.step());
    EXPECT_EQ(stmt.getInt64(0), 99);
}

TEST_F(SqliteUtilsTest, StatementGetDouble) {
    createTestTable();
    exec(db_, "INSERT INTO test (id, name, score) VALUES (1, 'a', 9.5)");

    Statement stmt(db_, "SELECT score FROM test WHERE id = 1");
    ASSERT_TRUE(stmt.step());
    EXPECT_DOUBLE_EQ(stmt.getDouble(0), 9.5);
}

TEST_F(SqliteUtilsTest, StatementGetBlob) {
    createTestTable();

    // Insert blob via bind
    Statement insert(db_, "INSERT INTO test (id, data) VALUES (?, ?)");
    std::vector<uint8_t> blob = {0x01, 0x02, 0x03};
    insert.bind(1, int64_t(1));
    insert.bind(2, blob);
    insert.step();

    Statement q(db_, "SELECT data FROM test WHERE id = 1");
    ASSERT_TRUE(q.step());
    auto result = q.getBlob(0);
    EXPECT_EQ(result, blob);
}

TEST_F(SqliteUtilsTest, StatementGetValueForEachType) {
    createTestTable();

    // Insert a row with all types
    Statement insert(db_, "INSERT INTO test (id, name, score, data) VALUES (?, ?, ?, ?)");
    insert.bind(1, int64_t(10));
    insert.bind(2, std::string("text"));
    insert.bind(3, 2.718);
    std::vector<uint8_t> blob = {0xAA, 0xBB};
    insert.bind(4, blob);
    insert.step();

    Statement stmt(db_, "SELECT id, name, score, data FROM test WHERE id = 10");
    ASSERT_TRUE(stmt.step());

    // INTEGER
    Value v0 = stmt.getValue(0);
    ASSERT_TRUE(std::holds_alternative<int64_t>(v0));
    EXPECT_EQ(std::get<int64_t>(v0), 10);

    // TEXT
    Value v1 = stmt.getValue(1);
    ASSERT_TRUE(std::holds_alternative<std::string>(v1));
    EXPECT_EQ(std::get<std::string>(v1), "text");

    // REAL
    Value v2 = stmt.getValue(2);
    ASSERT_TRUE(std::holds_alternative<double>(v2));
    EXPECT_DOUBLE_EQ(std::get<double>(v2), 2.718);

    // BLOB
    Value v3 = stmt.getValue(3);
    ASSERT_TRUE(std::holds_alternative<std::vector<uint8_t>>(v3));
    EXPECT_EQ(std::get<std::vector<uint8_t>>(v3), blob);
}

TEST_F(SqliteUtilsTest, StatementGetValueNull) {
    createTestTable();
    exec(db_, "INSERT INTO test (id) VALUES (1)");

    Statement stmt(db_, "SELECT name FROM test WHERE id = 1");
    ASSERT_TRUE(stmt.step());
    Value v = stmt.getValue(0);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(v));
}

TEST_F(SqliteUtilsTest, StatementColumnName) {
    createTestTable();
    Statement stmt(db_, "SELECT id, name, score FROM test");
    EXPECT_STREQ(stmt.columnName(0), "id");
    EXPECT_STREQ(stmt.columnName(1), "name");
    EXPECT_STREQ(stmt.columnName(2), "score");
}

TEST_F(SqliteUtilsTest, StatementResetAndReExecute) {
    createTestTable();
    exec(db_, "INSERT INTO test (id, name) VALUES (1, 'first')");
    exec(db_, "INSERT INTO test (id, name) VALUES (2, 'second')");

    Statement stmt(db_, "SELECT name FROM test WHERE id = ?");

    // First execution
    stmt.bind(1, int64_t(1));
    ASSERT_TRUE(stmt.step());
    EXPECT_EQ(stmt.getString(0), "first");

    // Reset and re-execute with different param
    stmt.reset();
    stmt.bind(1, int64_t(2));
    ASSERT_TRUE(stmt.step());
    EXPECT_EQ(stmt.getString(0), "second");
}

// =============================================================================
// Transaction
// =============================================================================

TEST_F(SqliteUtilsTest, TransactionCommit) {
    createTestTable();
    {
        Transaction txn(db_);
        exec(db_, "INSERT INTO test (id, name) VALUES (1, 'committed')");
        txn.commit();
    }

    // Data should persist after commit
    auto result = query(db_, "SELECT name FROM test WHERE id = 1");
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().size(), 1u);
    ASSERT_TRUE(std::holds_alternative<std::string>(result.value()[0][0]));
    EXPECT_EQ(std::get<std::string>(result.value()[0][0]), "committed");
}

TEST_F(SqliteUtilsTest, TransactionRollback) {
    createTestTable();
    exec(db_, "INSERT INTO test (id, name) VALUES (1, 'before')");

    {
        Transaction txn(db_);
        exec(db_, "UPDATE test SET name = 'during_tx' WHERE id = 1");
        txn.rollback();
    }

    // Data should revert after rollback
    auto result = query(db_, "SELECT name FROM test WHERE id = 1");
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().size(), 1u);
    EXPECT_EQ(std::get<std::string>(result.value()[0][0]), "before");
}

TEST_F(SqliteUtilsTest, TransactionDestructorRollsBackIfNotCommitted) {
    createTestTable();
    exec(db_, "INSERT INTO test (id, name) VALUES (1, 'original')");

    {
        Transaction txn(db_);
        exec(db_, "UPDATE test SET name = 'modified' WHERE id = 1");
        // txn goes out of scope without commit -- destructor rolls back
    }

    auto result = query(db_, "SELECT name FROM test WHERE id = 1");
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().size(), 1u);
    EXPECT_EQ(std::get<std::string>(result.value()[0][0]), "original");
}

// =============================================================================
// exec()
// =============================================================================

TEST_F(SqliteUtilsTest, ExecValidSQL) {
    auto result = exec(db_, "CREATE TABLE t (x INT)");
    EXPECT_TRUE(result.has_value());
}

TEST_F(SqliteUtilsTest, ExecInvalidSQLReturnsError) {
    auto result = exec(db_, "NOT VALID SQL AT ALL");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::DatabaseError);
}

// =============================================================================
// lastInsertRowId()
// =============================================================================

TEST_F(SqliteUtilsTest, LastInsertRowIdAfterInsert) {
    createTestTable();
    exec(db_, "INSERT INTO test (name) VALUES ('a')");
    int64_t id1 = lastInsertRowId(db_);
    EXPECT_GT(id1, int64_t(0));

    exec(db_, "INSERT INTO test (name) VALUES ('b')");
    int64_t id2 = lastInsertRowId(db_);
    EXPECT_GT(id2, id1);
}

// =============================================================================
// changes()
// =============================================================================

TEST_F(SqliteUtilsTest, ChangesAfterUpdate) {
    createTestTable();
    exec(db_, "INSERT INTO test (id, name) VALUES (1, 'x')");
    exec(db_, "INSERT INTO test (id, name) VALUES (2, 'x')");
    exec(db_, "INSERT INTO test (id, name) VALUES (3, 'y')");

    exec(db_, "UPDATE test SET name = 'updated' WHERE name = 'x'");
    EXPECT_EQ(changes(db_), 2);
}

// =============================================================================
// query()
// =============================================================================

TEST_F(SqliteUtilsTest, QueryReturnsResultSet) {
    createTestTable();
    exec(db_, "INSERT INTO test (id, name) VALUES (1, 'alpha')");
    exec(db_, "INSERT INTO test (id, name) VALUES (2, 'beta')");

    auto result = query(db_, "SELECT id, name FROM test ORDER BY id");
    ASSERT_TRUE(result.has_value());

    const auto& rows = result.value();
    ASSERT_EQ(rows.size(), 2u);

    // Row 0
    EXPECT_EQ(std::get<int64_t>(rows[0][0]), 1);
    EXPECT_EQ(std::get<std::string>(rows[0][1]), "alpha");

    // Row 1
    EXPECT_EQ(std::get<int64_t>(rows[1][0]), 2);
    EXPECT_EQ(std::get<std::string>(rows[1][1]), "beta");
}

// =============================================================================
// forEach()
// =============================================================================

TEST_F(SqliteUtilsTest, ForEachCallbackReceivesRows) {
    createTestTable();
    exec(db_, "INSERT INTO test (id, name) VALUES (1, 'a')");
    exec(db_, "INSERT INTO test (id, name) VALUES (2, 'b')");
    exec(db_, "INSERT INTO test (id, name) VALUES (3, 'c')");

    int count = 0;
    auto result = forEach(db_, "SELECT id FROM test ORDER BY id",
        [&count](const Row&) -> bool {
            ++count;
            return true;
        });

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(count, 3);
}

TEST_F(SqliteUtilsTest, ForEachCallbackReturningFalseStopsEarly) {
    createTestTable();
    exec(db_, "INSERT INTO test (id, name) VALUES (1, 'a')");
    exec(db_, "INSERT INTO test (id, name) VALUES (2, 'b')");
    exec(db_, "INSERT INTO test (id, name) VALUES (3, 'c')");

    int count = 0;
    auto result = forEach(db_, "SELECT id FROM test ORDER BY id",
        [&count](const Row&) -> bool {
            ++count;
            return count < 2;  // stop after 2nd row
        });

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(count, 2);
}

// =============================================================================
// backup()
// =============================================================================

TEST_F(SqliteUtilsTest, BackupToTempFile) {
    createTestTable();
    exec(db_, "INSERT INTO test (id, name) VALUES (1, 'backup_test')");

    auto tempPath = fs::temp_directory_path() / "makine_sqlite_backup_test.db";
    auto result = backup(db_, tempPath.string());
    ASSERT_TRUE(result.has_value());

    // Verify backup file exists
    EXPECT_TRUE(fs::exists(tempPath));

    // Open backup and verify contents
    sqlite3* backupDb = nullptr;
    int rc = sqlite3_open(tempPath.string().c_str(), &backupDb);
    ASSERT_EQ(rc, SQLITE_OK);

    auto backupResult = query(backupDb, "SELECT name FROM test WHERE id = 1");
    sqlite3_close(backupDb);

    ASSERT_TRUE(backupResult.has_value());
    ASSERT_EQ(backupResult.value().size(), 1u);
    EXPECT_EQ(std::get<std::string>(backupResult.value()[0][0]), "backup_test");

    // Cleanup
    std::error_code ec;
    fs::remove(tempPath, ec);
}


#else  // MAKINE_HAS_SQLITECPP

TEST(SqliteUtilsTest, SkippedWithSqliteCpp) {
    GTEST_SKIP() << "sqlite_utils raw API tests skipped (SQLiteCpp is available)";
}

#endif  // !MAKINE_HAS_SQLITECPP

// =============================================================================
// Info functions (always available, independent of SQLiteCpp)
// =============================================================================

TEST(SqliteInfoTest, SqliteVersionReturnsNonNull) {
    const char* version = makine::sqlite::sqliteVersion();
    ASSERT_NE(version, nullptr);
    EXPECT_GT(std::string(version).size(), 0u);
}
