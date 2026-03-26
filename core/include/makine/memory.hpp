#pragma once

/**
 * @file memory.hpp
 * @brief Memory profiling and tracking utilities for Makine
 *
 * Provides:
 * - MemoryTracker: Track allocations by category
 * - ScopedMemoryTracker: RAII memory tracking
 * - Memory statistics and reporting
 *
 * Usage:
 * @code
 * // Start global tracking
 * MemoryTracker::startTracking();
 *
 * // Track a specific category
 * MemoryTracker::record("cache", 1024);  // Allocated 1KB
 * MemoryTracker::release("cache", 512);  // Released 512B
 *
 * // Scoped tracking
 * {
 *     ScopedMemoryTrack tracker("operation");
 *     // Memory allocations tracked automatically
 * }
 *
 * // Get report
 * auto report = MemoryTracker::getReport();
 * std::cout << report.toText() << std::endl;
 *
 * // Stop tracking
 * MemoryTracker::stopTracking();
 * @endcode
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <set>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#include <psapi.h>
#endif

namespace makine {

// =============================================================================
// MEMORY STATS
// =============================================================================

/**
 * @brief Memory statistics for a single category
 */
struct CategoryMemoryStats {
    std::string name;
    int64_t currentBytes = 0;      ///< Currently allocated
    int64_t peakBytes = 0;         ///< Peak allocation
    uint64_t totalAllocated = 0;   ///< Total allocated over time
    uint64_t totalReleased = 0;    ///< Total released over time
    uint64_t allocationCount = 0;  ///< Number of allocations
    uint64_t releaseCount = 0;     ///< Number of releases

    [[nodiscard]] double averageAllocationSize() const noexcept {
        return allocationCount > 0
            ? static_cast<double>(totalAllocated) / allocationCount
            : 0.0;
    }
};

/**
 * @brief Overall memory report
 */
struct MemoryReport {
    std::chrono::system_clock::time_point timestamp;

    // Process memory
    size_t processCurrentBytes = 0;
    size_t processPeakBytes = 0;
    size_t processPrivateBytes = 0;

    // System memory
    size_t systemTotalBytes = 0;
    size_t systemAvailableBytes = 0;
    uint32_t systemMemoryLoad = 0;  // Percentage

    // Tracked categories
    std::map<std::string, CategoryMemoryStats> categories;

    // Totals from tracking
    int64_t trackedCurrentBytes = 0;
    int64_t trackedPeakBytes = 0;
    uint64_t trackedTotalAllocated = 0;

    /**
     * @brief Convert to human-readable text
     */
    [[nodiscard]] std::string toText() const {
        std::ostringstream oss;

        oss << "=== Makine Memory Report ===\n";

        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        oss << "Generated: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "\n\n";

        // Process memory
        oss << "--- Process Memory ---\n";
        oss << "Current:     " << formatBytes(processCurrentBytes) << "\n";
        oss << "Peak:        " << formatBytes(processPeakBytes) << "\n";
        oss << "Private:     " << formatBytes(processPrivateBytes) << "\n\n";

        // System memory
        oss << "--- System Memory ---\n";
        oss << "Total:       " << formatBytes(systemTotalBytes) << "\n";
        oss << "Available:   " << formatBytes(systemAvailableBytes) << "\n";
        oss << "Load:        " << systemMemoryLoad << "%\n\n";

        // Tracked categories
        if (!categories.empty()) {
            oss << "--- Tracked Categories ---\n";
            for (const auto& [name, stats] : categories) {
                oss << name << ":\n";
                oss << "  Current:     " << formatBytes(stats.currentBytes) << "\n";
                oss << "  Peak:        " << formatBytes(stats.peakBytes) << "\n";
                oss << "  Allocations: " << stats.allocationCount
                    << " (avg " << formatBytes(static_cast<size_t>(stats.averageAllocationSize())) << ")\n";
            }
            oss << "\n";

            oss << "--- Tracked Totals ---\n";
            oss << "Current:     " << formatBytes(trackedCurrentBytes) << "\n";
            oss << "Peak:        " << formatBytes(trackedPeakBytes) << "\n";
            oss << "Cumulative:  " << formatBytes(trackedTotalAllocated) << "\n";
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

        oss << "  \"process\": {\n";
        oss << "    \"currentBytes\": " << processCurrentBytes << ",\n";
        oss << "    \"peakBytes\": " << processPeakBytes << ",\n";
        oss << "    \"privateBytes\": " << processPrivateBytes << "\n";
        oss << "  },\n";

        oss << "  \"system\": {\n";
        oss << "    \"totalBytes\": " << systemTotalBytes << ",\n";
        oss << "    \"availableBytes\": " << systemAvailableBytes << ",\n";
        oss << "    \"loadPercent\": " << systemMemoryLoad << "\n";
        oss << "  },\n";

        oss << "  \"categories\": {\n";
        bool first = true;
        for (const auto& [name, stats] : categories) {
            if (!first) oss << ",\n";
            first = false;
            oss << "    \"" << name << "\": {\n";
            oss << "      \"currentBytes\": " << stats.currentBytes << ",\n";
            oss << "      \"peakBytes\": " << stats.peakBytes << ",\n";
            oss << "      \"totalAllocated\": " << stats.totalAllocated << ",\n";
            oss << "      \"allocationCount\": " << stats.allocationCount << "\n";
            oss << "    }";
        }
        oss << "\n  },\n";

        oss << "  \"tracked\": {\n";
        oss << "    \"currentBytes\": " << trackedCurrentBytes << ",\n";
        oss << "    \"peakBytes\": " << trackedPeakBytes << ",\n";
        oss << "    \"totalAllocated\": " << trackedTotalAllocated << "\n";
        oss << "  }\n";

        oss << "}\n";

        return oss.str();
    }

private:
    static std::string formatBytes(size_t bytes) {
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

// =============================================================================
// MEMORY TRACKER
// =============================================================================

/**
 * @brief Callback for memory events
 */
using MemoryCallback = std::function<void(const std::string& category, int64_t bytes, bool isAllocation)>;

/**
 * @brief Thread-safe memory tracking singleton
 */
class MemoryTracker {
public:
    /**
     * @brief Get singleton instance
     */
    static MemoryTracker& instance() {
        static MemoryTracker tracker;
        return tracker;
    }

    /**
     * @brief Start memory tracking
     */
    static void startTracking() {
        instance().enabled_ = true;
        instance().startTime_ = std::chrono::steady_clock::now();
    }

    /**
     * @brief Stop memory tracking
     */
    static void stopTracking() {
        instance().enabled_ = false;
    }

    /**
     * @brief Check if tracking is enabled
     */
    [[nodiscard]] static bool isTracking() {
        return instance().enabled_.load();
    }

    /**
     * @brief Record a memory allocation
     * @param category Category name (e.g., "cache", "strings", "buffers")
     * @param bytes Number of bytes allocated
     */
    static void record(const std::string& category, int64_t bytes) {
        auto& inst = instance();
        if (!inst.enabled_) return;

        std::lock_guard lock(inst.mutex_);

        auto& stats = inst.categories_[category];
        stats.name = category;
        stats.currentBytes += bytes;
        stats.totalAllocated += bytes;
        stats.allocationCount++;
        stats.peakBytes = std::max(stats.peakBytes, stats.currentBytes);

        inst.totalCurrent_ += bytes;
        inst.totalAllocated_ += bytes;
        inst.peakTotal_ = std::max(inst.peakTotal_.load(), inst.totalCurrent_.load());

        // Notify callbacks
        for (const auto& callback : inst.callbacks_) {
            try {
                callback(category, bytes, true);
            } catch (...) {}
        }
    }

    /**
     * @brief Record a memory release
     * @param category Category name
     * @param bytes Number of bytes released
     */
    static void release(const std::string& category, int64_t bytes) {
        auto& inst = instance();
        if (!inst.enabled_) return;

        std::lock_guard lock(inst.mutex_);

        auto it = inst.categories_.find(category);
        if (it != inst.categories_.end()) {
            it->second.currentBytes -= bytes;
            it->second.totalReleased += bytes;
            it->second.releaseCount++;
        }

        inst.totalCurrent_ -= bytes;
        inst.totalReleased_ += bytes;

        // Notify callbacks
        for (const auto& callback : inst.callbacks_) {
            try {
                callback(category, bytes, false);
            } catch (...) {}
        }
    }

    /**
     * @brief Get memory report
     */
    [[nodiscard]] static MemoryReport getReport() {
        auto& inst = instance();
        MemoryReport report;
        report.timestamp = std::chrono::system_clock::now();

        // Get process memory
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(),
                reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                sizeof(pmc))) {
            report.processCurrentBytes = pmc.WorkingSetSize;
            report.processPeakBytes = pmc.PeakWorkingSetSize;
            report.processPrivateBytes = pmc.PrivateUsage;
        }

        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memInfo)) {
            report.systemTotalBytes = memInfo.ullTotalPhys;
            report.systemAvailableBytes = memInfo.ullAvailPhys;
            report.systemMemoryLoad = memInfo.dwMemoryLoad;
        }
#endif

        // Get tracked categories
        {
            std::lock_guard lock(inst.mutex_);
            report.categories = inst.categories_;
            report.trackedCurrentBytes = inst.totalCurrent_;
            report.trackedPeakBytes = inst.peakTotal_;
            report.trackedTotalAllocated = inst.totalAllocated_;
        }

        return report;
    }

    /**
     * @brief Get current tracked bytes
     */
    [[nodiscard]] static int64_t currentBytes() {
        return instance().totalCurrent_.load();
    }

    /**
     * @brief Get peak tracked bytes
     */
    [[nodiscard]] static int64_t peakBytes() {
        return instance().peakTotal_.load();
    }

    /**
     * @brief Get category stats
     */
    [[nodiscard]] static CategoryMemoryStats getCategoryStats(const std::string& category) {
        auto& inst = instance();
        std::lock_guard lock(inst.mutex_);

        auto it = inst.categories_.find(category);
        if (it != inst.categories_.end()) {
            return it->second;
        }
        return CategoryMemoryStats{category};
    }

    /**
     * @brief Add callback for memory events
     */
    static void addCallback(MemoryCallback callback) {
        auto& inst = instance();
        std::lock_guard lock(inst.mutex_);
        inst.callbacks_.push_back(std::move(callback));
    }

    /**
     * @brief Reset all tracking data
     */
    static void reset() {
        auto& inst = instance();
        std::lock_guard lock(inst.mutex_);

        inst.categories_.clear();
        inst.totalCurrent_ = 0;
        inst.totalAllocated_ = 0;
        inst.totalReleased_ = 0;
        inst.peakTotal_ = 0;
        inst.callbacks_.clear();
        inst.startTime_ = std::chrono::steady_clock::now();
    }

    /**
     * @brief Get tracking duration
     */
    [[nodiscard]] static std::chrono::milliseconds trackingDuration() {
        auto& inst = instance();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - inst.startTime_);
    }

private:
    MemoryTracker() = default;

    std::atomic<bool> enabled_{false};
    std::chrono::steady_clock::time_point startTime_;

    mutable std::mutex mutex_;
    std::map<std::string, CategoryMemoryStats> categories_;

    std::atomic<int64_t> totalCurrent_{0};
    std::atomic<uint64_t> totalAllocated_{0};
    std::atomic<uint64_t> totalReleased_{0};
    std::atomic<int64_t> peakTotal_{0};

    std::vector<MemoryCallback> callbacks_;
};

// =============================================================================
// SCOPED MEMORY TRACKING
// =============================================================================

/**
 * @brief RAII helper for tracking memory in a scope
 */
class ScopedMemoryTrack {
public:
    /**
     * @brief Start tracking for a category
     * @param category Category name
     */
    explicit ScopedMemoryTrack(std::string category)
        : category_(std::move(category))
        , startBytes_(MemoryTracker::currentBytes())
    {}

    /**
     * @brief Record final allocation and stop tracking
     */
    ~ScopedMemoryTrack() {
        int64_t delta = MemoryTracker::currentBytes() - startBytes_;
        if (delta > 0) {
            MemoryTracker::record(category_, delta);
        }
    }

    // Non-copyable, non-movable
    ScopedMemoryTrack(const ScopedMemoryTrack&) = delete;
    ScopedMemoryTrack& operator=(const ScopedMemoryTrack&) = delete;

    /**
     * @brief Get bytes allocated so far in this scope
     */
    [[nodiscard]] int64_t bytesAllocated() const {
        return MemoryTracker::currentBytes() - startBytes_;
    }

private:
    std::string category_;
    int64_t startBytes_;
};

// =============================================================================
// MEMORY GUARD (Limit Enforcement)
// =============================================================================

/**
 * @brief Memory limit configuration
 */
struct MemoryLimits {
    size_t maxTotalBytes = 0;           ///< Max total tracked memory (0 = unlimited)
    size_t maxCategoryBytes = 0;        ///< Max per category (0 = unlimited)
    size_t warningThresholdPercent = 80; ///< Warn when reaching this percentage
};

/**
 * @brief Memory guard that enforces limits
 */
class MemoryGuard {
public:
    /**
     * @brief Set memory limits
     */
    static void setLimits(const MemoryLimits& limits) {
        instance().limits_ = limits;
    }

    /**
     * @brief Check if allocation would exceed limits
     * @param category Category to check
     * @param bytes Bytes to allocate
     * @return true if allocation is allowed
     */
    [[nodiscard]] static bool canAllocate(const std::string& category, int64_t bytes) {
        auto& guard = instance();

        if (guard.limits_.maxTotalBytes > 0) {
            int64_t current = MemoryTracker::currentBytes();
            if (current + bytes > static_cast<int64_t>(guard.limits_.maxTotalBytes)) {
                return false;
            }
        }

        if (guard.limits_.maxCategoryBytes > 0) {
            auto stats = MemoryTracker::getCategoryStats(category);
            if (stats.currentBytes + bytes > static_cast<int64_t>(guard.limits_.maxCategoryBytes)) {
                return false;
            }
        }

        return true;
    }

    /**
     * @brief Check if we're approaching memory limit
     */
    [[nodiscard]] static bool isNearLimit() {
        auto& guard = instance();
        if (guard.limits_.maxTotalBytes == 0) return false;

        int64_t current = MemoryTracker::currentBytes();
        size_t threshold = guard.limits_.maxTotalBytes * guard.limits_.warningThresholdPercent / 100;

        return current >= static_cast<int64_t>(threshold);
    }

    /**
     * @brief Get memory usage percentage
     */
    [[nodiscard]] static double usagePercent() {
        auto& guard = instance();
        if (guard.limits_.maxTotalBytes == 0) return 0.0;

        int64_t current = MemoryTracker::currentBytes();
        return 100.0 * static_cast<double>(current) / guard.limits_.maxTotalBytes;
    }

private:
    static MemoryGuard& instance() {
        static MemoryGuard guard;
        return guard;
    }

    MemoryLimits limits_;
};

// =============================================================================
// CONVENIENCE FUNCTIONS
// =============================================================================

/**
 * @brief Get quick memory snapshot
 */
inline MemoryReport memorySnapshot() {
    return MemoryTracker::getReport();
}

/**
 * @brief Start memory tracking with optional limits
 */
inline void startMemoryTracking(const MemoryLimits& limits = {}) {
    if (limits.maxTotalBytes > 0 || limits.maxCategoryBytes > 0) {
        MemoryGuard::setLimits(limits);
    }
    MemoryTracker::startTracking();
}

/**
 * @brief Stop memory tracking
 */
inline void stopMemoryTracking() {
    MemoryTracker::stopTracking();
}

// =============================================================================
// MACROS
// =============================================================================

/**
 * @brief Track memory allocation in current scope
 */
#define MAKINE_TRACK_MEMORY(category) \
    ::makine::ScopedMemoryTrack _memory_tracker_##__LINE__(category)

/**
 * @brief Record explicit allocation
 */
#define MAKINE_MEMORY_ALLOC(category, bytes) \
    ::makine::MemoryTracker::record(category, bytes)

/**
 * @brief Record explicit release
 */
#define MAKINE_MEMORY_FREE(category, bytes) \
    ::makine::MemoryTracker::release(category, bytes)

} // namespace makine
