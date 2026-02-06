/**
 * @file error.hpp
 * @brief MakineAI error handling and result types
 * @copyright (c) 2026 MakineAI Team
 */

#pragma once

#include <exception>
#include <expected>
#include <source_location>
#include <string>
#include <string_view>

namespace makineai {

/**
 * @brief Error categories for MakineAI operations
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

        default:                            return "Unknown error code";
    }
}

/**
 * @brief Detailed error information
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

    [[nodiscard]] ErrorCode code() const noexcept { return code_; }
    [[nodiscard]] const std::string& message() const noexcept { return message_; }
    [[nodiscard]] const char* file() const noexcept { return file_; }
    [[nodiscard]] uint32_t line() const noexcept { return line_; }
    [[nodiscard]] const char* function() const noexcept { return function_; }

    [[nodiscard]] bool isSuccess() const noexcept { return code_ == ErrorCode::Success; }
    [[nodiscard]] bool isError() const noexcept { return code_ != ErrorCode::Success; }

    [[nodiscard]] explicit operator bool() const noexcept { return isError(); }

    [[nodiscard]] std::string fullMessage() const {
        if (code_ == ErrorCode::Success) return "Success";
        return message_ + " [" + std::string(function_) + " at " +
               std::string(file_) + ":" + std::to_string(line_) + "]";
    }

private:
    ErrorCode code_;
    std::string message_;
    const char* file_ = "";
    uint32_t line_ = 0;
    const char* function_ = "";
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
 * @brief MakineAI exception base class
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
#define MAKINEAI_ERROR(code, msg) \
    ::makineai::Error(::makineai::ErrorCode::code, msg)

#define MAKINEAI_SUCCESS \
    ::makineai::Error(::makineai::ErrorCode::Success)

// Helper for returning errors
#define MAKINEAI_TRY(expr) \
    do { \
        auto&& _result = (expr); \
        if (!_result) return std::unexpected(_result.error()); \
    } while (0)

} // namespace makineai
