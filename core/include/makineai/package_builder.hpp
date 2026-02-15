/**
 * @file package_builder.hpp
 * @brief Translation package (.mkpkg) builder, validator, and inspector
 * @copyright (c) 2026 MakineAI Team
 *
 * Creates, validates, signs, and inspects .mkpkg translation packages.
 * Format: ZIP archive containing manifest.json, translation files, and optional signature.
 */

#pragma once

#include "types.hpp"
#include "error.hpp"

#include <memory>

namespace makineai {

// =============================================================================
// Package Build Manifest
// =============================================================================

/**
 * @brief Translator credit entry
 */
struct TranslatorCredit {
    std::string name;
    std::string discord;
    uint32_t stringCount{0};
};

/**
 * @brief Source file entry in the package
 */
struct PackageFileEntry {
    fs::path sourcePath;            // Absolute path on disk
    fs::path archivePath;           // Relative path in the archive
    std::string fileType;           // "unity_asset", "renpy_script", "binary", etc.
    uint64_t fileSize{0};
    std::string sha256;             // Computed during build
};

/**
 * @brief Full metadata for building a translation package
 *
 * This is the detailed manifest the builder uses. It maps to the
 * manifest.json inside the .mkpkg archive.
 */
struct PackageBuildManifest {
    // Identity
    std::string name;               // e.g. "hollow-knight-tr"
    std::string version;            // Semantic version e.g. "2.1.0"
    std::string displayName;        // e.g. "Hollow Knight Türkçe Yama"

    // Target game
    std::string gameName;           // e.g. "Hollow Knight"
    std::string gameId;             // Internal game ID
    uint32_t steamAppId{0};
    GameEngine engine{GameEngine::Unknown};
    std::string engineVersion;
    StringList supportedGameVersions;
    StringList supportedGameHashes;

    // Translation info
    std::string sourceLanguage{"en"};
    std::string targetLanguage{"tr"};
    uint32_t totalStrings{0};
    uint32_t translatedStrings{0};

    // Credits
    std::vector<TranslatorCredit> translators;

    // Changelog (markdown)
    std::string changelog;

    // Files to include
    std::vector<PackageFileEntry> files;

    // Dependencies
    bool requiresRuntime{false};    // Needs BepInEx/XUnity?
    std::string runtimeVersion;

    // Build metadata (auto-filled)
    uint64_t builtAt{0};
    std::string builderVersion;

    /// Translation completion percentage
    [[nodiscard]] double completionPercent() const {
        if (totalStrings == 0) return 0.0;
        return (static_cast<double>(translatedStrings) / totalStrings) * 100.0;
    }
};

// =============================================================================
// Build Options
// =============================================================================

/**
 * @brief Compression method for the package
 */
enum class PackageCompression {
    Store,      // No compression (fastest build, largest file)
    Deflate,    // Standard ZIP deflate (good balance)
    Zstd        // Zstandard (best compression, requires modern tooling)
};

/**
 * @brief Options controlling how the package is built
 */
struct PackageBuildOptions {
    PackageCompression compression{PackageCompression::Deflate};
    int compressionLevel{6};        // 1-9 for Deflate, 1-22 for Zstd
    bool includeChangelog{true};
    bool validateBeforeBuild{true};
    bool signAfterBuild{false};
    fs::path signingKeyPath;        // Ed25519 private key path
    fs::path outputPath;            // Output .mkpkg path (auto-generated if empty)
    fs::path outputDir;             // Output directory (default: current dir)
};

// =============================================================================
// Build Result
// =============================================================================

/**
 * @brief Result of a successful package build
 */
struct PackageBuildResult {
    fs::path outputPath;
    uint64_t packageSize{0};
    std::string sha256;
    uint32_t fileCount{0};
    bool signed_{false};
    std::vector<std::string> warnings;
};

// =============================================================================
// Validation
// =============================================================================

/**
 * @brief Severity level for validation findings
 */
enum class ValidationSeverity {
    Info,       // Informational note
    Warning,    // Potential issue, package still valid
    Error       // Must fix before building
};

/**
 * @brief Single validation finding
 */
struct ValidationFinding {
    ValidationSeverity severity;
    std::string category;   // "encoding", "placeholder", "version", "manifest", "file"
    std::string message;
    std::string file;       // Affected file (optional)
    uint32_t line{0};       // Line number (optional)
};

/**
 * @brief Full validation result
 */
struct PackageValidationResult {
    bool valid{true};       // No errors (warnings are OK)
    std::vector<ValidationFinding> findings;

    [[nodiscard]] size_t errorCount() const;
    [[nodiscard]] size_t warningCount() const;
    [[nodiscard]] std::string summary() const;
};

// =============================================================================
// Inspection
// =============================================================================

/**
 * @brief Result of inspecting a .mkpkg file
 */
struct PackageInspectResult {
    PackageBuildManifest manifest;
    std::vector<PackageFileEntry> files;
    uint64_t packageSize{0};
    std::string packageHash;
    bool hasSigned{false};
    bool signatureValid{false};
    std::string signedBy;
};

// =============================================================================
// Diff
// =============================================================================

/**
 * @brief File change type between two package versions
 */
enum class DiffChangeType {
    Added,
    Removed,
    Modified,
    Unchanged
};

/**
 * @brief Single file change between two package versions
 */
struct PackageDiffEntry {
    DiffChangeType type;
    fs::path path;
    uint64_t oldSize{0};
    uint64_t newSize{0};
    std::string oldHash;
    std::string newHash;
};

/**
 * @brief Result of comparing two packages
 */
struct PackageDiffResult {
    std::string oldVersion;
    std::string newVersion;
    std::vector<PackageDiffEntry> changes;
    int32_t stringsAdded{0};
    int32_t stringsRemoved{0};
    int32_t stringsModified{0};

    [[nodiscard]] std::string summary() const;
};

// =============================================================================
// PackageBuilder
// =============================================================================

/**
 * @brief Creates, validates, signs, and inspects .mkpkg translation packages
 *
 * The .mkpkg format is a ZIP archive containing:
 * - manifest.json  — Package metadata
 * - files/         — Translation files (paths preserved)
 * - changelog.md   — Optional changelog
 * - signature.json — Optional Ed25519 signature
 *
 * Usage:
 * @code
 * PackageBuilder builder;
 *
 * // Build from manifest
 * PackageBuildManifest manifest;
 * manifest.name = "hollow-knight-tr";
 * manifest.version = "2.1.0";
 * // ... fill other fields
 *
 * auto result = builder.build(manifest, options);
 * if (result) {
 *     // result->outputPath has the .mkpkg file
 * }
 *
 * // Inspect existing package
 * auto info = builder.inspect("package.mkpkg");
 *
 * // Validate before building
 * auto validation = builder.validate(manifest);
 * if (!validation.valid) { ... fix issues ... }
 * @endcode
 */
class PackageBuilder {
public:
    PackageBuilder();
    ~PackageBuilder();

    /**
     * @brief Build a .mkpkg package from manifest and files
     */
    [[nodiscard]] Result<PackageBuildResult> build(
        const PackageBuildManifest& manifest,
        const PackageBuildOptions& options = {}
    );

    /**
     * @brief Validate a package manifest and its files
     *
     * Checks:
     * - Manifest completeness (required fields)
     * - Semantic versioning format
     * - File existence and readability
     * - UTF-8 encoding of text files
     * - Placeholder consistency (if source/target pairs available)
     */
    [[nodiscard]] PackageValidationResult validate(
        const PackageBuildManifest& manifest
    );

    /**
     * @brief Inspect an existing .mkpkg file
     */
    [[nodiscard]] Result<PackageInspectResult> inspect(
        const fs::path& packagePath
    );

    /**
     * @brief Sign a .mkpkg file with Ed25519 key
     * @param packagePath Path to the .mkpkg file
     * @param privateKeyPath Path to Ed25519 private key (PEM)
     */
    [[nodiscard]] VoidResult sign(
        const fs::path& packagePath,
        const fs::path& privateKeyPath
    );

    /**
     * @brief Verify signature of a .mkpkg file
     * @param packagePath Path to the .mkpkg file
     * @param publicKeyPath Path to Ed25519 public key (PEM)
     */
    [[nodiscard]] Result<SignatureResult> verifySignature(
        const fs::path& packagePath,
        const fs::path& publicKeyPath
    );

    /**
     * @brief Compare two .mkpkg packages
     */
    [[nodiscard]] Result<PackageDiffResult> diff(
        const fs::path& oldPackage,
        const fs::path& newPackage
    );

    /**
     * @brief Initialize a new package project directory
     *
     * Creates a template directory structure:
     *   project/
     *     manifest.yaml
     *     files/
     *     README.md
     */
    [[nodiscard]] VoidResult initProject(
        const fs::path& directory,
        const std::string& gameName,
        GameEngine engine = GameEngine::Unknown
    );

    /**
     * @brief Load manifest from YAML file
     */
    [[nodiscard]] Result<PackageBuildManifest> loadManifestYaml(
        const fs::path& yamlPath
    );

private:
    // Archive operations
    [[nodiscard]] VoidResult createArchive(
        const fs::path& outputPath,
        const PackageBuildManifest& manifest,
        const PackageBuildOptions& options
    );

    [[nodiscard]] Result<std::string> readArchiveEntry(
        const fs::path& archivePath,
        const std::string& entryName
    );

    [[nodiscard]] Result<std::vector<PackageFileEntry>> listArchiveEntries(
        const fs::path& archivePath
    );

    // Manifest serialization
    [[nodiscard]] std::string manifestToJson(const PackageBuildManifest& manifest);
    [[nodiscard]] Result<PackageBuildManifest> parseManifestJson(const std::string& json);

    // Validation helpers
    void validateManifest(const PackageBuildManifest& manifest, PackageValidationResult& result);
    void validateFiles(const PackageBuildManifest& manifest, PackageValidationResult& result);
    void validateEncoding(const fs::path& filePath, PackageValidationResult& result);
    [[nodiscard]] bool isValidSemver(const std::string& version) const;
    [[nodiscard]] bool isValidUtf8(const fs::path& filePath) const;
};

} // namespace makineai
