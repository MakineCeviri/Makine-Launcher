/**
 * @file sceneprofiler.cpp
 * @brief Scene profiling implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "sceneprofiler.h"
#include "profiler.h"

namespace makine {

SceneProfiler::SceneProfiler(QObject* parent)
    : QObject(parent)
{
    m_appTimer.start();
}

// ============================================================================
// Screen Transitions
// ============================================================================

void SceneProfiler::beginTransition(const QString& from, const QString& to)
{
    m_transitionFrom = from;
    m_transitionTo = to;
    m_inTransition = true;
    m_transitionTimer.start();
}

void SceneProfiler::endTransition()
{
    if (!m_inTransition) return;
    m_inTransition = false;

    double ms = m_transitionTimer.elapsed();
    m_lastTransitionMs = ms;
    m_activeScreen = m_transitionTo;

    m_transitions.push_back({m_transitionFrom, m_transitionTo, ms});

#ifdef MAKINE_PERF_ACTIVE
    // Record as zone: "Transition::Home->GameDetail"
    QString zoneName = QStringLiteral("Transition::%1->%2").arg(m_transitionFrom, m_transitionTo);
    QByteArray nameBytes = zoneName.toUtf8();
    // Store in static set to keep pointer valid for PerfReporter
    static QSet<QByteArray> s_zoneNames;
    auto it = s_zoneNames.insert(nameBytes);
    PerfReporter::instance().recordZone(it->constData(),
        static_cast<uint64_t>(ms * 1'000'000.0));
#endif

    emit updated();
}

void SceneProfiler::screenLoaded(const QString& screenName)
{
    m_activeScreen = screenName;
    double ms = m_appTimer.elapsed();
    m_screenLoads.push_back({screenName, ms});

#ifdef MAKINE_PERF_ACTIVE
    QString zoneName = QStringLiteral("ScreenLoaded::%1").arg(screenName);
    QByteArray nameBytes = zoneName.toUtf8();
    static QSet<QByteArray> s_screenNames;
    auto it = s_screenNames.insert(nameBytes);
    PerfReporter::instance().recordZone(it->constData(),
        static_cast<uint64_t>(ms * 1'000'000.0));
#endif

    emit updated();
}

// ============================================================================
// Interaction Tracking
// ============================================================================

void SceneProfiler::beginInteraction(const QString& name)
{
    m_activeInteraction = name;
    m_inInteraction = true;
    m_interactionTimer.start();
    emit updated();
}

void SceneProfiler::endInteraction()
{
    if (!m_inInteraction) return;
    m_inInteraction = false;

    double ms = m_interactionTimer.elapsed();

#ifdef MAKINE_PERF_ACTIVE
    QString zoneName = QStringLiteral("Interaction::%1").arg(m_activeInteraction);
    QByteArray nameBytes = zoneName.toUtf8();
    static QSet<QByteArray> s_interactionNames;
    auto it = s_interactionNames.insert(nameBytes);
    PerfReporter::instance().recordZone(it->constData(),
        static_cast<uint64_t>(ms * 1'000'000.0));
#endif

    m_activeInteraction.clear();
    emit updated();
}

qreal SceneProfiler::interactionMs() const
{
    if (!m_inInteraction) return 0;
    return m_interactionTimer.elapsed();
}

// ============================================================================
// Dialog Tracking
// ============================================================================

void SceneProfiler::markDialogOpen(const QString& name)
{
    if (!m_openDialogs.contains(name))
        m_openDialogs.append(name);
    emit updated();
}

void SceneProfiler::markDialogClose(const QString& name)
{
    m_openDialogs.removeAll(name);
    emit updated();
}

// ============================================================================
// Loader Timing
// ============================================================================

void SceneProfiler::markLoaderReady(const QString& loaderName)
{
#ifdef MAKINE_PERF_ACTIVE
    double ms = m_appTimer.elapsed();
    QString zoneName = QStringLiteral("LoaderReady::%1").arg(loaderName);
    QByteArray nameBytes = zoneName.toUtf8();
    static QSet<QByteArray> s_loaderNames;
    auto it = s_loaderNames.insert(nameBytes);
    PerfReporter::instance().recordZone(it->constData(),
        static_cast<uint64_t>(ms * 1'000'000.0));
#else
    Q_UNUSED(loaderName)
#endif
}

// ============================================================================
// Animation Registry
// ============================================================================

void SceneProfiler::registerAnimation(const QString& name, bool running)
{
    for (auto& entry : m_animations) {
        if (entry.name == name) {
            entry.running = running;
            emit updated();
            return;
        }
    }
    m_animations.push_back({name, running});
    emit updated();
}

int SceneProfiler::runningAnimationCount() const
{
    int count = 0;
    for (const auto& entry : m_animations)
        if (entry.running) ++count;
    return count;
}

QVariantList SceneProfiler::animationRegistry() const
{
    QVariantList list;
    for (const auto& entry : m_animations) {
        QVariantMap map;
        map[QStringLiteral("name")] = entry.name;
        map[QStringLiteral("running")] = entry.running;
        list.append(map);
    }
    return list;
}

// ============================================================================
// PerfReporter JSON section
// ============================================================================

QJsonObject SceneProfiler::sceneReport() const
{
    QJsonObject obj;

    // Transitions
    QJsonArray transArr;
    for (const auto& t : m_transitions) {
        QJsonObject tObj;
        tObj[QStringLiteral("from")] = t.from;
        tObj[QStringLiteral("to")] = t.to;
        tObj[QStringLiteral("ms")] = t.ms;
        transArr.append(tObj);
    }
    obj[QStringLiteral("transitions")] = transArr;

    // Screen loads
    QJsonArray screenArr;
    for (const auto& s : m_screenLoads) {
        QJsonObject sObj;
        sObj[QStringLiteral("name")] = s.name;
        sObj[QStringLiteral("at_ms")] = s.ms;
        screenArr.append(sObj);
    }
    obj[QStringLiteral("screens_loaded")] = screenArr;

    // Animations
    QJsonObject animObj;
    animObj[QStringLiteral("registered")] = static_cast<int>(m_animations.size());
    animObj[QStringLiteral("running_at_exit")] = runningAnimationCount();
    obj[QStringLiteral("animations")] = animObj;

    return obj;
}

} // namespace makine
