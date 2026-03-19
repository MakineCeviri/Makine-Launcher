/**
 * @file security.hpp
 * @brief Security, cryptography, and signature verification
 * @copyright (c) 2026 MakineCeviri Team
 */

#pragma once

#include "types.hpp"
#include "error.hpp"

#include <memory>

namespace makine {

/**
 * @brief Hash algorithm types
 */
enum class HashAlgorithm {
    SHA256,
    SHA384,
    SHA512,
    MD5     // Only for legacy compatibility, not security
};

/**
 * @brief Signature verification result
 */
struct SignatureResult {
    bool valid;
    std::string signedBy;       // Certificate subject
    uint64_t signedAt;          // Timestamp
    std::string publicKeyId;    // Which key was used
    std::string message;        // Error message if invalid
};

/**
 * @brief Package signature structure
 */
struct PackageSignature {
    std::string packageHash;    // SHA256 of package
    std::string signature;      // Base64 RSA signature
    std::string publicKeyId;
    uint64_t timestamp;
};

/**
 * @brief Security manager for cryptographic operations
 */
class SecurityManager {
public:
    SecurityManager();
    ~SecurityManager();

    // Hashing
    /**
     * @brief Calculate hash of data
     */
    [[nodiscard]] Result<std::string> hash(
        ByteSpan data,
        HashAlgorithm algo = HashAlgorithm::SHA256
    ) const;

    /**
     * @brief Calculate hash of file
     */
    [[nodiscard]] Result<std::string> hashFile(
        const fs::path& file,
        HashAlgorithm algo = HashAlgorithm::SHA256
    ) const;

    /**
     * @brief Verify data hash
     */
    [[nodiscard]] bool verifyHash(
        ByteSpan data,
        const std::string& expectedHash,
        HashAlgorithm algo = HashAlgorithm::SHA256
    ) const;

    // Signature verification
    /**
     * @brief Load embedded public key (compiled into binary)
     *
     * This is the primary key loading method for production builds.
     * The key is embedded at compile time for tamper resistance.
     */
    [[nodiscard]] VoidResult loadEmbeddedKey();

    /**
     * @brief Load public key for verification (external file)
     */
    [[nodiscard]] VoidResult loadPublicKey(const fs::path& keyPath);

    /**
     * @brief Load public key from PEM string
     */
    [[nodiscard]] VoidResult loadPublicKeyPEM(const std::string& pem);

    /**
     * @brief Verify RSA signature
     */
    [[nodiscard]] Result<SignatureResult> verifySignature(
        ByteSpan data,
        const std::string& signatureBase64
    ) const;

    /**
     * @brief Verify package signature
     */
    [[nodiscard]] Result<SignatureResult> verifyPackageSignature(
        const fs::path& packagePath,
        const PackageSignature& signature
    ) const;

    /**
     * @brief Check if public key is loaded
     */
    [[nodiscard]] bool hasPublicKey() const { return publicKeyLoaded_; }

    // Certificate/code signing verification (Windows)
    /**
     * @brief Verify Windows Authenticode signature
     */
    [[nodiscard]] Result<SignatureResult> verifyAuthenticode(
        const fs::path& exePath
    ) const;

    // Secure random
    /**
     * @brief Generate cryptographically secure random bytes
     */
    [[nodiscard]] Result<ByteBuffer> randomBytes(size_t count) const;

    /**
     * @brief Generate random hex string
     */
    [[nodiscard]] Result<std::string> randomHex(size_t bytes) const;

private:
    bool publicKeyLoaded_ = false;
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief Integrity checker for installed translations
 */
class IntegrityChecker {
public:
    /**
     * @brief Manifest entry for a file
     */
    struct FileEntry {
        fs::path relativePath;
        std::string hash;
        uint64_t size;
        uint64_t modifiedTime;
    };

    /**
     * @brief Create integrity manifest for files
     */
    [[nodiscard]] static Result<std::vector<FileEntry>> createManifest(
        const fs::path& baseDir,
        const StringList& files
    );

    /**
     * @brief Verify files against manifest
     */
    [[nodiscard]] static Result<bool> verify(
        const fs::path& baseDir,
        const std::vector<FileEntry>& manifest,
        StringList* modifiedFiles = nullptr
    );

    /**
     * @brief Save manifest to JSON file
     */
    [[nodiscard]] static VoidResult saveManifest(
        const fs::path& path,
        const std::vector<FileEntry>& manifest
    );

    /**
     * @brief Load manifest from JSON file
     */
    [[nodiscard]] static Result<std::vector<FileEntry>> loadManifest(
        const fs::path& path
    );
};

// =============================================================================
// PATH SECURITY UTILITIES
// =============================================================================

/**
 * @brief Result of path validation
 */
struct PathValidationResult {
    bool valid = false;
    std::string sanitizedPath;
    std::string reason;         // If invalid, why
};

/**
 * @brief Path validation options
 */
struct PathValidationOptions {
    bool allowRelative = false;         // Allow relative paths
    bool allowUncPaths = false;         // Allow UNC paths (\\server\share)
    bool allowSymlinks = true;          // Allow symlinks (if false, resolves real path)
    bool allowTraversal = false;        // Allow .. in path (dangerous!)
    bool allowNullBytes = false;        // Allow null bytes (dangerous!)
    fs::path basePath;                  // Base path for relative resolution
    std::vector<fs::path> allowedDirs;  // If not empty, path must be under one of these
};

/**
 * @brief Path security validator
 *
 * Provides protection against:
 * - Path traversal attacks (../)
 * - Null byte injection
 * - UNC path attacks (\\server)
 * - Symlink attacks (optional)
 * - Escaping allowed directories
 *
 * Usage:
 * @code
 * PathValidator validator;
 * auto result = validator.validate(userInput);
 * if (result.valid) {
 *     // Safe to use result.sanitizedPath
 * }
 * @endcode
 */
class PathValidator {
public:
    /**
     * @brief Validate and sanitize a path
     *
     * @param path Path to validate (can be from untrusted source)
     * @param options Validation options
     * @return PathValidationResult with sanitized path if valid
     */
    [[nodiscard]] static PathValidationResult validate(
        const std::string& path,
        const PathValidationOptions& options = {}
    );

    /**
     * @brief Quick check if path is safe (no sanitization)
     *
     * @param path Path to check
     * @return true if path appears safe
     */
    [[nodiscard]] static bool isSafe(const std::string& path) noexcept;

    /**
     * @brief Check if path contains traversal sequences
     *
     * Detects:
     * - .. (parent directory)
     * - Encoded traversal (%2e%2e, etc.)
     *
     * @param path Path to check
     * @return true if path contains traversal
     */
    [[nodiscard]] static bool hasTraversal(const std::string& path) noexcept;

    /**
     * @brief Check if path is a UNC path (Windows)
     *
     * UNC paths start with \\ or // and can be used
     * to access network resources.
     *
     * @param path Path to check
     * @return true if path is UNC
     */
    [[nodiscard]] static bool isUncPath(const std::string& path) noexcept;

    /**
     * @brief Check if path contains null bytes
     *
     * Null bytes can be used to truncate paths and bypass checks.
     *
     * @param path Path to check
     * @return true if path contains null bytes
     */
    [[nodiscard]] static bool hasNullBytes(const std::string& path) noexcept;

    /**
     * @brief Sanitize a path by removing dangerous elements
     *
     * Removes:
     * - Null bytes
     * - Leading/trailing whitespace
     * - Consecutive path separators
     * - Trailing dots (Windows)
     *
     * Does NOT remove traversal - use validate() for that.
     *
     * @param path Path to sanitize
     * @return Sanitized path
     */
    [[nodiscard]] static std::string sanitize(const std::string& path);

    /**
     * @brief Check if a path is under an allowed directory
     *
     * Resolves both paths to canonical form and checks
     * if child is a subdirectory of parent.
     *
     * @param path Path to check
     * @param allowedDir Allowed parent directory
     * @return true if path is under allowedDir
     */
    [[nodiscard]] static bool isUnderDirectory(
        const fs::path& path,
        const fs::path& allowedDir
    );

    /**
     * @brief Join paths safely
     *
     * Joins base and relative paths, ensuring the result
     * stays under the base directory.
     *
     * @param base Base directory (trusted)
     * @param relative Relative path (potentially untrusted)
     * @return std::nullopt if result would escape base
     */
    [[nodiscard]] static std::optional<fs::path> joinSafe(
        const fs::path& base,
        const std::string& relative
    );

    /**
     * @brief Normalize path separators
     *
     * Converts all separators to the platform's preferred separator.
     *
     * @param path Path to normalize
     * @return Normalized path
     */
    [[nodiscard]] static std::string normalizeSeparators(const std::string& path);

private:
    // URL decode for detecting encoded traversal
    [[nodiscard]] static std::string urlDecode(const std::string& str);
};

/**
 * @brief RAII guard for temporary path access
 *
 * Used when code needs to operate on a path that must be validated.
 * Automatically checks path on construction and can be used in if statements.
 *
 * Usage:
 * @code
 * if (PathGuard guard{userPath, options}; guard.ok()) {
 *     // Safe to use guard.path()
 * }
 * @endcode
 */
class PathGuard {
public:
    PathGuard(const std::string& path, const PathValidationOptions& options = {});

    [[nodiscard]] bool ok() const noexcept { return result_.valid; }
    [[nodiscard]] const fs::path& path() const noexcept { return safePath_; }
    [[nodiscard]] const std::string& reason() const noexcept { return result_.reason; }

    // Allow use in boolean context
    explicit operator bool() const noexcept { return ok(); }

private:
    PathValidationResult result_;
    fs::path safePath_;
};

} // namespace makine
