/**
 * @file game_detector.hpp
 * @brief Game detection from various stores and launchers
 * @copyright (c) 2026 MakineCeviri Team
 *
 * This module provides game detection capabilities:
 * - Store-specific scanners (Steam, Epic, GOG)
 * - Engine detection with confidence scoring
 * - Game verification via hash
 *
 * Namespace: makine::scanners
 * (Backward compatible: also available in makine::)
 */

#pragma once

#include "makine/types.hpp"
#include "makine/error.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace makine {
namespace scanners {

// ============================================================================
// ENGINE DETECTION TYPES
// ============================================================================

/**
 * @brief Engine detection result with confidence scoring
 *
 * Confidence scoring system:
 * - 90%+ : Very confident, can proceed without verification
 * - 70-89%: Confident, recommended to verify
 * - 50-69%: Likely correct, should verify
 * - <50%  : Uncertain, may need manual verification
 */
struct EngineDetectionResult {
    GameEngine engine = GameEngine::Unknown;
    int confidence = 0;                    // 0-100%
    std::string version;                   // Engine version if detected
    std::string details;                   // Human-readable detection details
    std::unordered_map<std::string, std::string> metadata;  // Additional info

    [[nodiscard]] bool isConfident() const noexcept { return confidence >= 70; }
    [[nodiscard]] bool isLikelyCorrect() const noexcept { return confidence >= 50; }
    [[nodiscard]] bool isUncertain() const noexcept { return confidence < 50; }
};

/**
 * @brief Game file/folder signature flags for engine detection
 *
 * Each flag represents presence of a signature file or folder.
 * Used for confidence-based engine detection.
 */
struct GameSignatures {
    // Unity signatures
    bool hasUnityEngine = false;         // UnityEngine.dll
    bool hasAssemblyCSharp = false;      // Assembly-CSharp.dll
    bool hasGameAssembly = false;        // GameAssembly.dll (IL2CPP)
    bool hasGlobalGameManagers = false;  // globalgamemanagers
    bool hasResourcesAssets = false;     // resources.assets
    bool hasUnity3d = false;             // *.unity3d files
    bool hasManagedFolder = false;       // Managed/
    bool hasMonoFolder = false;          // Mono/
    bool hasIl2cppData = false;          // il2cpp_data/
    bool hasDataFolder = false;          // *_Data/

    // Unreal signatures
    bool hasPakFiles = false;            // *.pak files
    bool hasUasset = false;              // *.uasset files
    bool hasUmap = false;                // *.umap files
    bool hasUeExecutable = false;        // UE4Game.exe/UE5Game.exe
    bool hasEngineFolder = false;        // Engine/
    bool hasContentFolder = false;       // Content/

    // Ren'Py signatures
    bool hasRpaFiles = false;            // *.rpa archives
    bool hasRpycFiles = false;           // *.rpyc files
    bool hasRenpyExe = false;            // renpy.exe
    bool hasRenpyFolder = false;         // renpy/
    bool hasGameFolder = false;          // game/

    // RPG Maker MV/MZ signatures
    bool hasRpgCore = false;             // rpg_core.js
    bool hasNwJs = false;                // nw.exe/Game.exe
    bool hasPackageJson = false;         // package.json
    bool hasWwwFolder = false;           // www/
    bool hasJsFolder = false;            // js/
    bool hasImgFolder = false;           // img/

    // RPG Maker VX Ace signatures
    bool hasRgss = false;                // RGSS*.dll
    bool hasRvdata2 = false;             // *.rvdata2
    bool hasRgss3a = false;              // *.rgss3a
    bool hasDataFolderRpg = false;       // Data/
    bool hasGraphicsFolder = false;      // Graphics/
    bool hasAudioFolder = false;         // Audio/

    // Godot signatures
    bool hasPckFiles = false;            // *.pck files
    bool hasGodotExe = false;            // godot.exe

    // GameMaker signatures
    bool hasDataWin = false;             // data.win
    bool hasOptionsIni = false;          // options.ini

    // Source Engine signatures
    bool hasVpkFiles = false;            // *.vpk files
    bool hasBspFiles = false;            // *.bsp files
    bool hasSourceExe = false;           // hl2.exe/source.exe
    bool hasSourceGameFolder = false;    // hl2/csgo/tf

    // CryEngine signatures
    bool hasCrySystem = false;           // CrySystem.dll
    bool hasCryPak = false;              // gamedata/*.pak

    // Frostbite signatures
    bool hasFrostbiteFiles = false;      // *.sb, *.toc, cascat.cat

    // id Tech signatures
    bool hasPkFiles = false;             // *.pk3/*.pk4

    // Bethesda signatures
    bool hasBa2Files = false;            // *.ba2 files
    bool hasEsmEsp = false;              // *.esm/*.esp files
    bool hasStringsFiles = false;        // *.strings files

    // Extensions found (limited to relevant ones)
    std::unordered_set<std::string> extensions;

    // Performance: limit file count
    static constexpr size_t MAX_FILES_TO_SCAN = 2000;
};

/**
 * @brief Hash verification result
 */
struct VerificationResult {
    bool verified = false;
    std::string expectedHash;
    std::string actualHash;
    std::string version;
    bool isKnownVersion = false;      // Version exists in our database
    bool hasTranslation = false;      // Translation available for this version
};

// ============================================================================
// SCANNER INTERFACE
// ============================================================================

/**
 * @brief Interface for store-specific game scanners
 *
 * Each scanner (Steam, Epic, GOG) implements this interface to
 * discover games from their respective stores.
 */
class IGameScanner {
public:
    virtual ~IGameScanner() = default;

    /// @brief Get scanner name
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    /// @brief Get game store type
    [[nodiscard]] virtual GameStore storeType() const noexcept = 0;

    /// @brief Check if this scanner is available on the system
    [[nodiscard]] virtual bool isAvailable() const = 0;

    /// @brief Scan for installed games
    [[nodiscard]] virtual Result<std::vector<GameInfo>> scan() const = 0;

    /// @brief Get game info by store ID
    [[nodiscard]] virtual Result<GameInfo> getGame(const std::string& storeId) const = 0;
};

// ============================================================================
// GAME DETECTOR
// ============================================================================

/**
 * @brief Main game detector class
 *
 * Orchestrates game detection across multiple stores and
 * provides engine detection with confidence scoring.
 */
class GameDetector {
public:
    GameDetector();
    ~GameDetector();

    /// @brief Register a game scanner
    void registerScanner(std::unique_ptr<IGameScanner> scanner);

    /// @brief Scan all registered stores for games
    [[nodiscard]] Result<std::vector<GameInfo>> scanAll(
        ProgressCallback progress = nullptr
    ) const;

    /// @brief Scan a specific store
    [[nodiscard]] Result<std::vector<GameInfo>> scanStore(GameStore store) const;

    /// @brief Detect game from a directory path
    [[nodiscard]] Result<GameInfo> detectGame(const fs::path& gamePath) const;

    /// @brief Verify game executable hash
    [[nodiscard]] Result<VerificationResult> verify(const GameInfo& game) const;

    /// @brief Detect game engine from directory (simple version)
    [[nodiscard]] GameEngine detectEngine(const fs::path& gameDir) const;

    /**
     * @brief Detect game engine with confidence scoring
     *
     * This is the sophisticated version that returns confidence levels
     * and detailed information about the detection.
     *
     * @param gameDir Path to game directory
     * @return EngineDetectionResult with engine type and confidence
     */
    [[nodiscard]] EngineDetectionResult detectEngineWithConfidence(
        const fs::path& gameDir
    ) const;

    /**
     * @brief Scan directory for engine signatures
     *
     * Populates a GameSignatures struct by scanning the directory
     * for known file and folder patterns.
     *
     * @param gameDir Path to game directory
     * @return Populated GameSignatures struct
     */
    [[nodiscard]] GameSignatures scanForSignatures(const fs::path& gameDir) const;

    /// @brief Check if game is 64-bit
    [[nodiscard]] Result<bool> is64Bit(const fs::path& exePath) const;

    /// @brief Calculate SHA256 hash of file
    [[nodiscard]] Result<std::string> calculateHash(const fs::path& file) const;

    /// @brief Get list of available scanners
    [[nodiscard]] const std::vector<std::unique_ptr<IGameScanner>>& scanners() const {
        return scanners_;
    }

    /// @brief Filter games that have translations available
    [[nodiscard]] std::vector<GameInfo> filterWithTranslations(
        const std::vector<GameInfo>& games
    ) const;

private:
    std::vector<std::unique_ptr<IGameScanner>> scanners_;

    void registerBuiltinScanners();
    GameEngine detectEngineFromFiles(const fs::path& gameDir) const;

    // Signature scanning helpers
    void checkSignatureFile(GameSignatures& sig, const std::string& fileName,
                           const std::string& relativePath) const;
    void checkSignatureFolder(GameSignatures& sig, const std::string& folderName,
                             const std::string& relativePath) const;
    bool isRelevantExtension(const std::string& ext) const;


};

// ============================================================================
// BUILT-IN SCANNERS
// ============================================================================

/**
 * @brief Steam game scanner
 *
 * Scans Steam library folders for installed games by parsing
 * appmanifest_*.acf files.
 */
class SteamScanner : public IGameScanner {
public:
    [[nodiscard]] std::string_view name() const noexcept override { return "Steam"; }
    [[nodiscard]] GameStore storeType() const noexcept override { return GameStore::Steam; }
    [[nodiscard]] bool isAvailable() const override;
    [[nodiscard]] Result<std::vector<GameInfo>> scan() const override;
    [[nodiscard]] Result<GameInfo> getGame(const std::string& appId) const override;

private:
    [[nodiscard]] Result<StringList> findLibraryFolders() const;
    [[nodiscard]] Result<GameInfo> parseAppManifest(const fs::path& acfFile) const;
};

/**
 * @brief Epic Games scanner
 *
 * Scans Epic Games Launcher manifests for installed games.
 */
class EpicScanner : public IGameScanner {
public:
    [[nodiscard]] std::string_view name() const noexcept override { return "Epic Games"; }
    [[nodiscard]] GameStore storeType() const noexcept override { return GameStore::EpicGames; }
    [[nodiscard]] bool isAvailable() const override;
    [[nodiscard]] Result<std::vector<GameInfo>> scan() const override;
    [[nodiscard]] Result<GameInfo> getGame(const std::string& catalogId) const override;

private:
    [[nodiscard]] Result<fs::path> findManifestDirectory() const;
};

/**
 * @brief GOG Galaxy scanner
 *
 * Scans GOG Galaxy database for installed games.
 */
class GOGScanner : public IGameScanner {
public:
    [[nodiscard]] std::string_view name() const noexcept override { return "GOG Galaxy"; }
    [[nodiscard]] GameStore storeType() const noexcept override { return GameStore::GOG; }
    [[nodiscard]] bool isAvailable() const override;
    [[nodiscard]] Result<std::vector<GameInfo>> scan() const override;
    [[nodiscard]] Result<GameInfo> getGame(const std::string& gameId) const override;

private:
    [[nodiscard]] Result<fs::path> findDatabasePath() const;
};

// Forward declaration for future implementation
class XboxScanner;

} // namespace scanners

// ============================================================================
// BACKWARD COMPATIBILITY ALIASES
// ============================================================================
// These aliases allow existing code to use makine::GameDetector etc.
// without modification. New code should prefer makine::scanners::*.

using EngineDetectionResult = scanners::EngineDetectionResult;
using GameSignatures = scanners::GameSignatures;
using VerificationResult = scanners::VerificationResult;
using IGameScanner = scanners::IGameScanner;
using GameDetector = scanners::GameDetector;
using SteamScanner = scanners::SteamScanner;
using EpicScanner = scanners::EpicScanner;
using GOGScanner = scanners::GOGScanner;

} // namespace makine
