/**
 * @file update_detection.cpp
 * @brief Game update detection implementation — pure C++
 * @copyright (c) 2026 MakineAI Team
 *
 * Extracted from qml/src/services/updatedetectionservice.cpp.
 * No Qt dependency — uses std::filesystem, OpenSSL, nlohmann::json, and VDF parser.
 */

#include "makineai/update_detection.hpp"
#include "makineai/logging.hpp"
#include "makineai/vdf_parser.hpp"

#include <nlohmann/json.hpp>
#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <fstream>
#include <memory>
#include <set>
#include <sstream>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace makineai::update {

namespace {

// =========================================================================
// INTERNAL HELPERS
// =========================================================================

// RAII wrapper for OpenSSL EVP_MD_CTX (same pattern as file_integrity module)
struct EvpCtxDeleter {
    void operator()(EVP_MD_CTX* ctx) const noexcept {
        if (ctx) EVP_MD_CTX_free(ctx);
    }
};
using EvpCtxPtr = std::unique_ptr<EVP_MD_CTX, EvpCtxDeleter>;

// Convert raw hash bytes to lowercase hex string
std::string bytesToHex(const unsigned char* data, unsigned int len) {
    static constexpr char hexChars[] = "0123456789abcdef";
    std::string result;
    result.reserve(len * 2);
    for (unsigned int i = 0; i < len; ++i) {
        result.push_back(hexChars[(data[i] >> 4) & 0x0F]);
        result.push_back(hexChars[data[i] & 0x0F]);
    }
    return result;
}

// Case-insensitive string contains check
bool containsCI(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) return true;
    if (haystack.size() < needle.size()) return false;

    auto toLower = [](char c) -> char {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    };

    auto it = std::search(
        haystack.begin(), haystack.end(),
        needle.begin(), needle.end(),
        [&](char a, char b) { return toLower(a) == toLower(b); }
    );
    return it != haystack.end();
}

// Case-insensitive string comparison
bool equalsCI(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

// Case-insensitive string starts-with check
bool startsWithCI(std::string_view str, std::string_view prefix) {
    if (str.size() < prefix.size()) return false;
    return equalsCI(str.substr(0, prefix.size()), prefix);
}

// Check if a relative path falls under an ignored directory
bool isIgnoredDir(std::string_view relPath,
                  const std::vector<std::string>& ignoredDirs) {
    for (const auto& ignored : ignoredDirs) {
        // Check exact match or prefix match with '/'
        if (equalsCI(relPath, ignored)) {
            return true;
        }
        std::string prefix = ignored + "/";
        if (startsWithCI(relPath, prefix)) {
            return true;
        }
    }
    return false;
}

// Simple glob matching: supports "*" prefix (e.g., "*.pak") and exact match
// nameFilter examples: "*.pak", "*.dll", "globalgamemanagers", "*"
bool matchesNameFilter(std::string_view filename, std::string_view filter) {
    if (filter == "*") return true;

    // Extension filter: "*.ext"
    if (filter.size() >= 2 && filter[0] == '*' && filter[1] == '.') {
        std::string_view ext = filter.substr(1); // includes the dot
        if (filename.size() < ext.size()) return false;

        // Case-insensitive extension comparison
        std::string_view fileSuffix = filename.substr(filename.size() - ext.size());
        return equalsCI(fileSuffix, ext);
    }

    // Exact match (case-insensitive)
    return equalsCI(filename, filter);
}

// Check if a directory entry name ends with "_Data" (case-insensitive)
bool endsWithData(const std::string& name) {
    if (name.size() < 5) return false;
    return equalsCI(std::string_view(name).substr(name.size() - 5), "_Data");
}

// Get relative path from base directory
std::string getRelativePath(const fs::path& basePath, const fs::path& fullPath) {
    auto rel = fs::relative(fullPath, basePath);
    return rel.generic_string();  // forward slashes for consistency
}

// Read entire file contents into a string
std::string readFileContents(const fs::path& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) return {};

    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

// Get file mtime as seconds since epoch
int64_t getFileMtime(const fs::path& filePath) {
    std::error_code ec;
    auto ftime = fs::last_write_time(filePath, ec);
    if (ec) return 0;

    // Convert file_time to system_clock time point
    auto sctp = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
    return std::chrono::duration_cast<std::chrono::seconds>(
        sctp.time_since_epoch()
    ).count();
}

// Get file size
int64_t getFileSize(const fs::path& filePath) {
    std::error_code ec;
    auto sz = fs::file_size(filePath, ec);
    if (ec) return 0;
    return static_cast<int64_t>(sz);
}

// Get current time as seconds since epoch
int64_t currentTimeSecs() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

#ifdef _WIN32
// Convert UTF-8 string to wide string
std::wstring utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return {};

    int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
        static_cast<int>(utf8.size()), nullptr, 0);
    if (wideLen <= 0) return {};

    std::wstring wide(static_cast<size_t>(wideLen), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
        static_cast<int>(utf8.size()), wide.data(), wideLen);
    return wide;
}

// Read string value from Windows registry
std::string readRegistryStringValue(HKEY hKeyRoot,
                                     const std::wstring& subKey,
                                     const std::wstring& valueName) {
    HKEY hKey = nullptr;
    LONG result = RegOpenKeyExW(hKeyRoot, subKey.c_str(), 0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) {
        return {};
    }

    wchar_t buffer[MAX_PATH * 2] = {0};
    DWORD bufferSize = sizeof(buffer);
    DWORD type = 0;

    result = RegQueryValueExW(hKey, valueName.c_str(), nullptr, &type,
        reinterpret_cast<LPBYTE>(buffer), &bufferSize);
    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS || type != REG_SZ) {
        return {};
    }

    // Convert wide string to UTF-8
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, buffer, -1,
        nullptr, 0, nullptr, nullptr);
    if (utf8Len <= 0) {
        return {};
    }

    std::string utf8Str(static_cast<size_t>(utf8Len - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, buffer, -1,
        utf8Str.data(), utf8Len, nullptr, nullptr);
    return utf8Str;
}
#endif // _WIN32

} // anonymous namespace

// =============================================================================
// ENGINE PROFILES
// =============================================================================

EngineProfile profileForEngine(std::string_view engine) {
    using Rule = EngineProfile::Rule;

    if (containsCI(engine, "unity")) {
        return {
            {
                Rule{"", "globalgamemanagers", false},   // *_Data/ resolved at collection time
                Rule{"", "GameAssembly.dll", false},
                Rule{"", "UnityPlayer.dll", false},
                Rule{"", "*.assets", true},              // *_Data/**/*.assets
                Rule{"", "*.dll", false},                // Managed DLLs in *_Data/Managed/
            },
            {"StreamingAssets", "Saves", "Temp", "Logs"},
            80
        };
    }
    if (containsCI(engine, "unreal")) {
        return {
            {
                Rule{"Content/Paks", "*.pak", false},
                Rule{"Content/Localization", "*", true},
                Rule{"", "*.exe", false},
            },
            {"Saved", "Config", "Logs", "CrashReportClient"},
            50
        };
    }
    if (containsCI(engine, "renpy") || containsCI(engine, "ren'py")) {
        return {
            {
                Rule{"game", "*.rpa", false},
                Rule{"game", "*.rpy", false},
                Rule{"game", "*.rpyc", false},
            },
            {"saves", "tmp", "cache"},
            100
        };
    }
    if (containsCI(engine, "rpgmaker") || containsCI(engine, "rpg maker")) {
        return {
            {
                Rule{"www/data", "*.json", false},
                Rule{"www/js", "*.js", false},
                Rule{"data", "*.rvdata2", false},
                Rule{"data", "*.json", false},
            },
            {"save", "www/save"},
            100
        };
    }
    if (containsCI(engine, "bethesda") || containsCI(engine, "creation")) {
        return {
            {
                Rule{"Data", "*.esm", false},
                Rule{"Data", "*.esp", false},
                Rule{"Data/Strings", "*", false},
            },
            {"Saves", "Data/SKSE", "Data/F4SE"},
            60
        };
    }
    if (containsCI(engine, "gamemaker") || containsCI(engine, "game maker")) {
        return {
            {
                Rule{"", "data.win", false},
                Rule{"", "*.exe", false},
            },
            {"saves"},
            10
        };
    }
    if (containsCI(engine, "godot")) {
        return {
            {
                Rule{"", "*.pck", false},
                Rule{"", "*.exe", false},
            },
            {"logs"},
            10
        };
    }

    // Other engines - minimal tracking
    return {
        {
            Rule{"", "*.exe", false},
            Rule{"", "*.dll", false},
        },
        {"logs", "saves", "config", "cache"},
        10
    };
}

// =============================================================================
// FILE COLLECTION
// =============================================================================

std::vector<std::string> collectFiles(const fs::path& installPath,
                                       const EngineProfile& profile) {
    std::set<std::string> result;  // sorted, unique

    std::error_code ec;
    if (!fs::exists(installPath, ec) || !fs::is_directory(installPath, ec)) {
        return {};
    }

    for (const auto& rule : profile.rules) {
        fs::path searchDir;
        if (rule.directory.empty()) {
            searchDir = installPath;
        } else {
            searchDir = installPath / rule.directory;
        }

        // Unity special case: empty directory + specific Unity filters
        // need to search *_Data subdirectories
        if (rule.directory.empty() &&
            (rule.nameFilter == "globalgamemanagers" || rule.nameFilter == "*.assets")) {

            // Find *_Data directories in install root
            for (const auto& entry : fs::directory_iterator(installPath, ec)) {
                if (ec) break;
                if (!entry.is_directory(ec)) continue;

                std::string dirName = entry.path().filename().string();
                if (!endsWithData(dirName)) continue;

                fs::path dataDir = entry.path();

                if (rule.nameFilter == "globalgamemanagers") {
                    // Look for globalgamemanagers directly in *_Data/
                    fs::path target = dataDir / "globalgamemanagers";
                    if (fs::exists(target, ec)) {
                        std::string relPath = getRelativePath(installPath, target);
                        if (!isIgnoredDir(relPath, profile.ignoredDirs)) {
                            result.insert(relPath);
                        }
                    }
                } else {
                    // *.assets — search in _Data and optionally subdirectories
                    if (rule.recurse) {
                        for (const auto& fileEntry :
                             fs::recursive_directory_iterator(dataDir, ec)) {
                            if (ec) break;
                            if (!fileEntry.is_regular_file(ec)) continue;
                            std::string fname = fileEntry.path().filename().string();
                            if (matchesNameFilter(fname, rule.nameFilter)) {
                                std::string relPath = getRelativePath(installPath, fileEntry.path());
                                if (!isIgnoredDir(relPath, profile.ignoredDirs)) {
                                    result.insert(relPath);
                                }
                            }
                        }
                    } else {
                        for (const auto& fileEntry :
                             fs::directory_iterator(dataDir, ec)) {
                            if (ec) break;
                            if (!fileEntry.is_regular_file(ec)) continue;
                            std::string fname = fileEntry.path().filename().string();
                            if (matchesNameFilter(fname, rule.nameFilter)) {
                                std::string relPath = getRelativePath(installPath, fileEntry.path());
                                if (!isIgnoredDir(relPath, profile.ignoredDirs)) {
                                    result.insert(relPath);
                                }
                            }
                        }
                    }
                }
            }
            ec.clear();
            continue;
        }

        // Unity special case: empty directory + *.dll
        // check root-level DLLs AND *_Data/Managed/*.dll
        if (rule.directory.empty() && rule.nameFilter == "*.dll") {
            // Root-level DLLs
            for (const auto& entry : fs::directory_iterator(installPath, ec)) {
                if (ec) break;
                if (!entry.is_regular_file(ec)) continue;
                std::string fname = entry.path().filename().string();
                if (matchesNameFilter(fname, "*.dll")) {
                    std::string relPath = getRelativePath(installPath, entry.path());
                    result.insert(relPath);
                }
            }
            ec.clear();

            // Also check *_Data/Managed/
            for (const auto& entry : fs::directory_iterator(installPath, ec)) {
                if (ec) break;
                if (!entry.is_directory(ec)) continue;

                std::string dirName = entry.path().filename().string();
                if (!endsWithData(dirName)) continue;

                fs::path managedDir = entry.path() / "Managed";
                if (!fs::exists(managedDir, ec)) continue;

                for (const auto& mEntry : fs::directory_iterator(managedDir, ec)) {
                    if (ec) break;
                    if (!mEntry.is_regular_file(ec)) continue;
                    std::string mfname = mEntry.path().filename().string();
                    if (matchesNameFilter(mfname, "*.dll")) {
                        std::string relPath = getRelativePath(installPath, mEntry.path());
                        result.insert(relPath);
                    }
                }
            }
            ec.clear();
            continue;
        }

        // General case: search searchDir with the filter
        if (!fs::exists(searchDir, ec) || !fs::is_directory(searchDir, ec)) {
            continue;
        }

        if (rule.recurse) {
            for (const auto& entry : fs::recursive_directory_iterator(searchDir, ec)) {
                if (ec) break;
                if (!entry.is_regular_file(ec)) continue;

                std::string fname = entry.path().filename().string();
                if (matchesNameFilter(fname, rule.nameFilter)) {
                    std::string relPath = getRelativePath(installPath, entry.path());
                    if (!isIgnoredDir(relPath, profile.ignoredDirs)) {
                        result.insert(relPath);
                    }
                }
            }
        } else {
            for (const auto& entry : fs::directory_iterator(searchDir, ec)) {
                if (ec) break;
                if (!entry.is_regular_file(ec)) continue;

                std::string fname = entry.path().filename().string();
                if (matchesNameFilter(fname, rule.nameFilter)) {
                    std::string relPath = getRelativePath(installPath, entry.path());
                    if (!isIgnoredDir(relPath, profile.ignoredDirs)) {
                        result.insert(relPath);
                    }
                }
            }
        }
        ec.clear();
    }

    // Convert to vector (already sorted by std::set)
    std::vector<std::string> sorted(result.begin(), result.end());

    // Limit to maxFiles
    if (static_cast<int>(sorted.size()) > profile.maxFiles) {
        sorted.resize(static_cast<size_t>(profile.maxFiles));
    }

    return sorted;
}

// =============================================================================
// FILE HASHING
// =============================================================================

std::string computeFileHash(const fs::path& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        MAKINEAI_LOG_WARN(log::VERSION, "Cannot open file for hashing: {}", filePath.string());
        return {};
    }

    EvpCtxPtr ctx(EVP_MD_CTX_new());
    if (!ctx) {
        MAKINEAI_LOG_ERROR(log::VERSION, "Failed to create EVP_MD_CTX");
        return {};
    }

    if (EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1) {
        MAKINEAI_LOG_ERROR(log::VERSION, "EVP_DigestInit_ex failed");
        return {};
    }

    constexpr size_t chunkSize = 65536;  // 64KB
    std::vector<char> buffer(chunkSize);

    while (file.read(buffer.data(), static_cast<std::streamsize>(chunkSize)) || file.gcount() > 0) {
        auto bytesRead = file.gcount();
        if (bytesRead <= 0) break;

        if (EVP_DigestUpdate(ctx.get(), buffer.data(), static_cast<size_t>(bytesRead)) != 1) {
            MAKINEAI_LOG_ERROR(log::VERSION, "EVP_DigestUpdate failed for: {}", filePath.string());
            return {};
        }
    }

    if (file.bad()) {
        MAKINEAI_LOG_ERROR(log::VERSION, "I/O error reading file: {}", filePath.string());
        return {};
    }

    std::array<unsigned char, EVP_MAX_MD_SIZE> hash{};
    unsigned int hashLen = 0;
    if (EVP_DigestFinal_ex(ctx.get(), hash.data(), &hashLen) != 1) {
        MAKINEAI_LOG_ERROR(log::VERSION, "EVP_DigestFinal_ex failed for: {}", filePath.string());
        return {};
    }

    return bytesToHex(hash.data(), hashLen);
}

std::vector<FileHashRecord> hashGameFiles(const fs::path& installPath,
                                           const EngineProfile& profile) {
    std::vector<FileHashRecord> records;

    std::error_code ec;
    if (!fs::exists(installPath, ec) || !fs::is_directory(installPath, ec)) {
        return records;
    }

    auto matchingFiles = collectFiles(installPath, profile);
    records.reserve(matchingFiles.size());

    for (const auto& relPath : matchingFiles) {
        fs::path absPath = installPath / relPath;

        FileHashRecord rec;
        rec.relativePath = relPath;
        rec.fileSize = getFileSize(absPath);
        rec.lastModified = getFileMtime(absPath);
        rec.sha256 = computeFileHash(absPath);
        records.push_back(std::move(rec));
    }

    return records;
}

// =============================================================================
// STORE METADATA READERS (Tier 1)
// =============================================================================

std::string readSteamBuildId(const fs::path& installPath,
                              const std::string& steamAppId) {
    std::error_code ec;

    // Navigate up 2 dirs: common/{gameName} -> common -> steamapps
    fs::path steamappsPath = installPath.parent_path().parent_path();
    if (steamappsPath.empty() || !fs::exists(steamappsPath, ec)) {
        MAKINEAI_LOG_DEBUG(log::VERSION, "Cannot find steamapps directory from: {}",
                           installPath.string());
        return {};
    }

    // Direct lookup: appmanifest_{appId}.acf
    if (!steamAppId.empty()) {
        fs::path acfPath = steamappsPath / ("appmanifest_" + steamAppId + ".acf");
        std::string content = readFileContents(acfPath);
        if (!content.empty()) {
            auto root = vdf::parse(content);
            if (root) {
                const auto* appState = root->find("AppState");
                if (!appState) appState = &(*root);
                std::string buildId = appState->getString("buildid");
                if (!buildId.empty()) {
                    MAKINEAI_LOG_DEBUG(log::VERSION, "Steam buildid={} for appId={}",
                                       buildId, steamAppId);
                    return buildId;
                }
            }
        }
    }

    // Fallback: scan all ACFs matching installdir
    std::string dirName = installPath.filename().string();

    for (const auto& entry : fs::directory_iterator(steamappsPath, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;

        std::string fname = entry.path().filename().string();
        // Match appmanifest_*.acf
        if (fname.size() < 16) continue;  // "appmanifest_.acf" = 16 chars
        if (!startsWithCI(fname, "appmanifest_")) continue;
        if (!matchesNameFilter(fname, "*.acf")) continue;

        std::string content = readFileContents(entry.path());
        if (content.empty()) continue;

        auto root = vdf::parse(content);
        if (!root) continue;

        const auto* appState = root->find("AppState");
        if (!appState) appState = &(*root);

        std::string installDir = appState->getString("installdir");
        if (equalsCI(installDir, dirName)) {
            std::string buildId = appState->getString("buildid");
            MAKINEAI_LOG_DEBUG(log::VERSION, "Steam buildid={} (fallback scan) for dir={}",
                               buildId, dirName);
            return buildId;
        }
    }

    MAKINEAI_LOG_DEBUG(log::VERSION, "No Steam buildid found for: {}", installPath.string());
    return {};
}

std::string readEpicVersion(const fs::path& installPath) {
    const fs::path manifestDir = "C:/ProgramData/Epic/EpicGamesLauncher/Data/Manifests";

    std::error_code ec;
    if (!fs::exists(manifestDir, ec) || !fs::is_directory(manifestDir, ec)) {
        return {};
    }

    // Normalize install path for comparison
    fs::path cleanPath = fs::weakly_canonical(installPath, ec);
    if (ec) cleanPath = installPath;

    for (const auto& entry : fs::directory_iterator(manifestDir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;

        std::string fname = entry.path().filename().string();
        if (!matchesNameFilter(fname, "*.item")) continue;

        std::string content = readFileContents(entry.path());
        if (content.empty()) continue;

        try {
            auto j = nlohmann::json::parse(content);

            std::string installLoc = j.value("InstallLocation", "");
            if (installLoc.empty()) continue;

            fs::path manifestInstallPath = fs::weakly_canonical(installLoc, ec);
            if (ec) {
                ec.clear();
                manifestInstallPath = installLoc;
            }

            // Case-insensitive path comparison
            if (equalsCI(manifestInstallPath.generic_string(), cleanPath.generic_string())) {
                std::string version = j.value("AppVersionString", "");
                MAKINEAI_LOG_DEBUG(log::VERSION, "Epic version={} for: {}",
                                   version, installPath.string());
                return version;
            }
        } catch (const nlohmann::json::exception& e) {
            MAKINEAI_LOG_TRACE(log::VERSION, "Failed to parse Epic manifest {}: {}",
                               entry.path().string(), e.what());
            continue;
        }
    }

    MAKINEAI_LOG_DEBUG(log::VERSION, "No Epic version found for: {}", installPath.string());
    return {};
}

std::string readGogVersion(const std::string& gameId) {
    // Only process GOG game IDs
    if (gameId.size() <= 4 || gameId.substr(0, 4) != "gog_") {
        return {};
    }
    std::string gogKey = gameId.substr(4);

#ifdef _WIN32
    std::wstring subKey = L"SOFTWARE\\WOW6432Node\\GOG.com\\Games\\" + utf8ToWide(gogKey);

    // Try "ver" first
    std::string ver = readRegistryStringValue(HKEY_LOCAL_MACHINE, subKey, L"ver");
    if (!ver.empty()) {
        MAKINEAI_LOG_DEBUG(log::VERSION, "GOG ver={} for: {}", ver, gameId);
        return ver;
    }

    // Fallback to "buildId"
    ver = readRegistryStringValue(HKEY_LOCAL_MACHINE, subKey, L"buildId");
    if (!ver.empty()) {
        MAKINEAI_LOG_DEBUG(log::VERSION, "GOG buildId={} for: {}", ver, gameId);
        return ver;
    }

    MAKINEAI_LOG_DEBUG(log::VERSION, "No GOG version found for: {}", gameId);
#else
    MAKINEAI_LOG_DEBUG(log::VERSION, "GOG registry not available on non-Windows platform");
#endif

    return {};
}

int64_t readExeMtime(const fs::path& installPath) {
    std::error_code ec;
    if (!fs::exists(installPath, ec) || !fs::is_directory(installPath, ec)) {
        return 0;
    }

    int64_t latestMtime = 0;

    for (const auto& entry : fs::directory_iterator(installPath, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;

        std::string fname = entry.path().filename().string();
        if (!matchesNameFilter(fname, "*.exe")) continue;

        int64_t mtime = getFileMtime(entry.path());
        if (mtime > latestMtime) {
            latestMtime = mtime;
        }
    }

    return latestMtime;
}

// =============================================================================
// SNAPSHOT MANAGEMENT
// =============================================================================

void saveSnapshot(const fs::path& dataDir, const GameSnapshot& snapshot) {
    fs::path snapshotDir = dataDir / "snapshots";

    std::error_code ec;
    fs::create_directories(snapshotDir, ec);
    if (ec) {
        MAKINEAI_LOG_ERROR(log::VERSION, "Failed to create snapshot directory: {}",
                           snapshotDir.string());
        return;
    }

    nlohmann::json root;
    root["gameId"] = snapshot.gameId;
    root["patchVersion"] = snapshot.patchVersion;
    root["takenAt"] = snapshot.takenAt;

    nlohmann::json filesArr = nlohmann::json::array();
    for (const auto& f : snapshot.files) {
        nlohmann::json fObj;
        fObj["path"] = f.relativePath;
        fObj["sha256"] = f.sha256;
        fObj["size"] = f.fileSize;
        fObj["mtime"] = f.lastModified;
        filesArr.push_back(std::move(fObj));
    }
    root["files"] = std::move(filesArr);

    fs::path filePath = snapshotDir / (snapshot.gameId + ".json");
    std::ofstream file(filePath);
    if (file) {
        file << root.dump();  // compact JSON (no indentation)
        MAKINEAI_LOG_INFO(log::VERSION, "Saved snapshot for {} ({} files)",
                          snapshot.gameId, snapshot.files.size());
    } else {
        MAKINEAI_LOG_ERROR(log::VERSION, "Failed to write snapshot: {}", filePath.string());
    }
}

std::optional<GameSnapshot> loadSnapshot(const fs::path& dataDir,
                                          const std::string& gameId) {
    fs::path filePath = dataDir / "snapshots" / (gameId + ".json");

    std::string content = readFileContents(filePath);
    if (content.empty()) {
        return std::nullopt;
    }

    try {
        auto root = nlohmann::json::parse(content);

        GameSnapshot snapshot;
        snapshot.gameId = root.value("gameId", "");
        snapshot.patchVersion = root.value("patchVersion", "");
        snapshot.takenAt = root.value("takenAt", static_cast<int64_t>(0));

        if (root.contains("files") && root["files"].is_array()) {
            const auto& filesArr = root["files"];
            snapshot.files.reserve(filesArr.size());

            for (const auto& fObj : filesArr) {
                FileHashRecord rec;
                rec.relativePath = fObj.value("path", "");
                rec.sha256 = fObj.value("sha256", "");
                rec.fileSize = fObj.value("size", static_cast<int64_t>(0));
                rec.lastModified = fObj.value("mtime", static_cast<int64_t>(0));
                snapshot.files.push_back(std::move(rec));
            }
        }

        MAKINEAI_LOG_DEBUG(log::VERSION, "Loaded snapshot for {} ({} files)",
                           gameId, snapshot.files.size());
        return snapshot;

    } catch (const nlohmann::json::exception& e) {
        MAKINEAI_LOG_ERROR(log::VERSION, "Failed to parse snapshot {}: {}",
                           filePath.string(), e.what());
        return std::nullopt;
    }
}

bool hasSnapshot(const fs::path& dataDir, const std::string& gameId) {
    std::error_code ec;
    return fs::exists(dataDir / "snapshots" / (gameId + ".json"), ec);
}

void removeSnapshot(const fs::path& dataDir, const std::string& gameId) {
    fs::path filePath = dataDir / "snapshots" / (gameId + ".json");
    std::error_code ec;
    if (fs::remove(filePath, ec)) {
        MAKINEAI_LOG_INFO(log::VERSION, "Removed snapshot for {}", gameId);
    }
}

// =============================================================================
// COMPATIBILITY CHECKING (Tier 2)
// =============================================================================

CompatibilityResult checkCompatibility(const GameSnapshot& snapshot,
                                        const fs::path& installPath,
                                        std::string_view engine) {
    CompatibilityResult result;
    result.level = "unknown";
    result.integrityPercent = 100;
    result.summary = "No translation snapshot available yet";

    if (snapshot.files.empty()) {
        return result;
    }

    std::error_code ec;
    if (!fs::exists(installPath, ec) || !fs::is_directory(installPath, ec)) {
        result.level = "unknown";
        result.summary = "Game install path not found";
        return result;
    }

    int unchanged = 0;
    int modified = 0;
    int removed = 0;

    for (const auto& savedFile : snapshot.files) {
        fs::path absPath = installPath / savedFile.relativePath;

        if (!fs::exists(absPath, ec)) {
            ++removed;
            continue;
        }

        // mtime + size pre-filter (fast path)
        int64_t currentMtime = getFileMtime(absPath);
        int64_t currentSize = getFileSize(absPath);

        if (currentMtime == savedFile.lastModified && currentSize == savedFile.fileSize) {
            ++unchanged;
            continue;
        }

        // mtime changed — compute hash for definitive check
        std::string currentHash = computeFileHash(absPath);
        if (currentHash == savedFile.sha256) {
            ++unchanged;
        } else {
            ++modified;
        }
    }

    // Check for new files using engine profile
    EngineProfile profile = profileForEngine(engine);

    std::set<std::string> snapshotPaths;
    for (const auto& f : snapshot.files) {
        snapshotPaths.insert(f.relativePath);
    }

    auto currentFiles = collectFiles(installPath, profile);
    int added = 0;
    for (const auto& f : currentFiles) {
        if (snapshotPaths.find(f) == snapshotPaths.end()) {
            ++added;
        }
    }

    int total = static_cast<int>(snapshot.files.size());
    int integrityPercent = total > 0
        ? static_cast<int>(std::round(100.0 * unchanged / total))
        : 100;

    result.integrityPercent = integrityPercent;
    result.modifiedCount = modified;
    result.addedCount = added;
    result.removedCount = removed;

    if (modified == 0 && removed == 0 && added == 0) {
        result.level = "compatible";
        result.summary = "All " + std::to_string(total) +
                          " file(s) match the translation snapshot";
    } else if (integrityPercent >= 80) {
        result.level = "partial";
        result.summary = std::to_string(unchanged) + " of " + std::to_string(total) +
                          " files unchanged. Translation may need minor updates.";
    } else {
        result.level = "incompatible";
        result.summary = "Significant changes detected. Translation likely needs updating.";
    }

    MAKINEAI_LOG_INFO(log::VERSION,
        "Compatibility check: level={}, integrity={}%, modified={}, added={}, removed={}",
        result.level, result.integrityPercent,
        result.modifiedCount, result.addedCount, result.removedCount);

    return result;
}

// =============================================================================
// STORE VERSION PERSISTENCE
// =============================================================================

void saveStoreVersions(
    const fs::path& dataDir,
    const std::unordered_map<std::string, StoreVersionRecord>& versions) {

    std::error_code ec;
    fs::create_directories(dataDir, ec);
    if (ec) {
        MAKINEAI_LOG_ERROR(log::VERSION, "Failed to create data directory: {}",
                           dataDir.string());
        return;
    }

    nlohmann::json root = nlohmann::json::object();

    for (const auto& [gameId, rec] : versions) {
        nlohmann::json obj;
        obj["steamBuildId"] = rec.steamBuildId;
        obj["epicVersionString"] = rec.epicVersionString;
        obj["gogBuildId"] = rec.gogBuildId;
        obj["exeLastModified"] = rec.exeLastModified;
        obj["recordedAt"] = rec.recordedAt;
        root[gameId] = std::move(obj);
    }

    fs::path filePath = dataDir / "store_versions.json";
    std::ofstream file(filePath);
    if (file) {
        file << root.dump();  // compact JSON
        MAKINEAI_LOG_DEBUG(log::VERSION, "Saved {} store version records",
                           versions.size());
    } else {
        MAKINEAI_LOG_ERROR(log::VERSION, "Failed to write store versions: {}",
                           filePath.string());
    }
}

std::unordered_map<std::string, StoreVersionRecord> loadStoreVersions(
    const fs::path& dataDir) {

    std::unordered_map<std::string, StoreVersionRecord> versions;

    fs::path filePath = dataDir / "store_versions.json";
    std::string content = readFileContents(filePath);
    if (content.empty()) {
        return versions;
    }

    try {
        auto root = nlohmann::json::parse(content);
        if (!root.is_object()) {
            MAKINEAI_LOG_WARN(log::VERSION, "store_versions.json is not a JSON object");
            return versions;
        }

        for (auto it = root.begin(); it != root.end(); ++it) {
            const auto& obj = it.value();
            if (!obj.is_object()) continue;

            StoreVersionRecord rec;
            rec.gameId = it.key();
            rec.steamBuildId = obj.value("steamBuildId", "");
            rec.epicVersionString = obj.value("epicVersionString", "");
            rec.gogBuildId = obj.value("gogBuildId", "");
            rec.exeLastModified = obj.value("exeLastModified", static_cast<int64_t>(0));
            rec.recordedAt = obj.value("recordedAt", static_cast<int64_t>(0));
            versions[it.key()] = std::move(rec);
        }

        MAKINEAI_LOG_DEBUG(log::VERSION, "Loaded {} store version records",
                           versions.size());

    } catch (const nlohmann::json::exception& e) {
        MAKINEAI_LOG_ERROR(log::VERSION, "Failed to parse store_versions.json: {}", e.what());
    }

    return versions;
}

} // namespace makineai::update
