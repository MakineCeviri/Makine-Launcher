/**
 * @file memoryprofiler.cpp
 * @brief Memory profiling implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "memoryprofiler.h"
#include "imagecachemanager.h"
#include <QGuiApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

namespace makine {

static constexpr double kBytesToMB = 1.0 / (1024.0 * 1024.0);

MemoryProfiler::MemoryProfiler(QObject* parent)
    : QObject(parent)
{
    m_timer.setInterval(5000);
    connect(&m_timer, &QTimer::timeout, this, &MemoryProfiler::sample);

    // Pause sampling when app is inactive/minimized
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive) {
            m_timer.setInterval(5000);
            if (!m_timer.isActive()) m_timer.start();
        } else {
            m_timer.stop();
        }
    });

    m_timer.start();
    sample();
}

void MemoryProfiler::sample()
{
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        m_workingSetMB = static_cast<qreal>(pmc.WorkingSetSize) * kBytesToMB;
        m_peakWorkingSetMB = static_cast<qreal>(pmc.PeakWorkingSetSize) * kBytesToMB;
        m_privateBytesMB = static_cast<qreal>(pmc.PrivateUsage) * kBytesToMB;
    }
#endif

    // Image cache stats
    if (m_imageCache) {
        m_imageCacheCount = m_imageCache->cachedImageCount();
        m_imageCacheSizeMB = static_cast<qreal>(m_imageCache->cachedImageBytes()) * kBytesToMB;
    }

    emit updated();
}

QJsonObject MemoryProfiler::memoryReport() const
{
    QJsonObject obj;
    obj[QStringLiteral("final_working_set_mb")] = m_workingSetMB;
    obj[QStringLiteral("peak_working_set_mb")] = m_peakWorkingSetMB;
    obj[QStringLiteral("private_bytes_mb")] = m_privateBytesMB;
    obj[QStringLiteral("image_cache_count")] = m_imageCacheCount;
    obj[QStringLiteral("image_cache_size_mb")] = m_imageCacheSizeMB;
    return obj;
}

} // namespace makine
