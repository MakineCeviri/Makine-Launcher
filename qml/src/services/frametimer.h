/**
 * @file frametimer.h
 * @brief High-precision frame timing for QML render pipeline
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Hooks into QQuickWindow render signals to measure:
 * - Frame-to-frame time (vsync cadence)
 * - Render time (beforeRendering → afterRendering)
 * - Jank detection (frames > 16.67ms budget)
 *
 * Feeds data to QML PerformanceMonitor overlay and PerfReporter JSON.
 */

#pragma once

#include <QObject>
#include <QString>
#include <QQuickWindow>

#include <array>
#include <chrono>
#include <cstdint>

namespace makine {

class FrameTimer : public QObject
{
    Q_OBJECT

    Q_PROPERTY(qreal fps READ fps NOTIFY statsUpdated)
    Q_PROPERTY(qreal frameTime READ frameTime NOTIFY statsUpdated)
    Q_PROPERTY(qreal renderTime READ renderTime NOTIFY statsUpdated)
    Q_PROPERTY(qreal minFrameTime READ minFrameTime NOTIFY statsUpdated)
    Q_PROPERTY(qreal maxFrameTime READ maxFrameTime NOTIFY statsUpdated)
    Q_PROPERTY(int jankCount READ jankCount NOTIFY statsUpdated)
    Q_PROPERTY(int frameCount READ frameCount NOTIFY statsUpdated)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)

    // Interaction session metrics
    Q_PROPERTY(qreal interactionFps READ interactionFps NOTIFY statsUpdated)
    Q_PROPERTY(qreal interactionMaxFrame READ interactionMaxFrame NOTIFY statsUpdated)
    Q_PROPERTY(int interactionJankCount READ interactionJankCount NOTIFY statsUpdated)
    Q_PROPERTY(QString interactionName READ interactionName NOTIFY statsUpdated)

public:
    explicit FrameTimer(QObject* parent = nullptr);

    /// Attach to a window's render signals. Call once after window is created.
    void attachToWindow(QQuickWindow* window);

    // QML-readable properties
    qreal fps() const { return m_fps; }
    qreal frameTime() const { return m_avgFrameTime; }
    qreal renderTime() const { return m_avgRenderTime; }
    qreal minFrameTime() const { return m_minFrameTime; }
    qreal maxFrameTime() const { return m_maxFrameTime; }
    int jankCount() const { return m_jankCount; }
    int frameCount() const { return m_frameCount; }
    bool active() const { return m_window != nullptr; }

    /// Dump frame stats into PerfReporter before app exit.
    void dumpStats();

    Q_INVOKABLE void reset();

    // Interaction session tracking
    Q_INVOKABLE void beginInteraction(const QString& name);
    Q_INVOKABLE void endInteraction();

    qreal interactionFps() const { return m_interactionFps; }
    qreal interactionMaxFrame() const { return m_interactionMaxFrame; }
    int interactionJankCount() const { return m_interactionJankCount; }
    QString interactionName() const { return m_interactionName; }

signals:
    void statsUpdated();
    void activeChanged();
    void jankDetected(qreal frameTimeMs);

private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    void onBeforeRendering();
    void onAfterRendering();
    void onFrameSwapped();
    void publishStats();

    QQuickWindow* m_window{nullptr};

    // Timing points (set from render thread via DirectConnection)
    TimePoint m_beforeRender;
    TimePoint m_afterRender;
    TimePoint m_lastFrameSwap;

    // Circular buffer for frame times (milliseconds)
    static constexpr int kBufferSize = 120;
    std::array<double, kBufferSize> m_frameTimes{};
    std::array<double, kBufferSize> m_renderTimes{};
    int m_bufferHead{0};
    int m_bufferCount{0};

    // Published stats (updated every N frames, read from QML on main thread)
    qreal m_fps{0};
    qreal m_avgFrameTime{0};
    qreal m_avgRenderTime{0};
    qreal m_minFrameTime{999};
    qreal m_maxFrameTime{0};
    int m_jankCount{0};
    int m_frameCount{0};

    // Jank thresholds
    static constexpr double kFrameBudgetMs = 16.67;  // 60 FPS target
    static constexpr int kPublishInterval = 30;       // Update QML every N frames

    // Interaction session state
    QString m_interactionName;
    bool m_inInteraction{false};
    int m_interactionFrameStart{0};
    qreal m_interactionFps{0};
    qreal m_interactionMaxFrame{0};
    int m_interactionJankCount{0};

    // Per-interaction accumulators (set from render thread)
    double m_interactionFrameSum{0};
    int m_interactionFrameCount{0};
    double m_interactionMaxFrameRaw{0};
    int m_interactionJankRaw{0};
};

} // namespace makine
