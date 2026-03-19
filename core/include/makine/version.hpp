#pragma once

/**
 * @file version.hpp
 * @brief Version information and ABI compatibility for Makine
 *
 * Provides:
 * - Version constants and macros
 * - ABI version tracking
 * - Build information
 * - Runtime version checking
 *
 * Usage:
 * @code
 * // Get version string
 * std::cout << makine::VERSION_STRING << std::endl;  // "0.1.0"
 *
 * // Check ABI compatibility
 * if (makine::isABICompatible(library_abi_version)) {
 *     // Safe to use library
 * }
 *
 * // Get full build info
 * auto info = makine::getBuildInfo();
 * std::cout << info.toText() << std::endl;
 * @endcode
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <cstdint>
#include <string>
#include <sstream>

namespace makine {

// =============================================================================
// VERSION CONSTANTS
// =============================================================================

/**
 * @brief Major version (breaking changes)
 */
constexpr int VERSION_MAJOR = 0;

/**
 * @brief Minor version (new features, backward compatible)
 */
constexpr int VERSION_MINOR = 1;

/**
 * @brief Patch version (bug fixes)
 */
constexpr int VERSION_PATCH = 0;

/**
 * @brief Pre-release suffix (e.g., "alpha", "beta", "rc1", or empty)
 */
constexpr const char* VERSION_PRERELEASE = "pre-alpha";

/**
 * @brief Full version string
 */
constexpr const char* VERSION_STRING = "0.1.0-pre-alpha";

/**
 * @brief Short version string (no pre-release)
 */
constexpr const char* VERSION_SHORT = "0.1.0";

/**
 * @brief Version as single integer for comparison
 *
 * Format: MAJOR * 10000 + MINOR * 100 + PATCH
 * Example: 0.1.0 = 100, 1.2.3 = 10203
 */
constexpr int VERSION_NUMBER = VERSION_MAJOR * 10000 + VERSION_MINOR * 100 + VERSION_PATCH;

// =============================================================================
// ABI VERSION
// =============================================================================

/**
 * @brief ABI version number
 *
 * Increment when:
 * - Struct layout changes
 * - Virtual function order changes
 * - Symbol names change
 * - Calling convention changes
 *
 * Libraries compiled with different ABI versions are NOT compatible.
 */
constexpr int ABI_VERSION = 1;

/**
 * @brief Minimum compatible ABI version
 *
 * Libraries with ABI_VERSION >= MIN_ABI_VERSION are backward compatible.
 */
constexpr int MIN_ABI_VERSION = 1;

/**
 * @brief Check if given ABI version is compatible
 */
[[nodiscard]] constexpr bool isABICompatible(int version) noexcept {
    return version >= MIN_ABI_VERSION && version <= ABI_VERSION;
}

/**
 * @brief Get current ABI version
 */
[[nodiscard]] constexpr int getABIVersion() noexcept {
    return ABI_VERSION;
}

// =============================================================================
// BUILD INFORMATION
// =============================================================================

/**
 * @brief Build configuration
 */
enum class BuildType {
    Debug,
    Release,
    RelWithDebInfo,
    MinSizeRel
};

/**
 * @brief Compiler identification
 */
enum class Compiler {
    Unknown,
    MSVC,
    GCC,
    Clang,
    AppleClang
};

/**
 * @brief Build information structure
 */
struct BuildInfo {
    // Version info
    int versionMajor = VERSION_MAJOR;
    int versionMinor = VERSION_MINOR;
    int versionPatch = VERSION_PATCH;
    std::string versionString = VERSION_STRING;
    std::string versionPrerelease = VERSION_PRERELEASE;

    // ABI info
    int abiVersion = ABI_VERSION;
    int minAbiVersion = MIN_ABI_VERSION;

    // Build config
    BuildType buildType = BuildType::Release;
    Compiler compiler = Compiler::Unknown;
    std::string compilerVersion;
    std::string cppStandard;

    // Build metadata
    std::string buildDate;
    std::string buildTime;
    std::string gitCommit;
    std::string gitBranch;

    // Platform
    std::string platform;
    std::string architecture;

    /**
     * @brief Convert to human-readable text
     */
    [[nodiscard]] std::string toText() const {
        std::ostringstream oss;

        oss << "Makine " << versionString << "\n";
        oss << "ABI Version: " << abiVersion << " (min: " << minAbiVersion << ")\n";
        oss << "Build: " << buildTypeToString() << "\n";
        oss << "Compiler: " << compilerToString() << " " << compilerVersion << "\n";
        oss << "C++ Standard: " << cppStandard << "\n";
        oss << "Platform: " << platform << " " << architecture << "\n";

        if (!buildDate.empty()) {
            oss << "Built: " << buildDate << " " << buildTime << "\n";
        }
        if (!gitCommit.empty()) {
            oss << "Git: " << gitCommit;
            if (!gitBranch.empty()) {
                oss << " (" << gitBranch << ")";
            }
            oss << "\n";
        }

        return oss.str();
    }

    /**
     * @brief Convert to JSON
     */
    [[nodiscard]] std::string toJson() const {
        std::ostringstream oss;

        oss << "{\n";
        oss << "  \"version\": {\n";
        oss << "    \"major\": " << versionMajor << ",\n";
        oss << "    \"minor\": " << versionMinor << ",\n";
        oss << "    \"patch\": " << versionPatch << ",\n";
        oss << "    \"string\": \"" << versionString << "\",\n";
        oss << "    \"prerelease\": \"" << versionPrerelease << "\"\n";
        oss << "  },\n";

        oss << "  \"abi\": {\n";
        oss << "    \"version\": " << abiVersion << ",\n";
        oss << "    \"minVersion\": " << minAbiVersion << "\n";
        oss << "  },\n";

        oss << "  \"build\": {\n";
        oss << "    \"type\": \"" << buildTypeToString() << "\",\n";
        oss << "    \"compiler\": \"" << compilerToString() << "\",\n";
        oss << "    \"compilerVersion\": \"" << compilerVersion << "\",\n";
        oss << "    \"cppStandard\": \"" << cppStandard << "\",\n";
        oss << "    \"date\": \"" << buildDate << "\",\n";
        oss << "    \"time\": \"" << buildTime << "\"\n";
        oss << "  },\n";

        oss << "  \"platform\": {\n";
        oss << "    \"os\": \"" << platform << "\",\n";
        oss << "    \"architecture\": \"" << architecture << "\"\n";
        oss << "  },\n";

        oss << "  \"git\": {\n";
        oss << "    \"commit\": \"" << gitCommit << "\",\n";
        oss << "    \"branch\": \"" << gitBranch << "\"\n";
        oss << "  }\n";

        oss << "}\n";

        return oss.str();
    }

private:
    [[nodiscard]] const char* buildTypeToString() const {
        switch (buildType) {
            case BuildType::Debug: return "Debug";
            case BuildType::Release: return "Release";
            case BuildType::RelWithDebInfo: return "RelWithDebInfo";
            case BuildType::MinSizeRel: return "MinSizeRel";
            default: return "Unknown";
        }
    }

    [[nodiscard]] const char* compilerToString() const {
        switch (compiler) {
            case Compiler::MSVC: return "MSVC";
            case Compiler::GCC: return "GCC";
            case Compiler::Clang: return "Clang";
            case Compiler::AppleClang: return "AppleClang";
            default: return "Unknown";
        }
    }
};

/**
 * @brief Get build information
 */
[[nodiscard]] inline BuildInfo getBuildInfo() {
    BuildInfo info;

    // Build date/time from preprocessor
    info.buildDate = __DATE__;
    info.buildTime = __TIME__;

    // Detect compiler
#if defined(_MSC_VER)
    info.compiler = Compiler::MSVC;
    info.compilerVersion = std::to_string(_MSC_VER);
#elif defined(__clang__)
    #if defined(__apple_build_version__)
        info.compiler = Compiler::AppleClang;
    #else
        info.compiler = Compiler::Clang;
    #endif
    info.compilerVersion = std::to_string(__clang_major__) + "." +
                           std::to_string(__clang_minor__) + "." +
                           std::to_string(__clang_patchlevel__);
#elif defined(__GNUC__)
    info.compiler = Compiler::GCC;
    info.compilerVersion = std::to_string(__GNUC__) + "." +
                           std::to_string(__GNUC_MINOR__) + "." +
                           std::to_string(__GNUC_PATCHLEVEL__);
#endif

    // C++ standard
#if __cplusplus >= 202302L
    info.cppStandard = "C++23";
#elif __cplusplus >= 202002L
    info.cppStandard = "C++20";
#elif __cplusplus >= 201703L
    info.cppStandard = "C++17";
#elif __cplusplus >= 201402L
    info.cppStandard = "C++14";
#else
    info.cppStandard = "C++11";
#endif

    // Build type
#ifdef NDEBUG
    info.buildType = BuildType::Release;
#else
    info.buildType = BuildType::Debug;
#endif

    // Platform
#if defined(_WIN32)
    info.platform = "Windows";
    #if defined(_WIN64)
        info.architecture = "x64";
    #else
        info.architecture = "x86";
    #endif
#elif defined(__linux__)
    info.platform = "Linux";
    #if defined(__x86_64__)
        info.architecture = "x64";
    #elif defined(__aarch64__)
        info.architecture = "arm64";
    #else
        info.architecture = "unknown";
    #endif
#elif defined(__APPLE__)
    info.platform = "macOS";
    #if defined(__aarch64__)
        info.architecture = "arm64";
    #else
        info.architecture = "x64";
    #endif
#else
    info.platform = "Unknown";
    info.architecture = "unknown";
#endif

    return info;
}

// =============================================================================
// VERSION COMPARISON
// =============================================================================

/**
 * @brief Semantic version for comparison
 */
struct SemanticVersion {
    int major = 0;
    int minor = 0;
    int patch = 0;
    std::string prerelease;

    constexpr SemanticVersion() = default;

    constexpr SemanticVersion(int maj, int min, int pat, const char* pre = "")
        : major(maj), minor(min), patch(pat), prerelease(pre ? pre : "") {}

    /**
     * @brief Parse version string (e.g., "1.2.3" or "1.2.3-alpha")
     */
    static SemanticVersion parse(std::string_view str) {
        SemanticVersion v;

        // Find prerelease separator
        auto dashPos = str.find('-');
        if (dashPos != std::string_view::npos) {
            v.prerelease = std::string(str.substr(dashPos + 1));
            str = str.substr(0, dashPos);
        }

        // Parse major.minor.patch
        size_t pos = 0;
        size_t start = 0;
        int* parts[] = {&v.major, &v.minor, &v.patch};
        int partIndex = 0;

        while (pos <= str.size() && partIndex < 3) {
            if (pos == str.size() || str[pos] == '.') {
                if (pos > start) {
                    std::string numStr(str.substr(start, pos - start));
                    *parts[partIndex] = std::stoi(numStr);
                }
                start = pos + 1;
                ++partIndex;
            }
            ++pos;
        }

        return v;
    }

    [[nodiscard]] std::string toString() const {
        std::ostringstream oss;
        oss << major << "." << minor << "." << patch;
        if (!prerelease.empty()) {
            oss << "-" << prerelease;
        }
        return oss.str();
    }

    [[nodiscard]] int toNumber() const noexcept {
        return major * 10000 + minor * 100 + patch;
    }

    // Comparison operators
    [[nodiscard]] bool operator==(const SemanticVersion& other) const noexcept {
        return major == other.major && minor == other.minor &&
               patch == other.patch && prerelease == other.prerelease;
    }

    [[nodiscard]] bool operator!=(const SemanticVersion& other) const noexcept {
        return !(*this == other);
    }

    [[nodiscard]] bool operator<(const SemanticVersion& other) const noexcept {
        if (major != other.major) return major < other.major;
        if (minor != other.minor) return minor < other.minor;
        if (patch != other.patch) return patch < other.patch;

        // Pre-release versions have lower precedence
        if (prerelease.empty() && !other.prerelease.empty()) return false;
        if (!prerelease.empty() && other.prerelease.empty()) return true;

        return prerelease < other.prerelease;
    }

    [[nodiscard]] bool operator<=(const SemanticVersion& other) const noexcept {
        return *this < other || *this == other;
    }

    [[nodiscard]] bool operator>(const SemanticVersion& other) const noexcept {
        return other < *this;
    }

    [[nodiscard]] bool operator>=(const SemanticVersion& other) const noexcept {
        return !(*this < other);
    }
};

/**
 * @brief Current library version
 */
constexpr SemanticVersion CURRENT_VERSION{VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_PRERELEASE};

/**
 * @brief Check if version is at least the specified version
 */
[[nodiscard]] constexpr bool isVersionAtLeast(int major, int minor = 0, int patch = 0) noexcept {
    if (VERSION_MAJOR != major) return VERSION_MAJOR > major;
    if (VERSION_MINOR != minor) return VERSION_MINOR > minor;
    return VERSION_PATCH >= patch;
}

// =============================================================================
// VERSION MACROS
// =============================================================================

/**
 * @brief Check version at compile time
 *
 * Usage:
 * @code
 * #if MAKINE_VERSION_AT_LEAST(1, 0, 0)
 *     // Use new API
 * #else
 *     // Use old API
 * #endif
 * @endcode
 */
#define MAKINE_VERSION_AT_LEAST(major, minor, patch) \
    ((::makine::VERSION_MAJOR > (major)) || \
     ((::makine::VERSION_MAJOR == (major)) && (::makine::VERSION_MINOR > (minor))) || \
     ((::makine::VERSION_MAJOR == (major)) && (::makine::VERSION_MINOR == (minor)) && (::makine::VERSION_PATCH >= (patch))))

/**
 * @brief Version string macro
 *
 * If CMake defines MAKINE_APP_VERSION, use that (single source of truth).
 * Otherwise fall back to the constexpr VERSION_STRING above.
 */
#ifdef MAKINE_APP_VERSION
#define MAKINE_VERSION_STRING MAKINE_APP_VERSION
#else
#define MAKINE_VERSION_STRING "0.1.0-pre-alpha"
#endif

/**
 * @brief Encode version as single number
 */
#define MAKINE_VERSION_ENCODE(major, minor, patch) \
    ((major) * 10000 + (minor) * 100 + (patch))

/**
 * @brief Current version as encoded number
 */
#define MAKINE_VERSION_NUMBER MAKINE_VERSION_ENCODE(0, 1, 0)

} // namespace makine
