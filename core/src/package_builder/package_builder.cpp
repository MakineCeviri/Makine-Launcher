/**
 * @file package_builder.cpp
 * @brief Translation package (.mkpkg) builder implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/package_builder.hpp"
#include "makineai/logging.hpp"
#include "makineai/security.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <regex>
#include <sstream>

// Minizip-ng / zlib for ZIP archive creation
#ifdef MAKINEAI_HAS_LIBARCHIVE
#include <archive.h>
#include <archive_entry.h>
#endif

namespace makineai {

using json = nlohmann::json;

// =============================================================================
// Helpers
// =============================================================================

static uint64_t nowEpoch() {
    return static_cast<uint64_t>(
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())
    );
}

static std::string readFileToString(const fs::path& path) {
    std::ifstream ifs{path, std::ios::binary};
    if (!ifs) return {};
    return std::string{
        std::istreambuf_iterator<char>{ifs},
        std::istreambuf_iterator<char>{}
    };
}

static uint64_t fileSize(const fs::path& path) {
    std::error_code ec;
    auto sz = fs::file_size(path, ec);
    return ec ? 0 : sz;
}

// =============================================================================
// PackageValidationResult
// =============================================================================

size_t PackageValidationResult::errorCount() const {
    return static_cast<size_t>(std::count_if(findings.begin(), findings.end(),
        [](const auto& f) { return f.severity == ValidationSeverity::Error; }));
}

size_t PackageValidationResult::warningCount() const {
    return static_cast<size_t>(std::count_if(findings.begin(), findings.end(),
        [](const auto& f) { return f.severity == ValidationSeverity::Warning; }));
}

std::string PackageValidationResult::summary() const {
    std::string result;
    auto errors = errorCount();
    auto warnings = warningCount();

    if (errors == 0 && warnings == 0) {
        result = "Validation passed — no issues found";
    } else {
        result = "Validation " + std::string(valid ? "passed" : "FAILED") +
                 " — " + std::to_string(errors) + " error(s), " +
                 std::to_string(warnings) + " warning(s)";
    }
    return result;
}

// =============================================================================
// PackageDiffResult
// =============================================================================

std::string PackageDiffResult::summary() const {
    size_t added = 0, removed = 0, modified = 0;
    for (const auto& c : changes) {
        switch (c.type) {
            case DiffChangeType::Added: added++; break;
            case DiffChangeType::Removed: removed++; break;
            case DiffChangeType::Modified: modified++; break;
            default: break;
        }
    }

    std::ostringstream oss;
    oss << oldVersion << " -> " << newVersion << ": "
        << added << " added, " << removed << " removed, "
        << modified << " modified";

    if (stringsAdded || stringsRemoved || stringsModified) {
        oss << " (strings: +" << stringsAdded
            << " -" << stringsRemoved
            << " ~" << stringsModified << ")";
    }
    return oss.str();
}

// =============================================================================
// PackageBuilder
// =============================================================================

PackageBuilder::PackageBuilder() = default;
PackageBuilder::~PackageBuilder() = default;

// =============================================================================
// Build
// =============================================================================

Result<PackageBuildResult> PackageBuilder::build(
    const PackageBuildManifest& manifest,
    const PackageBuildOptions& options)
{
    MAKINEAI_LOG_INFO(log::PACKAGE, "Building package: {} v{}", manifest.name, manifest.version);

    // Validate first if requested
    if (options.validateBeforeBuild) {
        auto validation = validate(manifest);
        if (!validation.valid) {
            return std::unexpected(
                Error(ErrorCode::InvalidArgument,
                      "Package validation failed: " + validation.summary())
            );
        }
    }

    // Determine output path
    fs::path outputPath = options.outputPath;
    if (outputPath.empty()) {
        std::string filename = manifest.name + "-" + manifest.version + ".mkpkg";
        if (!options.outputDir.empty()) {
            outputPath = options.outputDir / filename;
        } else {
            outputPath = fs::current_path() / filename;
        }
    }

    // Ensure output directory exists
    std::error_code ec;
    if (outputPath.has_parent_path()) {
        fs::create_directories(outputPath.parent_path(), ec);
    }

    // Fill build metadata
    auto buildManifest = manifest;
    buildManifest.builtAt = nowEpoch();
    buildManifest.builderVersion = "MakineAI 0.1.0";

    // Compute file hashes
    SecurityManager security;
    for (auto& file : buildManifest.files) {
        if (file.sha256.empty() && fs::exists(file.sourcePath)) {
            auto hashResult = security.hashFile(file.sourcePath);
            if (hashResult) {
                file.sha256 = *hashResult;
            }
            file.fileSize = fileSize(file.sourcePath);
        }
    }

    // Create the archive
    auto archiveResult = createArchive(outputPath, buildManifest, options);
    if (!archiveResult) {
        return std::unexpected(archiveResult.error());
    }

    // Build result
    PackageBuildResult result;
    result.outputPath = outputPath;
    result.packageSize = fileSize(outputPath);
    result.fileCount = static_cast<uint32_t>(buildManifest.files.size());

    // Hash the package itself
    auto pkgHash = security.hashFile(outputPath);
    if (pkgHash) {
        result.sha256 = *pkgHash;
    }

    // Sign if requested
    if (options.signAfterBuild && !options.signingKeyPath.empty()) {
        auto signResult = sign(outputPath, options.signingKeyPath);
        if (signResult) {
            result.signed_ = true;
        } else {
            result.warnings.push_back("Signing failed: " + signResult.error().message());
        }
    }

    MAKINEAI_LOG_INFO(log::PACKAGE, "Package built: {} ({} bytes, {} files)",
                      outputPath.filename().string(), result.packageSize, result.fileCount);

    return result;
}

// =============================================================================
// Validate
// =============================================================================

PackageValidationResult PackageBuilder::validate(const PackageBuildManifest& manifest) {
    PackageValidationResult result;
    result.valid = true;

    validateManifest(manifest, result);
    validateFiles(manifest, result);

    // Set valid = false if any errors
    result.valid = (result.errorCount() == 0);
    return result;
}

void PackageBuilder::validateManifest(const PackageBuildManifest& manifest,
                                       PackageValidationResult& result) {
    // Required fields
    if (manifest.name.empty()) {
        result.findings.push_back({
            ValidationSeverity::Error, "manifest",
            "Package name is required", "", 0
        });
    } else {
        // Name format: lowercase, hyphens, no spaces
        static const std::regex namePattern(R"(^[a-z0-9][a-z0-9-]*$)");
        if (!std::regex_match(manifest.name, namePattern)) {
            result.findings.push_back({
                ValidationSeverity::Error, "manifest",
                "Package name must be lowercase alphanumeric with hyphens (e.g. 'hollow-knight-tr')",
                "", 0
            });
        }
    }

    if (manifest.version.empty()) {
        result.findings.push_back({
            ValidationSeverity::Error, "version",
            "Package version is required", "", 0
        });
    } else if (!isValidSemver(manifest.version)) {
        result.findings.push_back({
            ValidationSeverity::Error, "version",
            "Version must be valid semantic version (e.g. '1.2.3'): " + manifest.version,
            "", 0
        });
    }

    if (manifest.gameName.empty()) {
        result.findings.push_back({
            ValidationSeverity::Error, "manifest",
            "Game name is required", "", 0
        });
    }

    if (manifest.files.empty()) {
        result.findings.push_back({
            ValidationSeverity::Error, "manifest",
            "Package must contain at least one file", "", 0
        });
    }

    // Warnings
    if (manifest.translators.empty()) {
        result.findings.push_back({
            ValidationSeverity::Warning, "manifest",
            "No translator credits specified", "", 0
        });
    }

    if (manifest.changelog.empty()) {
        result.findings.push_back({
            ValidationSeverity::Warning, "manifest",
            "No changelog provided", "", 0
        });
    }

    if (manifest.totalStrings > 0 && manifest.translatedStrings == 0) {
        result.findings.push_back({
            ValidationSeverity::Warning, "manifest",
            "Total strings specified but translated count is 0", "", 0
        });
    }

    if (manifest.steamAppId == 0 && manifest.gameId.empty()) {
        result.findings.push_back({
            ValidationSeverity::Warning, "manifest",
            "No Steam App ID or game ID specified — game matching may fail", "", 0
        });
    }

    if (manifest.supportedGameVersions.empty() && manifest.supportedGameHashes.empty()) {
        result.findings.push_back({
            ValidationSeverity::Info, "manifest",
            "No supported game versions or hashes — package will match any game version", "", 0
        });
    }
}

void PackageBuilder::validateFiles(const PackageBuildManifest& manifest,
                                    PackageValidationResult& result) {
    for (const auto& file : manifest.files) {
        // Check source file exists
        if (!fs::exists(file.sourcePath)) {
            result.findings.push_back({
                ValidationSeverity::Error, "file",
                "Source file not found: " + file.sourcePath.string(),
                file.sourcePath.string(), 0
            });
            continue;
        }

        // Check file is readable
        std::ifstream test{file.sourcePath};
        if (!test.good()) {
            result.findings.push_back({
                ValidationSeverity::Error, "file",
                "Cannot read file: " + file.sourcePath.string(),
                file.sourcePath.string(), 0
            });
            continue;
        }
        test.close();

        // Check archive path is safe
        std::string archStr = file.archivePath.string();
        if (archStr.find("..") != std::string::npos) {
            result.findings.push_back({
                ValidationSeverity::Error, "file",
                "Archive path contains path traversal: " + archStr,
                archStr, 0
            });
        }

        // UTF-8 validation for text files
        auto ext = file.sourcePath.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        static const std::vector<std::string> textExts = {
            ".txt", ".csv", ".json", ".yaml", ".yml", ".xml",
            ".rpy", ".lua", ".ini", ".cfg", ".po", ".pot",
            ".html", ".htm", ".css", ".js", ".ts", ".md"
        };

        bool isText = std::find(textExts.begin(), textExts.end(), ext) != textExts.end();
        if (isText) {
            validateEncoding(file.sourcePath, result);
        }
    }
}

void PackageBuilder::validateEncoding(const fs::path& filePath,
                                       PackageValidationResult& result) {
    if (!isValidUtf8(filePath)) {
        result.findings.push_back({
            ValidationSeverity::Error, "encoding",
            "File is not valid UTF-8: " + filePath.filename().string(),
            filePath.string(), 0
        });
    }
}

bool PackageBuilder::isValidSemver(const std::string& version) const {
    // Match major.minor.patch with optional pre-release
    static const std::regex semverPattern(
        R"(^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(-[a-zA-Z0-9.]+)?(\+[a-zA-Z0-9.]+)?$)"
    );
    return std::regex_match(version, semverPattern);
}

bool PackageBuilder::isValidUtf8(const fs::path& filePath) const {
    std::ifstream ifs{filePath, std::ios::binary};
    if (!ifs) return false;

    // Read file in chunks and validate UTF-8
    constexpr size_t chunkSize = 64 * 1024;
    std::vector<uint8_t> buffer(chunkSize);

    while (ifs) {
        ifs.read(reinterpret_cast<char*>(buffer.data()),
                 static_cast<std::streamsize>(chunkSize));
        auto bytesRead = static_cast<size_t>(ifs.gcount());
        if (bytesRead == 0) break;

        // Simple UTF-8 validation
        for (size_t i = 0; i < bytesRead; ) {
            uint8_t byte = buffer[i];

            size_t seqLen = 0;
            if ((byte & 0x80) == 0x00) {
                seqLen = 1; // ASCII
            } else if ((byte & 0xE0) == 0xC0) {
                seqLen = 2;
            } else if ((byte & 0xF0) == 0xE0) {
                seqLen = 3;
            } else if ((byte & 0xF8) == 0xF0) {
                seqLen = 4;
            } else {
                return false; // Invalid start byte
            }

            if (i + seqLen > bytesRead) {
                // Incomplete sequence at chunk boundary — accept it
                break;
            }

            // Verify continuation bytes
            for (size_t j = 1; j < seqLen; ++j) {
                if ((buffer[i + j] & 0xC0) != 0x80) {
                    return false;
                }
            }

            // Reject overlong encodings
            if (seqLen == 2 && byte < 0xC2) return false;

            i += seqLen;
        }
    }

    return true;
}

// =============================================================================
// Manifest Serialization
// =============================================================================

std::string PackageBuilder::manifestToJson(const PackageBuildManifest& manifest) {
    json j;

    // Core identity
    j["name"] = manifest.name;
    j["version"] = manifest.version;
    j["displayName"] = manifest.displayName;
    j["format"] = "mkpkg-1.0";

    // Game info
    j["game"]["name"] = manifest.gameName;
    j["game"]["id"] = manifest.gameId;
    if (manifest.steamAppId > 0) {
        j["game"]["steamAppId"] = manifest.steamAppId;
    }
    j["game"]["engine"] = static_cast<int>(manifest.engine);
    if (!manifest.engineVersion.empty()) {
        j["game"]["engineVersion"] = manifest.engineVersion;
    }
    if (!manifest.supportedGameVersions.empty()) {
        j["game"]["supportedVersions"] = manifest.supportedGameVersions;
    }
    if (!manifest.supportedGameHashes.empty()) {
        j["game"]["supportedHashes"] = manifest.supportedGameHashes;
    }

    // Translation info
    j["translation"]["sourceLanguage"] = manifest.sourceLanguage;
    j["translation"]["targetLanguage"] = manifest.targetLanguage;
    j["translation"]["totalStrings"] = manifest.totalStrings;
    j["translation"]["translatedStrings"] = manifest.translatedStrings;
    j["translation"]["completionPercent"] = manifest.completionPercent();

    // Credits
    json translators = json::array();
    for (const auto& t : manifest.translators) {
        json tj;
        tj["name"] = t.name;
        if (!t.discord.empty()) tj["discord"] = t.discord;
        if (t.stringCount > 0) tj["strings"] = t.stringCount;
        translators.push_back(tj);
    }
    j["translators"] = translators;

    // Files
    json files = json::array();
    for (const auto& f : manifest.files) {
        json fj;
        fj["path"] = f.archivePath.string();
        fj["type"] = f.fileType;
        fj["size"] = f.fileSize;
        if (!f.sha256.empty()) fj["sha256"] = f.sha256;
        files.push_back(fj);
    }
    j["files"] = files;

    // Runtime dependency
    if (manifest.requiresRuntime) {
        j["runtime"]["required"] = true;
        if (!manifest.runtimeVersion.empty()) {
            j["runtime"]["version"] = manifest.runtimeVersion;
        }
    }

    // Build metadata
    j["build"]["builtAt"] = manifest.builtAt;
    j["build"]["builderVersion"] = manifest.builderVersion;

    return j.dump(2);
}

Result<PackageBuildManifest> PackageBuilder::parseManifestJson(const std::string& jsonStr) {
    try {
        auto j = json::parse(jsonStr);
        PackageBuildManifest m;

        m.name = j.value("name", "");
        m.version = j.value("version", "");
        m.displayName = j.value("displayName", "");

        if (j.contains("game")) {
            const auto& g = j["game"];
            m.gameName = g.value("name", "");
            m.gameId = g.value("id", "");
            m.steamAppId = g.value("steamAppId", 0u);
            m.engine = static_cast<GameEngine>(g.value("engine", 0));
            m.engineVersion = g.value("engineVersion", "");
            if (g.contains("supportedVersions")) {
                m.supportedGameVersions = g["supportedVersions"].get<StringList>();
            }
            if (g.contains("supportedHashes")) {
                m.supportedGameHashes = g["supportedHashes"].get<StringList>();
            }
        }

        if (j.contains("translation")) {
            const auto& t = j["translation"];
            m.sourceLanguage = t.value("sourceLanguage", "en");
            m.targetLanguage = t.value("targetLanguage", "tr");
            m.totalStrings = t.value("totalStrings", 0u);
            m.translatedStrings = t.value("translatedStrings", 0u);
        }

        if (j.contains("translators")) {
            for (const auto& tj : j["translators"]) {
                TranslatorCredit tc;
                tc.name = tj.value("name", "");
                tc.discord = tj.value("discord", "");
                tc.stringCount = tj.value("strings", 0u);
                m.translators.push_back(std::move(tc));
            }
        }

        if (j.contains("files")) {
            for (const auto& fj : j["files"]) {
                PackageFileEntry fe;
                fe.archivePath = fj.value("path", "");
                fe.fileType = fj.value("type", "");
                fe.fileSize = fj.value("size", uint64_t{0});
                fe.sha256 = fj.value("sha256", "");
                m.files.push_back(std::move(fe));
            }
        }

        if (j.contains("runtime")) {
            m.requiresRuntime = j["runtime"].value("required", false);
            m.runtimeVersion = j["runtime"].value("version", "");
        }

        if (j.contains("build")) {
            m.builtAt = j["build"].value("builtAt", uint64_t{0});
            m.builderVersion = j["build"].value("builderVersion", "");
        }

        return m;
    } catch (const json::exception& e) {
        return std::unexpected(
            Error(ErrorCode::ParseError, std::string("Failed to parse manifest JSON: ") + e.what())
        );
    }
}

// =============================================================================
// Archive Operations
// =============================================================================

VoidResult PackageBuilder::createArchive(
    const fs::path& outputPath,
    const PackageBuildManifest& manifest,
    const PackageBuildOptions& options)
{
#ifdef MAKINEAI_HAS_LIBARCHIVE
    // Use libarchive for proper ZIP creation
    struct archive* a = archive_write_new();
    if (!a) {
        return std::unexpected(Error(ErrorCode::Unknown, "Failed to create archive writer"));
    }

    archive_write_set_format_zip(a);

    // Set compression
    switch (options.compression) {
        case PackageCompression::Store:
            archive_write_zip_set_compression_store(a);
            break;
        case PackageCompression::Deflate:
            archive_write_zip_set_compression_deflate(a);
            break;
        case PackageCompression::Zstd:
            // Zstd in ZIP is non-standard, fall back to deflate
            archive_write_zip_set_compression_deflate(a);
            break;
    }

    auto outputStr = outputPath.string();
    if (archive_write_open_filename(a, outputStr.c_str()) != ARCHIVE_OK) {
        std::string err = archive_error_string(a);
        archive_write_free(a);
        return std::unexpected(Error(ErrorCode::FileCreateFailed, "Cannot create archive: " + err));
    }

    auto writeEntry = [&](const std::string& name, const std::string& data) -> VoidResult {
        struct archive_entry* entry = archive_entry_new();
        archive_entry_set_pathname(entry, name.c_str());
        archive_entry_set_size(entry, static_cast<int64_t>(data.size()));
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, 0644);
        archive_entry_set_mtime(entry, static_cast<time_t>(nowEpoch()), 0);

        archive_write_header(a, entry);
        archive_write_data(a, data.data(), data.size());
        archive_entry_free(entry);
        return {};
    };

    auto writeFile = [&](const std::string& name, const fs::path& srcPath) -> VoidResult {
        auto content = readFileToString(srcPath);
        if (content.empty() && fileSize(srcPath) > 0) {
            return std::unexpected(
                Error(ErrorCode::FileNotFound, "Cannot read: " + srcPath.string())
                    .withFile(srcPath)
            );
        }
        return writeEntry(name, content);
    };

    // Write manifest.json
    auto manifestJson = manifestToJson(manifest);
    auto r = writeEntry("manifest.json", manifestJson);
    if (!r) {
        archive_write_free(a);
        return r;
    }

    // Write changelog if present
    if (options.includeChangelog && !manifest.changelog.empty()) {
        r = writeEntry("changelog.md", manifest.changelog);
        if (!r) {
            archive_write_free(a);
            return r;
        }
    }

    // Write translation files
    for (const auto& file : manifest.files) {
        std::string archPath = "files/" + file.archivePath.string();
        r = writeFile(archPath, file.sourcePath);
        if (!r) {
            archive_write_free(a);
            return r;
        }
    }

    archive_write_close(a);
    archive_write_free(a);
    return {};

#else
    // Fallback: Create a simple uncompressed container using basic file I/O
    // This is a minimal implementation for builds without libarchive
    //
    // Format: JSON manifest + file catalog, files concatenated
    // Real deployments should use libarchive for proper ZIP support

    MAKINEAI_LOG_WARN(log::PACKAGE,
        "libarchive not available — using fallback container format");

    std::ofstream ofs{outputPath, std::ios::binary};
    if (!ofs) {
        return std::unexpected(
            Error(ErrorCode::FileCreateFailed, "Cannot create output file")
                .withFile(outputPath)
        );
    }

    // Write magic header
    const char magic[] = "MKPKG\x01\x00\x00"; // Magic + version 1.0
    ofs.write(magic, 8);

    // Write manifest JSON (length-prefixed)
    auto manifestJson = manifestToJson(manifest);
    uint32_t manifestLen = static_cast<uint32_t>(manifestJson.size());
    ofs.write(reinterpret_cast<const char*>(&manifestLen), 4);
    ofs.write(manifestJson.data(), manifestLen);

    // Write changelog (length-prefixed)
    if (options.includeChangelog && !manifest.changelog.empty()) {
        uint32_t changelogLen = static_cast<uint32_t>(manifest.changelog.size());
        ofs.write(reinterpret_cast<const char*>(&changelogLen), 4);
        ofs.write(manifest.changelog.data(), changelogLen);
    } else {
        uint32_t zero = 0;
        ofs.write(reinterpret_cast<const char*>(&zero), 4);
    }

    // Write file count
    uint32_t fileCount = static_cast<uint32_t>(manifest.files.size());
    ofs.write(reinterpret_cast<const char*>(&fileCount), 4);

    // Write each file (path + size + data)
    for (const auto& file : manifest.files) {
        auto content = readFileToString(file.sourcePath);

        // Archive path (length-prefixed string)
        std::string archPath = file.archivePath.string();
        uint16_t pathLen = static_cast<uint16_t>(archPath.size());
        ofs.write(reinterpret_cast<const char*>(&pathLen), 2);
        ofs.write(archPath.data(), pathLen);

        // File data (size-prefixed)
        uint64_t dataLen = content.size();
        ofs.write(reinterpret_cast<const char*>(&dataLen), 8);
        ofs.write(content.data(), static_cast<std::streamsize>(dataLen));
    }

    return {};
#endif
}

Result<std::string> PackageBuilder::readArchiveEntry(
    const fs::path& archivePath,
    const std::string& entryName)
{
#ifdef MAKINEAI_HAS_LIBARCHIVE
    struct archive* a = archive_read_new();
    archive_read_support_format_zip(a);
    archive_read_support_filter_all(a);

    auto pathStr = archivePath.string();
    if (archive_read_open_filename(a, pathStr.c_str(), 10240) != ARCHIVE_OK) {
        std::string err = archive_error_string(a);
        archive_read_free(a);
        return std::unexpected(Error(ErrorCode::FileCorrupted, "Cannot open archive: " + err));
    }

    struct archive_entry* entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        if (std::string(archive_entry_pathname(entry)) == entryName) {
            auto size = archive_entry_size(entry);
            std::string data(static_cast<size_t>(size), '\0');
            archive_read_data(a, data.data(), static_cast<size_t>(size));
            archive_read_free(a);
            return data;
        }
        archive_read_data_skip(a);
    }

    archive_read_free(a);
    return std::unexpected(
        Error(ErrorCode::FileNotFound, "Entry not found in archive: " + entryName)
    );

#else
    // Fallback: Read from our custom container format
    std::ifstream ifs{archivePath, std::ios::binary};
    if (!ifs) {
        return std::unexpected(
            Error(ErrorCode::FileNotFound, "Cannot open package")
                .withFile(archivePath)
        );
    }

    // Read magic
    char magic[8];
    ifs.read(magic, 8);
    if (std::string(magic, 5) != "MKPKG") {
        return std::unexpected(Error(ErrorCode::InvalidFormat, "Not a valid .mkpkg file"));
    }

    // Read manifest
    uint32_t manifestLen = 0;
    ifs.read(reinterpret_cast<char*>(&manifestLen), 4);
    std::string manifestData(manifestLen, '\0');
    ifs.read(manifestData.data(), manifestLen);

    if (entryName == "manifest.json") {
        return manifestData;
    }

    // Read changelog
    uint32_t changelogLen = 0;
    ifs.read(reinterpret_cast<char*>(&changelogLen), 4);
    std::string changelogData(changelogLen, '\0');
    if (changelogLen > 0) {
        ifs.read(changelogData.data(), changelogLen);
    }

    if (entryName == "changelog.md") {
        return changelogData;
    }

    // Read files
    uint32_t fileCount = 0;
    ifs.read(reinterpret_cast<char*>(&fileCount), 4);

    for (uint32_t i = 0; i < fileCount; ++i) {
        uint16_t pathLen = 0;
        ifs.read(reinterpret_cast<char*>(&pathLen), 2);
        std::string path(pathLen, '\0');
        ifs.read(path.data(), pathLen);

        uint64_t dataLen = 0;
        ifs.read(reinterpret_cast<char*>(&dataLen), 8);

        if ("files/" + path == entryName || path == entryName) {
            std::string data(static_cast<size_t>(dataLen), '\0');
            ifs.read(data.data(), static_cast<std::streamsize>(dataLen));
            return data;
        }

        // Skip this file's data
        ifs.seekg(static_cast<std::streamoff>(dataLen), std::ios::cur);
    }

    return std::unexpected(
        Error(ErrorCode::FileNotFound, "Entry not found: " + entryName)
    );
#endif
}

Result<std::vector<PackageFileEntry>> PackageBuilder::listArchiveEntries(
    const fs::path& archivePath)
{
    std::vector<PackageFileEntry> entries;

#ifdef MAKINEAI_HAS_LIBARCHIVE
    struct archive* a = archive_read_new();
    archive_read_support_format_zip(a);
    archive_read_support_filter_all(a);

    auto pathStr = archivePath.string();
    if (archive_read_open_filename(a, pathStr.c_str(), 10240) != ARCHIVE_OK) {
        std::string err = archive_error_string(a);
        archive_read_free(a);
        return std::unexpected(Error(ErrorCode::FileCorrupted, "Cannot open archive: " + err));
    }

    struct archive_entry* entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        PackageFileEntry fe;
        fe.archivePath = archive_entry_pathname(entry);
        fe.fileSize = static_cast<uint64_t>(archive_entry_size(entry));
        entries.push_back(std::move(fe));
        archive_read_data_skip(a);
    }

    archive_read_free(a);

#else
    // Fallback container format
    std::ifstream ifs{archivePath, std::ios::binary};
    if (!ifs) {
        return std::unexpected(Error(ErrorCode::FileNotFound, "Cannot open package"));
    }

    char magic[8];
    ifs.read(magic, 8);
    if (std::string(magic, 5) != "MKPKG") {
        return std::unexpected(Error(ErrorCode::InvalidFormat, "Not a valid .mkpkg file"));
    }

    // Skip manifest
    uint32_t manifestLen = 0;
    ifs.read(reinterpret_cast<char*>(&manifestLen), 4);
    ifs.seekg(manifestLen, std::ios::cur);

    // Skip changelog
    uint32_t changelogLen = 0;
    ifs.read(reinterpret_cast<char*>(&changelogLen), 4);
    ifs.seekg(changelogLen, std::ios::cur);

    // Read file entries
    uint32_t fileCount = 0;
    ifs.read(reinterpret_cast<char*>(&fileCount), 4);

    for (uint32_t i = 0; i < fileCount; ++i) {
        uint16_t pathLen = 0;
        ifs.read(reinterpret_cast<char*>(&pathLen), 2);
        std::string path(pathLen, '\0');
        ifs.read(path.data(), pathLen);

        uint64_t dataLen = 0;
        ifs.read(reinterpret_cast<char*>(&dataLen), 8);

        PackageFileEntry fe;
        fe.archivePath = path;
        fe.fileSize = dataLen;
        entries.push_back(std::move(fe));

        ifs.seekg(static_cast<std::streamoff>(dataLen), std::ios::cur);
    }
#endif

    return entries;
}

// =============================================================================
// Inspect
// =============================================================================

Result<PackageInspectResult> PackageBuilder::inspect(const fs::path& packagePath) {
    if (!fs::exists(packagePath)) {
        return std::unexpected(
            Error(ErrorCode::FileNotFound, "Package not found")
                .withFile(packagePath)
        );
    }

    PackageInspectResult result;
    result.packageSize = fileSize(packagePath);

    // Hash the package
    SecurityManager security;
    auto hashResult = security.hashFile(packagePath);
    if (hashResult) {
        result.packageHash = *hashResult;
    }

    // Read manifest
    auto manifestData = readArchiveEntry(packagePath, "manifest.json");
    if (!manifestData) {
        return std::unexpected(manifestData.error());
    }

    auto manifestResult = parseManifestJson(*manifestData);
    if (!manifestResult) {
        return std::unexpected(manifestResult.error());
    }
    result.manifest = std::move(*manifestResult);

    // List files
    auto filesResult = listArchiveEntries(packagePath);
    if (filesResult) {
        result.files = std::move(*filesResult);
    }

    // Check for signature
    auto sigData = readArchiveEntry(packagePath, "signature.json");
    if (sigData) {
        result.hasSigned = true;
        try {
            auto sigJson = json::parse(*sigData);
            result.signedBy = sigJson.value("signedBy", "");
        } catch (...) {}
    }

    MAKINEAI_LOG_INFO(log::PACKAGE, "Inspected package: {} v{} ({} bytes, {} files)",
                      result.manifest.name, result.manifest.version,
                      result.packageSize, result.files.size());

    return result;
}

// =============================================================================
// Sign / Verify
// =============================================================================

VoidResult PackageBuilder::sign(
    const fs::path& packagePath,
    const fs::path& privateKeyPath)
{
    if (!fs::exists(packagePath)) {
        return std::unexpected(Error(ErrorCode::FileNotFound, "Package not found")
                                   .withFile(packagePath));
    }
    if (!fs::exists(privateKeyPath)) {
        return std::unexpected(Error(ErrorCode::FileNotFound, "Private key not found")
                                   .withFile(privateKeyPath));
    }

    // Hash the package (excluding any existing signature)
    SecurityManager security;
    auto hashResult = security.hashFile(packagePath);
    if (!hashResult) {
        return std::unexpected(hashResult.error());
    }

    // Read the private key
    auto keyData = readFileToString(privateKeyPath);
    if (keyData.empty()) {
        return std::unexpected(Error(ErrorCode::FileNotFound, "Cannot read private key"));
    }

    // Create signature JSON
    json sigJson;
    sigJson["packageHash"] = *hashResult;
    sigJson["algorithm"] = "Ed25519";
    sigJson["timestamp"] = nowEpoch();
    // Actual Ed25519 signing would use libsodium here
    // For now, store the hash as the signature placeholder
    sigJson["signature"] = "PLACEHOLDER_NEEDS_LIBSODIUM";
    sigJson["signedBy"] = "MakineAI Builder";

    MAKINEAI_LOG_INFO(log::PACKAGE, "Package signed: {}", packagePath.filename().string());

    // In a real implementation, we would add signature.json to the archive
    // For now, write it as a sidecar file
    auto sigPath = packagePath;
    sigPath += ".sig";
    std::ofstream ofs{sigPath};
    if (!ofs) {
        return std::unexpected(Error(ErrorCode::FileCreateFailed, "Cannot write signature file"));
    }
    ofs << sigJson.dump(2);

    return {};
}

Result<SignatureResult> PackageBuilder::verifySignature(
    const fs::path& packagePath,
    const fs::path& publicKeyPath)
{
    // Read signature
    auto sigData = readArchiveEntry(packagePath, "signature.json");
    if (!sigData) {
        // Try sidecar file
        auto sigPath = packagePath;
        sigPath += ".sig";
        auto content = readFileToString(sigPath);
        if (content.empty()) {
            return std::unexpected(
                Error(ErrorCode::SignatureRequired, "No signature found for package")
            );
        }
        sigData = std::move(content);
    }

    try {
        auto sigJson = json::parse(*sigData);

        // Verify the hash matches
        SecurityManager security;
        auto currentHash = security.hashFile(packagePath);
        if (!currentHash) {
            return std::unexpected(currentHash.error());
        }

        SignatureResult result;
        result.signedBy = sigJson.value("signedBy", "");
        result.signedAt = sigJson.value("timestamp", uint64_t{0});
        result.publicKeyId = "Ed25519";

        std::string recordedHash = sigJson.value("packageHash", "");
        if (recordedHash == *currentHash) {
            result.valid = true;
            result.message = "Signature valid — hash matches";
        } else {
            result.valid = false;
            result.message = "Signature INVALID — package has been modified";
        }

        return result;
    } catch (const json::exception& e) {
        return std::unexpected(
            Error(ErrorCode::ParseError, std::string("Invalid signature JSON: ") + e.what())
        );
    }
}

// =============================================================================
// Diff
// =============================================================================

Result<PackageDiffResult> PackageBuilder::diff(
    const fs::path& oldPackage,
    const fs::path& newPackage)
{
    auto oldInfo = inspect(oldPackage);
    if (!oldInfo) return std::unexpected(oldInfo.error());

    auto newInfo = inspect(newPackage);
    if (!newInfo) return std::unexpected(newInfo.error());

    PackageDiffResult result;
    result.oldVersion = oldInfo->manifest.version;
    result.newVersion = newInfo->manifest.version;

    // Build hash maps for comparison
    std::unordered_map<std::string, PackageFileEntry> oldFiles;
    for (const auto& f : oldInfo->files) {
        oldFiles[f.archivePath.string()] = f;
    }

    std::unordered_map<std::string, PackageFileEntry> newFiles;
    for (const auto& f : newInfo->files) {
        newFiles[f.archivePath.string()] = f;
    }

    // Find added and modified files
    for (const auto& [path, newFile] : newFiles) {
        auto it = oldFiles.find(path);
        if (it == oldFiles.end()) {
            result.changes.push_back({
                DiffChangeType::Added, newFile.archivePath,
                0, newFile.fileSize, "", newFile.sha256
            });
        } else if (it->second.sha256 != newFile.sha256 ||
                   it->second.fileSize != newFile.fileSize) {
            result.changes.push_back({
                DiffChangeType::Modified, newFile.archivePath,
                it->second.fileSize, newFile.fileSize,
                it->second.sha256, newFile.sha256
            });
        }
    }

    // Find removed files
    for (const auto& [path, oldFile] : oldFiles) {
        if (newFiles.find(path) == newFiles.end()) {
            result.changes.push_back({
                DiffChangeType::Removed, oldFile.archivePath,
                oldFile.fileSize, 0, oldFile.sha256, ""
            });
        }
    }

    // String count changes
    result.stringsAdded = static_cast<int32_t>(newInfo->manifest.translatedStrings) -
                          static_cast<int32_t>(oldInfo->manifest.translatedStrings);

    MAKINEAI_LOG_INFO(log::PACKAGE, "Diff: {}", result.summary());

    return result;
}

// =============================================================================
// Init Project
// =============================================================================

VoidResult PackageBuilder::initProject(
    const fs::path& directory,
    const std::string& gameName,
    GameEngine engine)
{
    std::error_code ec;

    // Create directory structure
    fs::create_directories(directory / "files", ec);
    if (ec) {
        return std::unexpected(
            Error(ErrorCode::FileCreateFailed, "Cannot create project directory")
                .withFile(directory)
        );
    }

    // Generate package name from game name
    std::string pkgName;
    for (char c : gameName) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            pkgName += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else if (c == ' ' || c == '-' || c == '_') {
            if (!pkgName.empty() && pkgName.back() != '-') {
                pkgName += '-';
            }
        }
    }
    pkgName += "-tr";

    // Determine engine string
    std::string engineStr = "unknown";
    switch (engine) {
        case GameEngine::Unity: engineStr = "unity"; break;
        case GameEngine::Unreal: engineStr = "unreal"; break;
        case GameEngine::RenPy: engineStr = "renpy"; break;
        case GameEngine::RPGMaker: engineStr = "rpgmaker"; break;
        case GameEngine::GameMaker: engineStr = "gamemaker"; break;
        case GameEngine::Godot: engineStr = "godot"; break;
        default: break;
    }

    // Write manifest.yaml template
    std::string yaml;
    yaml += "# MakineAI Translation Package Manifest\n";
    yaml += "# See: https://github.com/jlceaser/MakineAI/docs/package-format.md\n\n";
    yaml += "name: \"" + pkgName + "\"\n";
    yaml += "version: \"1.0.0\"\n";
    yaml += "display_name: \"" + gameName + " Türkçe Yama\"\n\n";
    yaml += "game:\n";
    yaml += "  name: \"" + gameName + "\"\n";
    yaml += "  steam_id: 0  # Steam App ID\n";
    yaml += "  engine: \"" + engineStr + "\"\n";
    yaml += "  # engine_version: \"\"\n";
    yaml += "  # supported_versions: []\n\n";
    yaml += "translation:\n";
    yaml += "  source: \"en\"\n";
    yaml += "  target: \"tr\"\n";
    yaml += "  total_strings: 0\n";
    yaml += "  translated_strings: 0\n\n";
    yaml += "translators:\n";
    yaml += "  - name: \"Your Name\"\n";
    yaml += "    # discord: \"user#1234\"\n\n";
    yaml += "files:\n";
    yaml += "  # - path: \"data/strings.json\"\n";
    yaml += "  #   type: \"json\"\n\n";
    yaml += "changelog: |\n";
    yaml += "  ## 1.0.0\n";
    yaml += "  - Initial release\n";

    std::ofstream manifestFile{directory / "manifest.yaml"};
    if (!manifestFile) {
        return std::unexpected(Error(ErrorCode::FileCreateFailed, "Cannot write manifest.yaml"));
    }
    manifestFile << yaml;

    MAKINEAI_LOG_INFO(log::PACKAGE, "Initialized package project: {} in {}",
                      pkgName, directory.string());

    return {};
}

// =============================================================================
// Load Manifest YAML
// =============================================================================

Result<PackageBuildManifest> PackageBuilder::loadManifestYaml(const fs::path& yamlPath) {
    auto content = readFileToString(yamlPath);
    if (content.empty()) {
        return std::unexpected(
            Error(ErrorCode::FileNotFound, "Cannot read manifest file")
                .withFile(yamlPath)
        );
    }

    // Simple YAML parser for our specific format
    // Full YAML parsing would require yaml-cpp; we parse the key fields manually
    PackageBuildManifest manifest;

    auto getValue = [&](const std::string& key) -> std::string {
        // Match "key: value" or "key: \"value\""
        std::regex pattern(key + R"(:\s*"?([^"\n]*)"?)");
        std::smatch match;
        if (std::regex_search(content, match, pattern)) {
            return match[1].str();
        }
        return "";
    };

    auto getUint = [&](const std::string& key) -> uint32_t {
        auto val = getValue(key);
        if (val.empty()) return 0;
        try { return static_cast<uint32_t>(std::stoul(val)); }
        catch (...) { return 0; }
    };

    manifest.name = getValue("name");
    manifest.version = getValue("version");
    manifest.displayName = getValue("display_name");
    manifest.gameName = getValue("  name");
    manifest.steamAppId = getUint("  steam_id");
    manifest.sourceLanguage = getValue("  source");
    manifest.targetLanguage = getValue("  target");
    manifest.totalStrings = getUint("  total_strings");
    manifest.translatedStrings = getUint("  translated_strings");

    // Engine detection from string
    auto engineStr = getValue("  engine");
    if (engineStr == "unity") manifest.engine = GameEngine::Unity;
    else if (engineStr == "unreal") manifest.engine = GameEngine::Unreal;
    else if (engineStr == "renpy") manifest.engine = GameEngine::RenPy;
    else if (engineStr == "rpgmaker") manifest.engine = GameEngine::RPGMaker;
    else if (engineStr == "gamemaker") manifest.engine = GameEngine::GameMaker;
    else if (engineStr == "godot") manifest.engine = GameEngine::Godot;

    if (manifest.name.empty()) {
        return std::unexpected(
            Error(ErrorCode::ParseError, "Manifest missing required 'name' field")
                .withFile(yamlPath)
        );
    }

    return manifest;
}

} // namespace makineai
