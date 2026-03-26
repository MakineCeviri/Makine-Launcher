/**
 * @file rendergovernor.h
 * @brief FPS governance: 60fps active, on-demand idle, zero background
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Manages render frequency based on application state:
 *   Active     — VSync-capped (60fps on 60Hz), during user interaction/animation
 *   Idle       — On-demand rendering, after 800ms of no activity
 *   Background — Rendering stopped, window minimized/hidden
 */

#pragma once

#include <QObject>
#include <QTimer>
#include <QQuickWindow>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(lcRenderGov)

class RenderGovernor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString policy READ policy NOTIFY policyChanged)

public:
    explicit RenderGovernor(QQuickWindow* window, QObject* parent = nullptr);

    QString policy() const { return m_policy; }

    /// Force active rendering for the given duration (e.g. page transitions)
    Q_INVOKABLE void requestActive(int durationMs = 600);

signals:
    void policyChanged();

private slots:
    void onIdleTimeout();
    void onVisibilityChanged(QWindow::Visibility v);
    void onFrameSwapped();

private:
    void setPolicy(const QString& policy);

    QQuickWindow* m_window;
    QTimer m_idleTimer;
    QTimer m_activeTimer;     // For requestActive() duration
    QString m_policy{"active"};
    bool m_animating{false};

    static constexpr int kIdleTimeoutMs = 800;
};
