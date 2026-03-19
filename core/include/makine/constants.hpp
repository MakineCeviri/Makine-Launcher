/**
 * @file constants.hpp
 * @brief Named constants for magic numbers used across the codebase
 * @copyright (c) 2026 MakineCeviri Team
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace makine {

// Archive/IO
inline constexpr std::size_t kArchiveReadBufferSize = 10240;

// Security
inline constexpr int kPBKDF2Iterations = 100000;

// Cache
inline constexpr std::size_t kDefaultCacheMaxSize = 10000;

// Timeouts (milliseconds)
inline constexpr uint32_t kDefaultTimeoutMs = 30000;
inline constexpr uint32_t kShortTimeoutMs = 10000;

// Limits
inline constexpr uint32_t kMaxDatabaseEntries = 100000;

} // namespace makine
