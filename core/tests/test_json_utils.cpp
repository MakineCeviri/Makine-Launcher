/**
 * @file test_json_utils.cpp
 * @brief Unit tests for JSON utility classes (JsonValue, JsonDocument, ArrayIterator)
 * @copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <makine/json_utils.hpp>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace makine;
using namespace makine::json;

// =============================================================================
// TEST FIXTURE — temp directory for file-based tests
// =============================================================================

class JsonUtilsFileTest : public ::testing::Test {
protected:
    fs::path tempDir_;

    void SetUp() override {
        tempDir_ = fs::temp_directory_path() / "makine_json_test";
        fs::create_directories(tempDir_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(tempDir_, ec);
    }

    // Write content to a temp file and return the path
    fs::path writeFile(const std::string& name, const std::string& content) {
        auto path = tempDir_ / name;
        std::ofstream ofs(path);
        ofs << content;
        ofs.close();
        return path;
    }
};

// simdjson segfaults on MinGW GCC 13.1 — skip JsonDocument tests
#if !defined(__MINGW32__) && !defined(__MINGW64__)

// =============================================================================
// PARSE VALID JSON
// =============================================================================

TEST(JsonDocumentTest, ParseValidObject) {
    auto result = JsonDocument::parse(R"({"name": "test", "value": 42})");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->isValid());
    EXPECT_TRUE(result->root().isObject());
}

TEST(JsonDocumentTest, ParseValidArray) {
    auto result = JsonDocument::parse(R"([1, 2, 3])");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->isValid());
    EXPECT_TRUE(result->root().isArray());
}

TEST(JsonDocumentTest, ParsePrimitiveString) {
    auto result = JsonDocument::parse(R"("hello")");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->root().getString(), "hello");
}

TEST(JsonDocumentTest, ParsePrimitiveNumber) {
    auto result = JsonDocument::parse("42");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->root().getInt(), 42);
}

TEST(JsonDocumentTest, ParsePrimitiveBool) {
    auto result = JsonDocument::parse("true");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->root().getBool());
}

TEST(JsonDocumentTest, ParseNull) {
    auto result = JsonDocument::parse("null");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->root().isNull());
}

// =============================================================================
// PARSE INVALID JSON
// =============================================================================

TEST(JsonDocumentTest, ParseInvalidJsonReturnsError) {
    auto result = JsonDocument::parse("{broken json!!}");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::ParseError);
}

TEST(JsonDocumentTest, ParseEmptyStringReturnsError) {
    auto result = JsonDocument::parse("");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::ParseError);
}

#endif  // !__MINGW32__

// =============================================================================
// JSON VALUE TYPE CHECKING
// =============================================================================

TEST(JsonValueTest, TypeEnumValues) {
    // Verify all Type enum values via constructed JsonValues
    JsonValue nullVal;
    EXPECT_EQ(nullVal.type(), JsonValue::Type::Null);

    JsonValue boolVal(nlohmann::json(true));
    EXPECT_EQ(boolVal.type(), JsonValue::Type::Bool);

    JsonValue strVal(nlohmann::json("text"));
    EXPECT_EQ(strVal.type(), JsonValue::Type::String);

    JsonValue arrVal(nlohmann::json::array({1, 2}));
    EXPECT_EQ(arrVal.type(), JsonValue::Type::Array);

    JsonValue objVal(nlohmann::json::object({{"k", "v"}}));
    EXPECT_EQ(objVal.type(), JsonValue::Type::Object);

    // nlohmann: is_number_integer() returns true for both signed and unsigned,
    // and type() checks is_number_integer() first, so all integers map to Int64
    JsonValue uintVal(nlohmann::json(static_cast<uint64_t>(100)));
    EXPECT_EQ(uintVal.type(), JsonValue::Type::Int64);

    JsonValue intVal(nlohmann::json(static_cast<int64_t>(-5)));
    EXPECT_EQ(intVal.type(), JsonValue::Type::Int64);

    JsonValue dblVal(nlohmann::json(3.14));
    EXPECT_EQ(dblVal.type(), JsonValue::Type::Double);
}

TEST(JsonValueTest, IsNullOnDefaultConstructed) {
    JsonValue val;
    EXPECT_TRUE(val.isNull());
    EXPECT_FALSE(val.isObject());
    EXPECT_FALSE(val.isArray());
}

TEST(JsonValueTest, IsObjectAndIsArray) {
    JsonValue obj(nlohmann::json::object());
    EXPECT_TRUE(obj.isObject());
    EXPECT_FALSE(obj.isArray());

    JsonValue arr(nlohmann::json::array());
    EXPECT_TRUE(arr.isArray());
    EXPECT_FALSE(arr.isObject());
}

// =============================================================================
// JSON VALUE ACCESSORS (optional returning)
// =============================================================================

TEST(JsonValueTest, AsBoolReturnsValue) {
    JsonValue val(nlohmann::json(true));
    auto opt = val.asBool();
    ASSERT_TRUE(opt.has_value());
    EXPECT_TRUE(*opt);
}

TEST(JsonValueTest, AsBoolReturnsNulloptOnNonBool) {
    JsonValue val(nlohmann::json(42));
    EXPECT_FALSE(val.asBool().has_value());
}

TEST(JsonValueTest, AsInt64ReturnsValue) {
    JsonValue val(nlohmann::json(static_cast<int64_t>(-99)));
    auto opt = val.asInt64();
    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ(*opt, -99);
}

TEST(JsonValueTest, AsUint64ReturnsValue) {
    JsonValue val(nlohmann::json(static_cast<uint64_t>(123)));
    auto opt = val.asUint64();
    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ(*opt, 123u);
}

TEST(JsonValueTest, AsDoubleReturnsValue) {
    JsonValue val(nlohmann::json(2.718));
    auto opt = val.asDouble();
    ASSERT_TRUE(opt.has_value());
    EXPECT_DOUBLE_EQ(*opt, 2.718);
}

TEST(JsonValueTest, AsStringReturnsValue) {
    JsonValue val(nlohmann::json("hello world"));
    auto opt = val.asString();
    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ(*opt, "hello world");
}

TEST(JsonValueTest, AsStringReturnsNulloptOnNonString) {
    JsonValue val(nlohmann::json(42));
    EXPECT_FALSE(val.asString().has_value());
}

// =============================================================================
// DEFAULT VALUE GETTERS
// =============================================================================

TEST(JsonValueTest, GetStringWithDefault) {
    JsonValue strVal(nlohmann::json("found"));
    EXPECT_EQ(strVal.getString("fallback"), "found");

    // Non-string returns default
    JsonValue numVal(nlohmann::json(42));
    EXPECT_EQ(numVal.getString("fallback"), "fallback");
}

TEST(JsonValueTest, GetIntWithDefault) {
    JsonValue val(nlohmann::json(static_cast<int64_t>(7)));
    EXPECT_EQ(val.getInt(-1), 7);

    // Non-integer returns default
    JsonValue strVal(nlohmann::json("text"));
    EXPECT_EQ(strVal.getInt(-1), -1);
}

TEST(JsonValueTest, GetBoolWithDefault) {
    JsonValue val(nlohmann::json(false));
    EXPECT_FALSE(val.getBool(true));

    // Non-bool returns default
    JsonValue nullVal;
    EXPECT_TRUE(nullVal.getBool(true));
}

// ---------------------------------------------------------------------------
// ADDITIONAL EDGE CASES (nlohmann-only, MinGW-safe)
// ---------------------------------------------------------------------------

TEST(JsonValueTest, GetStringDefaultEmpty) {
    // Non-string value with empty string default
    JsonValue intVal(nlohmann::json(42));
    EXPECT_EQ(intVal.getString(), "");
    EXPECT_EQ(intVal.getString(""), "");

    // Null value with empty default
    JsonValue nullVal;
    EXPECT_EQ(nullVal.getString(), "");
}

TEST(JsonValueTest, GetIntDefaultZero) {
    // Non-integer value with zero default
    JsonValue strVal(nlohmann::json("text"));
    EXPECT_EQ(strVal.getInt(), 0);
    EXPECT_EQ(strVal.getInt(0), 0);

    // Null value returns zero
    JsonValue nullVal;
    EXPECT_EQ(nullVal.getInt(), 0);
}

TEST(JsonValueTest, GetBoolDefaultFalse) {
    // Non-bool value with false default
    JsonValue strVal(nlohmann::json("text"));
    EXPECT_FALSE(strVal.getBool());
    EXPECT_FALSE(strVal.getBool(false));

    // Null value returns false
    JsonValue nullVal;
    EXPECT_FALSE(nullVal.getBool());
}

TEST(JsonValueTest, NullTypeCheck) {
    // Default-constructed is Null type
    JsonValue nullVal;
    EXPECT_EQ(nullVal.type(), JsonValue::Type::Null);
    EXPECT_TRUE(nullVal.isNull());
    EXPECT_FALSE(nullVal.isObject());
    EXPECT_FALSE(nullVal.isArray());

    // Explicit null JSON also maps to Null
    JsonValue explicitNull(nlohmann::json(nullptr));
    EXPECT_EQ(explicitNull.type(), JsonValue::Type::Null);
    EXPECT_TRUE(explicitNull.isNull());
}

// simdjson-dependent tests (segfaults on MinGW)
#if !defined(__MINGW32__) && !defined(__MINGW64__)

// =============================================================================
// NESTED OBJECT ACCESS
// =============================================================================

TEST(JsonDocumentTest, NestedObjectAccess) {
    auto result = JsonDocument::parse(R"({
        "outer": {
            "inner": {
                "deep": "treasure"
            }
        }
    })");
    ASSERT_TRUE(result.has_value());

    auto deep = result->root()["outer"]["inner"]["deep"];
    EXPECT_EQ(deep.getString(), "treasure");
}

TEST(JsonDocumentTest, MissingKeyReturnsNullValue) {
    auto result = JsonDocument::parse(R"({"exists": 1})");
    ASSERT_TRUE(result.has_value());

    auto missing = result->root()["nonexistent"];
    EXPECT_TRUE(missing.isNull());
}

// =============================================================================
// ARRAY ACCESS
// =============================================================================

TEST(JsonDocumentTest, ArrayAccessByIndex) {
    auto result = JsonDocument::parse(R"(["alpha", "beta", "gamma"])");
    ASSERT_TRUE(result.has_value());

    auto root = result->root();
    EXPECT_EQ(root[0].getString(), "alpha");
    EXPECT_EQ(root[1].getString(), "beta");
    EXPECT_EQ(root[2].getString(), "gamma");
}

TEST(JsonDocumentTest, ArrayOutOfBoundsReturnsNull) {
    auto result = JsonDocument::parse(R"([1, 2])");
    ASSERT_TRUE(result.has_value());

    auto outOfBounds = result->root()[999];
    EXPECT_TRUE(outOfBounds.isNull());
}

// =============================================================================
// hasKey() AND size()
// =============================================================================

TEST(JsonDocumentTest, HasKeyOnObject) {
    auto result = JsonDocument::parse(R"({"name": "test", "count": 5})");
    ASSERT_TRUE(result.has_value());

    EXPECT_TRUE(result->hasKey("name"));
    EXPECT_TRUE(result->hasKey("count"));
    EXPECT_FALSE(result->hasKey("missing"));
}

TEST(JsonDocumentTest, SizeOnArrayAndObject) {
    auto arr = JsonDocument::parse(R"([10, 20, 30, 40])");
    ASSERT_TRUE(arr.has_value());
    EXPECT_EQ(arr->root().size(), 4u);

    auto obj = JsonDocument::parse(R"({"a": 1, "b": 2})");
    ASSERT_TRUE(obj.has_value());
    EXPECT_EQ(obj->root().size(), 2u);

    // Primitives have size 0
    auto prim = JsonDocument::parse("42");
    ASSERT_TRUE(prim.has_value());
    EXPECT_EQ(prim->root().size(), 0u);
}

// =============================================================================
// DOCUMENT CONVENIENCE ACCESSORS
// =============================================================================

TEST(JsonDocumentTest, DocumentGetStringAndGetInt) {
    auto result = JsonDocument::parse(R"({"title": "Makine-Launcher", "version": 1})");
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->getString("title"), "Makine-Launcher");
    EXPECT_EQ(result->getInt("version"), 1);
    EXPECT_EQ(result->getString("missing", "default"), "default");
    EXPECT_EQ(result->getInt("missing", -1), -1);
}

TEST(JsonDocumentTest, DocumentGetBoolDoubleUint) {
    auto result = JsonDocument::parse(R"({"flag": true, "pi": 3.14, "count": 100})");
    ASSERT_TRUE(result.has_value());

    EXPECT_TRUE(result->getBool("flag"));
    EXPECT_DOUBLE_EQ(result->getDouble("pi"), 3.14);
    EXPECT_EQ(result->getUint("count"), 100u);
}

// =============================================================================
// toNlohmann() CONVERSION
// =============================================================================

TEST(JsonDocumentTest, ToNlohmannRoundTrip) {
    std::string original = R"({"key":"value","num":42,"arr":[1,2,3]})";
    auto result = JsonDocument::parse(original);
    ASSERT_TRUE(result.has_value());

    nlohmann::json converted = result->toNlohmann();
    EXPECT_EQ(converted["key"], "value");
    EXPECT_EQ(converted["num"], 42);
    EXPECT_EQ(converted["arr"].size(), 3u);
    EXPECT_EQ(converted["arr"][0], 1);
}

// =============================================================================
// ARRAY ITERATOR
// =============================================================================

TEST(JsonDocumentTest, ArrayIteratorRangeFor) {
    auto result = JsonDocument::parse(R"({"items": ["a", "b", "c"]})");
    ASSERT_TRUE(result.has_value());

    // Store the array value to keep it alive during iteration
    auto items = result->root()["items"];
    std::vector<std::string> collected;
    for (auto elem : iterate(items)) {
        collected.push_back(elem.getString());
    }

    ASSERT_EQ(collected.size(), 3u);
    EXPECT_EQ(collected[0], "a");
    EXPECT_EQ(collected[1], "b");
    EXPECT_EQ(collected[2], "c");
}

TEST(JsonDocumentTest, ArrayIteratorOnEmptyArray) {
    auto result = JsonDocument::parse(R"({"items": []})");
    ASSERT_TRUE(result.has_value());

    auto items = result->root()["items"];
    int count = 0;
    for ([[maybe_unused]] auto elem : iterate(items)) {
        ++count;
    }
    EXPECT_EQ(count, 0);
}

// =============================================================================
// NULL VALUE HANDLING
// =============================================================================

TEST(JsonValueTest, NullValueAccessorsReturnDefaults) {
    JsonValue nullVal;

    EXPECT_EQ(nullVal.getString("def"), "def");
    EXPECT_EQ(nullVal.getInt(99), 99);
    EXPECT_EQ(nullVal.getUint(88), 88u);
    EXPECT_DOUBLE_EQ(nullVal.getDouble(1.5), 1.5);
    EXPECT_FALSE(nullVal.getBool(false));
    EXPECT_FALSE(nullVal.hasKey("anything"));
    EXPECT_EQ(nullVal.size(), 0u);
}

TEST(JsonValueTest, NullValueSubscriptReturnsNull) {
    JsonValue nullVal;
    auto child = nullVal["key"];
    EXPECT_TRUE(child.isNull());

    auto indexChild = nullVal[0];
    EXPECT_TRUE(indexChild.isNull());
}

// =============================================================================
// FILE PARSE TESTS
// =============================================================================

TEST_F(JsonUtilsFileTest, ParseFileValid) {
    auto path = writeFile("valid.json", R"({"game": "test", "version": 2})");

    auto result = JsonDocument::parseFile(path);
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_TRUE(result->isValid());
    EXPECT_EQ(result->getString("game"), "test");
    EXPECT_EQ(result->getInt("version"), 2);
}

TEST_F(JsonUtilsFileTest, ParseFileNonexistent) {
    auto result = JsonDocument::parseFile(tempDir_ / "does_not_exist.json");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::FileNotFound);
}

TEST_F(JsonUtilsFileTest, ParseFileMalformed) {
    auto path = writeFile("broken.json", "{this is not json!}");

    auto result = JsonDocument::parseFile(path);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::ParseError);
}

// =============================================================================
// FREE FUNCTIONS AND BACKEND INFO
// =============================================================================

TEST(JsonFreeFunctionsTest, ParseFreeFunction) {
    auto result = json::parse(R"({"free": true})");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->root()["free"].getBool());
}

TEST_F(JsonUtilsFileTest, ParseFileFreeFunction) {
    auto path = writeFile("free.json", R"({"loaded": true})");
    auto result = json::parseFile(path);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->getBool("loaded"));
}

TEST(JsonFreeFunctionsTest, BackendInfoNotEmpty) {
    std::string info = json::backendInfo();
    EXPECT_FALSE(info.empty());
}

TEST(JsonFreeFunctionsTest, HasSimdjsonReturnsBool) {
    // Just verify it compiles and returns a bool without crashing
    [[maybe_unused]] bool has = json::hasSimdjson();
}

#endif  // !__MINGW32__
