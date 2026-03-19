/**
 * @file frametimer.cpp
 * @brief High-precision frame timing implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "frametimer.h"
#include "profiler.h"

#include <QSet>

#include <algorithm>
#include <numeric>

namespace makine {

FrameTimer::FrameTimer(QObject* parent)
    : QObject(parent)
{
}

void FrameTimer::attachToWindow(QQuickWindow* window)
{
    if (m_window) return;  // Already attached
    m_window = window;

    // DirectConnection: these run on the render thread, not the GUI thread.
    // Only touch timing variables here — no QML property writes.
    connect(window, &QQuickWindow::beforeRendering, this,
            &FrameTimer::onBeforeRendering, Qt::DirectConnection);
    connect(window, &QQuickWindow::afterRendering, this,
            &FrameTimer::onAfterRendering, Qt::DirectConnection);
    connect(window, &QQuickWindow::frameSwapped, this,
            &FrameTimer::onFrameSwapped, Qt::DirectConnection);

    emit activeChanged();
}

void FrameTimer::onBeforeRendering()
{
    m_beforeRender = Clock::now();
}

void FrameTimer::onAfterRendering()
{
    m_afterRender = Clock::now();
}

void FrameTimer::onFrameSwapped()
{
    auto now = Clock::now();

    if (m_lastFrameSwap != TimePoint{}) {
        double frameMs = std::chrono::duration<double, std::milli>(now - m_lastFrameSwap).count();
        double renderMs = std::chrono::duration<double, std::milli>(m_afterRender - m_beforeRender).count();

        // Store in circular buffer
        m_frameTimes[m_bufferHead] = frameMs;
        m_renderTimes[m_bufferHead] = renderMs;
        m_bufferHead = (m_bufferHead + 1) % kBufferSize;
        if (m_bufferCount < kBufferSize) m_bufferCount++;

        m_frameCount++;

        if (frameMs > kFrameBudgetMs * 2) {
            m_jankCount++;
            // Emit on main thread so QML can react
            QMetaObject::invokeMethod(this, [this, frameMs]() {
                emit jankDetected(frameMs);
            }, Qt::QueuedConnection);
        }

        // Interaction session accumulators (render thread)
        if (m_inInteraction) {
            m_interactionFrameSum += frameMs;
            m_interactionFrameCount++;
            if (frameMs > m_interactionMaxFrameRaw) m_interactionMaxFrameRaw = frameMs;
            if (frameMs > kFrameBudgetMs * 2) m_interactionJankRaw++;
        }

        // Publish aggregated stats every N frames (avoids per-frame QML updates)
        if (m_frameCount % kPublishInterval == 0) {
            QMetaObject::invokeMethod(this, &FrameTimer::publishStats, Qt::QueuedConnection);
        }
    }

    m_lastFrameSwap = now;
}

void FrameTimer::publishStats()
{
    if (m_bufferCount == 0) return;

    double sum = 0, renderSum = 0;
    double minFt = 999, maxFt = 0;

    for (int i = 0; i < m_bufferCount; ++i) {
        double ft = m_frameTimes[i];
        sum += ft;
        if (ft < minFt) minFt = ft;
        if (ft > maxFt) maxFt = ft;
        renderSum += m_renderTimes[i];
    }

    m_avgFrameTime = sum / m_bufferCount;
    m_avgRenderTime = renderSum / m_bufferCount;
    m_minFrameTime = minFt;
    m_maxFrameTime = maxFt;
    m_fps = m_avgFrameTime > 0 ? 1000.0 / m_avgFrameTime : 0;

    // Interaction session stats
    if (m_inInteraction && m_interactionFrameCount > 0) {
        double avgInterFrame = m_interactionFrameSum / m_interactionFrameCount;
        m_interactionFps = avgInterFrame > 0 ? 1000.0 / avgInterFrame : 0;
        m_interactionMaxFrame = m_interactionMaxFrameRaw;
        m_interactionJankCount = m_interactionJankRaw;
    }

    emit statsUpdated();
}

void FrameTimer::reset()
{
    m_bufferHead = 0;
    m_bufferCount = 0;
    m_frameCount = 0;
    m_jankCount = 0;
    m_fps = 0;
    m_avgFrameTime = 0;
    m_avgRenderTime = 0;
    m_minFrameTime = 999;
    m_maxFrameTime = 0;
    m_lastFrameSwap = {};

    emit statsUpdated();
}

void FrameTimer::beginInteraction(const QString& name)
{
    m_interactionName = name;
    m_inInteraction = true;
    m_interactionFrameSum = 0;
    m_interactionFrameCount = 0;
    m_interactionMaxFrameRaw = 0;
    m_interactionJankRaw = 0;
    m_interactionFps = 0;
    m_interactionMaxFrame = 0;
    m_interactionJankCount = 0;
    emit statsUpdated();
}

void FrameTimer::endInteraction()
{
    if (!m_inInteraction) return;
    m_inInteraction = false;

#ifdef MAKINE_PERF_ACTIVE
    if (m_interactionFrameCount > 0) {
        double avgMs = m_interactionFrameSum / m_interactionFrameCount;
        QString zoneName = QStringLiteral("Interaction::%1").arg(m_interactionName);
        QByteArray nameBytes = zoneName.toUtf8();
        static QSet<QByteArray> s_names;
        auto it = s_names.insert(nameBytes);
        PerfReporter::instance().recordZone(it->constData(),
            static_cast<uint64_t>(avgMs * 1'000'000.0));
    }
#endif

    m_interactionName.clear();
    emit statsUpdated();
}

void FrameTimer::dumpStats()
{
#ifdef MAKINE_PERF_ACTIVE
    if (m_frameCount == 0) return;

    auto& reporter = PerfReporter::instance();

    // Record overall frame stats as a synthetic zone
    uint64_t avgNs = static_cast<uint64_t>(m_avgFrameTime * 1'000'000.0);
    reporter.recordZone("FrameTimer::avgFrame", avgNs);

    // Record jank ratio as a message-style zone (count = jank, not duration)
    if (m_jankCount > 0) {
        uint64_t maxNs = static_cast<uint64_t>(m_maxFrameTime * 1'000'000.0);
        reporter.recordZone("FrameTimer::jankFrame", maxNs);
    }
#endif
}

} // namespace makine
