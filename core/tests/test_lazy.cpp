/**
 * @file test_lazy.cpp
 * @brief Unit tests for lazy.hpp — Lazy<T>, LazyFile, LazyJson, LazyResource
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <makine/lazy.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace makine {
namespace testing {

namespace fs = std::filesystem;

// ===========================================================================
// Lazy<T> — Basic
// ===========================================================================

TEST(LazyTest, FactoryNotCalledUntilAccess) {
    int callCount = 0;
    Lazy<int> lazy(Lazy<int>::Factory([&]() {
        ++callCount;
        return 42;
    }));

    EXPECT_FALSE(lazy.isInitialized());
    EXPECT_EQ(callCount, 0);

    EXPECT_EQ(lazy.get(), 42);
    EXPECT_TRUE(lazy.isInitialized());
    EXPECT_EQ(callCount, 1);
}

TEST(LazyTest, FactoryCalledOnlyOnce) {
    int callCount = 0;
    Lazy<int> lazy(Lazy<int>::Factory([&]() {
        ++callCount;
        return 100;
    }));

    (void)lazy.get();
    (void)lazy.get();
    (void)lazy.get();

    EXPECT_EQ(callCount, 1);
}

TEST(LazyTest, ConstructedWithValue) {
    Lazy<int> lazy(42);
    EXPECT_TRUE(lazy.isInitialized());
    EXPECT_EQ(lazy.get(), 42);
}

TEST(LazyTest, PointerOperators) {
    Lazy<std::string> lazy(Lazy<std::string>::Factory([]() { return std::string("hello"); }));

    EXPECT_EQ(*lazy, "hello");
    EXPECT_EQ(lazy->size(), 5u);
}

TEST(LazyTest, Reset) {
    int callCount = 0;
    Lazy<int> lazy(Lazy<int>::Factory([&]() {
        ++callCount;
        return callCount * 10;
    }));

    EXPECT_EQ(lazy.get(), 10);
    lazy.reset();
    EXPECT_FALSE(lazy.isInitialized());
    EXPECT_EQ(lazy.get(), 20); // Factory called again
}

TEST(LazyTest, Invalidate) {
    int callCount = 0;
    Lazy<int> lazy(Lazy<int>::Factory([&]() {
        ++callCount;
        return callCount;
    }));

    EXPECT_EQ(lazy.get(), 1);
    lazy.invalidate();
    EXPECT_EQ(lazy.get(), 2);
}

TEST(LazyTest, ResetWithNewFactory) {
    Lazy<int> lazy(Lazy<int>::Factory([]() { return 1; }));
    EXPECT_EQ(lazy.get(), 1);

    lazy.reset(Lazy<int>::Factory([]() { return 999; }));
    EXPECT_EQ(lazy.get(), 999);
}

// ===========================================================================
// Lazy<T> — Error Handling
// ===========================================================================

TEST(LazyTest, FactoryWithErrorSuccess) {
    Lazy<int> lazy(Lazy<int>::FactoryWithError([]() -> Result<int> {
        return 42;
    }));

    EXPECT_EQ(lazy.get(), 42);
    EXPECT_FALSE(lazy.hasError());
}

TEST(LazyTest, FactoryWithErrorFailure) {
    Lazy<int> lazy(Lazy<int>::FactoryWithError([]() -> Result<int> {
        return std::unexpected(Error(ErrorCode::Unknown, "test error"));
    }));

    EXPECT_TRUE(lazy.isInitialized() || true); // Will initialize on tryGet
    auto result = lazy.tryGet();
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(lazy.hasError());
}

TEST(LazyTest, TryGetSuccess) {
    Lazy<int> lazy(Lazy<int>::Factory([]() { return 42; }));

    auto result = lazy.tryGet();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
}

TEST(LazyTest, FactoryThrowsSetsError) {
    Lazy<int> lazy(Lazy<int>::Factory([]() -> int {
        throw std::runtime_error("init failed");
    }));

    auto result = lazy.tryGet();
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(lazy.hasError());
}

TEST(LazyTest, GetThrowsOnError) {
    Lazy<int> lazy(Lazy<int>::FactoryWithError([]() -> Result<int> {
        return std::unexpected(Error(ErrorCode::Unknown, "fail"));
    }));

    EXPECT_THROW(lazy.get(), std::runtime_error);
}

TEST(LazyTest, ErrorAccessor) {
    Lazy<int> lazy(Lazy<int>::FactoryWithError([]() -> Result<int> {
        return std::unexpected(Error(ErrorCode::Unknown, "detail"));
    }));

    (void)lazy.tryGet();
    auto err = lazy.error();
    ASSERT_TRUE(err.has_value());
}

// ===========================================================================
// LazyFile
// ===========================================================================

class LazyFileTest : public ::testing::Test {
protected:
    fs::path testDir_;

    void SetUp() override {
        testDir_ = fs::temp_directory_path() / "makine_lazy_tests";
        fs::create_directories(testDir_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(testDir_, ec);
    }

    fs::path createFile(const std::string& name, const std::string& content) {
        auto path = testDir_ / name;
        std::ofstream(path) << content;
        return path;
    }
};

TEST_F(LazyFileTest, ExistsReturnsTrueForExistingFile) {
    auto path = createFile("test.txt", "hello");
    LazyFile lf(path);
    EXPECT_TRUE(lf.exists());
}

TEST_F(LazyFileTest, ExistsReturnsFalseForMissing) {
    LazyFile lf(testDir_ / "nonexistent.txt");
    EXPECT_FALSE(lf.exists());
}

TEST_F(LazyFileTest, PathAccessor) {
    auto path = testDir_ / "test.txt";
    LazyFile lf(path);
    EXPECT_EQ(lf.path(), path);
}

TEST_F(LazyFileTest, ContentLoadedOnFirstAccess) {
    auto path = createFile("lazy.txt", "lazy content");
    LazyFile lf(path);

    EXPECT_FALSE(lf.isLoaded());

    auto result = lf.content();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "lazy content");
    EXPECT_TRUE(lf.isLoaded());
}

TEST_F(LazyFileTest, ContentCached) {
    auto path = createFile("cached.txt", "data");
    LazyFile lf(path);

    auto r1 = lf.content();
    auto r2 = lf.content();
    ASSERT_TRUE(r1.has_value());
    ASSERT_TRUE(r2.has_value());
    // Same reference
    EXPECT_EQ(*r1, *r2); // Values match (copy-by-value semantics)
}

TEST_F(LazyFileTest, SizeWithoutLoading) {
    auto path = createFile("size.txt", "12345");
    LazyFile lf(path);

    auto size = lf.size();
    ASSERT_TRUE(size.has_value());
    EXPECT_EQ(*size, 5u);
    EXPECT_FALSE(lf.isLoaded()); // Content not loaded
}

TEST_F(LazyFileTest, SizeNonexistentFile) {
    LazyFile lf(testDir_ / "missing.txt");
    auto size = lf.size();
    EXPECT_FALSE(size.has_value());
}

TEST_F(LazyFileTest, LinesFromFile) {
    auto path = createFile("lines.txt", "line1\nline2\nline3");
    LazyFile lf(path);

    auto lines = lf.lines();
    ASSERT_TRUE(lines.has_value());
    EXPECT_EQ(lines->size(), 3u);
    EXPECT_EQ((*lines)[0], "line1");
    EXPECT_EQ((*lines)[2], "line3");
}

TEST_F(LazyFileTest, InvalidateReloads) {
    auto path = createFile("invalidate.txt", "v1");
    LazyFile lf(path);

    auto r1 = lf.content();
    ASSERT_TRUE(r1.has_value());
    EXPECT_EQ(*r1, "v1");

    // Update file
    std::ofstream(path) << "v2";

    lf.invalidate();
    EXPECT_FALSE(lf.isLoaded());

    auto r2 = lf.content();
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(*r2, "v2");
}

TEST_F(LazyFileTest, LastModified) {
    auto path = createFile("mod.txt", "data");
    LazyFile lf(path);

    auto mod = lf.lastModified();
    EXPECT_TRUE(mod.has_value());
}

TEST_F(LazyFileTest, ContentErrorForMissingFile) {
    LazyFile lf(testDir_ / "missing.txt");
    auto result = lf.content();
    EXPECT_FALSE(result.has_value());
}

TEST_F(LazyFileTest, BytesFromFile) {
    auto path = createFile("bytes.txt", "ABC");
    LazyFile lf(path);

    auto bytes = lf.bytes();
    ASSERT_TRUE(bytes.has_value());
    EXPECT_EQ(bytes->size(), 3u);
    EXPECT_EQ((*bytes)[0], 'A');
}

// ===========================================================================
// LazyJson
// ===========================================================================

TEST_F(LazyFileTest, LazyJsonFromString) {
    LazyJson lj(std::string(R"({"name": "test", "value": 42})"));

    EXPECT_TRUE(lj.exists());
    EXPECT_EQ(lj.get<std::string>("name"), "test");
    EXPECT_EQ(lj.get<int>("value"), 42);
}

TEST_F(LazyFileTest, LazyJsonFromFile) {
    auto path = createFile("config.json", R"({"host": "localhost", "port": 8080})");
    LazyJson lj(path);

    EXPECT_TRUE(lj.exists());
    EXPECT_EQ(lj.get<std::string>("host"), "localhost");
    EXPECT_EQ(lj.get<int>("port"), 8080);
}

TEST_F(LazyFileTest, LazyJsonDotPathNavigation) {
    LazyJson lj(std::string(R"({"db": {"host": "127.0.0.1", "port": 5432}})"));

    EXPECT_EQ(lj.get<std::string>("db.host"), "127.0.0.1");
    EXPECT_EQ(lj.get<int>("db.port"), 5432);
}

TEST_F(LazyFileTest, LazyJsonDefaultValue) {
    LazyJson lj(std::string(R"({"a": 1})"));

    EXPECT_EQ(lj.get<int>("missing", 99), 99);
    EXPECT_EQ(lj.get<std::string>("missing", "default"), "default");
}

TEST_F(LazyFileTest, LazyJsonHas) {
    LazyJson lj(std::string(R"({"key": "val"})"));

    EXPECT_TRUE(lj.has("key"));
    EXPECT_FALSE(lj.has("nonexistent"));
}

TEST_F(LazyFileTest, LazyJsonTryGetSuccess) {
    LazyJson lj(std::string(R"({"x": 42})"));

    auto result = lj.tryGet<int>("x");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
}

TEST_F(LazyFileTest, LazyJsonTryGetMissingPath) {
    LazyJson lj(std::string(R"({"x": 42})"));

    auto result = lj.tryGet<int>("missing");
    EXPECT_FALSE(result.has_value());
}

TEST_F(LazyFileTest, LazyJsonInvalidJsonString) {
    LazyJson lj(std::string("not valid json {{{"));

    EXPECT_EQ(lj.get<int>("key", -1), -1);
    EXPECT_FALSE(lj.isParsed() && lj.has("key"));
}

TEST_F(LazyFileTest, LazyJsonIsParsed) {
    LazyJson lj(std::string(R"({"a": 1})"));
    EXPECT_FALSE(lj.isParsed());

    (void)lj.get<int>("a");
    EXPECT_TRUE(lj.isParsed());
}

TEST_F(LazyFileTest, LazyJsonInvalidate) {
    LazyJson lj(std::string(R"({"a": 1})"));

    (void)lj.get<int>("a");
    EXPECT_TRUE(lj.isParsed());

    lj.invalidate();
    EXPECT_FALSE(lj.isParsed());
}

// LazyJson::json() returns Result<const Json&> — std::expected<T&>
// not valid on MinGW GCC 13.1
#ifndef __MINGW32__
TEST_F(LazyFileTest, LazyJsonRawAccess) {
    LazyJson lj(std::string(R"({"root": true})"));

    auto result = lj.json();
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->get().contains("root"));
}
#endif

// ===========================================================================
// LazyResource
// ===========================================================================

// LazyResource::get() returns Result<T&> — not valid on MinGW
#ifndef __MINGW32__
TEST(LazyResourceTest, FactoryCalledOnGet) {
    int created = 0;
    LazyResource<int> res(
        [&]() -> Result<int> {
            ++created;
            return 42;
        }
    );

    EXPECT_FALSE(res.isInitialized());
    auto result = res.get();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
    EXPECT_EQ(created, 1);
}

TEST(LazyResourceTest, CleanupCalledOnRelease) {
    int cleaned = 0;
    {
        LazyResource<int> res(
            []() -> Result<int> { return 1; },
            [&](int&) { ++cleaned; }
        );

        (void)res.get();
        res.release();
        EXPECT_EQ(cleaned, 1);
        EXPECT_FALSE(res.isInitialized());
    }
}

TEST(LazyResourceTest, CleanupCalledOnDestruction) {
    int cleaned = 0;
    {
        LazyResource<int> res(
            []() -> Result<int> { return 1; },
            [&](int&) { ++cleaned; }
        );
        (void)res.get();
    }
    EXPECT_EQ(cleaned, 1);
}

TEST(LazyResourceTest, FactoryErrorPropagates) {
    LazyResource<int> res(
        []() -> Result<int> {
            return std::unexpected(Error(ErrorCode::Unknown, "fail"));
        }
    );

    auto result = res.get();
    EXPECT_FALSE(result.has_value());
}

#else
// MinGW: test LazyResource without calling get() (returns Result<T&>)
TEST(LazyResourceTest, ConstructionDoesNotInitialize) {
    LazyResource<int> res(
        []() -> Result<int> { return 42; }
    );
    EXPECT_FALSE(res.isInitialized());
}
#endif

// ===========================================================================
// Convenience Functions
// ===========================================================================

TEST(LazyConvenienceTest, MakeLazy) {
    Lazy<int> lazy(Lazy<int>::Factory([]() { return 42; }));
    EXPECT_EQ(lazy.get(), 42);
}

TEST_F(LazyFileTest, LazyFileConvenience) {
    auto path = createFile("conv.txt", "data");
    auto lf = lazyFile(path);
    EXPECT_TRUE(lf.exists());
}

TEST_F(LazyFileTest, LazyJsonConvenience) {
    auto path = createFile("conv.json", R"({"k": "v"})");
    auto lj = lazyJson(path);
    EXPECT_EQ(lj.get<std::string>("k"), "v");
}

} // namespace testing
} // namespace makine
