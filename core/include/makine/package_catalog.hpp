/**
 * @file package_catalog.hpp
 * @brief Local translation package catalog — pure C++ business logic
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Extracts package catalog management from the QML LocalPackageManager
 * into a Qt-free core module. Handles manifest parsing, package discovery,
 * store ID resolution, variant support, and installed state tracking.
 */

#pragma once

#include "makine/error.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace makine {

namespace fs = std::filesystem;

namespace packages {

// =============================================================================
// DATA STRUCTURES
// =============================================================================

/**
 * @brief A single install step in a custom install recipe
 *
 * Describes one action to perform during package installation,
 * such as copying a file, running an executable, or installing a font.
 */
struct InstallStep {
    std::string action;     // "copy", "copyDir", "run", "delete", "installFont", "setSteamLanguage", "copyToDesktop", "rename"
    std::string src;        // source file/dir (relative to package dir)
    std::string dest;       // destination (relative to game dir)
    std::string exe;        // executable to run
    std::vector<std::string> args;
    std::string fallback;   // fallback executable
    std::string workDir;    // "game" (default) or "package"
    std::string language;   // Steam language name — for "setSteamLanguage"
};

/**
 * @brief An install option that can be independently selected (checkbox)
 *
 * Used for packages with multiple components (e.g. patch + dubbing)
 * that can be installed in any combination.
 */
struct InstallOption {
    std::string id;              // "patch", "dubbing"
    std::string label;           // "Türkçe Yama"
    std::string description;     // "Metin çevirisi, fontlar"
    std::string icon;            // "text", "voice"
    bool defaultSelected{false};
    std::string subDir;          // Subdirectory in package dir
    std::vector<InstallStep> steps;
};

/**
 * @brief Contributor information for a translation package
 */
struct ContributorInfo {
    std::string name;
    std::string role;
};

/**
 * @brief Variant-specific install config (options + combined steps)
 *
 * Used when a package has variants (e.g. trilogy sub-games) and
 * only some variants need the options dialog.
 */
struct VariantConfig {
    std::vector<InstallOption> installOptions;
    std::unordered_map<std::string, std::vector<InstallStep>> combinedSteps;
};

/**
 * @brief File-based fingerprint for identifying a game from its files
 *
 * Used when a game is manually added (drag-drop, CD, copied folder).
 * Multi-signal matching: exe names + key files + engine hint → confidence score.
 */
struct GameFingerprint {
    std::vector<std::string> exeNames;   // Expected executable names (lowercase)
    std::vector<std::string> keyFiles;   // Paths/files unique to this game
    std::string engineHint;              // Normalized engine type for cross-check
};

/**
 * @brief Result of fingerprint-based game matching
 */
struct FingerprintMatch {
    std::string steamAppId;
    int confidence{0};       // 0-100
    std::string matchedBy;   // Debug info: "exeName", "keyFiles+engine", etc.
};

/**
 * @brief Full metadata for a translation package in the catalog
 *
 * Represents one entry in the manifest, enriched with filesystem
 * information (file count, total size) from scanning.
 */
struct PackageCatalogEntry {
    std::string packageId;
    std::string steamAppId;
    std::string gameName;
    std::string engine;
    std::string version;
    std::string installType;  // "overlay", "runtime", "replace"
    std::string tier;          // "free" or "plus"
    std::string lastUpdated;   // ISO date (e.g. "2026-02-21")
    int64_t sizeBytes{0};
    int fileCount{0};
    std::unordered_map<std::string, std::string> storeIds;  // store -> id
    std::string dirName;
    std::vector<std::string> aliases;   // Alternative names for matching
    std::vector<std::string> variants;
    std::string variantType;  // "version" or "platform"
    std::vector<ContributorInfo> contributors;
    std::vector<InstallStep> installSteps;
    std::string installMethodType;    // "script", "userPath", "options"
    std::string installMethodTarget;  // for "userPath"
    std::string installNotes;
    std::vector<InstallOption> installOptions;  // for "options" type
    std::unordered_map<std::string, std::vector<InstallStep>> combinedSteps; // e.g. "dubbing+patch" -> steps
    std::string specialDialog;  // "" or "eldenRing" — triggers themed install dialog
    std::unordered_map<std::string, VariantConfig> variantInstallOptions; // variant -> options config
    std::optional<GameFingerprint> fingerprint;  // File-based identification data
    bool detailLoaded{false};  // true after enrichPackage merges per-game detail
};

/**
 * @brief Tracks the state of an installed translation package
 *
 * Persisted to JSON so we can restore/uninstall later.
 */
struct InstalledPackageState {
    std::string version;
    std::string gamePath;
    std::vector<std::string> installedFiles;     // All files (compatibility)
    std::vector<std::string> addedFiles;          // New files (didn't exist in game)
    std::vector<std::string> replacedFiles;       // Overwritten files (existed, backed up)
    std::string gameStoreVersion;                 // Store buildid at install time
    std::string gameSource;                       // "steam", "epic", "gog"
    int64_t installedAt{0};
};

// =============================================================================
// PACKAGE CATALOG CLASS
// =============================================================================

/**
 * @brief Pure C++ catalog of local translation packages
 *
 * Reads manifest.json, scans pak/ and game-name directories,
 * tracks installed state, and provides query/resolution APIs.
 * No Qt dependency — uses std::filesystem, nlohmann::json, and spdlog.
 *
 * Usage:
 * @code
 *   PackageCatalog catalog;
 *   catalog.loadFromIndex("index.json", "C:/cache/packages");
 *   catalog.enrichPackage("1245620", detailJsonStr);
 *   catalog.loadInstalledState("C:/Users/.../installed_packages.json");
 *
 *   if (catalog.hasPackage("1245620")) {
 *       auto pkg = catalog.getPackage("1245620");
 *       auto files = catalog.getPackageFileList("1245620");
 *   }
 *
 *   // Resolve Epic/GOG IDs to Steam AppID
 *   auto appId = catalog.resolveGameId("epic_abc123");
 * @endcode
 */
class PackageCatalog {
public:
    explicit PackageCatalog();

    /**
     * @brief Load catalog from lightweight index.json (network-based)
     *
     * Parses the index at @p indexPath (contains gameName, version, size, dirName)
     * and sets @p packageCacheRoot as the data path. Detail fields (installMethod,
     * contributors, etc.) are loaded later via enrichPackage().
     *
     * @param indexPath Path to the cached index.json
     * @param packageCacheRoot Path where downloaded packages are extracted
     * @return true if at least one package was loaded
     */
    [[nodiscard]] bool loadFromIndex(const fs::path& indexPath, const fs::path& packageCacheRoot);

    /**
     * @brief Merge per-game detail JSON into an existing catalog entry
     *
     * Called when packages/{appId}.json is fetched on-demand.
     * Populates installMethod, contributors, variants, storeIds, fingerprint, etc.
     *
     * @param steamAppId The app ID to enrich
     * @param detailJson Raw JSON string of the per-game detail file
     * @return true if entry was found and enriched
     */
    [[nodiscard]] bool enrichPackage(const std::string& steamAppId, const std::string& detailJson);

    /**
     * @brief Check if per-game detail has been loaded for a given app
     */
    [[nodiscard]] bool isDetailLoaded(const std::string& steamAppId) const;

    // =========================================================================
    // PACKAGE QUERIES
    // =========================================================================

    /**
     * @brief Check if a package exists for the given Steam App ID
     */
    [[nodiscard]] bool hasPackage(const std::string& steamAppId) const;

    /**
     * @brief Get full package metadata by Steam App ID
     */
    [[nodiscard]] std::optional<PackageCatalogEntry> getPackage(const std::string& steamAppId) const;

    /**
     * @brief Get the total number of packages in the catalog
     */
    [[nodiscard]] int packageCount() const;

    // =========================================================================
    // STORE ID RESOLUTION
    // =========================================================================

    /**
     * @brief Resolve any game ID (steamAppId, epic_xxx, gog_xxx) to canonical steamAppId
     * @param gameId The game ID to resolve
     * @return The canonical Steam App ID, or empty string if not found
     */
    [[nodiscard]] std::string resolveGameId(const std::string& gameId) const;

    // =========================================================================
    // VARIANT SUPPORT
    // =========================================================================

    /**
     * @brief Get available variants for a package (e.g. ["1.00", "1.04"])
     */
    [[nodiscard]] std::vector<std::string> getVariants(const std::string& steamAppId) const;

    /**
     * @brief Get variant type label ("version" or "platform")
     */
    [[nodiscard]] std::string getVariantType(const std::string& steamAppId) const;

    // =========================================================================
    // FILE LISTING
    // =========================================================================

    /**
     * @brief Get list of relative file paths for a translation package
     *
     * For script-based installs, returns the destination paths from install steps.
     * For overlay installs, enumerates the package directory recursively.
     *
     * @param steamAppId Package to query
     * @param variant Optional variant subdirectory
     * @return List of relative file paths
     */
    [[nodiscard]] std::vector<std::string> getPackageFileList(
        const std::string& steamAppId,
        const std::string& variant = {}) const;

    // =========================================================================
    // FOLDER MATCHING
    // =========================================================================

    /**
     * @brief Match a folder name against catalog entries (case-insensitive)
     *
     * First tries exact match against dirName and gameName, then
     * tries substring containment in both directions.
     *
     * @param folderName The folder name to match
     * @return The matching Steam App ID, or empty string
     */
    [[nodiscard]] std::string findMatchingAppId(const std::string& folderName) const;

    // =========================================================================
    // FILE-BASED GAME IDENTIFICATION
    // =========================================================================

    /**
     * @brief Match a game folder against catalog fingerprints using multi-signal scoring
     *
     * Compares executable names, key file paths, and engine type against
     * stored fingerprints (explicit or auto-derived from manifest data).
     *
     * @param exeNames    Executable names found in game folder (lowercase)
     * @param engine      Detected engine type string
     * @param topEntries  Top-level file/dir names in game folder
     * @param folderName  Folder name (bonus signal)
     * @return Matches sorted by confidence (descending), max 5 results
     */
    [[nodiscard]] std::vector<FingerprintMatch> findMatchingGames(
        const std::vector<std::string>& exeNames,
        const std::string& engine,
        const std::vector<std::string>& topEntries,
        const std::string& folderName = {}) const;

    // =========================================================================
    // INSTALLED STATE MANAGEMENT
    // =========================================================================

    /**
     * @brief Check if a package is currently installed
     */
    [[nodiscard]] bool isInstalled(const std::string& steamAppId) const;

    /**
     * @brief Mark a package as installed with the given state
     */
    void markInstalled(const std::string& steamAppId, const InstalledPackageState& state);

    /**
     * @brief Mark a package as uninstalled (remove from installed map)
     */
    void markUninstalled(const std::string& steamAppId);

    /**
     * @brief Get the installed state for a package
     */
    [[nodiscard]] std::optional<InstalledPackageState> getInstalledState(
        const std::string& steamAppId) const;

    // =========================================================================
    // PERSISTENCE
    // =========================================================================

    /**
     * @brief Load installed state from a JSON file
     * @param statePath Path to installed_packages.json
     *
     * Supports both legacy format (steamAppId -> version string) and
     * new format (steamAppId -> {version, gamePath, files, installedAt}).
     */
    void loadInstalledState(const fs::path& statePath);

    /**
     * @brief Save installed state to a JSON file (atomic write)
     * @param statePath Path to write installed_packages.json
     */
    void saveInstalledState(const fs::path& statePath) const;

    // =========================================================================
    // ENUMERATION
    // =========================================================================

    /**
     * @brief Get all packages as a vector (for UI enumeration)
     */
    [[nodiscard]] std::vector<PackageCatalogEntry> allPackages() const;

    /**
     * @brief Build exe name -> steamAppId map from all package fingerprints
     *
     * Returns lowercase exe names mapped to their canonical Steam App ID.
     * Used by ProcessScanner for dynamic running-game detection.
     */
    [[nodiscard]] std::unordered_map<std::string, std::string> getAllExeMap() const;

private:
    /**
     * @brief Parse index.json (lightweight) and populate packages_
     */
    void parseIndex(const fs::path& indexPath);

    void deriveFingerprint(PackageCatalogEntry& entry) const;

    // steamAppId -> PackageCatalogEntry
    std::unordered_map<std::string, PackageCatalogEntry> packages_;

    // steamAppId -> InstalledPackageState
    std::unordered_map<std::string, InstalledPackageState> installed_;

    // Reverse index: "epic_xxx" / "gog_xxx" -> steamAppId
    std::unordered_map<std::string, std::string> storeIdToSteamAppId_;

    // Root translation data path
    fs::path dataPath_;
};

} // namespace packages
} // namespace makine
