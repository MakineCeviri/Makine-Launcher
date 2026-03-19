#pragma once

/**
 * @file audit.hpp
 * @brief Audit logging and secure memory utilities for Makine
 *
 * Provides:
 * - AuditLogger: Security-critical operation logging
 * - SecureBuffer: Memory that's zeroed on destruction
 * - AuditEvent: Structured audit event data
 *
 * Usage:
 * @code
 * // Log file access
 * AuditLogger::logFileAccess(path, "read");
 *
 * // Log network request
 * AuditLogger::logNetworkRequest(url, "GET");
 *
 * // Log patch operation
 * AuditLogger::logPatchOperation(gameId, true);
 *
 * // Secure buffer for sensitive data
 * SecureBuffer<char> apiKey(64);
 * // ... use apiKey.data() ...
 * // Memory is automatically zeroed when apiKey goes out of scope
 * @endcode
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include "makine/types.hpp"
#include "makine/error.hpp"
#include "makine/logging.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace makine {

// =============================================================================
// AUDIT EVENTS
// =============================================================================

/**
 * @brief Audit event severity level
 */
enum class AuditSeverity {
    Info,       ///< Informational
    Warning,    ///< Potential security concern
    Critical,   ///< Security-critical event
    Alert       ///< Immediate attention needed
};

/**
 * @brief Audit event category
 */
enum class AuditCategory {
    FileAccess,         ///< File read/write operations
    NetworkRequest,     ///< Network requests
    SignatureVerify,    ///< Signature verification
    PatchOperation,     ///< Patch/restore operations
    ConfigChange,       ///< Configuration changes
    Authentication,     ///< Authentication events
    Authorization,      ///< Authorization checks
    DataExport,         ///< Data export/download
    SystemEvent         ///< System-level events
};

/**
 * @brief Structured audit event
 */
struct AuditEvent {
    std::chrono::system_clock::time_point timestamp;
    AuditSeverity severity = AuditSeverity::Info;
    AuditCategory category = AuditCategory::SystemEvent;
    std::string action;
    std::string target;
    std::string details;
    bool success = true;
    std::string userId;         // Optional user identifier
    std::string sessionId;      // Optional session identifier
    std::string sourceIp;       // Optional source IP

    /**
     * @brief Convert to log string
     */
    [[nodiscard]] std::string toLogString() const {
        std::ostringstream oss;

        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");

        oss << " [" << severityToString(severity) << "] ";
        oss << "[" << categoryToString(category) << "] ";
        oss << action;

        if (!target.empty()) {
            oss << " target=\"" << target << "\"";
        }
        if (!details.empty()) {
            oss << " details=\"" << details << "\"";
        }
        oss << " success=" << (success ? "true" : "false");

        if (!userId.empty()) {
            oss << " user=\"" << userId << "\"";
        }
        if (!sessionId.empty()) {
            oss << " session=\"" << sessionId << "\"";
        }

        return oss.str();
    }

    /**
     * @brief Convert to JSON
     */
    [[nodiscard]] std::string toJson() const {
        std::ostringstream oss;

        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        std::ostringstream ts;
        ts << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");

        oss << "{";
        oss << "\"timestamp\":\"" << ts.str() << "\",";
        oss << "\"severity\":\"" << severityToString(severity) << "\",";
        oss << "\"category\":\"" << categoryToString(category) << "\",";
        oss << "\"action\":\"" << escapeJson(action) << "\",";
        oss << "\"target\":\"" << escapeJson(target) << "\",";
        oss << "\"details\":\"" << escapeJson(details) << "\",";
        oss << "\"success\":" << (success ? "true" : "false");

        if (!userId.empty()) {
            oss << ",\"userId\":\"" << escapeJson(userId) << "\"";
        }
        if (!sessionId.empty()) {
            oss << ",\"sessionId\":\"" << escapeJson(sessionId) << "\"";
        }
        if (!sourceIp.empty()) {
            oss << ",\"sourceIp\":\"" << sourceIp << "\"";
        }

        oss << "}";
        return oss.str();
    }

private:
    static const char* severityToString(AuditSeverity sev) {
        switch (sev) {
            case AuditSeverity::Info: return "INFO";
            case AuditSeverity::Warning: return "WARN";
            case AuditSeverity::Critical: return "CRIT";
            case AuditSeverity::Alert: return "ALERT";
            default: return "UNKNOWN";
        }
    }

    static const char* categoryToString(AuditCategory cat) {
        switch (cat) {
            case AuditCategory::FileAccess: return "FILE";
            case AuditCategory::NetworkRequest: return "NET";
            case AuditCategory::SignatureVerify: return "SIG";
            case AuditCategory::PatchOperation: return "PATCH";
            case AuditCategory::ConfigChange: return "CONFIG";
            case AuditCategory::Authentication: return "AUTH";
            case AuditCategory::Authorization: return "AUTHZ";
            case AuditCategory::DataExport: return "EXPORT";
            case AuditCategory::SystemEvent: return "SYSTEM";
            default: return "UNKNOWN";
        }
    }

    static std::string escapeJson(const std::string& str) {
        std::string result;
        result.reserve(str.length());
        for (char c : str) {
            switch (c) {
                case '"':  result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default:   result += c; break;
            }
        }
        return result;
    }
};

// =============================================================================
// AUDIT LOGGER
// =============================================================================

/**
 * @brief Configuration for audit logger
 */
struct AuditConfig {
    bool enabled = true;                    ///< Master switch
    fs::path logFile;                       ///< Log file path
    AuditSeverity minSeverity = AuditSeverity::Info;  ///< Minimum severity to log
    size_t maxLogSizeMB = 100;              ///< Max log file size
    size_t maxRetainedLogs = 10;            ///< Max number of rotated logs
    bool logToConsole = false;              ///< Also log to console
    bool logToFile = true;                  ///< Log to file
    size_t inMemoryBufferSize = 1000;       ///< Events to keep in memory
};

/**
 * @brief Audit callback for custom handling
 */
using AuditCallback = std::function<void(const AuditEvent&)>;

/**
 * @brief Thread-safe audit logger singleton
 */
class AuditLogger {
public:
    /**
     * @brief Get singleton instance
     */
    static AuditLogger& instance() {
        static AuditLogger logger;
        return logger;
    }

    /**
     * @brief Configure the audit logger
     */
    void configure(const AuditConfig& config) {
        std::lock_guard lock(mutex_);
        config_ = config;

        if (config_.logToFile && !config_.logFile.empty()) {
            // Create directory if needed
            std::error_code ec;
            fs::create_directories(config_.logFile.parent_path(), ec);

            // Open log file
            logStream_.open(config_.logFile, std::ios::app);
        }
    }

    /**
     * @brief Add a callback for custom audit event handling
     */
    void addCallback(AuditCallback callback) {
        std::lock_guard lock(mutex_);
        callbacks_.push_back(std::move(callback));
    }

/**     * @brief Clear all callbacks (useful for test isolation)     */    void clearCallbacks() {        std::lock_guard lock(mutex_);        callbacks_.clear();    }
    /**
     * @brief Log a generic audit event
     */
    void log(AuditEvent event) {
        if (!config_.enabled) return;
        if (event.severity < config_.minSeverity) return;

        event.timestamp = std::chrono::system_clock::now();

        std::lock_guard lock(mutex_);

        // Write to file
        if (config_.logToFile && logStream_.is_open()) {
            logStream_ << event.toLogString() << "\n";
            logStream_.flush();
            checkRotation();
        }

        // Write to console
        if (config_.logToConsole) {
            MAKINE_LOG_INFO("AUDIT", "{}", event.toLogString());
        }

        // Keep in memory buffer
        events_.push_back(event);
        while (events_.size() > config_.inMemoryBufferSize) {
            events_.pop_front();
        }

        // Call callbacks
        for (const auto& callback : callbacks_) {
            try {
                callback(event);
            } catch (...) {
                // Don't let callback errors affect logging
            }
        }

        ++eventCount_;
    }

    // =========================================================================
    // CONVENIENCE LOGGING METHODS
    // =========================================================================

    /**
     * @brief Log file access operation
     */
    static void logFileAccess(const fs::path& path, const std::string& operation,
                              bool success = true, const std::string& details = "") {
        AuditEvent event;
        event.category = AuditCategory::FileAccess;
        event.action = operation;
        event.target = path.string();
        event.details = details;
        event.success = success;
        event.severity = success ? AuditSeverity::Info : AuditSeverity::Warning;

        instance().log(std::move(event));
    }

    /**
     * @brief Log network request
     */
    static void logNetworkRequest(const std::string& url, const std::string& method,
                                  bool success = true, const std::string& details = "") {
        AuditEvent event;
        event.category = AuditCategory::NetworkRequest;
        event.action = method;
        event.target = url;
        event.details = details;
        event.success = success;
        event.severity = success ? AuditSeverity::Info : AuditSeverity::Warning;

        instance().log(std::move(event));
    }

    /**
     * @brief Log signature verification
     */
    static void logSignatureVerification(const std::string& packageId, bool success,
                                         const std::string& details = "") {
        AuditEvent event;
        event.category = AuditCategory::SignatureVerify;
        event.action = "verify";
        event.target = packageId;
        event.details = details;
        event.success = success;
        event.severity = success ? AuditSeverity::Info : AuditSeverity::Critical;

        instance().log(std::move(event));
    }

    /**
     * @brief Log patch operation
     */
    static void logPatchOperation(const std::string& gameId, bool success,
                                  const std::string& operation = "apply",
                                  const std::string& details = "") {
        AuditEvent event;
        event.category = AuditCategory::PatchOperation;
        event.action = operation;
        event.target = gameId;
        event.details = details;
        event.success = success;
        event.severity = success ? AuditSeverity::Info : AuditSeverity::Warning;

        instance().log(std::move(event));
    }

    /**
     * @brief Log configuration change
     */
    static void logConfigChange(const std::string& setting, const std::string& oldValue,
                                const std::string& newValue) {
        AuditEvent event;
        event.category = AuditCategory::ConfigChange;
        event.action = "change";
        event.target = setting;
        event.details = "old=" + oldValue + " new=" + newValue;
        event.success = true;
        event.severity = AuditSeverity::Info;

        instance().log(std::move(event));
    }

    /**
     * @brief Log data export
     */
    static void logDataExport(const std::string& dataType, const fs::path& destination,
                              bool success = true) {
        AuditEvent event;
        event.category = AuditCategory::DataExport;
        event.action = "export";
        event.target = destination.string();
        event.details = "type=" + dataType;
        event.success = success;
        event.severity = AuditSeverity::Info;

        instance().log(std::move(event));
    }

    /**
     * @brief Log system event
     */
    static void logSystemEvent(const std::string& event_name,
                               const std::string& details = "",
                               AuditSeverity severity = AuditSeverity::Info) {
        AuditEvent event;
        event.category = AuditCategory::SystemEvent;
        event.action = event_name;
        event.details = details;
        event.success = true;
        event.severity = severity;

        instance().log(std::move(event));
    }

    // =========================================================================
    // QUERY METHODS
    // =========================================================================

    /**
     * @brief Get recent events from memory buffer
     */
    [[nodiscard]] std::vector<AuditEvent> getRecentEvents(size_t count = 100) const {
        std::lock_guard lock(mutex_);
        std::vector<AuditEvent> result;

        size_t start = events_.size() > count ? events_.size() - count : 0;
        for (size_t i = start; i < events_.size(); ++i) {
            result.push_back(events_[i]);
        }

        return result;
    }

    /**
     * @brief Get events by category
     */
    [[nodiscard]] std::vector<AuditEvent> getEventsByCategory(
        AuditCategory category, size_t maxCount = 100) const
    {
        std::lock_guard lock(mutex_);
        std::vector<AuditEvent> result;

        for (auto it = events_.rbegin(); it != events_.rend() && result.size() < maxCount; ++it) {
            if (it->category == category) {
                result.push_back(*it);
            }
        }

        std::reverse(result.begin(), result.end());
        return result;
    }

    /**
     * @brief Get total event count
     */
    [[nodiscard]] uint64_t totalEvents() const noexcept {
        return eventCount_.load();
    }

    /**
     * @brief Export events to JSON file
     */
    [[nodiscard]] Result<fs::path> exportToJson(const fs::path& path) const {
        try {
            std::ofstream ofs(path);
            if (!ofs.is_open()) {
                return std::unexpected(Error(ErrorCode::FileCreateFailed,
                    "Failed to create export file").withFile(path));
            }

            std::lock_guard lock(mutex_);

            ofs << "[\n";
            bool first = true;
            for (const auto& event : events_) {
                if (!first) ofs << ",\n";
                first = false;
                ofs << "  " << event.toJson();
            }
            ofs << "\n]\n";

            return path;

        } catch (const std::exception& e) {
            return std::unexpected(Error(ErrorCode::FileWriteFailed,
                std::string("Export failed: ") + e.what()));
        }
    }

private:
    AuditLogger() {
        // Default log path
        config_.logFile = fs::temp_directory_path() / "makine" / "audit.log";
    }

    void checkRotation() {
        if (!config_.logToFile || config_.logFile.empty()) return;

        std::error_code ec;
        auto size = fs::file_size(config_.logFile, ec);
        if (ec || size < config_.maxLogSizeMB * 1024 * 1024) return;

        // Rotate log file
        logStream_.close();

        // Move old logs
        for (int i = static_cast<int>(config_.maxRetainedLogs) - 1; i >= 0; --i) {
            fs::path oldPath = config_.logFile;
            oldPath += "." + std::to_string(i);

            fs::path newPath = config_.logFile;
            newPath += "." + std::to_string(i + 1);

            if (fs::exists(oldPath, ec)) {
                if (i + 1 >= static_cast<int>(config_.maxRetainedLogs)) {
                    fs::remove(oldPath, ec);
                } else {
                    fs::rename(oldPath, newPath, ec);
                }
            }
        }

        // Rename current log
        fs::path rotatedPath = config_.logFile;
        rotatedPath += ".0";
        fs::rename(config_.logFile, rotatedPath, ec);

        // Reopen
        logStream_.open(config_.logFile, std::ios::app);
    }

    AuditConfig config_;
    mutable std::mutex mutex_;
    std::ofstream logStream_;
    std::deque<AuditEvent> events_;
    std::vector<AuditCallback> callbacks_;
    std::atomic<uint64_t> eventCount_{0};
};

/**
 * @brief Convenience function to access audit logger
 */
inline AuditLogger& audit() {
    return AuditLogger::instance();
}

// =============================================================================
// SECURE MEMORY
// =============================================================================

/**
 * @brief Custom deleter that zeros memory before freeing
 */
template<typename T>
struct SecureDeleter {
    void operator()(T* ptr) const {
        if (ptr) {
            // Zero the memory
            volatile T* vptr = ptr;
            size_t count = 1;  // For single objects

            // Use volatile to prevent optimization
            for (size_t i = 0; i < count; ++i) {
                vptr[i] = T{};
            }

#ifdef _WIN32
            // Windows-specific secure zeroing
            SecureZeroMemory(ptr, sizeof(T));
#endif

            delete ptr;
        }
    }
};

/**
 * @brief Array deleter that zeros memory before freeing
 */
template<typename T>
struct SecureArrayDeleter {
    size_t size;

    explicit SecureArrayDeleter(size_t sz = 0) : size(sz) {}

    void operator()(T* ptr) const {
        if (ptr && size > 0) {
            // Zero the memory using volatile to prevent optimization
            volatile T* vptr = ptr;
            for (size_t i = 0; i < size; ++i) {
                vptr[i] = T{};
            }

#ifdef _WIN32
            SecureZeroMemory(ptr, size * sizeof(T));
#else
            // POSIX: explicit_bzero if available
            std::memset(ptr, 0, size * sizeof(T));
#endif

            delete[] ptr;
        }
    }
};

/**
 * @brief Secure buffer that zeros memory on destruction
 *
 * Use for sensitive data like API keys, passwords, etc.
 *
 * @tparam T Element type (usually char or uint8_t)
 */
template<typename T>
class SecureBuffer {
public:
    /**
     * @brief Create secure buffer of given size
     */
    explicit SecureBuffer(size_t size)
        : data_(new T[size], SecureArrayDeleter<T>(size))
        , size_(size)
    {
        // Initialize to zero
        std::memset(data_.get(), 0, size * sizeof(T));
    }

    /**
     * @brief Create secure buffer from existing data (copies and zeros original)
     */
    SecureBuffer(const T* source, size_t size)
        : data_(new T[size], SecureArrayDeleter<T>(size))
        , size_(size)
    {
        std::memcpy(data_.get(), source, size * sizeof(T));
    }

    // Non-copyable to prevent accidental data exposure
    SecureBuffer(const SecureBuffer&) = delete;
    SecureBuffer& operator=(const SecureBuffer&) = delete;

    // Movable
    SecureBuffer(SecureBuffer&& other) noexcept
        : data_(std::move(other.data_))
        , size_(other.size_)
    {
        other.size_ = 0;
    }

    SecureBuffer& operator=(SecureBuffer&& other) noexcept {
        if (this != &other) {
            data_ = std::move(other.data_);
            size_ = other.size_;
            other.size_ = 0;
        }
        return *this;
    }

    /**
     * @brief Get pointer to data
     */
    [[nodiscard]] T* data() noexcept { return data_.get(); }
    [[nodiscard]] const T* data() const noexcept { return data_.get(); }

    /**
     * @brief Get buffer size
     */
    [[nodiscard]] size_t size() const noexcept { return size_; }

    /**
     * @brief Check if buffer is empty
     */
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    /**
     * @brief Array access
     */
    [[nodiscard]] T& operator[](size_t idx) { return data_[idx]; }
    [[nodiscard]] const T& operator[](size_t idx) const { return data_[idx]; }

    /**
     * @brief Clear buffer contents (zero memory)
     */
    void clear() {
        if (data_ && size_ > 0) {
            volatile T* vptr = data_.get();
            for (size_t i = 0; i < size_; ++i) {
                vptr[i] = T{};
            }
        }
    }

    /**
     * @brief Convert to string_view (for char buffers)
     */
    template<typename U = T>
    [[nodiscard]] std::enable_if_t<std::is_same_v<U, char>, std::string_view>
    view() const noexcept {
        return std::string_view(data_.get(), size_);
    }

private:
    std::unique_ptr<T[], SecureArrayDeleter<T>> data_;
    size_t size_;
};

/**
 * @brief Convenience aliases
 */
using SecureString = SecureBuffer<char>;
using SecureBytes = SecureBuffer<uint8_t>;

/**
 * @brief Securely zero a memory region
 */
inline void secureZero(void* ptr, size_t size) {
    if (!ptr || size == 0) return;

    volatile char* vptr = static_cast<volatile char*>(ptr);
    for (size_t i = 0; i < size; ++i) {
        vptr[i] = 0;
    }

#ifdef _WIN32
    SecureZeroMemory(ptr, size);
#endif
}

/**
 * @brief Securely compare two memory regions (constant-time)
 *
 * Prevents timing attacks by always comparing all bytes.
 */
[[nodiscard]] inline bool secureCompare(const void* a, const void* b, size_t size) {
    const volatile uint8_t* va = static_cast<const volatile uint8_t*>(a);
    const volatile uint8_t* vb = static_cast<const volatile uint8_t*>(b);

    volatile uint8_t result = 0;
    for (size_t i = 0; i < size; ++i) {
        result |= va[i] ^ vb[i];
    }

    return result == 0;
}

} // namespace makine
