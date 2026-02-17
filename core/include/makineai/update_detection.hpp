/**
 * @file update_detection.hpp
 * @brief Game update detection — pure C++ core module
 * @copyright (c) 2026 MakineAI Team
 *
 * Two-tier game update detection:
 *   Tier 1 (fast):     Store metadata checks (Steam ACF, Epic manifest, GOG registry)
 *   Tier 2 (detailed): File hash comparison with mtime pre-filtering
 *
 * All functions are pure, thread-safe, and have NO Qt dependency.
 * Extracted from qml/src/services/updatedetectionservice.h/cpp.
 */

#pragma once

#include "makineai/error.hpp"
#include "makineai/types/common.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace makineai::update {

// =============================================================================
// DATA STRUCTURES
// =============================================================================

/// Store-level version record (Tier 1)
struct StoreVersionRecord {
    std::string gameId;
    std::string steamBuildId;       ///< ACF "buildid"
    std::string epicVersionString;  ///< Epic manifest "AppVersionString"
    std::string gogBuildId;         ///< GOG registry "ver"
    int64_t exeLastModified{0};     ///< Fallback: exe mtime (seconds since epoch)
    int64_t recordedAt{0};          ///< When this record was taken
};

/// File hash record (Tier 2 snapshot)
struct FileHashRecord {
    std::string relativePath;
    std::string sha256;
    int64_t fileSize{0};
    int64_t lastModified{0};        ///< mtime for pre-filtering
};

/// Game file snapshot — captures file state at translation patch time
struct GameSnapshot {
    std::string gameId;
    std::string patchVersion;
    int64_t takenAt{0};
    std::vector<FileHashRecord> files;
};

/// Engine profile — directory/extension rules for file tracking
struct EngineProfile {
    struct Rule {
        std::string directory;      ///< Subdirectory ("Content/Paks", "" for root)
        std::string nameFilter;     ///< Glob-like filter ("*.pak", "*.dll", "globalgamemanagers")
        bool recurse{false};        ///< Recurse into subdirectories
    };
    std::vector<Rule> rules;
    std::vector<std::string> ignoredDirs;  ///< Directories to skip (case-insensitive)
    int maxFiles{100};                      ///< Maximum files to track
};

/// Compatibility check result
struct CompatibilityResult {
    std::string level;              ///< "compatible", "partial", "incompatible", "unknown"
    int integrityPercent{100};
    int modifiedCount{0};
    int addedCount{0};
    int removedCount{0};
    std::string summary;
};

// =============================================================================
// ENGINE PROFILES
// =============================================================================

/**
 * @brief Get file tracking profile for a game engine
 *
 * Returns rules defining which files to track for update detection.
 * Engine name matching is case-insensitive and substring-based.
 *
 * @param engine Engine name (e.g. "Unity", "Unreal Engine", "RenPy")
 * @return EngineProfile with rules, ignored directories, and file limit
 */
[[nodiscard]] EngineProfile profileForEngine(std::string_view engine);

// =============================================================================
// FILE COLLECTION & HASHING
// =============================================================================

/**
 * @brief Collect tracked files according to engine profile rules
 *
 * Handles Unity *_Data directories specially: searches for globalgamemanagers,
 * *.assets, and Managed/*.dll inside any directory ending with "_Data".
 *
 * @param installPath Game installation root directory
 * @param profile Engine profile with rules
 * @return Sorted list of relative file paths (limited to profile.maxFiles)
 */
[[nodiscard]] std::vector<std::string> collectFiles(
    const fs::path& installPath,
    const EngineProfile& profile
);

/**
 * @brief Compute SHA-256 hash of a file
 *
 * Uses OpenSSL EVP API with 64KB chunked reading.
 *
 * @param filePath Absolute path to file
 * @return Lowercase hex-encoded SHA-256 hash, or empty string on failure
 */
[[nodiscard]] std::string computeFileHash(const fs::path& filePath);

/**
 * @brief Hash all tracked game files
 *
 * Collects files using the engine profile, then computes SHA-256 for each.
 *
 * @param installPath Game installation root directory
 * @param profile Engine profile with rules
 * @return Vector of FileHashRecord with path, hash, size, mtime
 */
[[nodiscard]] std::vector<FileHashRecord> hashGameFiles(
    const fs::path& installPath,
    const EngineProfile& profile
);

// =============================================================================
// STORE METADATA READERS (Tier 1)
// =============================================================================

/**
 * @brief Read Steam build ID from appmanifest ACF file
 *
 * Navigates from installPath up 2 directories to steamapps/,
 * then parses appmanifest_{appId}.acf using the VDF parser.
 * Falls back to scanning all ACF files if direct lookup fails.
 *
 * @param installPath Game installation directory (inside steamapps/common/)
 * @param steamAppId Steam application ID (empty for fallback scan)
 * @return Build ID string, or empty on failure
 */
[[nodiscard]] std::string readSteamBuildId(
    const fs::path& installPath,
    const std::string& steamAppId
);

/**
 * @brief Read Epic Games version string from launcher manifests
 *
 * Scans C:/ProgramData/Epic/EpicGamesLauncher/Data/Manifests/*.item
 * for a manifest matching the install location.
 *
 * @param installPath Game installation directory
 * @return AppVersionString, or empty on failure
 */
[[nodiscard]] std::string readEpicVersion(const fs::path& installPath);

/**
 * @brief Read GOG Galaxy version from Windows registry
 *
 * Reads HKLM\SOFTWARE\WOW6432Node\GOG.com\Games\{gogId}\ver (or buildId).
 * Only works when gameId starts with "gog_".
 *
 * @param gameId Game identifier (must start with "gog_")
 * @return Version string, or empty on failure
 */
[[nodiscard]] std::string readGogVersion(const std::string& gameId);

/**
 * @brief Read latest executable modification time
 *
 * Scans for *.exe files in the install directory and returns
 * the most recent mtime as seconds since epoch.
 *
 * @param installPath Game installation directory
 * @return Latest exe mtime (seconds since epoch), or 0 if none found
 */
[[nodiscard]] int64_t readExeMtime(const fs::path& installPath);

// =============================================================================
// SNAPSHOT MANAGEMENT
// =============================================================================

/**
 * @brief Save a game snapshot to disk
 *
 * Writes JSON to dataDir/snapshots/{gameId}.json
 *
 * @param dataDir Root data directory for update detection
 * @param snapshot Snapshot to save
 */
void saveSnapshot(const fs::path& dataDir, const GameSnapshot& snapshot);

/**
 * @brief Load a game snapshot from disk
 *
 * Reads JSON from dataDir/snapshots/{gameId}.json
 *
 * @param dataDir Root data directory for update detection
 * @param gameId Game identifier
 * @return Loaded snapshot, or nullopt if not found or parse error
 */
[[nodiscard]] std::optional<GameSnapshot> loadSnapshot(
    const fs::path& dataDir,
    const std::string& gameId
);

/**
 * @brief Check if a snapshot exists for a game
 *
 * @param dataDir Root data directory for update detection
 * @param gameId Game identifier
 * @return true if snapshot file exists
 */
[[nodiscard]] bool hasSnapshot(const fs::path& dataDir, const std::string& gameId);

/**
 * @brief Remove a game snapshot
 *
 * @param dataDir Root data directory for update detection
 * @param gameId Game identifier
 */
void removeSnapshot(const fs::path& dataDir, const std::string& gameId);

// =============================================================================
// COMPATIBILITY CHECKING (Tier 2)
// =============================================================================

/**
 * @brief Check compatibility of current game files against a snapshot
 *
 * Compares mtime+size first (fast), then SHA-256 hash for changed files.
 * Also detects newly added files using the engine profile.
 *
 * @param snapshot Previously taken snapshot
 * @param installPath Current game installation directory
 * @param engine Engine name for profile lookup
 * @return CompatibilityResult with level, counts, and summary
 */
[[nodiscard]] CompatibilityResult checkCompatibility(
    const GameSnapshot& snapshot,
    const fs::path& installPath,
    std::string_view engine
);

// =============================================================================
// STORE VERSION PERSISTENCE
// =============================================================================

/**
 * @brief Save store version records to disk
 *
 * Writes JSON to dataDir/store_versions.json
 *
 * @param dataDir Root data directory for update detection
 * @param versions Map of gameId -> StoreVersionRecord
 */
void saveStoreVersions(
    const fs::path& dataDir,
    const std::unordered_map<std::string, StoreVersionRecord>& versions
);

/**
 * @brief Load store version records from disk
 *
 * Reads JSON from dataDir/store_versions.json
 *
 * @param dataDir Root data directory for update detection
 * @return Map of gameId -> StoreVersionRecord
 */
[[nodiscard]] std::unordered_map<std::string, StoreVersionRecord> loadStoreVersions(
    const fs::path& dataDir
);

} // namespace makineai::update
