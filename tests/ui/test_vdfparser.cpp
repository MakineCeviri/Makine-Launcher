#include <gtest/gtest.h>
#include "vdfparser.h"

namespace makine::testing {

TEST(VdfParser, SimpleKeyValue) {
    auto result = vdf::parse(R"("key" "value")");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->getString("key"), "value");
    EXPECT_FALSE(result->isObject() && result->find("key")->isObject());
}

TEST(VdfParser, NestedObject) {
    auto result = vdf::parse(R"(
        "root"
        {
            "child"    "hello"
            "nested"
            {
                "deep"    "world"
            }
        }
    )");
    ASSERT_TRUE(result.has_value());

    const auto* root = result->find("root");
    ASSERT_NE(root, nullptr);
    EXPECT_TRUE(root->isObject());
    EXPECT_EQ(root->getString("child"), "hello");

    const auto* nested = root->find("nested");
    ASSERT_NE(nested, nullptr);
    EXPECT_TRUE(nested->isObject());
    EXPECT_EQ(nested->getString("deep"), "world");
}

TEST(VdfParser, SteamLibraryFolders) {
    auto result = vdf::parse(R"(
        "libraryfolders"
        {
            "0"
            {
                "path"        "C:\\Program Files (x86)\\Steam"
                "label"       ""
                "contentid"   "123456789"
                "apps"
                {
                    "228980"    "12345678"
                    "730"       "87654321"
                }
            }
            "1"
            {
                "path"        "D:\\SteamLibrary"
                "label"       "Games"
                "apps"
                {
                    "292030"    "55555555"
                }
            }
        }
    )");
    ASSERT_TRUE(result.has_value());

    const auto* folders = result->find("libraryfolders");
    ASSERT_NE(folders, nullptr);

    const auto* lib0 = folders->find("0");
    ASSERT_NE(lib0, nullptr);
    EXPECT_EQ(lib0->getString("path"), "C:\\Program Files (x86)\\Steam");
    EXPECT_EQ(lib0->getString("label"), "");

    const auto* apps0 = lib0->find("apps");
    ASSERT_NE(apps0, nullptr);
    EXPECT_EQ(apps0->getString("228980"), "12345678");
    EXPECT_EQ(apps0->getString("730"), "87654321");

    const auto* lib1 = folders->find("1");
    ASSERT_NE(lib1, nullptr);
    EXPECT_EQ(lib1->getString("path"), "D:\\SteamLibrary");
    EXPECT_EQ(lib1->getString("label"), "Games");
}

TEST(VdfParser, AppManifestAcf) {
    auto result = vdf::parse(R"(
        "AppState"
        {
            "appid"               "292030"
            "Universe"            "1"
            "name"                "The Witcher 3: Wild Hunt"
            "StateFlags"          "4"
            "installdir"          "The Witcher 3 Wild Hunt"
            "SizeOnDisk"          "31457280000"
            "UserConfig"
            {
                "language"        "english"
            }
            "InstalledDepots"
            {
                "292031"
                {
                    "manifest"    "1234567890123456789"
                    "size"        "31457280000"
                }
            }
        }
    )");
    ASSERT_TRUE(result.has_value());

    const auto* app = result->find("AppState");
    ASSERT_NE(app, nullptr);
    EXPECT_EQ(app->getString("appid"), "292030");
    EXPECT_EQ(app->getString("name"), "The Witcher 3: Wild Hunt");
    EXPECT_EQ(app->getString("installdir"), "The Witcher 3 Wild Hunt");

    const auto* userConfig = app->find("UserConfig");
    ASSERT_NE(userConfig, nullptr);
    EXPECT_EQ(userConfig->getString("language"), "english");

    const auto* depots = app->find("InstalledDepots");
    ASSERT_NE(depots, nullptr);
    const auto* depot = depots->find("292031");
    ASSERT_NE(depot, nullptr);
    EXPECT_EQ(depot->getString("manifest"), "1234567890123456789");
}

TEST(VdfParser, EscapeSequences) {
    auto result = vdf::parse(R"("backslash" "a\\b")");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->getString("backslash"), "a\\b");

    result = vdf::parse(R"("quote" "say \"hello\"")");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->getString("quote"), "say \"hello\"");

    result = vdf::parse(R"("newline" "line1\nline2")");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->getString("newline"), "line1\nline2");

    result = vdf::parse(R"("tab" "col1\tcol2")");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->getString("tab"), "col1\tcol2");
}

TEST(VdfParser, CommentsIgnored) {
    auto result = vdf::parse(
        "// This is a comment\n"
        "\"key1\"    \"value1\"\n"
        "// Another comment\n"
        "\"key2\"    \"value2\"\n"
    );
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->getString("key1"), "value1");
    EXPECT_EQ(result->getString("key2"), "value2");
}

TEST(VdfParser, GetStringDefault) {
    auto result = vdf::parse(R"("existing" "yes")");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->getString("existing"), "yes");
    EXPECT_EQ(result->getString("missing"), "");
    EXPECT_EQ(result->getString("missing", "fallback"), "fallback");
}

TEST(VdfParser, FindReturnsNullptrForMissing) {
    auto result = vdf::parse(R"("a" "1")");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->find("a"), nullptr);
    EXPECT_EQ(result->find("nonexistent"), nullptr);
    EXPECT_EQ(result->find(""), nullptr);
}

TEST(VdfParser, EmptyInput) {
    auto result = vdf::parse("");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->children.empty());
    EXPECT_FALSE(result->isObject());
}

TEST(VdfParser, MalformedMissingClosingQuote) {
    auto result = vdf::parse(R"("key" "unterminated)");
    ASSERT_TRUE(result.has_value());
    // Parser is lenient — it reads until EOF when quote is missing
    EXPECT_EQ(result->getString("key"), "unterminated");
}

TEST(VdfParser, MalformedMissingClosingBrace) {
    auto result = vdf::parse(R"(
        "root"
        {
            "child"    "value"
    )");
    ASSERT_TRUE(result.has_value());
    const auto* root = result->find("root");
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->getString("child"), "value");
}

} // namespace makine::testing
