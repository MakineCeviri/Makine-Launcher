#pragma once

/**
 * @file memoryprofiler.h
 * @brief Memory profiling: Win32 working set + image cache monitoring
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Periodically samples process memory via GetProcessMemoryInfo.
 * Also reads ImageCacheManager stats for image cache monitoring.
 * Updates every 2 seconds via QTimer.
 *
 * Gated by MAKINE_DEV_TOOLS — absent in release builds.
 */

#include <QObject>
#include <QTimer>
#include <QJsonObject>

namespace makine {

class ImageCacheManager;

class MemoryProfiler : public QObject
{
    Q_OBJECT

    Q_PROPERTY(qreal workingSetMB READ workingSetMB NOTIFY updated)
    Q_PROPERTY(qreal peakWorkingSetMB READ peakWorkingSetMB NOTIFY updated)
    Q_PROPERTY(qreal privateBytesMB READ privateBytesMB NOTIFY updated)
    Q_PROPERTY(int imageCacheCount READ imageCacheCount NOTIFY updated)
    Q_PROPERTY(qreal imageCacheSizeMB READ imageCacheSizeMB NOTIFY updated)

public:
    explicit MemoryProfiler(QObject* parent = nullptr);

    void setImageCacheManager(ImageCacheManager* mgr) { m_imageCache = mgr; }

    qreal workingSetMB() const { return m_workingSetMB; }
    qreal peakWorkingSetMB() const { return m_peakWorkingSetMB; }
    qreal privateBytesMB() const { return m_privateBytesMB; }
    int imageCacheCount() const { return m_imageCacheCount; }
    qreal imageCacheSizeMB() const { return m_imageCacheSizeMB; }

    // PerfReporter integration
    QJsonObject memoryReport() const;

signals:
    void updated();

private:
    void sample();

    QTimer m_timer;
    ImageCacheManager* m_imageCache{nullptr};

    qreal m_workingSetMB{0};
    qreal m_peakWorkingSetMB{0};
    qreal m_privateBytesMB{0};
    int m_imageCacheCount{0};
    qreal m_imageCacheSizeMB{0};
};

} // namespace makine
