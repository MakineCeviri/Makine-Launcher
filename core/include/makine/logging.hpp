/**
 * @file logging.hpp
 * @brief Makine structured logging system
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Provides consistent, module-based logging across the entire codebase.
 * All logging should use these macros instead of calling spdlog directly.
 */

#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>
#include <cstdlib>
#include <string>
#include <string_view>
#include <chrono>
#include <source_location>

namespace makine {

// =============================================================================
// MODULE IDENTIFIERS
// =============================================================================

namespace log {

/// Core system module
constexpr const char* CORE = "Core";

/// Game detection and scanning
constexpr const char* DETECTOR = "Detector";

/// Game scanners (Steam, Epic, GOG)
constexpr const char* SCANNER = "Scanner";

/// Engine handlers (Unity, Unreal, etc.)
constexpr const char* HANDLER = "Handler";

/// Security and cryptography
constexpr const char* SECURITY = "Security";

/// Database operations
constexpr const char* DATABASE = "Database";

/// Package management
constexpr const char* PACKAGE = "Package";

/// Asset parsing
constexpr const char* PARSER = "Parser";

/// Runtime management
constexpr const char* RUNTIME = "Runtime";

/// Font analysis and injection
constexpr const char* FONT = "Font";

/// File operations
constexpr const char* FILE = "File";

/// Network operations
constexpr const char* NETWORK = "Network";

/// Configuration
constexpr const char* CONFIG = "Config";

/// Version tracking
constexpr const char* VERSION = "Version";

} // namespace log

// =============================================================================
// LOGGING MACROS
// =============================================================================

/**
 * @brief Trace level logging (most verbose)
 * Use for detailed debugging information that is normally not needed.
 */
#define MAKINE_LOG_TRACE(module, fmt_str, ...) \
    spdlog::trace("[{}] " fmt_str, module, ##__VA_ARGS__)

/**
 * @brief Debug level logging
 * Use for debugging information useful during development.
 */
#define MAKINE_LOG_DEBUG(module, fmt_str, ...) \
    spdlog::debug("[{}] " fmt_str, module, ##__VA_ARGS__)

/**
 * @brief Info level logging
 * Use for general operational information.
 */
#define MAKINE_LOG_INFO(module, fmt_str, ...) \
    spdlog::info("[{}] " fmt_str, module, ##__VA_ARGS__)

/**
 * @brief Warning level logging
 * Use for potentially problematic situations that don't prevent operation.
 */
#define MAKINE_LOG_WARN(module, fmt_str, ...) \
    spdlog::warn("[{}] " fmt_str, module, ##__VA_ARGS__)

/**
 * @brief Error level logging
 * Use for error conditions that prevent normal operation.
 */
#define MAKINE_LOG_ERROR(module, fmt_str, ...) \
    spdlog::error("[{}] " fmt_str, module, ##__VA_ARGS__)

/**
 * @brief Critical level logging
 * Use for severe errors that may cause application failure.
 */
#define MAKINE_LOG_CRITICAL(module, fmt_str, ...) \
    spdlog::critical("[{}] " fmt_str, module, ##__VA_ARGS__)

// =============================================================================
// SCOPED TIMING
// =============================================================================

/**
 * @brief RAII timer for measuring operation duration
 *
 * Automatically logs the duration when the scope exits.
 *
 * @example
 * void processGame() {
 *     ScopedTimer timer("process_game", log::HANDLER);
 *     // ... operation ...
 * } // Logs: "[Handler] process_game completed in 123ms"
 */
class ScopedTimer {
public:
    /**
     * @brief Construct a scoped timer
     * @param operation Name of the operation being timed
     * @param module Module identifier for logging
     * @param logLevel Minimum level to log at (default: debug)
     */
    ScopedTimer(std::string_view operation,
                std::string_view module,
                spdlog::level::level_enum logLevel = spdlog::level::debug)
        : operation_(operation)
        , module_(module)
        , logLevel_(logLevel)
        , start_(std::chrono::steady_clock::now())
    {}

    ~ScopedTimer() {
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_);

        spdlog::log(logLevel_, "[{}] {} completed in {}ms",
                    module_, operation_, duration.count());
    }

    /// Get elapsed time without stopping the timer
    [[nodiscard]] std::chrono::milliseconds elapsed() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_);
    }

    // Non-copyable, non-movable
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
    ScopedTimer(ScopedTimer&&) = delete;
    ScopedTimer& operator=(ScopedTimer&&) = delete;

private:
    std::string operation_;
    std::string module_;
    spdlog::level::level_enum logLevel_;
    std::chrono::steady_clock::time_point start_;
};

/**
 * @brief Macro for creating a scoped timer with automatic naming
 *
 * @example
 * void scanGames() {
 *     MAKINE_TIMED_SCOPE(log::SCANNER, "scan_all_games");
 *     // ... operation ...
 * }
 */
#define MAKINE_TIMED_SCOPE(module, operation) \
    ::makine::ScopedTimer _timer_##__LINE__(operation, module)

/**
 * @brief Macro for creating a scoped timer at info level
 */
#define MAKINE_TIMED_SCOPE_INFO(module, operation) \
    ::makine::ScopedTimer _timer_##__LINE__(operation, module, spdlog::level::info)

// =============================================================================
// CONDITIONAL LOGGING
// =============================================================================

/**
 * @brief Log only if condition is true
 */
#define MAKINE_LOG_IF(condition, level, module, ...) \
    do { \
        if (condition) { \
            spdlog::level("[{}] " __VA_ARGS__, module); \
        } \
    } while(0)

/**
 * @brief Log only once (useful in loops)
 */
#define MAKINE_LOG_ONCE(level, module, ...) \
    do { \
        static bool _logged = false; \
        if (!_logged) { \
            spdlog::level("[{}] " __VA_ARGS__, module); \
            _logged = true; \
        } \
    } while(0)

/**
 * @brief Log at most every N milliseconds
 */
#define MAKINE_LOG_THROTTLE(ms, level, module, ...) \
    do { \
        static auto _lastLog = std::chrono::steady_clock::time_point{}; \
        auto _now = std::chrono::steady_clock::now(); \
        if (_now - _lastLog >= std::chrono::milliseconds(ms)) { \
            spdlog::level("[{}] " __VA_ARGS__, module); \
            _lastLog = _now; \
        } \
    } while(0)

// =============================================================================
// FUNCTION ENTRY/EXIT LOGGING
// =============================================================================

/**
 * @brief Log function entry (trace level)
 */
#define MAKINE_LOG_ENTER(module) \
    MAKINE_LOG_TRACE(module, "-> {}", __func__)

/**
 * @brief Log function exit (trace level)
 */
#define MAKINE_LOG_EXIT(module) \
    MAKINE_LOG_TRACE(module, "<- {}", __func__)

/**
 * @brief RAII helper for logging function entry/exit
 */
class ScopedFunctionLogger {
public:
    ScopedFunctionLogger(std::string_view module,
                        std::source_location loc = std::source_location::current())
        : module_(module)
        , function_(loc.function_name())
    {
        spdlog::trace("[{}] -> {}", module_, function_);
    }

    ~ScopedFunctionLogger() {
        spdlog::trace("[{}] <- {}", module_, function_);
    }

private:
    std::string module_;
    const char* function_;
};

/**
 * @brief Macro for automatic function entry/exit logging
 */
#define MAKINE_LOG_FUNCTION(module) \
    ::makine::ScopedFunctionLogger _funcLogger_##__LINE__(module)

// =============================================================================
// ASSERT WITH LOGGING
// =============================================================================

/**
 * @brief Assert with logging on failure
 */
#define MAKINE_ASSERT(condition, module, ...) \
    do { \
        if (!(condition)) { \
            MAKINE_LOG_CRITICAL(module, "Assertion failed: " #condition); \
            MAKINE_LOG_CRITICAL(module, __VA_ARGS__); \
            std::abort(); \
        } \
    } while(0)

/**
 * @brief Debug-only assert
 */
#ifdef NDEBUG
    #define MAKINE_DEBUG_ASSERT(condition, module, ...) ((void)0)
#else
    #define MAKINE_DEBUG_ASSERT(condition, module, ...) \
        MAKINE_ASSERT(condition, module, __VA_ARGS__)
#endif

// =============================================================================
// LOG CONFIGURATION
// =============================================================================

/**
 * @brief Configure logging for Makine
 *
 * Should be called once at application startup.
 *
 * @param level Minimum log level to display
 * @param pattern Log pattern (default includes timestamp, level, message)
 */
inline void configureLogging(
    spdlog::level::level_enum level = spdlog::level::info,
    const std::string& pattern = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v"
) {
    spdlog::set_level(level);
    spdlog::set_pattern(pattern);
}

/**
 * @brief Set log level at runtime
 */
inline void setLogLevel(spdlog::level::level_enum level) {
    spdlog::set_level(level);
}

/**
 * @brief Get current log level
 */
inline spdlog::level::level_enum getLogLevel() {
    return spdlog::get_level();
}

/**
 * @brief Flush all loggers
 */
inline void flushLogs() {
    spdlog::default_logger()->flush();
}

} // namespace makine
