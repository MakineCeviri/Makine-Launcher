/**
 * @file types/game_types.hpp
 * @brief Game-related type definitions
 * @copyright (c) 2026 MakineCeviri Team
 *
 * This file contains types for game detection, identification, and metadata.
 */

#pragma once

#include "makine/types/common.hpp"

#include <optional>

namespace makine {

// ============================================================================
// Game Engine Types
// ============================================================================

/**
 * @brief Game engine types supported by Makine
 *
 * Each engine type determines which handler will be used for
 * string extraction and translation application.
 */
enum class GameEngine {
    Unknown,
    Unity,           ///< Unity (generic, backend unknown)
    Unity_Mono,      ///< Unity with Mono scripting backend
    Unity_IL2CPP,    ///< Unity with IL2CPP scripting backend
    Unreal,          ///< Unreal Engine 4/5
    Bethesda,        ///< Creation Engine (Starfield, TES, Fallout)
    GameMaker,       ///< GameMaker Studio 1/2
    RenPy,           ///< Ren'Py visual novel engine
    RPGMaker,        ///< RPG Maker (generic)
    RPGMaker_MV,     ///< RPG Maker MV/MZ (JavaScript)
    RPGMaker_VX,     ///< RPG Maker VX Ace (Ruby)
    Godot,           ///< Godot Engine
    Source,          ///< Valve Source Engine
    CryEngine,       ///< CryEngine
    Frostbite,       ///< Frostbite Engine (EA)
    IdTech,          ///< id Tech Engine
    Custom           ///< Custom/unknown engine with recipe
};

/**
 * @brief Convert game engine enum to string
 * @param engine The engine type
 * @return Human-readable engine name
 */
[[nodiscard]] constexpr std::string_view engineToString(GameEngine engine) noexcept {
    switch (engine) {
        case GameEngine::Unity:         return "Unity";
        case GameEngine::Unity_Mono:    return "Unity (Mono)";
        case GameEngine::Unity_IL2CPP:  return "Unity (IL2CPP)";
        case GameEngine::RPGMaker:      return "RPG Maker";
        case GameEngine::Unreal:        return "Unreal Engine";
        case GameEngine::Bethesda:      return "Bethesda Creation Engine";
        case GameEngine::GameMaker:     return "GameMaker";
        case GameEngine::RenPy:         return "Ren'Py";
        case GameEngine::RPGMaker_MV:   return "RPG Maker MV/MZ";
        case GameEngine::RPGMaker_VX:   return "RPG Maker VX Ace";
        case GameEngine::Godot:         return "Godot";
        case GameEngine::Source:        return "Source Engine";
        case GameEngine::CryEngine:     return "CryEngine";
        case GameEngine::Frostbite:     return "Frostbite";
        case GameEngine::IdTech:        return "id Tech";
        case GameEngine::Custom:        return "Custom";
        default:                        return "Unknown";
    }
}

// ============================================================================
// Game Store Types
// ============================================================================

/**
 * @brief Game store/launcher type
 *
 * Identifies where the game was installed from.
 */
enum class GameStore {
    Unknown,
    Steam,
    EpicGames,
    GOG,
    Xbox,           ///< Microsoft Store / Xbox Game Pass
    Manual          ///< Manually added by user
};

// ============================================================================
// Game Identification
// ============================================================================

/**
 * @brief Unique identifier for a game
 *
 * Combines store-specific ID with an executable hash for
 * precise version identification.
 */
struct GameId {
    std::string storeId;      ///< e.g., Steam AppID "1716740"
    GameStore store = GameStore::Unknown;
    std::string exeHash;      ///< SHA256 of main executable

    bool operator==(const GameId&) const = default;
    auto operator<=>(const GameId&) const = default;

    /**
     * @brief Convert to string representation
     * @return String in format "store:storeId"
     */
    [[nodiscard]] std::string toString() const {
        std::string storeName;
        switch (store) {
            case GameStore::Steam: storeName = "steam"; break;
            case GameStore::EpicGames: storeName = "epic"; break;
            case GameStore::GOG: storeName = "gog"; break;
            case GameStore::Xbox: storeName = "xbox"; break;
            case GameStore::Manual: storeName = "manual"; break;
            default: storeName = "unknown"; break;
        }
        return storeName + ":" + storeId;
    }
};

/**
 * @brief Information about a detected game
 *
 * Contains all metadata about an installed game.
 */
struct GameInfo {
    GameId id;
    std::string name;             ///< Display name
    fs::path installPath;         ///< Game installation directory
    fs::path executablePath;      ///< Main executable path
    fs::path dataPath;            ///< Game data directory
    GameEngine engine = GameEngine::Unknown;
    std::string version;          ///< Game version string
    std::string engineVersion;    ///< Detected engine version
    uint64_t sizeBytes = 0;       ///< Total size
    bool is64Bit = true;          ///< x64 or x86
    double engineConfidence = 0.0; ///< Engine detection confidence (0.0-1.0)

    // Detection metadata
    std::string detectedAt;       ///< ISO timestamp when detected
    GameStore store = GameStore::Unknown;  ///< Store where game was found

    // Translation status
    bool hasTranslation = false;  ///< Has any translation installed
    StringList translatedLanguages;  ///< Languages with translations

    // File-based identification signals (populated by detectGame)
    StringList executableNames;    // All .exe files found (lowercase, filtered)
    StringList topLevelEntries;    // Top-level files/dirs in game root

    // Optional metadata
    std::optional<std::string> iconPath;
    std::optional<std::string> publisher;
    std::optional<std::string> developer;
};

// ============================================================================
// Translation Package
// ============================================================================

/**
 * @brief Translation package metadata
 *
 * Contains information about a downloadable translation package.
 */
struct TranslationPackage {
    std::string packageId;        ///< Unique package ID (e.g., "SF10310Hv19")
    std::string gameId;           ///< Target game ID
    std::string gameName;         ///< Game display name
    std::string version;          ///< Package version
    StringList supportedGameVersions;  ///< Compatible game versions
    StringList supportedGameHashes;    ///< Compatible exe hashes

    std::string downloadUrl;      ///< Package download URL
    std::string signature;        ///< RSA signature
    uint64_t sizeBytes = 0;       ///< Download size
    std::string sha256;           ///< Package checksum

    GameEngine targetEngine = GameEngine::Unknown;
    bool requiresRuntime = false; ///< Needs a runtime translation layer?
};

} // namespace makine
