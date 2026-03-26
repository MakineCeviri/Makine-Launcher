#pragma once

/**
 * @file debug.hpp
 * @brief Debug dump utilities for Makine
 *
 * Provides:
 * - State dumping for debugging
 * - Object serialization for diagnostics
 * - Crash report generation
 * - Debug file management
 *
 * Usage:
 * @code
 * // Dump game info
 * DebugDumper::dumpGameInfo(gameInfo, "pre_patch");
 *
 * // Generate crash report
 * auto report = DebugDumper::generateCrashReport("Unexpected error", error);
 *
 * // Auto-dump on scope exit (if needed)
 * auto guard = DebugDumper::scopedDump("operation_name", [&]() {
 *     return dumpCurrentState();
 * });
 * @endcode
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include "makine/types.hpp"
#include "makine/error.hpp"
#include "makine/logging.hpp"
#include "makine/version.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#include <psapi.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

namespace makine {

/**
 * @brief Debug dump configuration
 */
struct DebugConfig {
    fs::path dumpDirectory;     ///< Where to save dumps
    bool enableDumps = true;    ///< Master switch for dumps
    size_t maxDumpFiles = 100;  ///< Max dump files to keep
    size_t maxDumpSizeMB = 50;  ///< Max total dump size in MB
    bool includeStackTrace = true;  ///< Include stack trace in crash reports
    bool includeMemoryInfo = true;  ///< Include memory info
    bool includeEnvironment = false; ///< Include env vars (security risk)
};

/**
 * @brief Crash report data
 */
struct CrashReport {
    std::string reason;
    std::string errorMessage;
    std::string errorCode;
    std::string stackTrace;
    std::chrono::system_clock::time_point timestamp;

    // System info
    std::string osVersion;
    std::string architecture;
    uint64_t memoryUsageBytes = 0;
    uint64_t peakMemoryBytes = 0;

    // Makine context
    std::string version;
    std::vector<std::string> loadedModules;
    std::vector<std::pair<std::string, std::string>> context;

    /**
     * @brief Convert to human-readable text
     */
    [[nodiscard]] std::string toText() const {
        std::ostringstream oss;

        oss << "=== Makine Crash Report ===\n\n";

        // Timestamp
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        oss << "Timestamp: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "\n";
        oss << "Version: " << version << "\n\n";

        // Error info
        oss << "--- Error ---\n";
        oss << "Reason: " << reason << "\n";
        oss << "Message: " << errorMessage << "\n";
        if (!errorCode.empty()) {
            oss << "Code: " << errorCode << "\n";
        }
        oss << "\n";

        // System info
        oss << "--- System ---\n";
        oss << "OS: " << osVersion << "\n";
        oss << "Architecture: " << architecture << "\n";
        oss << "Memory Usage: " << (memoryUsageBytes / 1024 / 1024) << " MB\n";
        oss << "Peak Memory: " << (peakMemoryBytes / 1024 / 1024) << " MB\n\n";

        // Context
        if (!context.empty()) {
            oss << "--- Context ---\n";
            for (const auto& [key, value] : context) {
                oss << key << ": " << value << "\n";
            }
            oss << "\n";
        }

        // Stack trace
        if (!stackTrace.empty()) {
            oss << "--- Stack Trace ---\n";
            oss << stackTrace << "\n";
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

        oss << "{\n";
        oss << "  \"timestamp\": \"" << ts.str() << "\",\n";
        oss << "  \"version\": \"" << version << "\",\n";
        oss << "  \"reason\": \"" << escapeJson(reason) << "\",\n";
        oss << "  \"errorMessage\": \"" << escapeJson(errorMessage) << "\",\n";
        oss << "  \"errorCode\": \"" << escapeJson(errorCode) << "\",\n";
        oss << "  \"system\": {\n";
        oss << "    \"os\": \"" << escapeJson(osVersion) << "\",\n";
        oss << "    \"architecture\": \"" << architecture << "\",\n";
        oss << "    \"memoryUsageBytes\": " << memoryUsageBytes << ",\n";
        oss << "    \"peakMemoryBytes\": " << peakMemoryBytes << "\n";
        oss << "  },\n";

        oss << "  \"context\": {\n";
        bool first = true;
        for (const auto& [key, value] : context) {
            if (!first) oss << ",\n";
            first = false;
            oss << "    \"" << escapeJson(key) << "\": \"" << escapeJson(value) << "\"";
        }
        oss << "\n  },\n";

        oss << "  \"stackTrace\": \"" << escapeJson(stackTrace) << "\"\n";
        oss << "}\n";

        return oss.str();
    }

private:
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

/**
 * @brief RAII guard for dumping state on scope exit
 */
class ScopedDumpGuard {
public:
    using DumpFunction = std::function<std::string()>;

    ScopedDumpGuard(std::string name, DumpFunction dumpFn, bool dumpOnSuccess = false)
        : name_(std::move(name))
        , dumpFn_(std::move(dumpFn))
        , dumpOnSuccess_(dumpOnSuccess)
        , success_(false)
    {}

    ~ScopedDumpGuard() {
        if (dumpOnSuccess_ || !success_) {
            try {
                if (dumpFn_) {
                    auto content = dumpFn_();
                    // Write to debug dump directory
                    // (simplified - full implementation would use DebugDumper)
                    MAKINE_LOG_DEBUG("DEBUG", "Scope dump for {}: {} bytes", name_, content.size());
                }
            } catch (...) {
                // Don't throw from destructor
            }
        }
    }

    void setSuccess(bool success = true) { success_ = success; }
    void cancel() { dumpFn_ = nullptr; }

private:
    std::string name_;
    DumpFunction dumpFn_;
    bool dumpOnSuccess_;
    bool success_;
};

/**
 * @brief Debug dump utilities
 */
class DebugDumper {
public:
    /**
     * @brief Get singleton instance
     */
    static DebugDumper& instance() {
        static DebugDumper dumper;
        return dumper;
    }

    /**
     * @brief Configure debug dumper
     */
    void configure(const DebugConfig& config) {
        std::lock_guard lock(mutex_);
        config_ = config;

        // Create dump directory if needed
        if (!config_.dumpDirectory.empty()) {
            std::error_code ec;
            fs::create_directories(config_.dumpDirectory, ec);
        }
    }

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const DebugConfig& config() const {
        return config_;
    }

    /**
     * @brief Dump arbitrary state with a label
     */
    [[nodiscard]] Result<fs::path> dumpState(const std::string& label, const std::string& content) {
        if (!config_.enableDumps) {
            return std::unexpected(Error(ErrorCode::NotSupported,
                "Debug dumps are disabled"));
        }

        auto path = getDumpPath(label, "txt");

        try {
            std::ofstream ofs(path);
            if (!ofs.is_open()) {
                return std::unexpected(Error(ErrorCode::FileCreateFailed,
                    "Failed to create dump file")
                    .withFile(path));
            }

            ofs << "=== Makine Debug Dump ===\n";
            ofs << "Label: " << label << "\n";
            ofs << "Timestamp: " << getCurrentTimestamp() << "\n";
            ofs << "================================\n\n";
            ofs << content;

            MAKINE_LOG_DEBUG("DEBUG", "Created dump: {}", path.string());
            cleanupOldDumps();
            return path;

        } catch (const std::exception& e) {
            return std::unexpected(Error(ErrorCode::FileWriteFailed,
                std::string("Failed to write dump: ") + e.what())
                .withFile(path));
        }
    }

    /**
     * @brief Dump game info
     */
    [[nodiscard]] Result<fs::path> dumpGameInfo(const GameInfo& game, const std::string& label = "") {
        std::ostringstream oss;

        oss << "Game Info Dump\n";
        oss << "==============\n\n";

        oss << "ID: " << game.id.toString() << "\n";
        oss << "Name: " << game.name << "\n";
        oss << "Path: " << game.installPath.string() << "\n";
        oss << "Engine: " << static_cast<int>(game.engine) << "\n";
        oss << "Store: " << static_cast<int>(game.store) << "\n";
        oss << "Version: " << game.version << "\n";
        oss << "Detected: " << game.detectedAt << "\n\n";

        oss << "Executable: " << game.executablePath.string() << "\n";
        oss << "Data Path: " << game.dataPath.string() << "\n\n";

        oss << "Engine Confidence: " << game.engineConfidence << "\n";
        oss << "Engine Version: " << game.engineVersion << "\n\n";

        oss << "Has Translation: " << (game.hasTranslation ? "Yes" : "No") << "\n";
        if (!game.translatedLanguages.empty()) {
            oss << "Languages: ";
            for (const auto& lang : game.translatedLanguages) {
                oss << lang << " ";
            }
            oss << "\n";
        }

        std::string dumpLabel = label.empty() ? "game_" + game.id.toString() : label;
        return dumpState(dumpLabel, oss.str());
    }

    /**
     * @brief Generate crash report
     */
    CrashReport generateCrashReport(const std::string& reason, const Error& error) {
        CrashReport report;
        report.timestamp = std::chrono::system_clock::now();
        report.reason = reason;
        report.errorMessage = error.message();
        report.errorCode = std::to_string(static_cast<int>(error.code()));
        report.version = MAKINE_VERSION_STRING;

        // System info
#ifdef _WIN32
        report.architecture = "x64";
        report.osVersion = "Windows";

        if (config_.includeMemoryInfo) {
            PROCESS_MEMORY_COUNTERS_EX pmc;
            if (GetProcessMemoryInfo(GetCurrentProcess(),
                    reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                    sizeof(pmc))) {
                report.memoryUsageBytes = pmc.WorkingSetSize;
                report.peakMemoryBytes = pmc.PeakWorkingSetSize;
            }
        }

        if (config_.includeStackTrace) {
            report.stackTrace = captureStackTrace();
        }
#else
        report.architecture = "unknown";
        report.osVersion = "unknown";
#endif

        return report;
    }

    /**
     * @brief Save crash report to file
     */
    [[nodiscard]] Result<fs::path> saveCrashReport(const CrashReport& report) {
        auto txtPath = getDumpPath("crash", "txt");
        auto jsonPath = getDumpPath("crash", "json");

        try {
            // Save text version
            {
                std::ofstream ofs(txtPath);
                if (ofs.is_open()) {
                    ofs << report.toText();
                }
            }

            // Save JSON version
            {
                std::ofstream ofs(jsonPath);
                if (ofs.is_open()) {
                    ofs << report.toJson();
                }
            }

            MAKINE_LOG_ERROR("DEBUG", "Crash report saved: {}", txtPath.string());
            return txtPath;

        } catch (const std::exception& e) {
            return std::unexpected(Error(ErrorCode::FileWriteFailed,
                std::string("Failed to save crash report: ") + e.what()));
        }
    }

    /**
     * @brief Create scoped dump guard
     */
    [[nodiscard]] ScopedDumpGuard scopedDump(
        const std::string& name,
        ScopedDumpGuard::DumpFunction dumpFn,
        bool dumpOnSuccess = false
    ) {
        return ScopedDumpGuard(name, std::move(dumpFn), dumpOnSuccess);
    }

    /**
     * @brief Dump current memory stats
     */
    [[nodiscard]] Result<fs::path> dumpMemoryStats() {
        std::ostringstream oss;

        oss << "Memory Statistics\n";
        oss << "=================\n\n";

#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(),
                reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                sizeof(pmc))) {
            oss << "Working Set: " << (pmc.WorkingSetSize / 1024 / 1024) << " MB\n";
            oss << "Peak Working Set: " << (pmc.PeakWorkingSetSize / 1024 / 1024) << " MB\n";
            oss << "Private Usage: " << (pmc.PrivateUsage / 1024 / 1024) << " MB\n";
            oss << "Page Faults: " << pmc.PageFaultCount << "\n";
        }

        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memInfo)) {
            oss << "\nSystem Memory:\n";
            oss << "Total Physical: " << (memInfo.ullTotalPhys / 1024 / 1024 / 1024) << " GB\n";
            oss << "Available Physical: " << (memInfo.ullAvailPhys / 1024 / 1024 / 1024) << " GB\n";
            oss << "Memory Load: " << memInfo.dwMemoryLoad << "%\n";
        }
#else
        oss << "(Memory stats not available on this platform)\n";
#endif

        return dumpState("memory_stats", oss.str());
    }

    /**
     * @brief List all dump files
     */
    [[nodiscard]] std::vector<fs::path> listDumps() const {
        std::vector<fs::path> dumps;

        if (config_.dumpDirectory.empty() || !fs::exists(config_.dumpDirectory)) {
            return dumps;
        }

        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(config_.dumpDirectory, ec)) {
            if (entry.is_regular_file() &&
                entry.path().filename().string().starts_with("makine_")) {
                dumps.push_back(entry.path());
            }
        }

        // Sort by modification time (newest first)
        std::sort(dumps.begin(), dumps.end(), [](const fs::path& a, const fs::path& b) {
            std::error_code ec;
            return fs::last_write_time(a, ec) > fs::last_write_time(b, ec);
        });

        return dumps;
    }

    /**
     * @brief Clear all dump files
     */
    void clearDumps() {
        if (config_.dumpDirectory.empty()) return;

        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(config_.dumpDirectory, ec)) {
            if (entry.is_regular_file() &&
                entry.path().filename().string().starts_with("makine_")) {
                fs::remove(entry.path(), ec);
            }
        }

        MAKINE_LOG_INFO("DEBUG", "Cleared all dump files");
    }

private:
    DebugDumper() {
        // Default dump directory
        config_.dumpDirectory = fs::temp_directory_path() / "makine_dumps";
        std::error_code ec;
        fs::create_directories(config_.dumpDirectory, ec);
    }

    [[nodiscard]] fs::path getDumpPath(const std::string& label, const std::string& ext) const {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);

        std::ostringstream ss;
        ss << "makine_" << label << "_";
        ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
        ss << "." << ext;

        return config_.dumpDirectory / ss.str();
    }

    [[nodiscard]] std::string getCurrentTimestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);

        std::ostringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    void cleanupOldDumps() {
        auto dumps = listDumps();

        // Remove excess files
        while (dumps.size() > config_.maxDumpFiles) {
            std::error_code ec;
            fs::remove(dumps.back(), ec);
            dumps.pop_back();
        }

        // Check total size
        uint64_t totalSize = 0;
        for (const auto& dump : dumps) {
            std::error_code ec;
            totalSize += fs::file_size(dump, ec);
        }

        uint64_t maxSize = config_.maxDumpSizeMB * 1024 * 1024;
        while (totalSize > maxSize && !dumps.empty()) {
            std::error_code ec;
            totalSize -= fs::file_size(dumps.back(), ec);
            fs::remove(dumps.back(), ec);
            dumps.pop_back();
        }
    }

#ifdef _WIN32
    [[nodiscard]] std::string captureStackTrace() const {
        std::ostringstream oss;

        void* stack[64];
        USHORT frames = CaptureStackBackTrace(0, 64, stack, nullptr);

        HANDLE process = GetCurrentProcess();
        SymInitialize(process, nullptr, TRUE);

        char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
        PSYMBOL_INFO symbol = reinterpret_cast<PSYMBOL_INFO>(buffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;

        for (USHORT i = 0; i < frames; ++i) {
            DWORD64 address = reinterpret_cast<DWORD64>(stack[i]);

            if (SymFromAddr(process, address, nullptr, symbol)) {
                oss << "[" << i << "] " << symbol->Name << " (0x"
                    << std::hex << symbol->Address << ")\n";
            } else {
                oss << "[" << i << "] (unknown) (0x"
                    << std::hex << address << ")\n";
            }
        }

        SymCleanup(process);
        return oss.str();
    }
#else
    [[nodiscard]] std::string captureStackTrace() const {
        return "(Stack trace not available on this platform)";
    }
#endif

    DebugConfig config_;
    mutable std::mutex mutex_;
};

/**
 * @brief Convenience function to access global debug dumper
 */
inline DebugDumper& debugDumper() {
    return DebugDumper::instance();
}

/**
 * @brief Quick state dump
 */
inline Result<fs::path> dumpState(const std::string& label, const std::string& content) {
    return DebugDumper::instance().dumpState(label, content);
}

/**
 * @brief Quick crash report
 */
inline CrashReport generateCrashReport(const std::string& reason, const Error& error) {
    return DebugDumper::instance().generateCrashReport(reason, error);
}

} // namespace makine
