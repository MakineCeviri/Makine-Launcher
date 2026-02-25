/**
 * @file game_detector.cpp
 * @brief Game detector implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/game_detector.hpp"
#include "makineai/core.hpp"
#include "makineai/security.hpp"
#include "makineai/features.hpp"
#include "makineai/parallel.hpp"
#include "makineai/mio_utils.hpp"
#include "makineai/logging.hpp"
#include "makineai/metrics.hpp"

#include <algorithm>
#include <fstream>
#include <cstring>
#include <unordered_set>

// Optional: efsw for filesystem watching
#ifdef MAKINEAI_HAS_EFSW
#include <efsw/efsw.hpp>
#endif

namespace makineai::scanners {

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
    MAKINEAI_TIMED_SCOPE(log::DETECTOR, "scanAll");

    auto scanAllTimer = metrics().timer("detector_scan_all");
    const uint32_t totalScanners = static_cast<uint32_t>(scanners_.size());

    if (scanners_.empty()) {
        MAKINEAI_LOG_WARN(log::DETECTOR, "No scanners registered");
        return std::vector<GameInfo>{};
    }

    MAKINEAI_LOG_INFO(log::DETECTOR, "Starting game scan with {} scanners (backend: {})",
                      totalScanners, parallel::backendInfo());
    metrics().increment("scan_operations");

    // Filter to available scanners first
    std::vector<IGameScanner*> availableScanners;
    availableScanners.reserve(scanners_.size());

    for (const auto& scanner : scanners_) {
        if (scanner->isAvailable()) {
            availableScanners.push_back(scanner.get());
            MAKINEAI_LOG_DEBUG(log::DETECTOR, "Scanner available: {}", scanner->name());
        } else {
            MAKINEAI_LOG_DEBUG(log::DETECTOR, "Scanner not available: {}", scanner->name());
        }
    }

    if (availableScanners.empty()) {
        MAKINEAI_LOG_WARN(log::DETECTOR, "No game stores available");
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
            MAKINEAI_LOG_DEBUG(log::DETECTOR, "Scanning: {}", scanner->name());

            auto result = scanner->scan();
            if (!result) {
                MAKINEAI_LOG_WARN(log::DETECTOR, "Scanner {} failed: {}",
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

            MAKINEAI_LOG_INFO(log::DETECTOR, "Found {} games from {}",
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

    // Final progress callback
    if (progress) {
        progress(totalScanners, totalScanners,
                 "Found " + std::to_string(allGames.size()) + " games");
    }

    // Sort by name for consistent ordering
    std::sort(allGames.begin(), allGames.end(),
              [](const GameInfo& a, const GameInfo& b) {
                  return a.name < b.name;
              });

    MAKINEAI_LOG_INFO(log::DETECTOR, "Scan complete: {} total games from {} stores",
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
                    std::string(scanner->name()) + " not available"));
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
            "Path not found: " + gamePath.string()));
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
            "No executable found in: " + gamePath.string()));
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

    // Check against package manifest for known versions
    result.isKnownVersion = false;
    result.hasTranslation = false;

    if (Core::instance().isInitialized()) {
        auto& packageManager = Core::instance().packageManager();
        const auto& manifest = packageManager.cachedManifest();

        for (const auto& pkg : manifest.packages) {
            // Check by game ID
            if (pkg.gameId == game.id.storeId) {
                result.isKnownVersion = true;
                result.hasTranslation = true;
                break;
            }

            // Check by hash
            for (const auto& hash : pkg.supportedGameHashes) {
                if (hash == result.actualHash) {
                    result.isKnownVersion = true;
                    result.hasTranslation = true;
                    break;
                }
            }

            if (result.hasTranslation) break;
        }
    }

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

    // Detector functions with priority order
    using DetectorFunc = EngineDetectionResult (GameDetector::*)(
        const fs::path&, const GameSignatures&) const;

    std::vector<DetectorFunc> detectors = {
        &GameDetector::detectUnity,
        &GameDetector::detectUnreal,
        &GameDetector::detectBethesda,
        &GameDetector::detectRenpy,
        &GameDetector::detectRpgMakerMvMz,
        &GameDetector::detectRpgMakerVxAce,
        &GameDetector::detectGodot,
        &GameDetector::detectGameMaker,
        &GameDetector::detectSource,
        &GameDetector::detectCryEngine,
        &GameDetector::detectFrostbite,
        &GameDetector::detectIdTech,
    };

    EngineDetectionResult bestResult;

    for (const auto& detector : detectors) {
        auto result = (this->*detector)(gameDir, signatures);

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
    if (!Core::instance().isInitialized()) {
        return games;  // Return all if not initialized
    }

    auto& packageManager = Core::instance().packageManager();
    const auto& manifest = packageManager.cachedManifest();

    if (manifest.packages.empty()) {
        return games;  // Return all if no manifest loaded
    }

    // Build a set of game IDs and hashes that have translations
    std::unordered_set<std::string> translatedGameIds;
    std::unordered_set<std::string> translatedHashes;

    for (const auto& pkg : manifest.packages) {
        translatedGameIds.insert(pkg.gameId);
        for (const auto& hash : pkg.supportedGameHashes) {
            translatedHashes.insert(hash);
        }
    }

    // Filter games
    std::vector<GameInfo> filtered;
    filtered.reserve(games.size());

    for (const auto& game : games) {
        if (translatedGameIds.count(game.id.storeId) > 0 ||
            translatedHashes.count(game.id.exeHash) > 0) {
            filtered.push_back(game);
        }
    }

    logger()->debug("Filtered {} games to {} with translations",
        games.size(), filtered.size());

    return filtered;
}

// =============================================================================
// CONFIDENCE-BASED ENGINE DETECTORS
// =============================================================================

EngineDetectionResult GameDetector::detectUnity(
    const fs::path& path, const GameSignatures& sig
) const {
    int confidence = 0;
    bool isIl2cpp = false;
    std::string version;
    std::vector<std::string> details;

    // IL2CPP detection (highest priority)
    if (sig.hasGameAssembly) {
        confidence += 50;
        isIl2cpp = true;
        details.push_back("GameAssembly.dll found");
    }

    if (sig.hasIl2cppData) {
        confidence += 30;
        isIl2cpp = true;
        details.push_back("il2cpp_data folder found");
    }

    // Mono detection
    if (sig.hasUnityEngine) {
        confidence += 40;
        details.push_back("UnityEngine.dll found");
    }

    if (sig.hasAssemblyCSharp) {
        confidence += 30;
        details.push_back("Assembly-CSharp.dll found");
    }

    if (sig.hasManagedFolder && !isIl2cpp) {
        confidence += 20;
        details.push_back("Managed folder found");
    }

    if (sig.hasMonoFolder) {
        confidence += 15;
        details.push_back("Mono folder found");
    }

    // General Unity indicators
    if (sig.hasGlobalGameManagers) {
        confidence += 25;
        details.push_back("globalgamemanagers found");
        version = readUnityVersion(path);
    }

    if (sig.hasResourcesAssets) {
        confidence += 15;
        details.push_back("resources.assets found");
    }

    if (sig.hasDataFolder) {
        confidence += 10;
        details.push_back("*_Data folder found");
    }

    if (sig.hasUnity3d) {
        confidence += 20;
        details.push_back(".unity3d files found");
    }

    if (confidence < 30) {
        return EngineDetectionResult{};
    }

    // Build details string
    std::string detailStr;
    for (const auto& detail : details) {
        if (!detailStr.empty()) detailStr += ", ";
        detailStr += detail;
    }

    EngineDetectionResult result;
    result.engine = isIl2cpp ? GameEngine::Unity_IL2CPP : GameEngine::Unity_Mono;
    result.confidence = std::min(confidence, 100);
    result.version = version;
    result.details = detailStr;
    result.metadata["isIl2cpp"] = isIl2cpp ? "true" : "false";
    result.metadata["hasManagedDlls"] = sig.hasAssemblyCSharp ? "true" : "false";

    return result;
}

EngineDetectionResult GameDetector::detectUnreal(
    const fs::path& /*path*/, const GameSignatures& sig
) const {
    int confidence = 0;
    std::vector<std::string> details;

    if (sig.hasPakFiles) {
        confidence += 30;
        details.push_back(".pak files found");
    }

    if (sig.hasEngineFolder) {
        confidence += 25;
        details.push_back("Engine folder found");
    }

    if (sig.hasContentFolder) {
        confidence += 20;
        details.push_back("Content folder found");
    }

    if (sig.hasUasset) {
        confidence += 25;
        details.push_back(".uasset files found");
    }

    if (sig.hasUmap) {
        confidence += 15;
        details.push_back(".umap files found");
    }

    if (sig.hasUeExecutable) {
        confidence += 30;
        details.push_back("UE executable found");
    }

    if (confidence < 30) {
        return EngineDetectionResult{};
    }

    std::string detailStr;
    for (const auto& detail : details) {
        if (!detailStr.empty()) detailStr += ", ";
        detailStr += detail;
    }

    EngineDetectionResult result;
    result.engine = GameEngine::Unreal;
    result.confidence = std::min(confidence, 100);
    result.details = detailStr;

    return result;
}

EngineDetectionResult GameDetector::detectBethesda(
    const fs::path& /*path*/, const GameSignatures& sig
) const {
    int confidence = 0;
    std::vector<std::string> details;

    if (sig.hasBa2Files) {
        confidence += 50;
        details.push_back(".ba2 archives found");
    }

    if (sig.hasEsmEsp) {
        confidence += 40;
        details.push_back(".esm/.esp plugins found");
    }

    if (sig.hasStringsFiles) {
        confidence += 25;
        details.push_back(".strings files found");
    }

    if (confidence < 30) {
        return EngineDetectionResult{};
    }

    std::string detailStr;
    for (const auto& detail : details) {
        if (!detailStr.empty()) detailStr += ", ";
        detailStr += detail;
    }

    EngineDetectionResult result;
    result.engine = GameEngine::Bethesda;
    result.confidence = std::min(confidence, 100);
    result.details = detailStr;

    return result;
}

EngineDetectionResult GameDetector::detectRenpy(
    const fs::path& path, const GameSignatures& sig
) const {
    int confidence = 0;
    std::string version;
    std::vector<std::string> details;

    if (sig.hasRenpyFolder) {
        confidence += 40;
        details.push_back("renpy folder found");
    }

    if (sig.hasRpaFiles) {
        confidence += 35;
        details.push_back(".rpa archives found");
    }

    if (sig.hasRpycFiles) {
        confidence += 25;
        details.push_back(".rpyc files found");
    }

    if (sig.hasRenpyExe) {
        confidence += 30;
        details.push_back("renpy.exe found");
    }

    if (sig.hasGameFolder && sig.hasRenpyFolder) {
        confidence += 15;
        details.push_back("Standard Ren'Py structure");
    }

    // Try to get version
    version = readRenpyVersion(path);
    if (!version.empty()) {
        confidence += 10;
        details.push_back("Version: " + version);
    }

    if (confidence < 30) {
        return EngineDetectionResult{};
    }

    std::string detailStr;
    for (const auto& detail : details) {
        if (!detailStr.empty()) detailStr += ", ";
        detailStr += detail;
    }

    EngineDetectionResult result;
    result.engine = GameEngine::RenPy;
    result.confidence = std::min(confidence, 100);
    result.version = version;
    result.details = detailStr;

    return result;
}

EngineDetectionResult GameDetector::detectRpgMakerMvMz(
    const fs::path& path, const GameSignatures& sig
) const {
    int confidence = 0;
    bool isMz = false;
    std::vector<std::string> details;

    if (sig.hasWwwFolder) {
        confidence += 30;
        details.push_back("www folder found");
    }

    if (sig.hasRpgCore) {
        confidence += 40;
        details.push_back("rpg_core.js found");
    }

    if (sig.hasJsFolder) {
        confidence += 15;
        details.push_back("js folder found");
    }

    if (sig.hasImgFolder) {
        confidence += 10;
        details.push_back("img folder found");
    }

    if (sig.hasNwJs) {
        confidence += 15;
        details.push_back("NW.js executable found");
    }

    // Try to differentiate MV from MZ
    if (sig.hasPackageJson || sig.hasWwwFolder) {
        try {
            // MZ has an 'effects' folder in www
            auto effectsDir = path / "www" / "effects";
            if (fs::exists(effectsDir) && fs::is_directory(effectsDir)) {
                isMz = true;
                confidence += 10;
                details.push_back("RPG Maker MZ detected (effects folder)");
            }

            // Check package.json for MZ indicators
            auto packageJson = path / "package.json";
            if (fs::exists(packageJson)) {
                std::ifstream file(packageJson);
                if (file) {
                    std::string content((std::istreambuf_iterator<char>(file)),
                                       std::istreambuf_iterator<char>());
                    if (content.find("MZ") != std::string::npos) {
                        isMz = true;
                        confidence += 10;
                        details.push_back("RPG Maker MZ in package.json");
                    }
                }
            }
        }
        catch (const std::exception&) {
            // Continue without MZ detection
        }
    }

    if (confidence < 30) {
        return EngineDetectionResult{};
    }

    std::string detailStr;
    for (const auto& detail : details) {
        if (!detailStr.empty()) detailStr += ", ";
        detailStr += detail;
    }

    EngineDetectionResult result;
    result.engine = GameEngine::RPGMaker_MV;  // Both MV and MZ use same enum for now
    result.confidence = std::min(confidence, 100);
    result.details = detailStr;
    result.metadata["isMz"] = isMz ? "true" : "false";

    return result;
}

EngineDetectionResult GameDetector::detectRpgMakerVxAce(
    const fs::path& /*path*/, const GameSignatures& sig
) const {
    int confidence = 0;
    std::vector<std::string> details;

    if (sig.hasRgss) {
        confidence += 45;
        details.push_back("RGSS DLL found");
    }

    if (sig.hasRvdata2) {
        confidence += 35;
        details.push_back(".rvdata2 files found");
    }

    if (sig.hasRgss3a) {
        confidence += 30;
        details.push_back(".rgss3a archive found");
    }

    if (sig.hasDataFolderRpg) {
        confidence += 15;
        details.push_back("Data folder found");
    }

    if (sig.hasGraphicsFolder) {
        confidence += 10;
        details.push_back("Graphics folder found");
    }

    if (sig.hasAudioFolder) {
        confidence += 5;
        details.push_back("Audio folder found");
    }

    if (confidence < 30) {
        return EngineDetectionResult{};
    }

    std::string detailStr;
    for (const auto& detail : details) {
        if (!detailStr.empty()) detailStr += ", ";
        detailStr += detail;
    }

    EngineDetectionResult result;
    result.engine = GameEngine::RPGMaker_VX;
    result.confidence = std::min(confidence, 100);
    result.details = detailStr;

    return result;
}

EngineDetectionResult GameDetector::detectGodot(
    const fs::path& path, const GameSignatures& sig
) const {
    int confidence = 0;
    std::vector<std::string> details;

    if (sig.hasPckFiles) {
        confidence += 40;
        details.push_back(".pck files found");
    }

    if (sig.hasGodotExe) {
        confidence += 45;
        details.push_back("Godot executable found");
    }

    // Check for project.godot file (strong indicator)
    try {
        auto projectFile = path / "project.godot";
        if (fs::exists(projectFile)) {
            confidence += 30;
            details.push_back("project.godot found");
        }

        // Check for export_presets.cfg
        auto presetsFile = path / "export_presets.cfg";
        if (fs::exists(presetsFile)) {
            confidence += 15;
            details.push_back("export_presets.cfg found");
        }
    }
    catch (const std::exception&) {
        // Continue with existing confidence
    }

    if (confidence < 30) {
        return EngineDetectionResult{};
    }

    std::string detailStr;
    for (const auto& detail : details) {
        if (!detailStr.empty()) detailStr += ", ";
        detailStr += detail;
    }

    EngineDetectionResult result;
    result.engine = GameEngine::Godot;
    result.confidence = std::min(confidence, 100);
    result.details = detailStr;

    return result;
}

EngineDetectionResult GameDetector::detectGameMaker(
    const fs::path& path, const GameSignatures& sig
) const {
    int confidence = 0;
    std::vector<std::string> details;

    if (sig.hasDataWin) {
        confidence += 50;
        details.push_back("data.win found");
    }

    if (sig.hasOptionsIni) {
        // Verify options.ini content for GameMaker signatures
        try {
            auto optionsPath = path / "options.ini";
            if (fs::exists(optionsPath)) {
                std::ifstream file(optionsPath);
                if (file) {
                    std::string content((std::istreambuf_iterator<char>(file)),
                                       std::istreambuf_iterator<char>());

                    // Convert to lowercase for case-insensitive search
                    std::string lowerContent = content;
                    std::transform(lowerContent.begin(), lowerContent.end(),
                        lowerContent.begin(), ::tolower);

                    if (lowerContent.find("gamemaker") != std::string::npos ||
                        content.find("YoYoGames") != std::string::npos) {
                        confidence += 40;
                        details.push_back("GameMaker options.ini confirmed");
                    } else {
                        confidence += 15;
                        details.push_back("options.ini found (unverified)");
                    }
                }
            }
        }
        catch (const std::exception&) {
            confidence += 15;
            details.push_back("options.ini found");
        }
    }

    if (confidence < 30) {
        return EngineDetectionResult{};
    }

    std::string detailStr;
    for (const auto& detail : details) {
        if (!detailStr.empty()) detailStr += ", ";
        detailStr += detail;
    }

    EngineDetectionResult result;
    result.engine = GameEngine::GameMaker;
    result.confidence = std::min(confidence, 100);
    result.details = detailStr;

    return result;
}

EngineDetectionResult GameDetector::detectSource(
    const fs::path& /*path*/, const GameSignatures& sig
) const {
    int confidence = 0;
    std::vector<std::string> details;

    if (sig.hasVpkFiles) {
        confidence += 35;
        details.push_back(".vpk files found");
    }

    if (sig.hasBspFiles) {
        confidence += 25;
        details.push_back(".bsp map files found");
    }

    if (sig.hasSourceExe) {
        confidence += 30;
        details.push_back("Source executable found");
    }

    if (sig.hasSourceGameFolder) {
        confidence += 20;
        details.push_back("Source game folder found");
    }

    if (confidence < 30) {
        return EngineDetectionResult{};
    }

    std::string detailStr;
    for (const auto& detail : details) {
        if (!detailStr.empty()) detailStr += ", ";
        detailStr += detail;
    }

    EngineDetectionResult result;
    result.engine = GameEngine::Source;
    result.confidence = std::min(confidence, 100);
    result.details = detailStr;

    return result;
}

EngineDetectionResult GameDetector::detectCryEngine(
    const fs::path& /*path*/, const GameSignatures& sig
) const {
    int confidence = 0;
    std::vector<std::string> details;

    if (sig.hasCrySystem) {
        confidence += 55;
        details.push_back("CrySystem.dll found");
    }

    if (sig.hasCryPak) {
        confidence += 35;
        details.push_back("CryEngine .pak files found");
    }

    if (confidence < 30) {
        return EngineDetectionResult{};
    }

    std::string detailStr;
    for (const auto& detail : details) {
        if (!detailStr.empty()) detailStr += ", ";
        detailStr += detail;
    }

    EngineDetectionResult result;
    result.engine = GameEngine::CryEngine;
    result.confidence = std::min(confidence, 100);
    result.details = detailStr;

    return result;
}

EngineDetectionResult GameDetector::detectFrostbite(
    const fs::path& path, const GameSignatures& sig
) const {
    int confidence = 0;
    std::vector<std::string> details;

    // Check Frostbite signature files
    if (sig.hasFrostbiteFiles) {
        confidence += 50;
        details.push_back("Frostbite files found (.sb/.toc/cascat.cat)");
    }

    // Additional check for Frostbite-specific structures
    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.is_regular_file()) {
                auto name = entry.path().filename().string();
                std::transform(name.begin(), name.end(), name.begin(), ::tolower);

                // Frostbite games have specific file patterns
                if (name.find("frosty") != std::string::npos) {
                    confidence += 25;
                    details.push_back("Frosty mod tool detected");
                }

                // Check for superbundle files
                if (name.ends_with(".sb")) {
                    confidence += 20;
                    details.push_back("Superbundle files found");
                    break;
                }

                // Check for table of contents
                if (name.ends_with(".toc")) {
                    confidence += 15;
                    details.push_back("TOC files found");
                    break;
                }
            }
        }

        // Check Data folder for cas/cat files
        auto dataDir = path / "Data";
        if (fs::exists(dataDir)) {
            for (const auto& entry : fs::directory_iterator(dataDir)) {
                auto name = entry.path().filename().string();
                std::transform(name.begin(), name.end(), name.begin(), ::tolower);
                if (name.starts_with("cas_") || name == "cascat.cat") {
                    confidence += 30;
                    details.push_back("CAS/CAT files found");
                    break;
                }
            }
        }
    }
    catch (const std::exception&) {
        // Continue with existing confidence
    }

    if (confidence < 30) {
        return EngineDetectionResult{};
    }

    std::string detailStr;
    for (const auto& detail : details) {
        if (!detailStr.empty()) detailStr += ", ";
        detailStr += detail;
    }

    EngineDetectionResult result;
    result.engine = GameEngine::Frostbite;
    result.confidence = std::min(confidence, 100);
    result.details = detailStr;

    return result;
}

EngineDetectionResult GameDetector::detectIdTech(
    const fs::path& path, const GameSignatures& sig
) const {
    int confidence = 0;
    std::vector<std::string> details;

    // Check for pk3/pk4 files (Quake/Doom style archives)
    if (sig.hasPkFiles) {
        confidence += 45;
        details.push_back(".pk3/.pk4 files found");
    }

    // Additional id Tech specific checks
    try {
        // Look for 'base' folder (common in id Tech games)
        auto baseDir = path / "base";
        if (fs::exists(baseDir) && fs::is_directory(baseDir)) {
            confidence += 25;
            details.push_back("'base' folder found");

            // Check for pak files inside base
            for (const auto& entry : fs::directory_iterator(baseDir)) {
                auto ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".pk3" || ext == ".pk4" || ext == ".pak") {
                    confidence += 15;
                    details.push_back("PAK archives in base folder");
                    break;
                }
            }
        }

        // Check for id Tech signatures in root
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.is_regular_file()) {
                auto name = entry.path().filename().string();
                std::transform(name.begin(), name.end(), name.begin(), ::tolower);

                // id Tech specific executables
                if (name.find("quake") != std::string::npos ||
                    name.find("doom") != std::string::npos ||
                    name.find("wolfenstein") != std::string::npos ||
                    name.find("rage") != std::string::npos) {
                    confidence += 20;
                    details.push_back("id Tech game executable found");
                    break;
                }

                // Check for .qvm files (Quake VM)
                if (name.ends_with(".qvm")) {
                    confidence += 25;
                    details.push_back("QVM files found");
                    break;
                }
            }
        }
    }
    catch (const std::exception&) {
        // Continue with existing confidence
    }

    if (confidence < 30) {
        return EngineDetectionResult{};
    }

    std::string detailStr;
    for (const auto& detail : details) {
        if (!detailStr.empty()) detailStr += ", ";
        detailStr += detail;
    }

    EngineDetectionResult result;
    result.engine = GameEngine::IdTech;
    result.confidence = std::min(confidence, 100);
    result.details = detailStr;

    return result;
}

// =============================================================================
// VERSION DETECTION HELPERS
// =============================================================================

std::string GameDetector::readUnityVersion(const fs::path& gameDir) const {
    try {
        // Find *_Data folder
        for (const auto& entry : fs::directory_iterator(gameDir)) {
            if (entry.is_directory()) {
                auto name = entry.path().filename().string();
                std::string lowerName = name;
                std::transform(lowerName.begin(), lowerName.end(),
                               lowerName.begin(), ::tolower);

                if (lowerName.ends_with("_data")) {
                    auto ggmPath = entry.path() / "globalgamemanagers";
                    if (fs::exists(ggmPath)) {
                        // Use memory-mapped I/O for efficient reading
                        // Only read first 256 bytes where version info is located
                        constexpr size_t READ_SIZE = 256;

                        auto mappedResult = mio_utils::mapFile(ggmPath, 0, READ_SIZE);
                        if (!mappedResult) {
                            MAKINEAI_LOG_TRACE(log::DETECTOR,
                                "Failed to map globalgamemanagers: {}",
                                mappedResult.error().message());
                            break;
                        }

                        auto view = mappedResult->view();
                        std::string content(view.stringView());

                        // Search for version pattern (e.g., "2021.3.14f1", "2022.1.0b3")
                        // Manual scan — avoids <regex> header overhead
                        for (size_t i = 0; i < content.size(); ++i) {
                            char c = content[i];
                            if (c < '0' || c > '9') continue;

                            // Try to parse digits.digits.digits[letter][digits]
                            size_t start = i;
                            // First number
                            while (i < content.size() && content[i] >= '0' && content[i] <= '9') ++i;
                            if (i >= content.size() || content[i] != '.') continue;
                            ++i; // skip '.'
                            // Second number
                            if (i >= content.size() || content[i] < '0' || content[i] > '9') continue;
                            while (i < content.size() && content[i] >= '0' && content[i] <= '9') ++i;
                            if (i >= content.size() || content[i] != '.') continue;
                            ++i; // skip '.'
                            // Third number
                            if (i >= content.size() || content[i] < '0' || content[i] > '9') continue;
                            while (i < content.size() && content[i] >= '0' && content[i] <= '9') ++i;
                            // Optional letter suffix (f1, b3, etc.)
                            if (i < content.size() && content[i] >= 'a' && content[i] <= 'z') {
                                ++i;
                                while (i < content.size() && content[i] >= '0' && content[i] <= '9') ++i;
                            }

                            std::string ver = content.substr(start, i - start);
                            // Sanity: Unity versions start with year >= 3
                            if (ver.size() >= 5) {
                                MAKINEAI_LOG_TRACE(log::DETECTOR,
                                    "Unity version detected: {}", ver);
                                return ver;
                            }
                        }
                    }
                    break;
                }
            }
        }
    }
    catch (const std::exception& e) {
        MAKINEAI_LOG_TRACE(log::DETECTOR, "Failed to read Unity version: {}", e.what());
    }
    return "";
}

std::string GameDetector::readRenpyVersion(const fs::path& gameDir) const {
    try {
        auto initPy = gameDir / "renpy" / "__init__.py";
        if (fs::exists(initPy)) {
            std::ifstream file(initPy);
            if (file) {
                std::string content((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());

                // Search for version_tuple = (major, minor, patch)
                // Manual parse — avoids <regex> header overhead
                auto tuplePos = content.find("version_tuple");
                if (tuplePos != std::string::npos) {
                    auto parenPos = content.find('(', tuplePos);
                    if (parenPos != std::string::npos) {
                        // Extract three comma-separated numbers
                        auto extractNum = [&](size_t& pos) -> std::string {
                            while (pos < content.size() && (content[pos] < '0' || content[pos] > '9')) ++pos;
                            size_t numStart = pos;
                            while (pos < content.size() && content[pos] >= '0' && content[pos] <= '9') ++pos;
                            return content.substr(numStart, pos - numStart);
                        };

                        size_t pos = parenPos + 1;
                        std::string major = extractNum(pos);
                        std::string minor = extractNum(pos);
                        std::string patch = extractNum(pos);

                        if (!major.empty() && !minor.empty() && !patch.empty()) {
                            return major + "." + minor + "." + patch;
                        }
                    }
                }
            }
        }
    }
    catch (const std::exception&) {
        // Version could not be read
    }
    return "";
}

} // namespace makineai::scanners
