/**
 * @file game_detector.cpp
 * @brief Game detector implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "makine/game_detector.hpp"
#include "makine/core.hpp"
#include "makine/security.hpp"
#include "makine/features.hpp"
#include "makine/parallel.hpp"
#include "makine/mio_utils.hpp"
#include "makine/logging.hpp"
#include "makine/metrics.hpp"

#include "engines/engine_detectors.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

// Optional: efsw for filesystem watching
#ifdef MAKINE_HAS_EFSW
#include <efsw/efsw.hpp>
#endif

namespace makine::scanners {

GameDetector::GameDetector() {
    registerBuiltinScanners();
}

GameDetector::~GameDetector() = default;

void GameDetector::registerScanner(std::unique_ptr<IGameScanner> scanner) {
    if (scanner) {
        logger()->debug("Registering scanner: {}", scanner->name());
        scanners_.push_back(std::move(scanner));
    }
}

void GameDetector::registerBuiltinScanners() {
    // Register built-in scanners
    registerScanner(std::make_unique<SteamScanner>());
    registerScanner(std::make_unique<EpicScanner>());
    registerScanner(std::make_unique<GOGScanner>());

    logger()->info("Registered {} game scanners", scanners_.size());
}

Result<std::vector<GameInfo>> GameDetector::scanAll(ProgressCallback progress) const {
    MAKINE_TIMED_SCOPE(log::DETECTOR, "scanAll");

    auto scanAllTimer = metrics().timer("detector_scan_all");
    const uint32_t totalScanners = static_cast<uint32_t>(scanners_.size());

    if (scanners_.empty()) {
        MAKINE_LOG_WARN(log::DETECTOR, "No scanners registered");
        return std::vector<GameInfo>{};
    }

    MAKINE_LOG_INFO(log::DETECTOR, "Starting game scan with {} scanners (backend: {})",
                      totalScanners, parallel::backendInfo());
    metrics().increment("scan_operations");

    // Filter to available scanners first
    std::vector<IGameScanner*> availableScanners;
    availableScanners.reserve(scanners_.size());

    for (const auto& scanner : scanners_) {
        if (scanner->isAvailable()) {
            availableScanners.push_back(scanner.get());
            MAKINE_LOG_DEBUG(log::DETECTOR, "Scanner available: {}", scanner->name());
        } else {
            MAKINE_LOG_DEBUG(log::DETECTOR, "Scanner not available: {}", scanner->name());
        }
    }

    if (availableScanners.empty()) {
        MAKINE_LOG_WARN(log::DETECTOR, "No game stores available");
        return std::vector<GameInfo>{};
    }

    // Progress callback wrapper for parallel execution
    auto parallelProgress = [&progress, totalScanners](uint32_t current, uint32_t /*total*/,
                                                        const std::string& message) {
        if (progress) {
            progress(current, totalScanners, message);
        }
    };

    // Scan all stores in parallel
    auto* self = this;  // MSVC C3482 workaround
    auto scanResults = parallel::map(
        availableScanners,
        [self](IGameScanner* scanner) -> std::vector<GameInfo> {
            MAKINE_LOG_DEBUG(log::DETECTOR, "Scanning: {}", scanner->name());

            auto result = scanner->scan();
            if (!result) {
                MAKINE_LOG_WARN(log::DETECTOR, "Scanner {} failed: {}",
                                  scanner->name(), result.error().message());
                return {};
            }

            std::vector<GameInfo> games = std::move(*result);

            // Detect engine for games that don't have it set
            for (auto& game : games) {
                if (game.engine == GameEngine::Unknown) {
                    game.engine = self->detectEngine(game.installPath);
                }
            }

            MAKINE_LOG_INFO(log::DETECTOR, "Found {} games from {}",
                              games.size(), scanner->name());
            return games;
        },
        parallelProgress
    );

    // Merge results from all scanners
    std::vector<GameInfo> allGames;
    size_t totalGames = 0;
    for (const auto& games : scanResults) {
        totalGames += games.size();
    }
    allGames.reserve(totalGames);

    for (auto& games : scanResults) {
        for (auto& game : games) {
            allGames.push_back(std::move(game));
        }
    }

    // Deduplicate cross-store entries by install path
    {
        std::unordered_map<std::string, size_t> pathMap;
        std::vector<GameInfo> unique;
        unique.reserve(allGames.size());

        for (auto& g : allGames) {
            std::string normPath = g.installPath.string();
            std::transform(normPath.begin(), normPath.end(), normPath.begin(), ::tolower);

            auto it = pathMap.find(normPath);
            if (it != pathMap.end()) {
                // Prefer Steam over Epic (Steam has AppID for catalog matching)
                auto& existing = unique[it->second];
                if (g.id.store == GameStore::Steam && existing.id.store != GameStore::Steam) {
                    existing = std::move(g);
                }
                MAKINE_LOG_DEBUG(log::DETECTOR, "Dedup: Skipping duplicate '{}'", g.name);
                continue;
            }
            pathMap[normPath] = unique.size();
            unique.push_back(std::move(g));
        }
        allGames = std::move(unique);
    }
    MAKINE_LOG_INFO(log::DETECTOR, "After dedup: {} unique games", allGames.size());

    // Final progress callback
    if (progress) {
        progress(totalScanners, totalScanners,
                 fmt::format("Found {} games", allGames.size()));
    }

    // Sort by name for consistent ordering
    std::sort(allGames.begin(), allGames.end(),
              [](const GameInfo& a, const GameInfo& b) {
                  return a.name < b.name;
              });

    MAKINE_LOG_INFO(log::DETECTOR, "Scan complete: {} total games from {} stores",
                      allGames.size(), availableScanners.size());

    metrics().gauge("total_games_detected", static_cast<double>(allGames.size()));
    metrics().gauge("active_scanners", static_cast<double>(availableScanners.size()));

    return allGames;
}

Result<std::vector<GameInfo>> GameDetector::scanStore(GameStore store) const {
    for (const auto& scanner : scanners_) {
        if (scanner->storeType() == store) {
            if (!scanner->isAvailable()) {
                return std::unexpected(Error(ErrorCode::GameNotFound,
                    fmt::format("{} not available", scanner->name())));
            }
            return scanner->scan();
        }
    }

    return std::unexpected(Error(ErrorCode::InvalidArgument,
        "No scanner registered for this store"));
}

Result<GameInfo> GameDetector::detectGame(const fs::path& gamePath) const {
    if (!fs::exists(gamePath)) {
        return std::unexpected(Error(ErrorCode::DirectoryNotFound,
            fmt::format("Path not found: {}", gamePath.string())));
    }

    GameInfo game;
    game.installPath = gamePath;
    game.id.store = GameStore::Manual;

    // Scan top-level directory: collect exe names and all entries
    fs::path exePath;
    fs::path bestExePath;
    for (const auto& entry : fs::directory_iterator(gamePath)) {
        auto name = entry.path().filename().string();
        game.topLevelEntries.push_back(name);

        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".exe") {
                auto filename = name;
                std::transform(filename.begin(), filename.end(),
                    filename.begin(), ::tolower);

                // Skip known non-game executables
                bool isLauncher = (filename.find("launcher") != std::string::npos ||
                                   filename.find("crash") != std::string::npos ||
                                   filename.find("unins") != std::string::npos ||
                                   filename.find("redist") != std::string::npos ||
                                   filename.find("setup") != std::string::npos ||
                                   filename.find("dxsetup") != std::string::npos ||
                                   filename.find("vcredist") != std::string::npos ||
                                   filename.find("dotnet") != std::string::npos);

                if (!isLauncher) {
                    game.executableNames.push_back(filename);
                    if (!bestExePath.empty()) {
                        // Keep first non-launcher as best
                    } else {
                        bestExePath = entry.path();
                    }
                }
                // Always track for fallback
                exePath = entry.path();
            }
        }
    }

    // Use best non-launcher exe, or fallback to any exe
    if (!bestExePath.empty()) exePath = bestExePath;

    if (exePath.empty()) {
        return std::unexpected(Error(ErrorCode::GameNotFound,
            fmt::format("No executable found in: {}", gamePath.string())));
    }

    game.executablePath = exePath;
    game.name = gamePath.filename().string();

    // Calculate hash
    auto hashResult = calculateHash(exePath);
    if (hashResult) {
        game.id.exeHash = *hashResult;
    }

    // Detect architecture
    auto archResult = is64Bit(exePath);
    if (archResult) {
        game.is64Bit = *archResult;
    }

    // Detect engine
    game.engine = detectEngine(gamePath);

    return game;
}

Result<VerificationResult> GameDetector::verify(const GameInfo& game) const {
    VerificationResult result;
    result.verified = false;

    if (!fs::exists(game.executablePath)) {
        return std::unexpected(Error(ErrorCode::FileNotFound,
            "Executable not found"));
    }

    auto hashResult = calculateHash(game.executablePath);
    if (!hashResult) {
        return std::unexpected(hashResult.error());
    }

    result.actualHash = *hashResult;
    result.expectedHash = game.id.exeHash;
    result.verified = (result.actualHash == result.expectedHash);

    // Translation availability is determined by QML layer (PackageCatalog + ManifestSync)
    result.isKnownVersion = false;
    result.hasTranslation = false;

    return result;
}

GameEngine GameDetector::detectEngine(const fs::path& gameDir) const {
    // Use confidence-based detection and return engine type
    auto result = detectEngineWithConfidence(gameDir);
    return result.engine;
}

EngineDetectionResult GameDetector::detectEngineWithConfidence(const fs::path& gameDir) const {
    if (!fs::exists(gameDir) || !fs::is_directory(gameDir)) {
        return EngineDetectionResult{
            GameEngine::Unknown, 0, "", "Directory not found", {}
        };
    }

    // Scan for all signatures first
    auto signatures = scanForSignatures(gameDir);

    // Detector functions with priority order (free functions from engines/)
    using DetectorFunc = EngineDetectionResult (*)(const fs::path&, const GameSignatures&);

    std::vector<DetectorFunc> detectors = {
        &detectUnity,
        &detectUnreal,
        &detectBethesda,
        &detectRenpy,
        &detectRpgMakerMvMz,
        &detectRpgMakerVxAce,
        &detectGodot,
        &detectGameMaker,
        &detectSource,
        &detectCryEngine,
        &detectFrostbite,
        &detectIdTech,
    };

    EngineDetectionResult bestResult;

    for (const auto& detector : detectors) {
        auto result = detector(gameDir, signatures);

        if (result.confidence > bestResult.confidence) {
            bestResult = result;
        }

        // Early termination at 90%+ confidence
        if (result.confidence >= 90) {
            logger()->debug("Engine detected: {} ({}% confidence)",
                static_cast<int>(result.engine), result.confidence);
            return result;
        }
    }

    if (bestResult.confidence < 30) {
        return EngineDetectionResult{
            GameEngine::Unknown, 0, "", "Engine could not be detected", {}
        };
    }

    logger()->debug("Engine detected: {} ({}% confidence)",
        static_cast<int>(bestResult.engine), bestResult.confidence);
    return bestResult;
}

GameSignatures GameDetector::scanForSignatures(const fs::path& gameDir) const {
    GameSignatures sig;

    if (!fs::exists(gameDir)) {
        return sig;
    }

    try {
        size_t fileCount = 0;
        const std::string gamePath = gameDir.string();

        for (auto it = fs::recursive_directory_iterator(
                gameDir,
                fs::directory_options::skip_permission_denied |
                fs::directory_options::follow_directory_symlink);
             it != fs::recursive_directory_iterator(); ++it) {

            // Performance limit
            if (++fileCount > GameSignatures::MAX_FILES_TO_SCAN) {
                logger()->debug("File limit reached, stopping scan");
                break;
            }

            try {
                const auto& entry = *it;
                auto relativePath = fs::relative(entry.path(), gameDir).string();
                auto fileName = entry.path().filename().string();

                // Convert to lowercase for comparison
                std::string lowerFileName = fileName;
                std::transform(lowerFileName.begin(), lowerFileName.end(),
                    lowerFileName.begin(), ::tolower);

                if (entry.is_regular_file()) {
                    // Get extension
                    auto ext = entry.path().extension().string();
                    if (!ext.empty() && ext[0] == '.') {
                        ext = ext.substr(1);
                    }
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                    if (isRelevantExtension(ext)) {
                        sig.extensions.insert(ext);
                    }

                    checkSignatureFile(sig, lowerFileName, relativePath);
                }
                else if (entry.is_directory()) {
                    checkSignatureFolder(sig, lowerFileName, relativePath);
                }
            }
            catch (const std::exception&) {
                // Skip individual file errors
                continue;
            }
        }
    }
    catch (const std::exception& e) {
        logger()->warn("Signature scan error: {}", e.what());
    }

    return sig;
}

bool GameDetector::isRelevantExtension(const std::string& ext) const {
    static const std::unordered_set<std::string> relevantExtensions = {
        // Unity
        "dll", "assets", "unity3d", "resource", "ress",
        // Unreal
        "pak", "uasset", "umap", "uexp", "ubulk",
        // Ren'Py
        "rpy", "rpyc", "rpa",
        // RPG Maker
        "rgss3a", "rvdata2", "json", "rvdata",
        // Bethesda
        "ba2", "esm", "esp", "strings",
        // Godot
        "pck",
        // GameMaker
        "win",
        // Source Engine
        "vpk", "bsp",
        // id Tech
        "pk3", "pk4", "qvm",
        // Frostbite
        "sb", "toc", "cat",
        // General
        "exe", "so", "dylib",
    };
    return relevantExtensions.count(ext) > 0;
}

void GameDetector::checkSignatureFile(GameSignatures& sig, const std::string& fileName,
                                      const std::string& path) const {
    // Unity
    if (fileName == "unityengine.dll") sig.hasUnityEngine = true;
    if (fileName == "assembly-csharp.dll") sig.hasAssemblyCSharp = true;
    if (fileName == "gameassembly.dll") sig.hasGameAssembly = true;
    if (fileName == "globalgamemanagers") sig.hasGlobalGameManagers = true;
    if (fileName == "resources.assets") sig.hasResourcesAssets = true;
    if (fileName.ends_with(".unity3d")) sig.hasUnity3d = true;

    // Unreal
    if (fileName.ends_with(".pak")) sig.hasPakFiles = true;
    if (fileName.ends_with(".uasset")) sig.hasUasset = true;
    if (fileName.ends_with(".umap")) sig.hasUmap = true;
    if (fileName == "ue4game.exe" || fileName == "ue5game.exe") sig.hasUeExecutable = true;

    // Ren'Py
    if (fileName.ends_with(".rpa")) sig.hasRpaFiles = true;
    if (fileName.ends_with(".rpyc")) sig.hasRpycFiles = true;
    if (fileName == "renpy.exe") sig.hasRenpyExe = true;

    // RPG Maker MV/MZ
    if (fileName == "rpg_core.js") sig.hasRpgCore = true;
    if (fileName == "nw.exe" || fileName == "game.exe") sig.hasNwJs = true;
    if (fileName == "package.json") sig.hasPackageJson = true;

    // RPG Maker VX Ace
    if (fileName.starts_with("rgss") && fileName.ends_with(".dll")) sig.hasRgss = true;
    if (fileName.ends_with(".rvdata2")) sig.hasRvdata2 = true;
    if (fileName.ends_with(".rgss3a")) sig.hasRgss3a = true;

    // Godot
    if (fileName.ends_with(".pck")) sig.hasPckFiles = true;
    if (fileName == "godot.exe" || fileName.find("godot") != std::string::npos) sig.hasGodotExe = true;

    // GameMaker
    if (fileName == "data.win") sig.hasDataWin = true;
    if (fileName == "options.ini") sig.hasOptionsIni = true;

    // Source Engine
    if (fileName.ends_with(".vpk")) sig.hasVpkFiles = true;
    if (fileName.ends_with(".bsp")) sig.hasBspFiles = true;
    if (fileName == "hl2.exe" || fileName == "source.exe") sig.hasSourceExe = true;

    // CryEngine
    if (fileName == "crysystem.dll") sig.hasCrySystem = true;
    if (fileName.ends_with(".pak") && path.find("gamedata") != std::string::npos) sig.hasCryPak = true;

    // Frostbite
    if (fileName.find("frosty") != std::string::npos ||
        fileName == "cascat.cat" ||
        fileName.ends_with(".sb") ||
        fileName.ends_with(".toc")) {
        sig.hasFrostbiteFiles = true;
    }

    // id Tech
    if (fileName.ends_with(".pk3") || fileName.ends_with(".pk4")) sig.hasPkFiles = true;

    // Bethesda
    if (fileName.ends_with(".ba2")) sig.hasBa2Files = true;
    if (fileName.ends_with(".esm") || fileName.ends_with(".esp")) sig.hasEsmEsp = true;
    if (fileName.ends_with(".strings")) sig.hasStringsFiles = true;
}

void GameDetector::checkSignatureFolder(GameSignatures& sig, const std::string& folderName,
                                        const std::string& /*path*/) const {
    // Unity
    if (folderName == "managed") sig.hasManagedFolder = true;
    if (folderName == "mono") sig.hasMonoFolder = true;
    if (folderName == "il2cpp_data") sig.hasIl2cppData = true;
    if (folderName.ends_with("_data")) sig.hasDataFolder = true;

    // Unreal
    if (folderName == "engine") sig.hasEngineFolder = true;
    if (folderName == "content") sig.hasContentFolder = true;

    // Ren'Py
    if (folderName == "renpy") sig.hasRenpyFolder = true;
    if (folderName == "game") sig.hasGameFolder = true;

    // RPG Maker MV/MZ
    if (folderName == "www") sig.hasWwwFolder = true;
    if (folderName == "js") sig.hasJsFolder = true;
    if (folderName == "img") sig.hasImgFolder = true;

    // RPG Maker VX Ace
    if (folderName == "data") sig.hasDataFolderRpg = true;
    if (folderName == "graphics") sig.hasGraphicsFolder = true;
    if (folderName == "audio") sig.hasAudioFolder = true;

    // Source Engine
    if (folderName == "hl2" || folderName == "csgo" || folderName == "tf") {
        sig.hasSourceGameFolder = true;
    }
}

GameEngine GameDetector::detectEngineFromFiles(const fs::path& gameDir) const {
    if (!fs::exists(gameDir) || !fs::is_directory(gameDir)) {
        return GameEngine::Unknown;
    }

    // Unity IL2CPP check
    if (fs::exists(gameDir / "GameAssembly.dll")) {
        return GameEngine::Unity_IL2CPP;
    }

    // Unity Mono check
    for (const auto& entry : fs::directory_iterator(gameDir)) {
        if (entry.is_directory()) {
            auto name = entry.path().filename().string();
            if (name.ends_with("_Data")) {
                auto managedPath = entry.path() / "Managed";
                if (fs::exists(managedPath)) {
                    if (fs::exists(managedPath / "Assembly-CSharp.dll") ||
                        fs::exists(managedPath / "UnityEngine.dll")) {
                        return GameEngine::Unity_Mono;
                    }
                }
            }
        }
    }

    // Unreal Engine check
    if (fs::exists(gameDir / "Engine") ||
        fs::exists(gameDir / "Content" / "Paks")) {
        return GameEngine::Unreal;
    }

    // Check for .pak files in common locations
    std::vector<fs::path> pakSearchPaths = {
        gameDir,
        gameDir / "Content" / "Paks",
        gameDir / "Game" / "Content" / "Paks"
    };

    for (const auto& searchPath : pakSearchPaths) {
        if (fs::exists(searchPath)) {
            for (const auto& entry : fs::directory_iterator(searchPath)) {
                auto ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".pak") {
                    return GameEngine::Unreal;
                }
            }
        }
    }

    // Bethesda check
    if (fs::exists(gameDir / "Data")) {
        for (const auto& entry : fs::directory_iterator(gameDir / "Data")) {
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".ba2" || ext == ".esm" || ext == ".esp") {
                return GameEngine::Bethesda;
            }
        }
    }

    // GameMaker check
    if (fs::exists(gameDir / "data.win")) {
        return GameEngine::GameMaker;
    }

    // Ren'Py check
    if (fs::exists(gameDir / "renpy") ||
        fs::exists(gameDir / "game" / "script.rpy") ||
        fs::exists(gameDir / "lib" / "python2.7")) {
        return GameEngine::RenPy;
    }

    // RPG Maker MV/MZ check
    if (fs::exists(gameDir / "www" / "data") ||
        fs::exists(gameDir / "data" / "System.json") ||
        fs::exists(gameDir / "js" / "rpg_core.js")) {
        return GameEngine::RPGMaker_MV;
    }

    // RPG Maker VX Ace check
    if (fs::exists(gameDir / "Data" / "System.rvdata2") ||
        fs::exists(gameDir / "RGSS301.dll") ||
        fs::exists(gameDir / "RGSS300.dll")) {
        return GameEngine::RPGMaker_VX;
    }

    return GameEngine::Unknown;
}

Result<bool> GameDetector::is64Bit(const fs::path& exePath) const {
#ifdef _WIN32
    // Read PE header to determine architecture
    std::ifstream file(exePath, std::ios::binary);
    if (!file) {
        return std::unexpected(Error(ErrorCode::FileAccessDenied,
            "Cannot open executable"));
    }

    // Read DOS header
    char dosHeader[64];
    file.read(dosHeader, 64);
    if (dosHeader[0] != 'M' || dosHeader[1] != 'Z') {
        return std::unexpected(Error(ErrorCode::InvalidFormat,
            "Not a valid PE file"));
    }

    // Get PE header offset
    uint32_t peOffset;
    std::memcpy(&peOffset, &dosHeader[60], 4);
    if (peOffset > 0x10000) {
        return std::unexpected(Error(ErrorCode::InvalidFormat,
            "PE offset too large, possibly corrupt file"));
    }

    // Seek to PE header
    file.seekg(peOffset);

    // Read PE signature
    char peSignature[4];
    file.read(peSignature, 4);
    if (peSignature[0] != 'P' || peSignature[1] != 'E') {
        return std::unexpected(Error(ErrorCode::InvalidFormat,
            "Invalid PE signature"));
    }

    // Read machine type
    uint16_t machineType;
    file.read(reinterpret_cast<char*>(&machineType), 2);

    // 0x8664 = AMD64, 0x014c = i386
    return machineType == 0x8664;
#else
    return std::unexpected(Error(ErrorCode::NotImplemented,
        "PE parsing not implemented for this platform"));
#endif
}

Result<std::string> GameDetector::calculateHash(const fs::path& file) const {
    auto& security = Core::instance().securityManager();
    return security.hashFile(file);
}

std::vector<GameInfo> GameDetector::filterWithTranslations(
    const std::vector<GameInfo>& games
) const {
    // Translation filtering is handled by the QML layer (PackageCatalog).
    // Core returns all detected games; the UI cross-references against the catalog.
    return games;
}

// =============================================================================
// CONFIDENCE-BASED ENGINE DETECTORS
// =============================================================================
// Implementations moved to engines/ subdirectory as free functions.
// Included via engines/engine_detectors.hpp (see top of this file):
//   unity_detector.cpp    — detectUnity, readUnityVersion
//   unreal_detector.cpp   — detectUnreal
//   renpy_detector.cpp    — detectRenpy, readRenpyVersion
//   rpgmaker_detector.cpp — detectRpgMakerMvMz, detectRpgMakerVxAce
//   misc_detectors.cpp    — detectBethesda, detectGodot, detectGameMaker,
//                           detectSource, detectCryEngine, detectFrostbite,
//                           detectIdTech
// =============================================================================

} // namespace makine::scanners
