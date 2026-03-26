/**
 * @file error.hpp
 * @brief Makine error handling and result types
 * @copyright (c) 2026 MakineCeviri Team
 */

#pragma once

#include <exception>
#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace makine {

/**
 * @brief Error categories for Makine operations
 */
enum class ErrorCode {
    // General errors (0-99)
    Success = 0,
    Unknown = 1,
    InvalidArgument = 2,
    NotImplemented = 3,
    Cancelled = 4,
    NotSupported = 5,

    // File system errors (100-199)
    FileNotFound = 100,
    FileAccessDenied = 101,
    FileAlreadyExists = 102,
    DirectoryNotFound = 103,
    DiskFull = 104,
    InvalidPath = 105,
    FileCorrupted = 106,
    FileCreateFailed = 107,
    FileWriteFailed = 108,

    // Network errors (200-299)
    NetworkError = 200,
    ConnectionFailed = 201,
    DownloadFailed = 202,
    ServerError = 203,
    Timeout = 204,
    InvalidResponse = 205,

    // Game detection errors (300-399)
    GameNotFound = 300,
    GameNotSupported = 301,
    EngineNotDetected = 302,
    VersionMismatch = 303,
    InvalidGamePath = 304,

    // Patch errors (400-499)
    PatchFailed = 400,
    BackupFailed = 401,
    RestoreFailed = 402,
    IncompatibleVersion = 403,
    AlreadyPatched = 404,
    NotPatched = 405,

    // Security errors (500-599)
    SignatureInvalid = 500,
    ChecksumMismatch = 501,
    CertificateError = 502,
    TamperingDetected = 503,
    SignatureRequired = 504,

    // Parse errors (600-699)
    ParseError = 600,
    InvalidFormat = 601,
    UnsupportedVersion = 602,
    DecompressionFailed = 603,
    CompressionFailed = 604,

    // Runtime errors (700-799)
    RuntimeInstallFailed = 700,
    RuntimeNotFound = 701,
    PluginLoadFailed = 702,

    // Database errors (800-899)
    DatabaseError = 800,
    IOError = 801,
    QueryFailed = 802,
    TransactionFailed = 803,
    NotFound = 804,

    // Patch engine specific errors (900-999)
    InvalidOffset = 900,
    BackupNotFound = 901,
    FileLocked = 902,
    InsufficientDiskSpace = 903,
    SecurityViolation = 904,
    StorageError = 905,

    // Configuration errors (1000-1099)
    InvalidConfiguration = 1000,
};

/**
 * @brief Get human-readable error message
 */
constexpr std::string_view errorMessage(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::Success:            return "Success";
        case ErrorCode::Unknown:            return "Unknown error";
        case ErrorCode::InvalidArgument:    return "Invalid argument";
        case ErrorCode::NotImplemented:     return "Not implemented";
        case ErrorCode::Cancelled:          return "Operation cancelled";
        case ErrorCode::NotSupported:       return "Operation not supported";

        case ErrorCode::FileNotFound:       return "File not found";
        case ErrorCode::FileAccessDenied:   return "File access denied";
        case ErrorCode::FileAlreadyExists:  return "File already exists";
        case ErrorCode::DirectoryNotFound:  return "Directory not found";
        case ErrorCode::DiskFull:           return "Disk full";
        case ErrorCode::InvalidPath:        return "Invalid path";
        case ErrorCode::FileCorrupted:      return "File corrupted";

        case ErrorCode::NetworkError:       return "Network error";
        case ErrorCode::ConnectionFailed:   return "Connection failed";
        case ErrorCode::DownloadFailed:     return "Download failed";
        case ErrorCode::ServerError:        return "Server error";
        case ErrorCode::Timeout:            return "Operation timed out";
        case ErrorCode::InvalidResponse:    return "Invalid server response";

        case ErrorCode::GameNotFound:       return "Game not found";
        case ErrorCode::GameNotSupported:   return "Game not supported";
        case ErrorCode::EngineNotDetected:  return "Game engine not detected";
        case ErrorCode::VersionMismatch:    return "Version mismatch";
        case ErrorCode::InvalidGamePath:    return "Invalid game path";

        case ErrorCode::PatchFailed:        return "Patch failed";
        case ErrorCode::BackupFailed:       return "Backup failed";
        case ErrorCode::RestoreFailed:      return "Restore failed";
        case ErrorCode::IncompatibleVersion: return "Incompatible version";
        case ErrorCode::AlreadyPatched:     return "Already patched";
        case ErrorCode::NotPatched:         return "Not patched";

        case ErrorCode::SignatureInvalid:   return "Invalid signature";
        case ErrorCode::ChecksumMismatch:   return "Checksum mismatch";
        case ErrorCode::CertificateError:   return "Certificate error";
        case ErrorCode::TamperingDetected:  return "Tampering detected";
        case ErrorCode::SignatureRequired:  return "Signature required";

        case ErrorCode::ParseError:         return "Parse error";
        case ErrorCode::InvalidFormat:      return "Invalid format";
        case ErrorCode::UnsupportedVersion: return "Unsupported version";
        case ErrorCode::DecompressionFailed: return "Decompression failed";
        case ErrorCode::CompressionFailed:  return "Compression failed";

        case ErrorCode::RuntimeInstallFailed: return "Runtime installation failed";
        case ErrorCode::RuntimeNotFound:    return "Runtime not found";
        case ErrorCode::PluginLoadFailed:   return "Plugin load failed";

        case ErrorCode::DatabaseError:      return "Database error";
        case ErrorCode::IOError:            return "I/O error";
        case ErrorCode::QueryFailed:        return "Query failed";
        case ErrorCode::TransactionFailed:  return "Transaction failed";
        case ErrorCode::NotFound:           return "Item not found";

        case ErrorCode::InvalidOffset:      return "Invalid offset in file";
        case ErrorCode::BackupNotFound:     return "Backup not found";
        case ErrorCode::FileLocked:         return "File is locked by another process";
        case ErrorCode::InsufficientDiskSpace: return "Insufficient disk space";
        case ErrorCode::SecurityViolation:  return "Security violation";
        case ErrorCode::StorageError:       return "Storage error";

        case ErrorCode::InvalidConfiguration: return "Invalid configuration";

        default:                            return "Unknown error code";
    }
}

/**
 * @brief Detailed error information with context chain support
 *
 * Errors can be enriched with contextual information as they propagate
 * up the call stack. This helps with debugging and error reporting.
 *
 * @example
 * return std::unexpected(
 *     Error(ErrorCode::PatchFailed, "Binary patch failed")
 *         .withFile(targetFile)
 *         .withGame(game.name)
 *         .withContext("IL2CPP metadata patching")
 *         .withContext("Unity handler")
 * );
 */
class Error {
public:
    Error() : code_(ErrorCode::Success) {}

    Error(ErrorCode code, std::string message = "",
          std::source_location loc = std::source_location::current())
        : code_(code)
        , message_(message.empty() ? std::string(errorMessage(code)) : std::move(message))
        , file_(loc.file_name())
        , line_(loc.line())
        , function_(loc.function_name())
    {}

    // =========================================================================
    // BASIC ACCESSORS
    // =========================================================================

    [[nodiscard]] ErrorCode code() const noexcept { return code_; }
    [[nodiscard]] const std::string& message() const noexcept { return message_; }
    [[nodiscard]] const char* file() const noexcept { return file_; }
    [[nodiscard]] uint32_t line() const noexcept { return line_; }
    [[nodiscard]] const char* function() const noexcept { return function_; }

    [[nodiscard]] bool isSuccess() const noexcept { return code_ == ErrorCode::Success; }
    [[nodiscard]] bool isError() const noexcept { return code_ != ErrorCode::Success; }

    [[nodiscard]] explicit operator bool() const noexcept { return isError(); }

    // =========================================================================
    // CONTEXT CHAIN - Fluent interface for adding context
    // =========================================================================

    /**
     * @brief Add context information to the error
     * @param context Description of the context where error occurred
     * @return Reference to this error for chaining
     */
    Error& withContext(std::string context) & {
        context_.push_back(std::move(context));
        return *this;
    }

    Error&& withContext(std::string context) && {
        context_.push_back(std::move(context));
        return std::move(*this);
    }

    /**
     * @brief Add file path information to the error
     * @param filePath Path to the file involved in the error
     * @return Reference to this error for chaining
     */
    Error& withFile(const std::filesystem::path& filePath) & {
        affectedFile_ = filePath;
        return *this;
    }

    Error&& withFile(const std::filesystem::path& filePath) && {
        affectedFile_ = filePath;
        return std::move(*this);
    }

    /**
     * @brief Add game information to the error
     * @param gameName Name of the game involved in the error
     * @return Reference to this error for chaining
     */
    Error& withGame(std::string gameName) & {
        gameName_ = std::move(gameName);
        return *this;
    }

    Error&& withGame(std::string gameName) && {
        gameName_ = std::move(gameName);
        return std::move(*this);
    }

    /**
     * @brief Add a detail key-value pair
     * @param key Detail name
     * @param value Detail value
     * @return Reference to this error for chaining
     */
    Error& withDetail(std::string key, std::string value) & {
        details_[std::move(key)] = std::move(value);
        return *this;
    }

    Error&& withDetail(std::string key, std::string value) && {
        details_[std::move(key)] = std::move(value);
        return std::move(*this);
    }

    // =========================================================================
    // CONTEXT ACCESSORS
    // =========================================================================

    /**
     * @brief Get the context chain (most recent first)
     */
    [[nodiscard]] const std::vector<std::string>& contextChain() const noexcept {
        return context_;
    }

    /**
     * @brief Get the affected file path (if set)
     */
    [[nodiscard]] const std::optional<std::filesystem::path>& affectedFile() const noexcept {
        return affectedFile_;
    }

    /**
     * @brief Get the game name (if set)
     */
    [[nodiscard]] const std::optional<std::string>& gameName() const noexcept {
        return gameName_;
    }

    /**
     * @brief Get additional details
     */
    [[nodiscard]] const std::unordered_map<std::string, std::string>& details() const noexcept {
        return details_;
    }

    // =========================================================================
    // MESSAGE FORMATTING
    // =========================================================================

    /**
     * @brief Get full error message with source location
     */
    [[nodiscard]] std::string fullMessage() const {
        if (code_ == ErrorCode::Success) return "Success";
        return message_ + " [" + std::string(function_) + " at " +
               std::string(file_) + ":" + std::to_string(line_) + "]";
    }

    /**
     * @brief Get detailed error message with all context
     */
    [[nodiscard]] std::string detailedMessage() const {
        if (code_ == ErrorCode::Success) return "Success";

        std::string result = message_;

        // Add context chain
        if (!context_.empty()) {
            result += "\n  Context:";
            for (const auto& ctx : context_) {
                result += "\n    - " + ctx;
            }
        }

        // Add game info
        if (gameName_) {
            result += "\n  Game: " + *gameName_;
        }

        // Add file info
        if (affectedFile_) {
            result += "\n  File: " + affectedFile_->string();
        }

        // Add details
        if (!details_.empty()) {
            result += "\n  Details:";
            for (const auto& [key, value] : details_) {
                result += "\n    " + key + ": " + value;
            }
        }

        // Add source location
        result += "\n  Location: " + std::string(function_) + " at " +
                  std::string(file_) + ":" + std::to_string(line_);

        return result;
    }

    /**
     * @brief Format error as JSON for logging
     */
    [[nodiscard]] std::string toJson() const {
        std::string json = "{";
        json += "\"code\":" + std::to_string(static_cast<int>(code_)) + ",";
        json += "\"message\":\"" + escapeJson(message_) + "\"";

        if (gameName_) {
            json += ",\"game\":\"" + escapeJson(*gameName_) + "\"";
        }

        if (affectedFile_) {
            json += ",\"file\":\"" + escapeJson(affectedFile_->string()) + "\"";
        }

        if (!context_.empty()) {
            json += ",\"context\":[";
            for (size_t i = 0; i < context_.size(); ++i) {
                if (i > 0) json += ",";
                json += "\"" + escapeJson(context_[i]) + "\"";
            }
            json += "]";
        }

        json += ",\"location\":{";
        json += "\"function\":\"" + escapeJson(function_) + "\",";
        json += "\"file\":\"" + escapeJson(file_) + "\",";
        json += "\"line\":" + std::to_string(line_);
        json += "}";

        json += "}";
        return json;
    }

private:
    ErrorCode code_;
    std::string message_;
    const char* file_ = "";
    uint32_t line_ = 0;
    const char* function_ = "";

    // Context information
    std::vector<std::string> context_;
    std::optional<std::filesystem::path> affectedFile_;
    std::optional<std::string> gameName_;
    std::unordered_map<std::string, std::string> details_;

    // Helper for JSON escaping
    static std::string escapeJson(const std::string& str) {
        std::string result;
        result.reserve(str.size());
        for (char c : str) {
            switch (c) {
                case '\\': result += "\\\\"; break;
                case '"':  result += "\\\""; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default:   result += c; break;
            }
        }
        return result;
    }
};

/**
 * @brief Result type for operations that can fail
 * @tparam T The success value type
 */
template<typename T>
using Result = std::expected<T, Error>;

/**
 * @brief Result type for void operations
 */
using VoidResult = std::expected<void, Error>;

/**
 * @brief Makine exception base class
 */
class Exception : public std::exception {
public:
    explicit Exception(Error error) : error_(std::move(error)) {}

    [[nodiscard]] const char* what() const noexcept override {
        return error_.message().c_str();
    }

    [[nodiscard]] const Error& error() const noexcept { return error_; }
    [[nodiscard]] ErrorCode code() const noexcept { return error_.code(); }

private:
    Error error_;
};

// Helper macros for error creation
#define MAKINE_ERROR(code, msg) \
    ::makine::Error(::makine::ErrorCode::code, msg)

#define MAKINE_SUCCESS \
    ::makine::Error(::makine::ErrorCode::Success)

// Helper for returning errors
#define MAKINE_TRY(expr) \
    do { \
        auto&& _result = (expr); \
        if (!_result) return std::unexpected(_result.error()); \
    } while (0)

// Helper for returning errors with additional context
#define MAKINE_TRY_CONTEXT(expr, ctx) \
    do { \
        auto&& _result = (expr); \
        if (!_result) { \
            auto _err = _result.error(); \
            _err.withContext(ctx); \
            return std::unexpected(std::move(_err)); \
        } \
    } while (0)

// =============================================================================
// ERROR COLLECTOR
// =============================================================================

/**
 * @brief Collects multiple errors for batch processing
 *
 * Useful when an operation can produce multiple errors and you want
 * to report all of them rather than stopping at the first one.
 *
 * @example
 * ErrorCollector errors;
 * for (const auto& file : files) {
 *     auto result = processFile(file);
 *     if (!result) {
 *         errors.add(result.error());
 *     }
 * }
 * if (errors.hasErrors()) {
 *     return std::unexpected(errors.aggregated());
 * }
 */
class ErrorCollector {
public:
    /**
     * @brief Add an error to the collection
     */
    void add(Error error) {
        if (error.isError()) {
            errors_.push_back(std::move(error));
        }
    }

    /**
     * @brief Add an error with code and message
     */
    void add(ErrorCode code, std::string message) {
        errors_.emplace_back(code, std::move(message));
    }

    /**
     * @brief Add an error from a failed result
     */
    template<typename T>
    void addIfFailed(const Result<T>& result) {
        if (!result) {
            errors_.push_back(result.error());
        }
    }

    /**
     * @brief Check if any errors were collected
     */
    [[nodiscard]] bool hasErrors() const noexcept {
        return !errors_.empty();
    }

    /**
     * @brief Check if any fatal errors were collected
     */
    [[nodiscard]] bool hasFatalErrors() const noexcept {
        for (const auto& err : errors_) {
            // Consider certain error codes as fatal
            if (err.code() == ErrorCode::SecurityViolation ||
                err.code() == ErrorCode::FileCorrupted ||
                err.code() == ErrorCode::TamperingDetected) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Get all collected errors
     */
    [[nodiscard]] const std::vector<Error>& errors() const noexcept {
        return errors_;
    }

    /**
     * @brief Get error count
     */
    [[nodiscard]] size_t count() const noexcept {
        return errors_.size();
    }

    /**
     * @brief Clear all collected errors
     */
    void clear() {
        errors_.clear();
    }

    /**
     * @brief Get aggregated error containing all messages
     * @param code Error code for the aggregated error
     * @return Single error with all messages combined
     */
    [[nodiscard]] Error aggregated(ErrorCode code = ErrorCode::Unknown) const {
        if (errors_.empty()) {
            return Error(ErrorCode::Success);
        }

        if (errors_.size() == 1) {
            return errors_[0];
        }

        std::string message = std::to_string(errors_.size()) + " errors occurred:";
        for (size_t i = 0; i < errors_.size(); ++i) {
            message += "\n  [" + std::to_string(i + 1) + "] " + errors_[i].message();
        }

        Error result(code, message);
        result.withDetail("error_count", std::to_string(errors_.size()));

        return result;
    }

    /**
     * @brief Convert to Result - fails if any errors
     */
    [[nodiscard]] VoidResult toResult(ErrorCode code = ErrorCode::Unknown) const {
        if (hasErrors()) {
            return std::unexpected(aggregated(code));
        }
        return {};
    }

private:
    std::vector<Error> errors_;
};

// =============================================================================
// RETRY LOGIC
// =============================================================================

/**
 * @brief Retry a function with exponential backoff
 *
 * @tparam Func Function type returning Result<T>
 * @param func Function to retry
 * @param maxAttempts Maximum number of attempts
 * @param initialDelayMs Initial delay between attempts
 * @param backoffMultiplier Multiplier for delay after each attempt
 * @return Result of the function
 *
 * @example
 * auto result = retry(
 *     [&]() { return downloadPackage(url); },
 *     3,                              // max attempts
 *     std::chrono::milliseconds(100), // initial delay
 *     2.0                             // backoff multiplier
 * );
 */
template<typename Func>
auto retry(
    Func&& func,
    int maxAttempts = 3,
    std::chrono::milliseconds initialDelay = std::chrono::milliseconds(100),
    double backoffMultiplier = 2.0
) -> decltype(func()) {
    Error lastError;
    auto delay = initialDelay;

    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        auto result = func();

        if (result) {
            return result;
        }

        lastError = result.error();

        // Don't retry on certain error types
        if (lastError.code() == ErrorCode::InvalidArgument ||
            lastError.code() == ErrorCode::NotSupported ||
            lastError.code() == ErrorCode::SecurityViolation ||
            lastError.code() == ErrorCode::Cancelled) {
            break;
        }

        if (attempt < maxAttempts) {
            std::this_thread::sleep_for(delay);
            delay = std::chrono::milliseconds(
                static_cast<int64_t>(delay.count() * backoffMultiplier)
            );
        }
    }

    lastError.withDetail("retry_attempts", std::to_string(maxAttempts));
    return std::unexpected(std::move(lastError));
}

/**
 * @brief Retry with custom retry predicate
 *
 * @tparam Func Function type returning Result<T>
 * @tparam Predicate Predicate to decide if retry should happen
 */
template<typename Func, typename Predicate>
auto retryIf(
    Func&& func,
    Predicate&& shouldRetry,
    int maxAttempts = 3,
    std::chrono::milliseconds initialDelay = std::chrono::milliseconds(100)
) -> decltype(func()) {
    Error lastError;
    auto delay = initialDelay;

    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        auto result = func();

        if (result) {
            return result;
        }

        lastError = result.error();

        if (!shouldRetry(lastError) || attempt >= maxAttempts) {
            break;
        }

        std::this_thread::sleep_for(delay);
        delay *= 2;
    }

    return std::unexpected(std::move(lastError));
}

// =============================================================================
// ERROR SUGGESTIONS
// =============================================================================

/**
 * @brief Suggestion for error recovery
 */
struct ErrorSuggestion {
    std::string description;
    std::string action;  // "retry", "check_permissions", "free_disk_space", etc.
    bool userActionRequired = false;
};

/**
 * @brief Get recovery suggestions for an error code
 */
inline std::vector<ErrorSuggestion> getSuggestions(ErrorCode code) {
    std::vector<ErrorSuggestion> suggestions;

    switch (code) {
        case ErrorCode::FileAccessDenied:
            suggestions.push_back({
                "File may be in use by another program",
                "close_other_programs",
                true
            });
            suggestions.push_back({
                "Run the application as administrator",
                "run_as_admin",
                true
            });
            break;

        case ErrorCode::DiskFull:
        case ErrorCode::InsufficientDiskSpace:
            suggestions.push_back({
                "Free up disk space",
                "free_disk_space",
                true
            });
            suggestions.push_back({
                "Choose a different drive for backups",
                "change_backup_location",
                true
            });
            break;

        case ErrorCode::FileLocked:
            suggestions.push_back({
                "Close the game if it's running",
                "close_game",
                true
            });
            suggestions.push_back({
                "Wait a moment and try again",
                "retry",
                false
            });
            break;

        case ErrorCode::NetworkError:
        case ErrorCode::ConnectionFailed:
        case ErrorCode::Timeout:
            suggestions.push_back({
                "Check your internet connection",
                "check_network",
                true
            });
            suggestions.push_back({
                "Try again later",
                "retry",
                false
            });
            break;

        case ErrorCode::SignatureInvalid:
        case ErrorCode::SignatureRequired:
        case ErrorCode::ChecksumMismatch:
            suggestions.push_back({
                "Download the package again",
                "redownload",
                false
            });
            suggestions.push_back({
                "Report this to the package maintainer",
                "report_issue",
                true
            });
            break;

        case ErrorCode::VersionMismatch:
        case ErrorCode::IncompatibleVersion:
            suggestions.push_back({
                "Check for a newer translation package",
                "check_updates",
                false
            });
            suggestions.push_back({
                "The game may have been updated",
                "game_update_notice",
                false
            });
            break;

        case ErrorCode::GameNotFound:
        case ErrorCode::InvalidGamePath:
            suggestions.push_back({
                "Verify the game installation path",
                "verify_path",
                true
            });
            suggestions.push_back({
                "Scan for games again",
                "rescan",
                false
            });
            break;

        default:
            suggestions.push_back({
                "Try the operation again",
                "retry",
                false
            });
            break;
    }

    return suggestions;
}

} // namespace makine
