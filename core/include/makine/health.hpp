#pragma once

/**
 * @file health.hpp
 * @brief System health check utilities for Makine
 *
 * Provides:
 * - HealthStatus: Overall system health report
 * - HealthChecker: Comprehensive health validation
 * - Component health checks (database, filesystem, network, memory)
 *
 * Usage:
 * @code
 * auto health = HealthChecker::instance().check();
 * if (!health.isHealthy()) {
 *     for (const auto& error : health.errors) {
 *         std::cerr << "ERROR: " << error << std::endl;
 *     }
 * }
 *
 * // Quick check
 * if (HealthChecker::instance().isHealthy()) {
 *     // System is operational
 * }
 * @endcode
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <set>

#include "makine/types.hpp"
#include "makine/error.hpp"
#include "makine/logging.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#include <psapi.h>
#else
#include <sys/statvfs.h>
#include <unistd.h>
#endif

namespace makine {

/**
 * @brief Health check result for a single component
 */
struct ComponentHealth {
    std::string name;                       ///< Component name
    bool healthy = true;                    ///< Is component healthy?
    std::string status = "OK";              ///< Status message
    std::chrono::milliseconds latency{0};   ///< Check latency
    std::vector<std::string> details;       ///< Additional details

    [[nodiscard]] bool isHealthy() const noexcept { return healthy; }
};

/**
 * @brief Overall system health status
 */
struct HealthStatus {
    bool healthy = true;                    ///< Overall health
    std::chrono::system_clock::time_point timestamp; ///< Check timestamp

    // Component health
    ComponentHealth database;
    ComponentHealth fileSystem;
    ComponentHealth memory;
    ComponentHealth network;

    // Resource metrics
    uint64_t availableDiskSpaceBytes = 0;
    uint64_t totalDiskSpaceBytes = 0;
    uint64_t memoryUsageBytes = 0;
    uint64_t peakMemoryUsageBytes = 0;
    uint64_t availableMemoryBytes = 0;

    // Aggregated issues
    std::vector<std::string> warnings;
    std::vector<std::string> errors;

    /**
     * @brief Check if system is healthy
     */
    [[nodiscard]] bool isHealthy() const noexcept {
        return healthy && errors.empty();
    }

    /**
     * @brief Check if system has warnings
     */
    [[nodiscard]] bool hasWarnings() const noexcept {
        return !warnings.empty();
    }

    /**
     * @brief Get disk space as percentage used
     */
    [[nodiscard]] double diskUsagePercent() const noexcept {
        if (totalDiskSpaceBytes == 0) return 0.0;
        return 100.0 * (1.0 - static_cast<double>(availableDiskSpaceBytes) /
                              totalDiskSpaceBytes);
    }

    /**
     * @brief Get human-readable disk space
     */
    [[nodiscard]] std::string availableDiskSpaceHuman() const {
        return formatBytes(availableDiskSpaceBytes);
    }

    /**
     * @brief Get human-readable memory usage
     */
    [[nodiscard]] std::string memoryUsageHuman() const {
        return formatBytes(memoryUsageBytes);
    }

    /**
     * @brief Convert to human-readable summary
     */
    [[nodiscard]] std::string toText() const {
        std::ostringstream oss;

        oss << "=== Makine Health Status ===\n";
        oss << "Overall: " << (healthy ? "HEALTHY" : "UNHEALTHY") << "\n\n";

        oss << "Components:\n";
        oss << "  Database:   " << (database.healthy ? "OK" : "FAIL")
            << " - " << database.status << "\n";
        oss << "  FileSystem: " << (fileSystem.healthy ? "OK" : "FAIL")
            << " - " << fileSystem.status << "\n";
        oss << "  Memory:     " << (memory.healthy ? "OK" : "FAIL")
            << " - " << memory.status << "\n";
        oss << "  Network:    " << (network.healthy ? "OK" : "FAIL")
            << " - " << network.status << "\n";

        oss << "\nResources:\n";
        oss << "  Disk Space: " << availableDiskSpaceHuman()
            << " available (" << static_cast<int>(diskUsagePercent()) << "% used)\n";
        oss << "  Memory:     " << memoryUsageHuman() << " used\n";

        if (!warnings.empty()) {
            oss << "\nWarnings:\n";
            for (const auto& w : warnings) {
                oss << "  - " << w << "\n";
            }
        }

        if (!errors.empty()) {
            oss << "\nErrors:\n";
            for (const auto& e : errors) {
                oss << "  - " << e << "\n";
            }
        }

        return oss.str();
    }

private:
    static std::string formatBytes(uint64_t bytes) {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        int unit = 0;
        double size = static_cast<double>(bytes);

        while (size >= 1024.0 && unit < 4) {
            size /= 1024.0;
            ++unit;
        }

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << size << " " << units[unit];
        return oss.str();
    }
};

/**
 * @brief System health checker
 */
class HealthChecker {
public:
    /**
     * @brief Get singleton instance
     */
    static HealthChecker& instance() {
        static HealthChecker checker;
        return checker;
    }

    /**
     * @brief Minimum required disk space (default: 100MB)
     */
    void setMinDiskSpace(uint64_t bytes) { minDiskSpaceBytes_ = bytes; }

    /**
     * @brief Maximum memory usage before warning (default: 500MB)
     */
    void setMaxMemoryUsage(uint64_t bytes) { maxMemoryUsageBytes_ = bytes; }

    /**
     * @brief Set data directory to check
     */
    void setDataDirectory(const fs::path& path) { dataDirectory_ = path; }

    /**
     * @brief Set database path to check
     */
    void setDatabasePath(const fs::path& path) { databasePath_ = path; }

    /**
     * @brief Perform full health check
     */
    [[nodiscard]] HealthStatus check() {
        HealthStatus status;
        status.timestamp = std::chrono::system_clock::now();
        status.healthy = true;

        // Check each component
        status.database = checkDatabase();
        status.fileSystem = checkFileSystem();
        status.memory = checkMemory();
        status.network = checkNetwork();

        // Get resource metrics
        getDiskSpace(status);
        getMemoryUsage(status);

        // Aggregate health
        if (!status.database.healthy) {
            status.healthy = false;
            status.errors.push_back("Database: " + status.database.status);
        }
        if (!status.fileSystem.healthy) {
            status.healthy = false;
            status.errors.push_back("FileSystem: " + status.fileSystem.status);
        }
        if (!status.memory.healthy) {
            status.healthy = false;
            status.errors.push_back("Memory: " + status.memory.status);
        }
        // Network is optional - only warn
        if (!status.network.healthy) {
            status.warnings.push_back("Network: " + status.network.status);
        }

        // Resource warnings
        if (status.availableDiskSpaceBytes < minDiskSpaceBytes_) {
            status.warnings.push_back(
                "Low disk space: " + status.availableDiskSpaceHuman() + " available");
        }
        if (status.memoryUsageBytes > maxMemoryUsageBytes_) {
            status.warnings.push_back(
                "High memory usage: " + status.memoryUsageHuman());
        }

        return status;
    }

    /**
     * @brief Quick health check (just overall status)
     */
    [[nodiscard]] bool isHealthy() {
        auto status = check();
        return status.isHealthy();
    }

    /**
     * @brief Check database health only
     */
    [[nodiscard]] ComponentHealth checkDatabase() {
        ComponentHealth health;
        health.name = "Database";
        auto start = std::chrono::steady_clock::now();

        if (databasePath_.empty()) {
            health.status = "No database path configured";
            health.healthy = true; // Not an error if not configured
        } else if (!fs::exists(databasePath_)) {
            health.status = "Database file not found";
            health.healthy = false;
        } else {
            // Try to open database file
            std::ifstream file(databasePath_, std::ios::binary);
            if (!file.is_open()) {
                health.status = "Cannot open database file";
                health.healthy = false;
            } else {
                // Check SQLite header
                char header[16] = {0};
                file.read(header, 16);
                if (std::string(header, 15) == "SQLite format 3") {
                    health.status = "OK";
                    health.healthy = true;
                } else {
                    health.status = "Invalid database format";
                    health.healthy = false;
                }
            }
        }

        health.latency = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);

        return health;
    }

    /**
     * @brief Check filesystem health
     */
    [[nodiscard]] ComponentHealth checkFileSystem() {
        ComponentHealth health;
        health.name = "FileSystem";
        auto start = std::chrono::steady_clock::now();

        fs::path checkDir = dataDirectory_.empty()
            ? fs::temp_directory_path()
            : dataDirectory_;

        if (!fs::exists(checkDir)) {
            health.status = "Data directory does not exist";
            health.healthy = false;
        } else {
            // Try to create and delete a test file
            auto testFile = checkDir / ".makine_health_check";
            try {
                {
                    std::ofstream ofs(testFile);
                    ofs << "health_check";
                }
                fs::remove(testFile);
                health.status = "OK";
                health.healthy = true;
            } catch (const std::exception& e) {
                health.status = std::string("Write test failed: ") + e.what();
                health.healthy = false;
            }
        }

        health.latency = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);

        return health;
    }

    /**
     * @brief Check memory health
     */
    [[nodiscard]] ComponentHealth checkMemory() {
        ComponentHealth health;
        health.name = "Memory";
        auto start = std::chrono::steady_clock::now();

        // Try to allocate a small buffer
        try {
            auto testBuffer = std::make_unique<char[]>(1024 * 1024); // 1MB
            testBuffer[0] = 'x';
            testBuffer[1024 * 1024 - 1] = 'y';
            health.status = "OK";
            health.healthy = true;
        } catch (const std::bad_alloc&) {
            health.status = "Memory allocation failed";
            health.healthy = false;
        }

        health.latency = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);

        return health;
    }

    /**
     * @brief Check network health (basic connectivity)
     */
    [[nodiscard]] ComponentHealth checkNetwork() {
        ComponentHealth health;
        health.name = "Network";
        auto start = std::chrono::steady_clock::now();

        // For now, just mark as OK - actual network check requires CURL
        // This is a placeholder for future implementation
        health.status = "OK (not fully verified)";
        health.healthy = true;

        health.latency = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);

        return health;
    }

private:
    HealthChecker() = default;

    void getDiskSpace(HealthStatus& status) {
        fs::path checkPath = dataDirectory_.empty()
            ? fs::current_path()
            : dataDirectory_;

        std::error_code ec;
        auto spaceInfo = fs::space(checkPath, ec);

        if (!ec) {
            status.availableDiskSpaceBytes = spaceInfo.available;
            status.totalDiskSpaceBytes = spaceInfo.capacity;
        }
    }

    void getMemoryUsage(HealthStatus& status) {
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(),
                                reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                                sizeof(pmc))) {
            status.memoryUsageBytes = pmc.WorkingSetSize;
            status.peakMemoryUsageBytes = pmc.PeakWorkingSetSize;
        }

        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memInfo)) {
            status.availableMemoryBytes = memInfo.ullAvailPhys;
        }
#else
        // Linux/macOS implementation would go here
        // For now, leave as 0
#endif
    }

    fs::path dataDirectory_;
    fs::path databasePath_;
    uint64_t minDiskSpaceBytes_ = 100 * 1024 * 1024;  // 100MB
    uint64_t maxMemoryUsageBytes_ = 500 * 1024 * 1024; // 500MB
};

/**
 * @brief Convenience function to access global health checker
 */
inline HealthChecker& healthChecker() {
    return HealthChecker::instance();
}

/**
 * @brief Quick health check
 */
inline bool isSystemHealthy() {
    return HealthChecker::instance().isHealthy();
}

} // namespace makine
