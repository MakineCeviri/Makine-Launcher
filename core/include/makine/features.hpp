#pragma once

/**
 * features.hpp - Compile-time feature detection
 *
 * This header provides macros to check which optional libraries
 * are available at compile time. Use these to write fallback code.
 *
 * Usage:
 *   #ifdef MAKINE_HAS_SIMDJSON
 *       // Use simdjson for fast parsing
 *   #else
 *       // Fallback to nlohmann-json
 *   #endif
 */

namespace makine {

/**
 * Runtime feature check
 */
struct Features {
    // Compression
    static constexpr bool has_bit7z =
#ifdef MAKINE_HAS_BIT7Z
        true;
#else
        false;
#endif

    static constexpr bool has_libarchive =
#ifdef MAKINE_HAS_LIBARCHIVE
        true;
#else
        false;
#endif

    static constexpr bool has_minizip =
#ifdef MAKINE_HAS_MINIZIP
        true;
#else
        false;
#endif

    // Serialization
    static constexpr bool has_simdjson =
#ifdef MAKINE_HAS_SIMDJSON
        true;
#else
        false;
#endif

    // Filesystem
    static constexpr bool has_efsw =
#ifdef MAKINE_HAS_EFSW
        true;
#else
        false;
#endif

    static constexpr bool has_mio =
#ifdef MAKINE_HAS_MIO
        true;
#else
        false;
#endif

    // Concurrency
    static constexpr bool has_taskflow =
#ifdef MAKINE_HAS_TASKFLOW
        true;
#else
        false;
#endif

    static constexpr bool has_concurrentqueue =
#ifdef MAKINE_HAS_CONCURRENTQUEUE
        true;
#else
        false;
#endif

    // i18n
    static constexpr bool has_simdutf =
#ifdef MAKINE_HAS_SIMDUTF
        true;
#else
        false;
#endif

    // Database
    static constexpr bool has_sqlitecpp =
#ifdef MAKINE_HAS_SQLITECPP
        true;
#else
        false;
#endif

    // Crypto
    static constexpr bool has_sodium =
#ifdef MAKINE_HAS_SODIUM
        true;
#else
        false;
#endif

    /**
     * Print enabled features to log
     */
    static void log_features();
};

} // namespace makine
