/**
 * @file rendergovernor.cpp
 * @brief FPS governance implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "rendergovernor.h"

Q_LOGGING_CATEGORY(lcRenderGov, "makine.render")

RenderGovernor::RenderGovernor(QQuickWindow* window, QObject* parent)
    : QObject(parent)
    , m_window(window)
{
    // Idle timer: after kIdleTimeoutMs of no frame swaps, enter idle
    m_idleTimer.setSingleShot(true);
    m_idleTimer.setInterval(kIdleTimeoutMs);
    connect(&m_idleTimer, &QTimer::timeout, this, &RenderGovernor::onIdleTimeout);

    // Active timer: for explicit requestActive() calls
    m_activeTimer.setSingleShot(true);
    connect(&m_activeTimer, &QTimer::timeout, this, [this]() {
        // Don't go idle if animations are still running
        m_idleTimer.start();
    });

    // Track frame swaps to reset idle timer
    connect(m_window, &QQuickWindow::frameSwapped,
            this, &RenderGovernor::onFrameSwapped);

    // Stop rendering when minimized/hidden
    connect(m_window, &QWindow::visibilityChanged,
            this, &RenderGovernor::onVisibilityChanged);

    // Track animation state via afterAnimating signal
    connect(m_window, &QQuickWindow::afterAnimating, this, [this]() {
        m_animating = true;
    });

    // Start in active mode, transition to idle after first settle
    m_idleTimer.start();
}

void RenderGovernor::requestActive(int durationMs)
{
    if (m_policy == QStringLiteral("background"))
        return;  // Don't wake from background

    setPolicy(QStringLiteral("active"));
    m_activeTimer.start(durationMs);
    m_idleTimer.stop();
    m_window->update();
}

void RenderGovernor::onIdleTimeout()
{
    if (m_policy != QStringLiteral("background")) {
        setPolicy(QStringLiteral("idle"));
    }
}

void RenderGovernor::onVisibilityChanged(QWindow::Visibility v)
{
    if (v == QWindow::Hidden || v == QWindow::Minimized) {
        setPolicy(QStringLiteral("background"));
        m_idleTimer.stop();
        m_activeTimer.stop();
    } else {
        // Restore to active on show
        setPolicy(QStringLiteral("active"));
        m_idleTimer.start();
        m_window->update();
    }
}

void RenderGovernor::onFrameSwapped()
{
    if (m_policy == QStringLiteral("active")) {
        // Reset idle countdown on every frame
        if (!m_activeTimer.isActive())
            m_idleTimer.start();
    }

    // Check if animations are still running
    if (!m_animating && m_policy == QStringLiteral("active")
        && !m_activeTimer.isActive()) {
        // No animations this frame — let idle timer run
    }
    m_animating = false;
}

void RenderGovernor::setPolicy(const QString& policy)
{
    if (m_policy == policy)
        return;

    qCDebug(lcRenderGov) << "render policy:" << m_policy << "->" << policy;
    m_policy = policy;
    emit policyChanged();
}
