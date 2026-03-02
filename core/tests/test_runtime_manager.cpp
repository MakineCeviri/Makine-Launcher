/**
 * @file test_runtime_manager.cpp
 * @brief Unit tests for Unity runtime translation manager
 *
 * Copyright (c) 2026 MakineAI Team
 */

// runtime_manager.hpp transitively includes <chrono> which triggers
// utc_clock errors on MinGW GCC 13.1 — skip all tests
#if defined(__MINGW32__) || defined(__MINGW64__)
#include <gtest/gtest.h>
TEST(RuntimeManagerTest, DISABLED_SkippedOnMinGW) { GTEST_SKIP() << "utc_clock broken on MinGW GCC 13.1"; }
#else
#include <gtest/gtest.h>
#include <makineai/runtime_manager.hpp>

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

namespace makineai {
namespace testing {

namespace fs = std::filesystem;

class RuntimeManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = fs::temp_directory_path() / "makineai_runtime_tests";
        fs::create_directories(testDir_);
        manager_.setBundleDirectory(testDir_ / "bundles");
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(testDir_, ec);
    }

    // Helper: create a minimal Unity Mono game layout
    void createMonoGameDir(const fs::path& dir) {
        fs::create_directories(dir);
        // _Data/Managed with UnityEngine.dll
        auto dataDir = dir / "TestGame_Data" / "Managed";
        fs::create_directories(dataDir);
        std::ofstream(dataDir / "UnityEngine.dll") << "fake";
        std::ofstream(dataDir / "Assembly-CSharp.dll") << "fake";
    }

    // Helper: create a minimal Unity IL2CPP game layout
    void createIL2CPPGameDir(const fs::path& dir) {
        fs::create_directories(dir);
        std::ofstream(dir / "GameAssembly.dll") << "fake";
        auto dataDir = dir / "TestGame_Data";
        fs::create_directories(dataDir);
    }

    // Helper: create a bundle directory structure
    void createBundle(const fs::path& bundleDir, const std::string& backend) {
        auto bundlePath = bundleDir / ("bepinex_" + backend);
        fs::create_directories(bundlePath / "BepInEx" / "core");
        fs::create_directories(bundlePath / "BepInEx" / "plugins");
        std::ofstream(bundlePath / "winhttp.dll") << "fake";
        std::ofstream(bundlePath / "doorstop_config.ini") << "fake";
        if (backend == "il2cpp") {
            std::ofstream(bundlePath / "dobby.dll") << "fake";
        }
    }

    // Helper: create a BepInEx installation in game dir
    void installBepInEx(const fs::path& gameDir) {
        auto bepinex = gameDir / "BepInEx";
        fs::create_directories(bepinex / "core");
        fs::create_directories(bepinex / "plugins" / "XUnity.AutoTranslator");
        std::ofstream(bepinex / "plugins" / "XUnity.AutoTranslator" / "XUnity.AutoTranslator.Plugin.dll")
            << "fake";
    }

    fs::path testDir_;
    RuntimeManager manager_;
};

// =========================================================================
// needsRuntime
// =========================================================================

TEST_F(RuntimeManagerTest, NeedsRuntimeUnityMono) {
    GameInfo game;
    game.engine = GameEngine::Unity_Mono;
    EXPECT_TRUE(manager_.needsRuntime(game));
}

TEST_F(RuntimeManagerTest, NeedsRuntimeUnityIL2CPP) {
    GameInfo game;
    game.engine = GameEngine::Unity_IL2CPP;
    EXPECT_TRUE(manager_.needsRuntime(game));
}

TEST_F(RuntimeManagerTest, NeedsRuntimeUnreal) {
    GameInfo game;
    game.engine = GameEngine::Unreal;
    EXPECT_FALSE(manager_.needsRuntime(game));
}

TEST_F(RuntimeManagerTest, NeedsRuntimeGameMaker) {
    GameInfo game;
    game.engine = GameEngine::GameMaker;
    EXPECT_FALSE(manager_.needsRuntime(game));
}

TEST_F(RuntimeManagerTest, NeedsRuntimeUnknown) {
    GameInfo game;
    game.engine = GameEngine::Unknown;
    EXPECT_FALSE(manager_.needsRuntime(game));
}

TEST_F(RuntimeManagerTest, NeedsRuntimeRenPy) {
    GameInfo game;
    game.engine = GameEngine::RenPy;
    EXPECT_FALSE(manager_.needsRuntime(game));
}

TEST_F(RuntimeManagerTest, NeedsRuntimeRPGMaker) {
    GameInfo game;
    game.engine = GameEngine::RPGMaker;
    EXPECT_FALSE(manager_.needsRuntime(game));
}

TEST_F(RuntimeManagerTest, NeedsRuntimeGenericUnity) {
    GameInfo game;
    game.engine = GameEngine::Unity;
    EXPECT_FALSE(manager_.needsRuntime(game));
}

// =========================================================================
// detectBackend
// =========================================================================

TEST_F(RuntimeManagerTest, DetectBackendMono) {
    auto gameDir = testDir_ / "mono_game";
    createMonoGameDir(gameDir);

    EXPECT_EQ(manager_.detectBackend(gameDir), UnityBackend::Mono);
}

TEST_F(RuntimeManagerTest, DetectBackendIL2CPP) {
    auto gameDir = testDir_ / "il2cpp_game";
    createIL2CPPGameDir(gameDir);

    EXPECT_EQ(manager_.detectBackend(gameDir), UnityBackend::IL2CPP);
}

TEST_F(RuntimeManagerTest, DetectBackendUnknown) {
    auto gameDir = testDir_ / "unknown_game";
    fs::create_directories(gameDir);
    // Empty dir, no Unity indicators
    std::ofstream(gameDir / "readme.txt") << "not a game";

    EXPECT_EQ(manager_.detectBackend(gameDir), UnityBackend::Unknown);
}

TEST_F(RuntimeManagerTest, DetectBackendIL2CPPTakesPriority) {
    // Both indicators present — IL2CPP should win because
    // GameAssembly.dll is checked first
    auto gameDir = testDir_ / "both_game";
    createMonoGameDir(gameDir);
    std::ofstream(gameDir / "GameAssembly.dll") << "fake";

    EXPECT_EQ(manager_.detectBackend(gameDir), UnityBackend::IL2CPP);
}

TEST_F(RuntimeManagerTest, DetectBackendDataDirWithoutManaged) {
    auto gameDir = testDir_ / "no_managed";
    fs::create_directories(gameDir / "Game_Data");
    // _Data exists but no Managed subdirectory

    EXPECT_EQ(manager_.detectBackend(gameDir), UnityBackend::Unknown);
}

// =========================================================================
// bundledVersion
// =========================================================================

TEST_F(RuntimeManagerTest, BundledVersionDefaultFallback) {
    // No manifest file — should return hardcoded defaults
    auto version = manager_.bundledVersion();
    EXPECT_EQ(version.bepinex.major, 5);
    EXPECT_EQ(version.bepinex.minor, 4);
    EXPECT_EQ(version.bepinex.patch, 23);
    EXPECT_EQ(version.xunity.major, 5);
    EXPECT_EQ(version.xunity.minor, 3);
    EXPECT_EQ(version.xunity.patch, 0);
    EXPECT_EQ(version.makineaiPlugin.major, 1);
    EXPECT_EQ(version.makineaiPlugin.minor, 0);
    EXPECT_EQ(version.makineaiPlugin.patch, 0);
}

TEST_F(RuntimeManagerTest, BundledVersionFromManifest) {
    auto bundleDir = testDir_ / "bundles";
    fs::create_directories(bundleDir);
    manager_.setBundleDirectory(bundleDir);

    nlohmann::json manifest;
    manifest["bepinex"] = {{"major", 6}, {"minor", 0}, {"patch", 1}};
    manifest["xunity"] = {{"major", 5}, {"minor", 4}, {"patch", 2}};
    manifest["makineai_plugin"] = {{"major", 2}, {"minor", 1}, {"patch", 0}};

    std::ofstream(bundleDir / "bundle_manifest.json") << manifest.dump(2);

    auto version = manager_.bundledVersion();
    EXPECT_EQ(version.bepinex.major, 6);
    EXPECT_EQ(version.bepinex.minor, 0);
    EXPECT_EQ(version.bepinex.patch, 1);
    EXPECT_EQ(version.xunity.major, 5);
    EXPECT_EQ(version.xunity.minor, 4);
    EXPECT_EQ(version.xunity.patch, 2);
    EXPECT_EQ(version.makineaiPlugin.major, 2);
    EXPECT_EQ(version.makineaiPlugin.minor, 1);
    EXPECT_EQ(version.makineaiPlugin.patch, 0);
}

TEST_F(RuntimeManagerTest, BundledVersionInvalidManifest) {
    auto bundleDir = testDir_ / "bundles";
    fs::create_directories(bundleDir);
    manager_.setBundleDirectory(bundleDir);

    // Write invalid JSON
    std::ofstream(bundleDir / "bundle_manifest.json") << "{ broken json !!!";

    // Should fallback to defaults
    auto version = manager_.bundledVersion();
    EXPECT_EQ(version.bepinex.major, 5);
    EXPECT_EQ(version.bepinex.minor, 4);
}

// =========================================================================
// validateBundle
// =========================================================================

TEST_F(RuntimeManagerTest, ValidateBundleMonoComplete) {
    auto bundleDir = testDir_ / "bundles";
    manager_.setBundleDirectory(bundleDir);
    createBundle(bundleDir, "mono");

    auto result = manager_.validateBundle(UnityBackend::Mono);
    EXPECT_TRUE(result.has_value()) << result.error().message();
}

TEST_F(RuntimeManagerTest, ValidateBundleIL2CPPComplete) {
    auto bundleDir = testDir_ / "bundles";
    manager_.setBundleDirectory(bundleDir);
    createBundle(bundleDir, "il2cpp");

    auto result = manager_.validateBundle(UnityBackend::IL2CPP);
    EXPECT_TRUE(result.has_value()) << result.error().message();
}

TEST_F(RuntimeManagerTest, ValidateBundleMonoMissingWinhttp) {
    auto bundleDir = testDir_ / "bundles";
    manager_.setBundleDirectory(bundleDir);
    createBundle(bundleDir, "mono");
    // Remove a required file
    fs::remove(bundleDir / "bepinex_mono" / "winhttp.dll");

    auto result = manager_.validateBundle(UnityBackend::Mono);
    EXPECT_FALSE(result.has_value());
}

TEST_F(RuntimeManagerTest, ValidateBundleIL2CPPMissingDobby) {
    auto bundleDir = testDir_ / "bundles";
    manager_.setBundleDirectory(bundleDir);
    createBundle(bundleDir, "il2cpp");
    // Remove IL2CPP-specific file
    fs::remove(bundleDir / "bepinex_il2cpp" / "dobby.dll");

    auto result = manager_.validateBundle(UnityBackend::IL2CPP);
    EXPECT_FALSE(result.has_value());
}

TEST_F(RuntimeManagerTest, ValidateBundleEmptyBundleDir) {
    // Bundle dir not set
    RuntimeManager fresh;
    auto result = fresh.validateBundle(UnityBackend::Mono);
    EXPECT_FALSE(result.has_value());
}

TEST_F(RuntimeManagerTest, ValidateBundleNonexistentDir) {
    manager_.setBundleDirectory(testDir_ / "nonexistent");
    auto result = manager_.validateBundle(UnityBackend::Mono);
    EXPECT_FALSE(result.has_value());
}

// =========================================================================
// checkStatus
// =========================================================================

TEST_F(RuntimeManagerTest, CheckStatusNotUnity) {
    auto gameDir = testDir_ / "non_unity";
    fs::create_directories(gameDir);
    std::ofstream(gameDir / "game.exe") << "fake";

    GameInfo game;
    game.installPath = gameDir;

    auto result = manager_.checkStatus(game);
    EXPECT_FALSE(result.has_value());
}

TEST_F(RuntimeManagerTest, CheckStatusNotInstalled) {
    auto gameDir = testDir_ / "mono_no_bepinex";
    createMonoGameDir(gameDir);

    GameInfo game;
    game.installPath = gameDir;

    auto result = manager_.checkStatus(game);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->installed);
    EXPECT_EQ(result->backend, UnityBackend::Mono);
}

TEST_F(RuntimeManagerTest, CheckStatusInstalled) {
    auto gameDir = testDir_ / "mono_with_bepinex";
    createMonoGameDir(gameDir);
    installBepInEx(gameDir);

    GameInfo game;
    game.installPath = gameDir;

    auto result = manager_.checkStatus(game);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->installed);
    EXPECT_EQ(result->backend, UnityBackend::Mono);
}

TEST_F(RuntimeManagerTest, CheckStatusWithTranslationFiles) {
    auto gameDir = testDir_ / "mono_with_translations";
    createMonoGameDir(gameDir);
    installBepInEx(gameDir);

    // Add translation files
    auto translationDir = gameDir / "BepInEx" / "Translation" / "tr" / "Text";
    fs::create_directories(translationDir);
    std::ofstream(translationDir / "dialog.txt") << "Hello=Merhaba";
    std::ofstream(translationDir / "ui.txt") << "Start=Baslat";

    GameInfo game;
    game.installPath = gameDir;

    auto result = manager_.checkStatus(game);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->installed);
    EXPECT_EQ(result->translationFiles.size(), 2u);
}

TEST_F(RuntimeManagerTest, CheckStatusWithVersionFile) {
    auto gameDir = testDir_ / "mono_with_version";
    createMonoGameDir(gameDir);
    installBepInEx(gameDir);

    // Create version file
    nlohmann::json versionJson;
    versionJson["bepinex"] = "5.4.23";
    versionJson["xunity"] = "5.3.0";
    versionJson["makineai"] = "1.0.0";
    std::ofstream(gameDir / "BepInEx" / "MakineAI_version.json")
        << versionJson.dump(2);

    GameInfo game;
    game.installPath = gameDir;

    auto result = manager_.checkStatus(game);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->installed);
}

// =========================================================================
// bundleDirectory accessors
// =========================================================================

TEST_F(RuntimeManagerTest, BundleDirectorySetGet) {
    auto dir = testDir_ / "custom_bundles";
    manager_.setBundleDirectory(dir);
    EXPECT_EQ(manager_.bundleDirectory(), dir);
}

TEST_F(RuntimeManagerTest, BundleDirectoryDefaultEmpty) {
    RuntimeManager fresh;
    EXPECT_TRUE(fresh.bundleDirectory().empty());
}

// =========================================================================
// analyzeUnityGame
// =========================================================================

TEST_F(RuntimeManagerTest, AnalyzeUnityGameMono) {
    auto gameDir = testDir_ / "analyze_mono";
    createMonoGameDir(gameDir);

    auto result = analyzeUnityGame(gameDir);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->isUnity);
    EXPECT_EQ(result->backend, UnityBackend::Mono);
    EXPECT_FALSE(result->hasExistingBepInEx);
}

TEST_F(RuntimeManagerTest, AnalyzeUnityGameIL2CPP) {
    auto gameDir = testDir_ / "analyze_il2cpp";
    createIL2CPPGameDir(gameDir);

    auto result = analyzeUnityGame(gameDir);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->isUnity);
    EXPECT_EQ(result->backend, UnityBackend::IL2CPP);
}

TEST_F(RuntimeManagerTest, AnalyzeUnityGameNotUnity) {
    auto gameDir = testDir_ / "analyze_not_unity";
    fs::create_directories(gameDir);
    std::ofstream(gameDir / "readme.txt") << "not a game";

    auto result = analyzeUnityGame(gameDir);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->isUnity);
    EXPECT_EQ(result->backend, UnityBackend::Unknown);
}

TEST_F(RuntimeManagerTest, AnalyzeUnityGameNonexistentDir) {
    auto result = analyzeUnityGame(testDir_ / "nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST_F(RuntimeManagerTest, AnalyzeUnityGameWithExistingBepInEx) {
    auto gameDir = testDir_ / "analyze_bepinex";
    createMonoGameDir(gameDir);
    fs::create_directories(gameDir / "BepInEx");

    auto result = analyzeUnityGame(gameDir);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->hasExistingBepInEx);
}

// =========================================================================
// Struct defaults
// =========================================================================

TEST_F(RuntimeManagerTest, RuntimeVersionDefaultEquality) {
    RuntimeVersion a, b;
    EXPECT_EQ(a, b);
}

TEST_F(RuntimeManagerTest, RuntimeStatusDefaults) {
    RuntimeStatus status;
    EXPECT_FALSE(status.installed);
    EXPECT_FALSE(status.upToDate);
    EXPECT_EQ(status.backend, UnityBackend::Unknown);
    EXPECT_TRUE(status.translationFiles.empty());
}

TEST_F(RuntimeManagerTest, XUnityConfigDefaults) {
    XUnityConfig config;
    EXPECT_EQ(config.endpoint, "MakineAI");
    EXPECT_FALSE(config.enableAutoTranslation);
    EXPECT_TRUE(config.enableTextureTranslation);
    EXPECT_TRUE(config.enableUGUIHooking);
    EXPECT_TRUE(config.enableIMGUIHooking);
    EXPECT_TRUE(config.enableNGUIHooking);
    EXPECT_TRUE(config.enableTextMeshProHooking);
}

TEST_F(RuntimeManagerTest, UnityAnalysisDefaults) {
    UnityAnalysis analysis;
    EXPECT_FALSE(analysis.isUnity);
    EXPECT_EQ(analysis.backend, UnityBackend::Unknown);
    EXPECT_TRUE(analysis.unityVersion.empty());
    EXPECT_FALSE(analysis.hasExistingBepInEx);
}

// =========================================================================
// fetchLatestRelease
// =========================================================================

TEST_F(RuntimeManagerTest, FetchLatestReleaseMono) {
    auto result = manager_.fetchLatestRelease(UnityBackend::Mono);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->downloadUrl.empty());
    EXPECT_NE(result->assetName.find("x64"), std::string::npos);
}

TEST_F(RuntimeManagerTest, FetchLatestReleaseIL2CPP) {
    auto result = manager_.fetchLatestRelease(UnityBackend::IL2CPP);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->downloadUrl.empty());
    EXPECT_NE(result->assetName.find("IL2CPP"), std::string::npos);
}

} // namespace testing
} // namespace makineai

#endif // !__MINGW32__
