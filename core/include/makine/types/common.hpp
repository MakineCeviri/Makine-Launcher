/**
 * @file types/common.hpp
 * @brief Common type definitions and aliases
 * @copyright (c) 2026 MakineCeviri Team
 *
 * This file contains fundamental type definitions used throughout Makine.
 * It has no dependencies on other Makine headers.
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace makine {

// ============================================================================
// Namespace Aliases
// ============================================================================

/// @brief Filesystem namespace alias for convenience
namespace fs = std::filesystem;

// ============================================================================
// Basic Type Aliases
// ============================================================================

/// @brief Binary data buffer
using ByteBuffer = std::vector<uint8_t>;

/// @brief Read-only view into binary data
using ByteSpan = std::span<const uint8_t>;

/// @brief List of strings
using StringList = std::vector<std::string>;

// ============================================================================
// Progress and Cancellation
// ============================================================================

/**
 * @brief Progress callback for long-running operations
 *
 * @param current  Current step number
 * @param total    Total number of steps
 * @param message  Status message describing current operation
 *
 * @code
 * auto progress = [](uint32_t cur, uint32_t total, std::string_view msg) {
 *     std::cout << "[" << cur << "/" << total << "] " << msg << std::endl;
 * };
 * core.scanGames(progress);
 * @endcode
 */
using ProgressCallback = std::function<void(
    uint32_t current,
    uint32_t total,
    std::string_view message
)>;

/**
 * @brief Cancellation token for async operations
 *
 * Thread-safe cancellation mechanism for long-running operations.
 *
 * @code
 * CancellationToken token;
 *
 * // In another thread:
 * token.cancel();
 *
 * // In the operation:
 * while (!token.isCancelled()) {
 *     // Do work...
 * }
 * @endcode
 */
class CancellationToken {
public:
    /// @brief Request cancellation
    void cancel() noexcept { cancelled_ = true; }

    /// @brief Check if cancellation was requested
    [[nodiscard]] bool isCancelled() const noexcept { return cancelled_; }

    /// @brief Reset the token for reuse
    void reset() noexcept { cancelled_ = false; }

private:
    std::atomic<bool> cancelled_{false};
};

// ============================================================================
// Version Information
// ============================================================================

/**
 * @brief Semantic version structure
 *
 * Represents a version number in the format major.minor.patch.build
 */
struct Version {
    uint32_t major = 0;
    uint32_t minor = 0;
    uint32_t patch = 0;
    uint32_t build = 0;

    /// @brief Convert to string representation
    [[nodiscard]] std::string toString() const {
        return std::to_string(major) + "." +
               std::to_string(minor) + "." +
               std::to_string(patch) + "." +
               std::to_string(build);
    }

    /// @brief Parse version string (e.g., "1.2.3.4" or "1.2.3")
    [[nodiscard]] static std::optional<Version> parse(std::string_view str);

    auto operator<=>(const Version&) const = default;
};

} // namespace makine
