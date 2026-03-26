#pragma once

/**
 * @file sceneprofiler.h
 * @brief QML-C++ bridge for scene transition, interaction, dialog and animation profiling
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Provides Q_INVOKABLE methods callable from QML to track:
 * - Screen transitions (navigateTo timing)
 * - Interactions (scroll, drag, search)
 * - Dialog open/close
 * - Animation registry (infinite loops audit)
 *
 * All data feeds into PerfReporter zones and F3 overlay properties.
 * Gated by MAKINE_DEV_TOOLS — absent in release builds.
 */

#include <QObject>
#include <QString>
#include <QElapsedTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QJsonObject>
#include <QJsonArray>

#include <vector>

namespace makine {

class SceneProfiler : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString activeScreen READ activeScreen NOTIFY updated)
    Q_PROPERTY(qreal lastTransitionMs READ lastTransitionMs NOTIFY updated)
    Q_PROPERTY(QString activeInteraction READ activeInteraction NOTIFY updated)
    Q_PROPERTY(qreal interactionMs READ interactionMs NOTIFY updated)
    Q_PROPERTY(int dialogCount READ dialogCount NOTIFY updated)
    Q_PROPERTY(int runningAnimationCount READ runningAnimationCount NOTIFY updated)

public:
    explicit SceneProfiler(QObject* parent = nullptr);

    // Screen transitions
    Q_INVOKABLE void beginTransition(const QString& from, const QString& to);
    Q_INVOKABLE void endTransition();
    Q_INVOKABLE void screenLoaded(const QString& screenName);

    // Interaction tracking (scroll, drag, search)
    Q_INVOKABLE void beginInteraction(const QString& name);
    Q_INVOKABLE void endInteraction();

    // Dialog tracking
    Q_INVOKABLE void markDialogOpen(const QString& name);
    Q_INVOKABLE void markDialogClose(const QString& name);

    // Loader timing
    Q_INVOKABLE void markLoaderReady(const QString& loaderName);

    // Animation registry
    Q_INVOKABLE void registerAnimation(const QString& name, bool running);

    // Property getters
    QString activeScreen() const { return m_activeScreen; }
    qreal lastTransitionMs() const { return m_lastTransitionMs; }
    QString activeInteraction() const { return m_activeInteraction; }
    qreal interactionMs() const;
    int dialogCount() const { return m_openDialogs.size(); }
    int runningAnimationCount() const;

    Q_INVOKABLE QVariantList animationRegistry() const;

    // PerfReporter integration: call on aboutToQuit to add custom sections
    QJsonObject sceneReport() const;

signals:
    void updated();

private:
    struct TransitionRecord {
        QString from;
        QString to;
        double ms;
    };

    struct ScreenLoadRecord {
        QString name;
        double ms;
    };

    struct AnimationEntry {
        QString name;
        bool running = false;
    };

    QString m_activeScreen;
    qreal m_lastTransitionMs{0};

    // Transition tracking
    QElapsedTimer m_transitionTimer;
    QString m_transitionFrom;
    QString m_transitionTo;
    bool m_inTransition{false};
    std::vector<TransitionRecord> m_transitions;

    // Interaction tracking
    QString m_activeInteraction;
    QElapsedTimer m_interactionTimer;
    bool m_inInteraction{false};

    // Dialog tracking
    QStringList m_openDialogs;

    // Screen load timing
    QElapsedTimer m_appTimer;
    std::vector<ScreenLoadRecord> m_screenLoads;

    // Animation registry
    std::vector<AnimationEntry> m_animations;
};

} // namespace makine
