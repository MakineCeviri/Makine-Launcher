#pragma once

/**
 * perfreporter.h - Lightweight built-in performance reporter.
 *
 * Records timing statistics for every MAKINE_ZONE_NAMED call.
 * Dumps JSON report on app exit for automated analysis.
 *
 * Hot path is lock-free: uses thread-local accumulation buffers
 * that flush to the global map periodically and at report time.
 *
 * Works alongside Tracy (both can be active simultaneously).
 */

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <QJsonObject>

class QString;

namespace makine {

struct ZoneStats {
    const char* name = nullptr;
    uint64_t count = 0;
    uint64_t totalNs = 0;
    uint64_t minNs = UINT64_MAX;
    uint64_t maxNs = 0;
    bool mainThread = false; // true if ANY call was on main thread

    void record(uint64_t durationNs, bool isMain) {
        ++count;
        totalNs += durationNs;
        if (durationNs < minNs) minNs = durationNs;
        if (durationNs > maxNs) maxNs = durationNs;
        if (isMain) mainThread = true;
    }

    double totalMs() const { return static_cast<double>(totalNs) / 1'000'000.0; }
    double meanMs() const { return count > 0 ? totalMs() / count : 0.0; }
    double minMs() const { return static_cast<double>(minNs) / 1'000'000.0; }
    double maxMs() const { return static_cast<double>(maxNs) / 1'000'000.0; }
};

struct ThreadStats {
    uint64_t entries = 0;
    uint64_t totalNs = 0;
    double totalMs() const { return static_cast<double>(totalNs) / 1'000'000.0; }
};

class PerfReporter {
public:
    static PerfReporter& instance();

    // Register current thread name (call once per thread, pairs with MAKINE_THREAD_NAME)
    void registerThread(const char* name);

    // Record a zone measurement (called from PerfZone destructor)
    void recordZone(const char* name, uint64_t durationNs);

    // Write JSON report to file
    void dumpReport(const QString& path);

    // Add a custom JSON section to the report (call before dumpReport)
    void addCustomSection(const QString& name, const QJsonObject& data);

    // Mark current thread as main (call from main() before event loop)
    void setMainThread();

private:
    PerfReporter();

    bool isMainThread() const;
    const char* threadName(std::thread::id id) const;

    mutable std::mutex m_mutex;
    std::unordered_map<const char*, ZoneStats> m_zones;
    std::unordered_map<std::thread::id, const char*> m_threadNames;
    std::unordered_map<std::thread::id, ThreadStats> m_threadStats;
    std::thread::id m_mainThreadId;
    std::chrono::steady_clock::time_point m_startTime;
    std::unordered_map<std::string, QJsonObject> m_customSections;
};

// RAII guard — measures zone duration via steady_clock
struct PerfZone {
    const char* name;
    std::chrono::steady_clock::time_point start;

    explicit PerfZone(const char* n)
        : name(n), start(std::chrono::steady_clock::now()) {}

    ~PerfZone() {
        auto dur = std::chrono::steady_clock::now() - start;
        PerfReporter::instance().recordZone(
            name,
            static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(dur).count()));
    }

    PerfZone(const PerfZone&) = delete;
    PerfZone& operator=(const PerfZone&) = delete;
};

} // namespace makine
