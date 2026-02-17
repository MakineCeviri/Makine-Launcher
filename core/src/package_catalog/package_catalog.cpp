/**
 * @file package_catalog.cpp
 * @brief Local translation package catalog implementation
 * @copyright (c) 2026 MakineAI Team
 *
 * Pure C++ implementation — no Qt dependency.
 * Extracts business logic from LocalPackageManager (QML layer).
 */

#include "makineai/package_catalog.hpp"
#include "makineai/logging.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>

namespace makineai {
namespace packages {

using json = nlohmann::json;

// =============================================================================
// FALLBACK MAPPING
// =============================================================================
// Used when manifest.json is not available (transition period).
// Maps legacy pak/ directory names to {steamAppId, gameName}.

static const std::unordered_map<std::string, std::pair<std::string, std::string>> s_fallbackMapping = {
    {"ER1080",          {"1245620", "Elden Ring"}},
    {"BMW_10714712",    {"2358720", "Black Myth: Wukong"}},
    {"SF10310Hv16",     {"1716740", "Starfield"}},
    {"RDR2TRV17F",      {"1174180", "Red Dead Redemption 2"}},
    {"AH1010",          {"668580",  "Atomic Heart"}},
    {"CS21015V1",       {"949230",  "Cities: Skylines II"}},
    {"ACM107",          {"3035570", "Assassin's Creed Mirage"}},
    {"ACS100",          {"3159330", "Assassin's Creed Shadows"}},
    {"ACV170DLC123",    {"2208920", "Assassin's Creed Valhalla"}},
    {"ACO156D1234",     {"812140",  "Assassin's Creed Odyssey"}},
    {"ACOR160D12F",     {"582160",  "Assassin's Creed Origins"}},
    {"AW2_1012V1",      {"3611110", "Alan Wake 2"}},
    {"BAK1620V1",       {"208650",  "Batman: Arkham Knight"}},
    {"COTL101877",      {"1313140", "Cult of the Lamb"}},
    {"COTDG12444",      {"1123770", "Curse of the Dead Gods"}},
    {"DOS2",            {"435150",  "Divinity: Original Sin 2"}},
    {"DOSEE",           {"373420",  "Divinity: Original Sin Enhanced Edition"}},
    {"EW104D",          {"1065310", "Evil West"}},
    {"HL_1120320",      {"1583230", "High On Life"}},
    {"IFR134D2",        {"2221920", "Immortals Fenyx Rising"}},
    {"MEA",             {"1238000", "Mass Effect: Andromeda"}},
    {"POE_1381318",     {"291650",  "Pillars of Eternity"}},
    {"SM2_113010",      {"2651280", "Marvel's Spider-Man 2"}},
    {"TEATS109V1",      {"1708010", "The Expanse: A Telltale Series"}},
    {"TES4OR_04111400", {"22330",   "The Elder Scrolls IV: Oblivion Remastered"}},
    {"COE33_56442",     {"1903340", "Clair Obscur: Expedition 33"}},
    {"JGC_1000",        {"2677660", "Indiana Jones and the Great Circle"}},
    {"D2R_1471776",     {"1293830", "Diablo II: Resurrected"}},
    {"TCP_1544020",     {"1544020", "The Callisto Protocol"}},
};

// =============================================================================
// HELPERS
// =============================================================================

namespace {

/**
 * @brief Convert a string to lowercase (ASCII-safe)
 */
std::string toLower(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return result;
}

/**
 * @brief Trim whitespace from both ends of a string
 */
std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

/**
 * @brief Count files and total size under a directory recursively
 * @return {fileCount, totalSizeBytes}
 */
std::pair<int, int64_t> countFilesAndSize(const fs::path& dir) {
    int count = 0;
    int64_t totalSize = 0;
    std::error_code ec;

    for (auto it = fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec))
    {
        if (ec) continue;
        if (it->is_regular_file(ec) && !ec) {
            count++;
            totalSize += static_cast<int64_t>(it->file_size(ec));
            if (ec) ec.clear(); // ignore individual file size errors
        }
    }

    return {count, totalSize};
}

/**
 * @brief Find a subdirectory starting with "extracted_" inside the given directory
 * @return The full path to the first extracted_* subdirectory, or empty path
 */
fs::path findExtractedSubdir(const fs::path& dir) {
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (!entry.is_directory(ec) || ec) continue;
        const auto name = entry.path().filename().string();
        if (name.starts_with("extracted_")) {
            return entry.path();
        }
    }
    return {};
}

} // anonymous namespace

// =============================================================================
// CONSTRUCTOR
// =============================================================================

PackageCatalog::PackageCatalog() = default;

// =============================================================================
// LOADING
// =============================================================================

bool PackageCatalog::loadFromPath(const fs::path& translationDataPath)
{
    dataPath_ = translationDataPath;
    packages_.clear();
    storeIdToSteamAppId_.clear();

    std::error_code ec;
    if (!fs::is_directory(translationDataPath, ec)) {
        MAKINEAI_LOG_WARN(log::PACKAGE, "Translation data path does not exist: {}",
                          translationDataPath.string());
        return false;
    }

    // Try loading manifest.json first
    fs::path manifestPath = translationDataPath / "manifest.json";
    if (fs::exists(manifestPath, ec)) {
        loadManifest(manifestPath);
    }

    // Scan pak/ directories for legacy packages (mc-main format)
    fs::path pakPath = translationDataPath / "pak";
    if (fs::is_directory(pakPath, ec)) {
        scanPackageDirectories(pakPath);
    }

    // Scan root-level game-name directories (new format)
    scanGameNameDirectories(translationDataPath);

    MAKINEAI_LOG_INFO(log::PACKAGE, "PackageCatalog: loaded {} packages from {}",
                      packages_.size(), translationDataPath.string());
    return !packages_.empty();
}

// =============================================================================
// MANIFEST PARSING
// =============================================================================

void PackageCatalog::loadManifest(const fs::path& manifestPath)
{
    std::ifstream file(manifestPath);
    if (!file.is_open()) {
        MAKINEAI_LOG_WARN(log::PACKAGE, "Cannot open manifest: {}", manifestPath.string());
        return;
    }

    json doc;
    try {
        doc = json::parse(file);
    } catch (const json::parse_error& e) {
        MAKINEAI_LOG_WARN(log::PACKAGE, "Manifest parse error: {}", e.what());
        return;
    }

    if (!doc.contains("packages") || !doc["packages"].is_object()) {
        MAKINEAI_LOG_WARN(log::PACKAGE, "Manifest missing 'packages' object");
        return;
    }

    const auto& packagesObj = doc["packages"];

    for (auto it = packagesObj.begin(); it != packagesObj.end(); ++it) {
        const auto& pkgObj = it.value();
        if (!pkgObj.is_object()) continue;

        PackageCatalogEntry info;
        info.steamAppId   = it.key();
        info.packageId    = pkgObj.value("packageId", "");
        info.gameName     = pkgObj.value("gameName", "");
        info.engine       = pkgObj.value("engine", "");
        info.version      = pkgObj.value("version", "");
        info.installType  = pkgObj.value("installType", "overlay");
        info.dirName      = pkgObj.value("dirName", "");
        info.variantType  = pkgObj.value("variantType", "");

        // Parse variants array
        if (pkgObj.contains("variants") && pkgObj["variants"].is_array()) {
            for (const auto& v : pkgObj["variants"]) {
                if (v.is_string()) {
                    info.variants.push_back(v.get<std::string>());
                }
            }
        }

        // Parse contributors array [{name, role}]
        if (pkgObj.contains("contributors") && pkgObj["contributors"].is_array()) {
            for (const auto& c : pkgObj["contributors"]) {
                if (!c.is_object()) continue;
                ContributorInfo ci;
                ci.name = c.value("name", "");
                ci.role = c.value("role", "");
                info.contributors.push_back(std::move(ci));
            }
        }

        // Parse installNotes (support both field names)
        info.installNotes = pkgObj.value("installNotes", "");
        if (info.installNotes.empty()) {
            info.installNotes = pkgObj.value("installNote", "");
        }

        // Parse installMethod
        if (pkgObj.contains("installMethod") && pkgObj["installMethod"].is_object()) {
            const auto& installMethod = pkgObj["installMethod"];
            info.installMethodType   = installMethod.value("type", "");
            info.installMethodTarget = installMethod.value("target", "");

            if (installMethod.contains("steps") && installMethod["steps"].is_array()) {
                for (const auto& s : installMethod["steps"]) {
                    if (!s.is_object()) continue;
                    InstallStep step;
                    step.action   = s.value("action", "");
                    step.src      = s.value("src", "");
                    step.dest     = s.value("dest", "");
                    step.exe      = s.value("exe", "");
                    step.fallback = s.value("fallback", "");
                    step.workDir  = s.value("workDir", "game");

                    if (s.contains("args") && s["args"].is_array()) {
                        for (const auto& a : s["args"]) {
                            if (a.is_string()) {
                                step.args.push_back(a.get<std::string>());
                            }
                        }
                    }

                    info.installSteps.push_back(std::move(step));
                }
            }
        }

        // Parse storeIds for cross-store resolution
        if (pkgObj.contains("storeIds") && pkgObj["storeIds"].is_object()) {
            for (auto sit = pkgObj["storeIds"].begin(); sit != pkgObj["storeIds"].end(); ++sit) {
                const std::string store   = sit.key();
                const std::string storeId = sit.value().get<std::string>();
                info.storeIds[store] = storeId;

                // Build reverse index for non-steam stores
                if (store == "epic") {
                    storeIdToSteamAppId_["epic_" + storeId] = info.steamAppId;
                } else if (store == "gog") {
                    storeIdToSteamAppId_["gog_" + storeId] = info.steamAppId;
                }
            }
        }

        packages_[info.steamAppId] = std::move(info);
    }

    MAKINEAI_LOG_DEBUG(log::PACKAGE, "Manifest loaded: {} packages, {} store ID mappings",
                       packages_.size(), storeIdToSteamAppId_.size());
}

// =============================================================================
// PACKAGE DIRECTORY SCANNING (pak/ legacy format)
// =============================================================================

void PackageCatalog::scanPackageDirectories(const fs::path& basePath)
{
    std::error_code ec;

    for (const auto& entry : fs::directory_iterator(basePath, ec)) {
        if (!entry.is_directory(ec) || ec) continue;

        const std::string dirName = entry.path().filename().string();
        const fs::path fullPath = entry.path();

        // Check for extracted_* subdirectory
        fs::path extractedPath = findExtractedSubdir(fullPath);
        bool hasExtracted = !extractedPath.empty();

        // If already loaded from manifest by packageId, update with disk info
        bool foundInManifest = false;
        for (auto& [appId, pkg] : packages_) {
            if (pkg.packageId == dirName) {
                foundInManifest = true;
                if (hasExtracted && pkg.fileCount == 0) {
                    auto [count, totalSize] = countFilesAndSize(extractedPath);
                    pkg.fileCount = count;
                    pkg.sizeBytes = totalSize;
                }
                break;
            }
        }

        if (foundInManifest) continue;

        // Fallback: use hardcoded mapping if manifest didn't provide this package
        auto mappingIt = s_fallbackMapping.find(dirName);
        if (mappingIt == s_fallbackMapping.end()) {
            continue; // Unknown package, skip
        }

        const std::string& steamAppId = mappingIt->second.first;
        const std::string& gameName   = mappingIt->second.second;

        // Skip if already loaded (e.g. manifest had it by steamAppId)
        if (packages_.contains(steamAppId)) {
            auto& pkg = packages_[steamAppId];
            if (pkg.packageId.empty()) {
                pkg.packageId = dirName;
            }
            if (hasExtracted && pkg.fileCount == 0) {
                auto [count, totalSize] = countFilesAndSize(extractedPath);
                pkg.fileCount = count;
                pkg.sizeBytes = totalSize;
            }
            continue;
        }

        // Create package info from directory scan + fallback mapping
        PackageCatalogEntry info;
        info.packageId   = dirName;
        info.steamAppId  = steamAppId;
        info.gameName    = gameName;
        info.installType = "overlay";
        info.storeIds["steam"] = steamAppId;

        if (hasExtracted) {
            auto [count, totalSize] = countFilesAndSize(extractedPath);
            info.fileCount = count;
            info.sizeBytes = totalSize;
        }

        packages_[steamAppId] = std::move(info);
    }
}

// =============================================================================
// GAME-NAME DIRECTORY SCANNING (new format)
// =============================================================================

void PackageCatalog::scanGameNameDirectories(const fs::path& basePath)
{
    static const std::set<std::string> skipDirs = {"pak", "mc-main", ".git"};

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(basePath, ec)) {
        if (!entry.is_directory(ec) || ec) continue;

        const std::string dirName = entry.path().filename().string();
        if (skipDirs.contains(dirName)) continue;

        // Find manifest entry that matches this dirName
        bool found = false;
        for (auto& [appId, pkg] : packages_) {
            if (pkg.dirName == dirName) {
                found = true;
                // Update file stats if not already set
                if (pkg.fileCount == 0) {
                    fs::path scanPath = basePath / dirName;
                    // If has variants, count files in first variant
                    if (!pkg.variants.empty()) {
                        scanPath = basePath / dirName / pkg.variants.front();
                    }
                    if (fs::is_directory(scanPath, ec)) {
                        auto [count, totalSize] = countFilesAndSize(scanPath);
                        pkg.fileCount = count;
                        pkg.sizeBytes = totalSize;
                    }
                }
                break;
            }
        }

        if (!found) {
            MAKINEAI_LOG_DEBUG(log::PACKAGE,
                "scanGameNameDirectories: unrecognized directory {}", dirName);
        }
    }
}

// =============================================================================
// PACKAGE QUERIES
// =============================================================================

bool PackageCatalog::hasPackage(const std::string& steamAppId) const
{
    return packages_.contains(steamAppId);
}

std::optional<PackageCatalogEntry> PackageCatalog::getPackage(const std::string& steamAppId) const
{
    auto it = packages_.find(steamAppId);
    if (it != packages_.end()) {
        return it->second;
    }
    return std::nullopt;
}

int PackageCatalog::packageCount() const
{
    return static_cast<int>(packages_.size());
}

// =============================================================================
// STORE ID RESOLUTION
// =============================================================================

std::string PackageCatalog::resolveGameId(const std::string& gameId) const
{
    // Direct match — already a steamAppId
    if (packages_.contains(gameId)) {
        return gameId;
    }

    // Reverse lookup via store IDs (epic_xxx, gog_xxx)
    auto it = storeIdToSteamAppId_.find(gameId);
    if (it != storeIdToSteamAppId_.end()) {
        return it->second;
    }

    return {};
}

// =============================================================================
// VARIANT SUPPORT
// =============================================================================

std::vector<std::string> PackageCatalog::getVariants(const std::string& steamAppId) const
{
    auto it = packages_.find(steamAppId);
    if (it == packages_.end()) return {};
    return it->second.variants;
}

std::string PackageCatalog::getVariantType(const std::string& steamAppId) const
{
    auto it = packages_.find(steamAppId);
    if (it == packages_.end()) return {};
    return it->second.variantType;
}

// =============================================================================
// FILE LISTING
// =============================================================================

std::vector<std::string> PackageCatalog::getPackageFileList(
    const std::string& steamAppId,
    const std::string& variant) const
{
    auto pkgIt = packages_.find(steamAppId);
    if (pkgIt == packages_.end()) return {};

    const PackageCatalogEntry& pkg = pkgIt->second;
    std::error_code ec;

    // For script-based installs, compute target files from install steps.
    // This ensures backup covers the actual game files that will be overwritten.
    if (!pkg.installSteps.empty()) {
        std::vector<std::string> targetFiles;
        fs::path sourcePath = dataPath_ / pkg.dirName;

        for (const InstallStep& step : pkg.installSteps) {
            if (step.action == "copy") {
                targetFiles.push_back(step.dest);
            } else if (step.action == "copyDir") {
                // Scan the source dir to get the actual file list
                fs::path srcDir = fs::weakly_canonical(sourcePath / step.src);
                if (!fs::is_directory(srcDir, ec)) continue;

                for (auto it = fs::recursive_directory_iterator(srcDir, fs::directory_options::skip_permission_denied, ec);
                     it != fs::recursive_directory_iterator(); it.increment(ec))
                {
                    if (ec) continue;
                    if (!it->is_regular_file(ec) || ec) continue;

                    // Compute relative path from srcDir
                    auto relPath = fs::relative(it->path(), srcDir, ec);
                    if (ec) continue;
                    targetFiles.push_back(step.dest + "/" + relPath.generic_string());
                }
            }
            // "run", "delete", "installFont" don't produce predictable target files
        }
        return targetFiles;
    }

    // Default: scan package directory for overlay installs
    fs::path sourcePath;

    if (!pkg.dirName.empty()) {
        sourcePath = !variant.empty()
            ? dataPath_ / pkg.dirName / variant
            : dataPath_ / pkg.dirName;
    }

    if (sourcePath.empty() || !fs::is_directory(sourcePath, ec)) {
        // Fall back to pak/ legacy format
        fs::path pkgDirPath = dataPath_ / "pak" / pkg.packageId;
        if (fs::is_directory(pkgDirPath, ec)) {
            fs::path extracted = findExtractedSubdir(pkgDirPath);
            if (!extracted.empty()) {
                sourcePath = extracted;
            }
        }
    }

    if (sourcePath.empty() || !fs::is_directory(sourcePath, ec)) return {};

    std::vector<std::string> files;
    for (auto it = fs::recursive_directory_iterator(sourcePath, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec))
    {
        if (ec) continue;
        if (!it->is_regular_file(ec) || ec) continue;

        auto relPath = fs::relative(it->path(), sourcePath, ec);
        if (ec) continue;
        files.push_back(relPath.generic_string());
    }

    return files;
}

// =============================================================================
// FOLDER MATCHING
// =============================================================================

std::string PackageCatalog::findMatchingAppId(const std::string& folderName) const
{
    const std::string normalized = toLower(trim(folderName));

    // Exact match against dirName or gameName (case-insensitive)
    for (const auto& [appId, pkg] : packages_) {
        if (toLower(pkg.dirName) == normalized || toLower(pkg.gameName) == normalized) {
            return appId;
        }
    }

    // Substring match: folder contains gameName or vice versa
    for (const auto& [appId, pkg] : packages_) {
        const std::string dirLower  = toLower(pkg.dirName);
        const std::string nameLower = toLower(pkg.gameName);

        if (normalized.find(nameLower) != std::string::npos ||
            nameLower.find(normalized) != std::string::npos ||
            normalized.find(dirLower) != std::string::npos ||
            dirLower.find(normalized) != std::string::npos)
        {
            return appId;
        }
    }

    return {};
}

// =============================================================================
// INSTALLED STATE MANAGEMENT
// =============================================================================

bool PackageCatalog::isInstalled(const std::string& steamAppId) const
{
    return installed_.contains(steamAppId);
}

void PackageCatalog::markInstalled(const std::string& steamAppId,
                                   const InstalledPackageState& state)
{
    installed_[steamAppId] = state;
}

void PackageCatalog::markUninstalled(const std::string& steamAppId)
{
    installed_.erase(steamAppId);
}

std::optional<InstalledPackageState> PackageCatalog::getInstalledState(
    const std::string& steamAppId) const
{
    auto it = installed_.find(steamAppId);
    if (it != installed_.end()) {
        return it->second;
    }
    return std::nullopt;
}

// =============================================================================
// PERSISTENCE — LOAD
// =============================================================================

void PackageCatalog::loadInstalledState(const fs::path& statePath)
{
    std::error_code ec;
    if (!fs::exists(statePath, ec)) return;

    std::ifstream file(statePath);
    if (!file.is_open()) {
        MAKINEAI_LOG_WARN(log::PACKAGE, "Cannot open installed state file: {}",
                          statePath.string());
        return;
    }

    json doc;
    try {
        doc = json::parse(file);
    } catch (const json::parse_error& e) {
        MAKINEAI_LOG_WARN(log::PACKAGE, "Installed state parse error: {}", e.what());
        return;
    }

    if (!doc.is_object()) return;

    for (auto it = doc.begin(); it != doc.end(); ++it) {
        InstalledPackageState state;

        if (it.value().is_string()) {
            // Legacy format: steamAppId -> version string
            state.version = it.value().get<std::string>();
        } else if (it.value().is_object()) {
            // New format: steamAppId -> { version, gamePath, files, installedAt }
            const auto& obj = it.value();
            state.version     = obj.value("version", "");
            state.gamePath    = obj.value("gamePath", "");
            state.installedAt = obj.value("installedAt", static_cast<int64_t>(0));

            if (obj.contains("files") && obj["files"].is_array()) {
                for (const auto& f : obj["files"]) {
                    if (f.is_string()) {
                        state.installedFiles.push_back(f.get<std::string>());
                    }
                }
            }
        } else {
            continue; // Skip unknown value types
        }

        installed_[it.key()] = std::move(state);
    }

    MAKINEAI_LOG_DEBUG(log::PACKAGE, "Loaded installed state: {} entries from {}",
                       installed_.size(), statePath.string());
}

// =============================================================================
// PERSISTENCE — SAVE
// =============================================================================

void PackageCatalog::saveInstalledState(const fs::path& statePath) const
{
    std::error_code ec;

    // Ensure parent directory exists
    fs::path parentDir = statePath.parent_path();
    if (!parentDir.empty()) {
        fs::create_directories(parentDir, ec);
        if (ec) {
            MAKINEAI_LOG_WARN(log::PACKAGE, "Cannot create directory for installed state: {}",
                              ec.message());
        }
    }

    json root = json::object();
    for (const auto& [appId, state] : installed_) {
        json obj = json::object();
        obj["version"]     = state.version;
        obj["gamePath"]    = state.gamePath;
        obj["installedAt"] = state.installedAt;

        json filesArr = json::array();
        for (const auto& f : state.installedFiles) {
            filesArr.push_back(f);
        }
        obj["files"] = std::move(filesArr);

        root[appId] = std::move(obj);
    }

    std::string data = root.dump(2);

    // Atomic write: write to temp file, then rename
    fs::path tempPath = statePath;
    tempPath += ".tmp";

    {
        std::ofstream file(tempPath, std::ios::trunc);
        if (!file.is_open()) {
            MAKINEAI_LOG_ERROR(log::PACKAGE, "Cannot write installed state file: {}",
                               statePath.string());
            return;
        }
        file << data;
        file.flush();

        if (!file.good()) {
            file.close();
            fs::remove(tempPath, ec);
            MAKINEAI_LOG_ERROR(log::PACKAGE, "Installed state write failed (flush error)");
            return;
        }
    } // file closed here

    fs::rename(tempPath, statePath, ec);
    if (ec) {
        // Rename failed — try to clean up temp file
        fs::remove(tempPath, ec);
        MAKINEAI_LOG_ERROR(log::PACKAGE, "Installed state atomic rename failed: {}",
                           ec.message());
        return;
    }

    MAKINEAI_LOG_DEBUG(log::PACKAGE, "Saved installed state: {} entries to {}",
                       installed_.size(), statePath.string());
}

// =============================================================================
// ENUMERATION
// =============================================================================

std::vector<PackageCatalogEntry> PackageCatalog::allPackages() const
{
    std::vector<PackageCatalogEntry> result;
    result.reserve(packages_.size());
    for (const auto& [appId, pkg] : packages_) {
        result.push_back(pkg);
    }
    return result;
}

} // namespace packages
} // namespace makineai
