/**
 * @file package_catalog.cpp
 * @brief Network-based translation package catalog implementation
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Pure C++ implementation — no Qt dependency.
 * Loads lightweight index.json from CDN cache, enriches on-demand
 * with per-game detail JSON.
 */

#include "makine/package_catalog.hpp"
#include "makine/logging.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace makine {
namespace packages {

using json = nlohmann::json;

// =============================================================================
// HELPERS
// =============================================================================

namespace {

std::string toLower(std::string_view s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return result;
}

std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Strip everything except letters and digits, then lowercase.
// "Dave the Diver" → "davethediver"
// "DAVE THE DIVER" → "davethediver"
// "Dave.The.Diver"  → "davethediver"
std::string alphaOnly(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    for (unsigned char c : s) {
        if (std::isalnum(c))
            r += static_cast<char>(std::tolower(c));
    }
    return r;
}

// Split into lowercase word tokens (split on non-alnum boundaries).
// Filters out short noise words: "the", "a", "of", "and", "to".
std::vector<std::string> tokenize(const std::string& s) {
    static const std::set<std::string> noise = {"the", "a", "of", "and", "to", "in", "on", "for"};
    std::vector<std::string> tokens;
    std::string current;
    for (unsigned char c : s) {
        if (std::isalnum(c)) {
            current += static_cast<char>(std::tolower(c));
        } else if (!current.empty()) {
            if (current.size() > 1 || std::isdigit(static_cast<unsigned char>(current[0]))) {
                if (noise.find(current) == noise.end())
                    tokens.push_back(current);
            }
            current.clear();
        }
    }
    if (!current.empty() && (current.size() > 1 || std::isdigit(static_cast<unsigned char>(current[0])))) {
        if (noise.find(current) == noise.end())
            tokens.push_back(current);
    }
    return tokens;
}

// Compute Jaccard similarity between two token sets (0.0 - 1.0)
double tokenSimilarity(const std::vector<std::string>& a, const std::vector<std::string>& b) {
    if (a.empty() || b.empty()) return 0.0;
    std::set<std::string> sa(a.begin(), a.end());
    std::set<std::string> sb(b.begin(), b.end());
    int intersection = 0;
    for (const auto& t : sa) {
        if (sb.count(t)) ++intersection;
    }
    int unionSize = static_cast<int>(sa.size() + sb.size()) - intersection;
    return unionSize > 0 ? static_cast<double>(intersection) / unionSize : 0.0;
}

} // anonymous namespace

// =============================================================================
// CONSTRUCTOR
// =============================================================================

PackageCatalog::PackageCatalog() = default;

// =============================================================================
// LOADING
// =============================================================================

bool PackageCatalog::loadFromIndex(const fs::path& indexPath, const fs::path& packageCacheRoot)
{
    packages_.clear();
    storeIdToSteamAppId_.clear();
    dataPath_ = packageCacheRoot;

    std::error_code ec;
    if (!fs::exists(indexPath, ec)) {
        MAKINE_LOG_WARN(log::PACKAGE, "Index file does not exist: {}",
                          indexPath.string());
        return false;
    }

    parseIndex(indexPath);

    MAKINE_LOG_INFO(log::PACKAGE, "PackageCatalog: loaded {} packages from index (network-only)",
                      packages_.size());
    return !packages_.empty();
}

// =============================================================================
// SHARED PARSE HELPERS (free functions, used by parseIndex and enrichPackage)
// =============================================================================

namespace {

std::vector<InstallStep> parseStepsArray(const json& stepsArr)
{
    std::vector<InstallStep> steps;
    if (!stepsArr.is_array()) return steps;
    steps.reserve(stepsArr.size());
    for (const auto& s : stepsArr) {
        if (!s.is_object()) continue;
        InstallStep step;
        step.action   = s.value("action", "");
        step.src      = s.value("src", "");
        step.dest     = s.value("dest", "");
        step.exe      = s.value("exe", "");
        step.fallback = s.value("fallback", "");
        step.workDir  = s.value("workDir", "game");
        step.language = s.value("language", "");
        if (s.contains("args") && s["args"].is_array()) {
            for (const auto& a : s["args"]) {
                if (a.is_string()) {
                    step.args.push_back(a.get<std::string>());
                }
            }
        }
        steps.push_back(std::move(step));
    }
    return steps;
}

std::vector<InstallOption> parseOptionsArray(const json& arr)
{
    std::vector<InstallOption> options;
    if (!arr.is_array()) return options;
    for (const auto& opt : arr) {
        if (!opt.is_object()) continue;
        InstallOption option;
        option.id              = opt.value("id", "");
        option.label           = opt.value("label", "");
        option.description     = opt.value("description", "");
        option.icon            = opt.value("icon", "");
        option.defaultSelected = opt.value("default", false);
        option.subDir          = opt.value("subDir", "");
        if (opt.contains("steps")) {
            option.steps = parseStepsArray(opt["steps"]);
        }
        options.push_back(std::move(option));
    }
    return options;
}

std::unordered_map<std::string, std::vector<InstallStep>> parseCombinedSteps(const json& obj)
{
    std::unordered_map<std::string, std::vector<InstallStep>> result;
    if (!obj.is_object()) return result;
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        result[it.key()] = parseStepsArray(it.value());
    }
    return result;
}

void parseInstallMethod(PackageCatalogEntry& entry, const json& obj)
{
    if (!obj.contains("installMethod") || !obj["installMethod"].is_object()) return;

    const auto& im = obj["installMethod"];
    entry.installMethodType   = im.value("type", "");
    entry.installMethodTarget = im.value("target", "");

    if (im.contains("steps")) {
        entry.installSteps = parseStepsArray(im["steps"]);
    }

    if (im.contains("options")) {
        entry.installOptions = parseOptionsArray(im["options"]);
    }

    if (im.contains("combinedSteps")) {
        entry.combinedSteps = parseCombinedSteps(im["combinedSteps"]);
    }

    if (im.contains("variantInstallOptions") && im["variantInstallOptions"].is_object()) {
        for (auto vit = im["variantInstallOptions"].begin();
             vit != im["variantInstallOptions"].end(); ++vit)
        {
            if (!vit.value().is_object()) continue;
            const auto& vcObj = vit.value();
            VariantConfig vc;
            if (vcObj.contains("options"))
                vc.installOptions = parseOptionsArray(vcObj["options"]);
            if (vcObj.contains("combinedSteps"))
                vc.combinedSteps = parseCombinedSteps(vcObj["combinedSteps"]);
            entry.variantInstallOptions[vit.key()] = std::move(vc);
        }
    }
}

void parseStoreIds(PackageCatalogEntry& entry, const json& obj,
                    std::unordered_map<std::string, std::string>& reverseIndex)
{
    if (!obj.contains("storeIds") || !obj["storeIds"].is_object()) return;

    for (auto sit = obj["storeIds"].begin(); sit != obj["storeIds"].end(); ++sit) {
        const std::string store   = sit.key();
        const std::string storeId = sit.value().get<std::string>();
        entry.storeIds[store] = storeId;

        if (store == "epic") {
            reverseIndex["epic_" + storeId] = entry.steamAppId;
        } else if (store == "gog") {
            reverseIndex["gog_" + storeId] = entry.steamAppId;
        }
    }
}

void parseFingerprint(PackageCatalogEntry& entry, const json& obj)
{
    if (!obj.contains("fingerprint") || !obj["fingerprint"].is_object()) return;

    const auto& fp = obj["fingerprint"];
    GameFingerprint gfp;
    if (fp.contains("exeNames") && fp["exeNames"].is_array()) {
        for (const auto& e : fp["exeNames"]) {
            if (e.is_string()) gfp.exeNames.push_back(e.get<std::string>());
        }
    }
    if (fp.contains("keyFiles") && fp["keyFiles"].is_array()) {
        for (const auto& k : fp["keyFiles"]) {
            if (k.is_string()) gfp.keyFiles.push_back(k.get<std::string>());
        }
    }
    gfp.engineHint = fp.value("engineHint", "");
    entry.fingerprint = std::move(gfp);
}

void parseContributors(PackageCatalogEntry& entry, const json& obj)
{
    if (!obj.contains("contributors") || !obj["contributors"].is_array()) return;

    entry.contributors.clear();
    for (const auto& c : obj["contributors"]) {
        if (!c.is_object()) continue;
        ContributorInfo ci;
        ci.name = c.value("name", "");
        ci.role = c.value("role", "");
        entry.contributors.push_back(std::move(ci));
    }
}

} // anonymous namespace (parse helpers)

bool PackageCatalog::enrichPackage(const std::string& steamAppId, const std::string& detailJson)
{
    auto it = packages_.find(steamAppId);
    if (it == packages_.end()) {
        MAKINE_LOG_WARN(log::PACKAGE, "enrichPackage: unknown appId {}", steamAppId);
        return false;
    }

    json doc;
    try {
        doc = json::parse(detailJson);
    } catch (const json::parse_error& e) {
        MAKINE_LOG_WARN(log::PACKAGE, "enrichPackage parse error for {}: {}", steamAppId, e.what());
        return false;
    }

    if (!doc.is_object()) return false;

    auto& entry = it->second;

    // Merge basic fields that may be richer in detail
    if (doc.contains("engine") && doc["engine"].is_string())
        entry.engine = doc["engine"].get<std::string>();
    if (doc.contains("installType") && doc["installType"].is_string())
        entry.installType = doc["installType"].get<std::string>();
    if (doc.contains("tier") && doc["tier"].is_string())
        entry.tier = doc["tier"].get<std::string>();
    if (doc.contains("lastUpdated") && doc["lastUpdated"].is_string())
        entry.lastUpdated = doc["lastUpdated"].get<std::string>();
    if (doc.contains("variantType") && doc["variantType"].is_string())
        entry.variantType = doc["variantType"].get<std::string>();
    if (doc.contains("installNotes") && doc["installNotes"].is_string())
        entry.installNotes = doc["installNotes"].get<std::string>();
    if (entry.installNotes.empty() && doc.contains("installNote") && doc["installNote"].is_string())
        entry.installNotes = doc["installNote"].get<std::string>();
    if (doc.contains("specialDialog") && doc["specialDialog"].is_string())
        entry.specialDialog = doc["specialDialog"].get<std::string>();

    // Parse variants array
    if (doc.contains("variants") && doc["variants"].is_array()) {
        entry.variants.clear();
        for (const auto& v : doc["variants"]) {
            if (v.is_string()) entry.variants.push_back(v.get<std::string>());
        }
    }

    // Parse complex fields via shared helpers
    parseContributors(entry, doc);
    parseInstallMethod(entry, doc);
    parseStoreIds(entry, doc, storeIdToSteamAppId_);
    parseFingerprint(entry, doc);

    // Merge aliases (deduplicated)
    if (doc.contains("aliases") && doc["aliases"].is_array()) {
        for (const auto& alias : doc["aliases"]) {
            if (alias.is_string()) {
                std::string a = alias.get<std::string>();
                if (std::find(entry.aliases.begin(), entry.aliases.end(), a) == entry.aliases.end()) {
                    entry.aliases.push_back(std::move(a));
                }
            }
        }
    }

    // Auto-derive fingerprint if not explicit (neither from index nor detail)
    if (!entry.fingerprint.has_value())
        deriveFingerprint(entry);

    entry.detailLoaded = true;

    MAKINE_LOG_DEBUG(log::PACKAGE, "enrichPackage: {} enriched (installMethod={})",
                       steamAppId, entry.installMethodType);
    return true;
}

bool PackageCatalog::isDetailLoaded(const std::string& steamAppId) const
{
    auto it = packages_.find(steamAppId);
    if (it == packages_.end()) return false;
    return it->second.detailLoaded;
}

// =============================================================================
// INDEX PARSING (lightweight catalog from index.json)
// =============================================================================

void PackageCatalog::parseIndex(const fs::path& indexPath)
{
    std::ifstream file(indexPath);
    if (!file.is_open()) {
        MAKINE_LOG_WARN(log::PACKAGE, "Cannot open index: {}", indexPath.string());
        return;
    }

    json doc;
    try {
        doc = json::parse(file);
    } catch (const json::parse_error& e) {
        MAKINE_LOG_WARN(log::PACKAGE, "Index parse error: {}", e.what());
        return;
    }

    if (!doc.contains("packages") || !doc["packages"].is_object()) {
        MAKINE_LOG_WARN(log::PACKAGE, "Index missing 'packages' object");
        return;
    }

    const auto& packagesObj = doc["packages"];

    for (auto it = packagesObj.begin(); it != packagesObj.end(); ++it) {
        const auto& entry = it.value();
        if (!entry.is_object()) continue;

        PackageCatalogEntry info;
        info.steamAppId = it.key();
        info.gameName   = entry.value("name", "");
        info.version    = entry.value("v", "");
        info.dirName    = entry.value("dirName", "");
        if (entry.contains("aliases") && entry["aliases"].is_array()) {
            for (const auto& alias : entry["aliases"]) {
                if (alias.is_string()) {
                    info.aliases.push_back(alias.get<std::string>());
                }
            }
        }
        info.sizeBytes  = entry.value("sizeBytes", static_cast<int64_t>(0));
        info.installType = "overlay";  // default, enriched later
        info.tier = "free";            // default, enriched later
        info.detailLoaded = false;

        // Parse exe names from index for immediate fingerprint (no CDN round-trip)
        if (entry.contains("exe") && entry["exe"].is_array()) {
            GameFingerprint fp;
            for (const auto& e : entry["exe"]) {
                if (e.is_string()) fp.exeNames.push_back(e.get<std::string>());
            }
            if (!fp.exeNames.empty())
                info.fingerprint = std::move(fp);
        }

        // Auto-derive fingerprint if not explicitly provided
        if (!info.fingerprint.has_value())
            deriveFingerprint(info);

        packages_[info.steamAppId] = std::move(info);
    }

    MAKINE_LOG_DEBUG(log::PACKAGE, "Index loaded: {} packages", packages_.size());
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
        targetFiles.reserve(pkg.installSteps.size());
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
    // Strip common scene release tags before matching:
    // "Death.Stranding.2.On.The.Beach-InsaneRamZes" → "Death.Stranding.2.On.The.Beach"
    std::string cleaned = trim(folderName);
    if (auto pos = cleaned.rfind('-'); pos != std::string::npos && pos > cleaned.size() / 2) {
        // Only strip if the part after '-' looks like a scene tag (single word, no spaces)
        std::string suffix = cleaned.substr(pos + 1);
        if (!suffix.empty() && suffix.find(' ') == std::string::npos &&
            suffix.find('.') == std::string::npos && suffix.size() >= 4) {
            cleaned = cleaned.substr(0, pos);
        }
    }
    const std::string normalized = toLower(trim(cleaned));
    if (normalized.empty()) return {};

    // Tier 1: Exact case-insensitive match against dirName or gameName
    for (const auto& [appId, pkg] : packages_) {
        if (toLower(pkg.dirName) == normalized || toLower(pkg.gameName) == normalized) {
            return appId;
        }
    }

    // Tier 1.5: Check aliases (exact and alphanumeric-normalized)
    const std::string alphaInput = alphaOnly(normalized);
    for (const auto& [appId, pkg] : packages_) {
        for (const auto& alias : pkg.aliases) {
            if (toLower(alias) == normalized) {
                return appId;
            }
            if (alphaOnly(alias) == alphaInput) {
                return appId;
            }
        }
    }

    // Tier 1.5b: Alias substring match (bidirectional, min 5 chars)
    if (normalized.size() >= 5) {
        for (const auto& [appId, pkg] : packages_) {
            for (const auto& alias : pkg.aliases) {
                std::string aliasLower = toLower(alias);
                if (aliasLower.size() >= 5 &&
                    (aliasLower.find(normalized) != std::string::npos ||
                     normalized.find(aliasLower) != std::string::npos)) {
                    return appId;
                }
            }
        }
    }

    // Tier 2: Alphanumeric-normalized match (handles punctuation/spacing diffs)
    //   "Dave the Diver" ↔ "DaveTheDiver" ↔ "DAVE THE DIVER"
    //   "Elden Ring" ↔ "EldenRing" ↔ "ELDEN_RING"
    if (alphaInput.size() >= 3) {
        for (const auto& [appId, pkg] : packages_) {
            if (alphaOnly(pkg.dirName) == alphaInput ||
                alphaOnly(pkg.gameName) == alphaInput) {
                return appId;
            }
        }
    }

    // Tier 3: Substring match — require minimum 5 chars to avoid false positives
    // Guard: reject sequel mismatches where the only difference is a trailing number.
    // e.g. "red dead redemption" should NOT match "red dead redemption 2"
    auto isSequelMismatch = [](const std::string& shorter, const std::string& longer) -> bool {
        if (shorter.size() >= longer.size()) return false;
        auto pos = longer.find(shorter);
        if (pos == std::string::npos) return false;
        // Check what remains after the match
        std::string remainder = longer.substr(pos + shorter.size());
        // Trim leading spaces/punctuation
        size_t start = remainder.find_first_not_of(" :-_.");
        if (start == std::string::npos) return false;
        remainder = remainder.substr(start);
        // If remainder starts with a digit, it's a sequel
        if (!remainder.empty() && std::isdigit(static_cast<unsigned char>(remainder[0])))
            return true;
        // Also check prefix (e.g. "2 red dead" unlikely but safe)
        std::string prefix = longer.substr(0, pos);
        size_t end = prefix.find_last_not_of(" :-_.");
        if (end != std::string::npos) {
            if (std::isdigit(static_cast<unsigned char>(prefix[end])))
                return true;
        }
        return false;
    };

    if (normalized.size() >= 5) {
        for (const auto& [appId, pkg] : packages_) {
            const std::string dirLower  = toLower(pkg.dirName);
            const std::string nameLower = toLower(pkg.gameName);

            // Check containment in both directions, skip sequel mismatches
            bool nameContainsInput = nameLower.size() >= 5 && nameLower.find(normalized) != std::string::npos;
            bool inputContainsName = nameLower.size() >= 5 && normalized.find(nameLower) != std::string::npos;
            bool dirContainsInput  = dirLower.size() >= 5 && dirLower.find(normalized) != std::string::npos;
            bool inputContainsDir  = dirLower.size() >= 5 && normalized.find(dirLower) != std::string::npos;

            if (nameContainsInput && !isSequelMismatch(normalized, nameLower)) return appId;
            if (inputContainsName && !isSequelMismatch(nameLower, normalized)) return appId;
            if (dirContainsInput && !isSequelMismatch(normalized, dirLower)) return appId;
            if (inputContainsDir && !isSequelMismatch(dirLower, normalized)) return appId;
        }
    }

    // Tier 4: Token-based similarity (handles word reordering, noise words)
    //   "Grand Theft Auto V" ↔ "GrandTheftAutoV"
    //   Requires >= 80% Jaccard similarity AND >= 2 matching tokens

    // Normalize Roman numerals for token comparison
    static const std::vector<std::pair<std::string, std::string>> numerals = {
        {"ii", "2"}, {"iii", "3"}, {"iv", "4"}, {"v", "5"},
        {"vi", "6"}, {"vii", "7"}, {"viii", "8"}, {"ix", "9"}, {"x", "10"}
    };

    auto normalizeNumerals = [&](std::vector<std::string>& tokens) {
        for (auto& t : tokens) {
            for (const auto& [roman, arabic] : numerals) {
                if (t == roman) { t = arabic; break; }
            }
        }
    };

    auto inputTokens = tokenize(normalized);
    normalizeNumerals(inputTokens);

    // Helper: count matching tokens between two sets
    auto countIntersection = [](const std::vector<std::string>& a,
                                const std::vector<std::string>& b) -> int {
        std::set<std::string> sb(b.begin(), b.end());
        int count = 0;
        for (const auto& t : a) {
            if (sb.count(t)) ++count;
        }
        return count;
    };

    // Tier 3.5: Strong token overlap — catches trilogy sub-games and similar
    // "Grand Theft Auto San Andreas Definitive Edition" ↔ "Grand Theft Auto The Trilogy The Definitive Edition"
    // Intersection: {grand, theft, auto, definitive, edition} = 5 tokens, ratio 5/8 = 0.625
    // Requires: intersection >= 4 AND ratio >= 0.55
    if (inputTokens.size() >= 3) {
        std::string bestAppId;
        int bestIntersection = 0;
        double bestRatio = 0.0;

        for (const auto& [appId, pkg] : packages_) {
            for (const auto* src : {&pkg.gameName, &pkg.dirName}) {
                auto tokens = tokenize(*src);
                normalizeNumerals(tokens);
                if (tokens.size() < 3) continue;

                int inter = countIntersection(inputTokens, tokens);
                int unionSize = static_cast<int>(
                    std::set<std::string>(inputTokens.begin(), inputTokens.end()).size() +
                    std::set<std::string>(tokens.begin(), tokens.end()).size()) - inter;
                double ratio = unionSize > 0 ? static_cast<double>(inter) / unionSize : 0.0;

                if (inter >= 4 && ratio >= 0.55 && inter > bestIntersection) {
                    bestIntersection = inter;
                    bestRatio = ratio;
                    bestAppId = appId;
                }
            }
        }

        if (!bestAppId.empty()) {
            return bestAppId;
        }
    }

    // Tier 4: Jaccard similarity — strict match for shorter names
    if (inputTokens.size() >= 2) {
        std::string bestAppId;
        double bestScore = 0.0;

        for (const auto& [appId, pkg] : packages_) {
            auto nameTokens = tokenize(pkg.gameName);
            normalizeNumerals(nameTokens);
            double score = tokenSimilarity(inputTokens, nameTokens);
            if (score > bestScore) {
                bestScore = score;
                bestAppId = appId;
            }

            auto dirTokens = tokenize(pkg.dirName);
            normalizeNumerals(dirTokens);
            score = tokenSimilarity(inputTokens, dirTokens);
            if (score > bestScore) {
                bestScore = score;
                bestAppId = appId;
            }
        }

        // Adaptive threshold: lower for longer token sets (more words = more variation)
        double threshold = (inputTokens.size() >= 5) ? 0.65 : 0.8;
        if (bestScore >= threshold && !bestAppId.empty()) {
            return bestAppId;
        }
    }

    return {};
}

// =============================================================================
// FILE-BASED GAME IDENTIFICATION
// =============================================================================

namespace {

// Normalize engine string to a lowercase hint for comparison
std::string normalizeEngine(const std::string& engine)
{
    std::string lower = toLower(engine);

    // Map common engine names to canonical hints
    if (lower.find("unreal") != std::string::npos) return "unreal";
    if (lower.find("unity") != std::string::npos)  return "unity";
    if (lower.find("renpy") != std::string::npos || lower.find("ren'py") != std::string::npos) return "renpy";
    if (lower.find("rpg maker") != std::string::npos || lower.find("rpgmaker") != std::string::npos) return "rpgmaker";
    if (lower.find("gamemaker") != std::string::npos) return "gamemaker";
    if (lower.find("godot") != std::string::npos)  return "godot";
    if (lower.find("source") != std::string::npos)  return "source";
    if (lower.find("cryengine") != std::string::npos) return "cryengine";
    if (lower.find("frostbite") != std::string::npos) return "frostbite";
    if (lower.find("id tech") != std::string::npos) return "idtech";
    if (lower.find("bethesda") != std::string::npos || lower.find("creation") != std::string::npos) return "bethesda";
    if (lower.find("re engine") != std::string::npos) return "reengine";

    return "custom";
}

// Remove all characters in `chars` from `s`
std::string removeChars(std::string_view s, std::string_view chars)
{
    std::string r;
    r.reserve(s.size());
    for (char c : s) {
        if (chars.find(c) == std::string_view::npos) r += c;
    }
    return r;
}

// Derive possible exe names from a game name
std::vector<std::string> deriveExeNames(const std::string& gameName)
{
    std::vector<std::string> names;
    if (gameName.empty()) return names;

    // "Hades" -> "hades.exe"
    names.push_back(toLower(gameName) + ".exe");

    // "The Witcher" -> "thewitcher.exe" (no spaces)
    std::string noSpaceLower = toLower(removeChars(gameName, " "));
    if (noSpaceLower + ".exe" != names[0])
        names.push_back(noSpaceLower + ".exe");

    // "DOOM (2016)" -> "doom.exe" (remove parens, colons, etc.)
    std::string cleaned = removeChars(gameName, ":'-()!.,");
    // Collapse multiple spaces and trim
    std::string collapsed;
    bool prevSpace = false;
    for (char c : cleaned) {
        if (c == ' ') {
            if (!prevSpace && !collapsed.empty()) collapsed += ' ';
            prevSpace = true;
        } else {
            collapsed += c;
            prevSpace = false;
        }
    }
    while (!collapsed.empty() && collapsed.back() == ' ') collapsed.pop_back();

    std::string cleanedLower = toLower(collapsed);
    if (cleanedLower + ".exe" != names[0])
        names.push_back(cleanedLower + ".exe");

    // No-space version of cleaned
    std::string cleanedNoSpaceLower = toLower(removeChars(collapsed, " "));
    if (cleanedNoSpaceLower + ".exe" != names[0] && cleanedNoSpaceLower + ".exe" != noSpaceLower + ".exe")
        names.push_back(cleanedNoSpaceLower + ".exe");

    return names;
}

// Extract the top-level directory component from an install step dest path
std::string extractTopDir(const std::string& path)
{
    if (path.empty()) return {};
    // "Content/Game/Text" -> "Content"
    // "Data/Starfield - Localization.ba2" -> "Data"
    auto pos = path.find('/');
    if (pos == std::string::npos) pos = path.find('\\');
    if (pos != std::string::npos) return path.substr(0, pos);
    // Single component with extension is a file, not useful as keyFile
    if (path.find('.') != std::string::npos) return {};
    return path;
}

} // anonymous namespace

void PackageCatalog::deriveFingerprint(PackageCatalogEntry& entry) const
{
    GameFingerprint fp;

    // 1. Derive exe names from gameName
    fp.exeNames = deriveExeNames(entry.gameName);

    // Also try dirName if different from gameName
    if (!entry.dirName.empty() && entry.dirName != entry.gameName) {
        auto dirExes = deriveExeNames(entry.dirName);
        for (auto& e : dirExes) {
            if (std::find(fp.exeNames.begin(), fp.exeNames.end(), e) == fp.exeNames.end())
                fp.exeNames.push_back(std::move(e));
        }
    }

    // 2. Derive keyFiles from install steps dest paths
    std::set<std::string> keySet;
    for (const auto& step : entry.installSteps) {
        if (step.action == "copyDir" || step.action == "copy" || step.action == "copyFile") {
            std::string top = extractTopDir(step.dest);
            if (!top.empty()) keySet.insert(top);
        }
    }
    // Also check install options steps
    for (const auto& opt : entry.installOptions) {
        for (const auto& step : opt.steps) {
            std::string top = extractTopDir(step.dest);
            if (!top.empty()) keySet.insert(top);
        }
    }
    fp.keyFiles.assign(keySet.begin(), keySet.end());

    // 3. Engine hint
    fp.engineHint = normalizeEngine(entry.engine);

    entry.fingerprint = std::move(fp);
}

std::vector<FingerprintMatch> PackageCatalog::findMatchingGames(
    const std::vector<std::string>& exeNames,
    const std::string& engine,
    const std::vector<std::string>& topEntries,
    const std::string& folderName) const
{
    const std::string engineHint = normalizeEngine(engine);

    // Build a lowercase set of top-level entries for fast lookup
    std::set<std::string> topSet;
    for (const auto& e : topEntries) topSet.insert(toLower(e));

    // Build lowercase set of exe names
    std::set<std::string> exeSet;
    for (const auto& e : exeNames) exeSet.insert(toLower(e));

    const std::string folderLower = toLower(trim(folderName));

    std::vector<FingerprintMatch> results;
    results.reserve(packages_.size());

    for (const auto& [appId, entry] : packages_) {
        if (!entry.fingerprint.has_value()) continue;
        const auto& fp = entry.fingerprint.value();

        int score = 0;
        std::string matchedBy;

        // Tier 1: Exe name matching (+60 exact, +40 gameName-derived)
        bool exeExact = false;
        for (const auto& expected : fp.exeNames) {
            if (exeSet.count(expected)) {
                exeExact = true;
                break;
            }
        }

        if (exeExact) {
            score += 60;
            matchedBy = "exeName";
        } else {
            // Check if any game exe matches gameName-derived pattern
            std::string nameExe = toLower(entry.gameName) + ".exe";
            if (exeSet.count(nameExe)) {
                score += 40;
                matchedBy = "gameNameExe";
            }
        }

        // Tier 2: Key file/dir matching (+25 all, +15 partial)
        if (!fp.keyFiles.empty()) {
            int matched = 0;
            for (const auto& kf : fp.keyFiles) {
                if (topSet.count(toLower(kf))) matched++;
            }

            const double ratio = static_cast<double>(matched) / static_cast<double>(fp.keyFiles.size());
            if (ratio >= 1.0) {
                score += 25;
                matchedBy += matchedBy.empty() ? "keyFiles" : "+keyFiles";
            } else if (ratio > 0.5) {
                score += 15;
                matchedBy += matchedBy.empty() ? "keyFilesPartial" : "+keyFilesPartial";
            }
        }

        // Tier 3: Engine cross-check (+15 match, -20 contradiction)
        if (!fp.engineHint.empty() && !engineHint.empty()) {
            if (fp.engineHint == engineHint) {
                score += 15;
                matchedBy += matchedBy.empty() ? "engine" : "+engine";
            } else if (fp.engineHint != "custom" && engineHint != "custom" && engineHint != "unknown") {
                // Engine contradiction: reduce penalty (was -20, now -10)
                // Some games report different engines depending on launcher version
                score -= 10;
            }
        }

        // Bonus: folder name match (+10)
        if (!folderLower.empty()) {
            const std::string dirLower = toLower(entry.dirName);
            const std::string nameLower = toLower(entry.gameName);
            if (folderLower == dirLower || folderLower == nameLower) {
                score += 10;
                matchedBy += matchedBy.empty() ? "folderName" : "+folderName";
            }
        }

        if (score > 0) {
            results.push_back({appId, std::min(score, 100), matchedBy});
        }
    }

    // Sort by confidence descending
    std::sort(results.begin(), results.end(),
              [](const FingerprintMatch& a, const FingerprintMatch& b) {
                  return a.confidence > b.confidence;
              });

    // Return top 5 at most
    if (results.size() > 5)
        results.resize(5);

    return results;
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
        MAKINE_LOG_WARN(log::PACKAGE, "Cannot open installed state file: {}",
                          statePath.string());
        return;
    }

    json doc;
    try {
        doc = json::parse(file);
    } catch (const json::parse_error& e) {
        MAKINE_LOG_WARN(log::PACKAGE, "Installed state parse error: {}", e.what());
        return;
    }

    if (!doc.is_object()) return;

    for (auto it = doc.begin(); it != doc.end(); ++it) {
        InstalledPackageState state;

        if (it.value().is_string()) {
            // Legacy format: steamAppId -> version string
            state.version = it.value().get<std::string>();
        } else if (it.value().is_object()) {
            // New format: steamAppId -> { version, gamePath, files, installedAt, ... }
            const auto& obj = it.value();
            state.version           = obj.value("version", "");
            state.gamePath          = obj.value("gamePath", "");
            state.installedAt       = obj.value("installedAt", static_cast<int64_t>(0));
            state.gameStoreVersion  = obj.value("gameStoreVersion", "");
            state.gameSource        = obj.value("gameSource", "");

            auto loadStringArray = [&](const char* key, std::vector<std::string>& out) {
                if (obj.contains(key) && obj[key].is_array()) {
                    for (const auto& f : obj[key]) {
                        if (f.is_string()) out.push_back(f.get<std::string>());
                    }
                }
            };

            loadStringArray("files", state.installedFiles);
            loadStringArray("addedFiles", state.addedFiles);
            loadStringArray("replacedFiles", state.replacedFiles);
        } else {
            continue; // Skip unknown value types
        }

        installed_[it.key()] = std::move(state);
    }

    MAKINE_LOG_DEBUG(log::PACKAGE, "Loaded installed state: {} entries from {}",
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
            MAKINE_LOG_WARN(log::PACKAGE, "Cannot create directory for installed state: {}",
                              ec.message());
        }
    }

    json root = json::object();
    for (const auto& [appId, state] : installed_) {
        json obj = json::object();
        obj["version"]     = state.version;
        obj["gamePath"]    = state.gamePath;
        obj["installedAt"] = state.installedAt;

        if (!state.gameStoreVersion.empty())
            obj["gameStoreVersion"] = state.gameStoreVersion;
        if (!state.gameSource.empty())
            obj["gameSource"] = state.gameSource;

        auto saveStringArray = [&](const char* key, const std::vector<std::string>& arr) {
            if (!arr.empty()) {
                json jsonArr = json::array();
                for (const auto& f : arr) jsonArr.push_back(f);
                obj[key] = std::move(jsonArr);
            }
        };

        saveStringArray("files", state.installedFiles);
        saveStringArray("addedFiles", state.addedFiles);
        saveStringArray("replacedFiles", state.replacedFiles);

        root[appId] = std::move(obj);
    }

    std::string data = root.dump(2);

    // Atomic write: write to temp file, then rename
    fs::path tempPath = statePath;
    tempPath += ".tmp";

    {
        std::ofstream file(tempPath, std::ios::trunc);
        if (!file.is_open()) {
            MAKINE_LOG_ERROR(log::PACKAGE, "Cannot write installed state file: {}",
                               statePath.string());
            return;
        }
        file << data;
        file.flush();

        if (!file.good()) {
            file.close();
            fs::remove(tempPath, ec);
            MAKINE_LOG_ERROR(log::PACKAGE, "Installed state write failed (flush error)");
            return;
        }
    } // file closed here

    fs::rename(tempPath, statePath, ec);
    if (ec) {
        // Rename failed — try to clean up temp file
        fs::remove(tempPath, ec);
        MAKINE_LOG_ERROR(log::PACKAGE, "Installed state atomic rename failed: {}",
                           ec.message());
        return;
    }

    MAKINE_LOG_DEBUG(log::PACKAGE, "Saved installed state: {} entries to {}",
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

std::unordered_map<std::string, std::string> PackageCatalog::getAllExeMap() const
{
    std::unordered_map<std::string, std::string> map;
    for (const auto& [appId, entry] : packages_) {
        if (entry.fingerprint) {
            for (const auto& exe : entry.fingerprint->exeNames) {
                map[exe] = appId;
            }
        }
    }
    return map;
}

} // namespace packages
} // namespace makine
