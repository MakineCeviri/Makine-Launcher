#include "perfreporter.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>

#include <algorithm>
#include <mutex>
#include <thread>

namespace makine {

PerfReporter& PerfReporter::instance() {
    static PerfReporter s;
    return s;
}

PerfReporter::PerfReporter()
    : m_mainThreadId(std::this_thread::get_id())
    , m_startTime(std::chrono::steady_clock::now())
{
}

void PerfReporter::setMainThread() {
    std::lock_guard lock(m_mutex);
    m_mainThreadId = std::this_thread::get_id();
}

void PerfReporter::registerThread(const char* name) {
    std::lock_guard lock(m_mutex);
    m_threadNames[std::this_thread::get_id()] = name;
}

bool PerfReporter::isMainThread() const {
    return std::this_thread::get_id() == m_mainThreadId;
}

const char* PerfReporter::threadName(std::thread::id id) const {
    auto it = m_threadNames.find(id);
    return it != m_threadNames.end() ? it->second : "unknown";
}

void PerfReporter::addCustomSection(const QString& name, const QJsonObject& data) {
    std::lock_guard lock(m_mutex);
    m_customSections[name.toStdString()] = data;
}

void PerfReporter::recordZone(const char* name, uint64_t durationNs) {
    // Lock per-call: acceptable because zones are typically ms-scale operations,
    // not micro-hot loops. The mutex cost (~25ns) is negligible vs zone durations.
    std::lock_guard lock(m_mutex);

    auto& zone = m_zones[name];
    if (!zone.name) zone.name = name;

    bool isMain = isMainThread();
    zone.record(durationNs, isMain);

    auto tid = std::this_thread::get_id();
    auto& ts = m_threadStats[tid];
    ts.entries++;
    ts.totalNs += durationNs;
}

void PerfReporter::dumpReport(const QString& path) {
    std::lock_guard lock(m_mutex);

    auto elapsed = std::chrono::steady_clock::now() - m_startTime;
    double uptimeMs = std::chrono::duration<double, std::milli>(elapsed).count();

    // Sort zones by total time descending
    std::vector<const ZoneStats*> sorted;
    sorted.reserve(m_zones.size());
    for (const auto& [_, zone] : m_zones)
        sorted.push_back(&zone);
    std::sort(sorted.begin(), sorted.end(),
              [](const ZoneStats* a, const ZoneStats* b) {
                  return a->totalNs > b->totalNs;
              });

    // Build zones array
    QJsonArray zonesArr;
    uint64_t totalEntries = 0;
    for (const auto* z : sorted) {
        QJsonObject zObj;
        zObj["name"] = z->name;
        zObj["count"] = static_cast<qint64>(z->count);
        zObj["total_ms"] = z->totalMs();
        zObj["mean_ms"] = z->meanMs();
        zObj["min_ms"] = z->minMs();
        zObj["max_ms"] = z->maxMs();
        zObj["main_thread"] = z->mainThread;
        zonesArr.append(zObj);
        totalEntries += z->count;
    }

    // Build threads object
    QJsonObject threadsObj;
    for (const auto& [tid, ts] : m_threadStats) {
        const char* name = threadName(tid);
        QJsonObject tObj;
        tObj["entries"] = static_cast<qint64>(ts.entries);
        tObj["total_ms"] = ts.totalMs();
        threadsObj[name] = tObj;
    }

    // Build warnings (main thread zones with max > 16ms frame budget)
    QJsonArray warningsArr;
    for (const auto* z : sorted) {
        if (z->mainThread && z->maxMs() > 16.0) {
            warningsArr.append(
                QString("%1: max=%2ms on main thread (>16ms frame budget)")
                    .arg(z->name)
                    .arg(z->maxMs(), 0, 'f', 1));
        }
    }

    // Root object
    QJsonObject root;
    root["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["uptime_ms"] = uptimeMs;
    root["zone_count"] = static_cast<int>(m_zones.size());
    root["total_entries"] = static_cast<qint64>(totalEntries);
    root["zones"] = zonesArr;
    root["threads"] = threadsObj;
    root["warnings"] = warningsArr;

    // Custom sections (scene, memory, images, animations)
    if (!m_customSections.empty()) {
        QJsonObject customObj;
        for (const auto& [key, value] : m_customSections)
            customObj[QString::fromStdString(key)] = value;
        root["custom_sections"] = customObj;
    }

    // Write file
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.close();
    }
}

} // namespace makine
