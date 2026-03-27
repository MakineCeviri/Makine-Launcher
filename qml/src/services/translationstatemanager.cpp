/**
 * @file translationstatemanager.cpp
 * @brief Translation button 7-state resolver implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "translationstatemanager.h"

namespace makine {

TranslationStateManager::TranslationStateManager(QObject* parent)
    : QObject(parent)
    , m_label(stateToLabel(m_state))
    , m_accessibleText(stateToAccessibleText(m_state))
{
}

void TranslationStateManager::evaluate(bool hasUpdate, bool packageInstalled,
                                        bool isInstalling, bool installCompleted,
                                        const QString& impactLevel,
                                        const QString& externalUrl,
                                        const QString& source)
{
    State newState = State::Download;

    if (impactLevel == u"broken") {
        newState = State::Broken;
    } else if (installCompleted) {
        newState = State::Completed;
    } else if (isInstalling) {
        newState = State::Installing;
    } else if (hasUpdate) {
        newState = State::Update;
    } else if (packageInstalled) {
        newState = State::Installed;
    } else if (!externalUrl.isEmpty()) {
        newState = State::External;
    }

    const bool sourceChanged = (m_externalSource != source);
    m_externalSource = source;

    if (newState == m_state && !sourceChanged)
        return;

    m_state = newState;
    m_stateStr = stateToString(m_state);
    m_label = stateToLabel(m_state);
    m_accessibleText = stateToAccessibleText(m_state);
    emit stateChanged();
}

QString TranslationStateManager::state() const { return m_stateStr; }
QString TranslationStateManager::label() const { return m_label; }
QString TranslationStateManager::accessibleText() const { return m_accessibleText; }

QString TranslationStateManager::stateToString(State s)
{
    switch (s) {
    case State::Broken:     return QStringLiteral("broken");
    case State::Completed:  return QStringLiteral("completed");
    case State::Installing: return QStringLiteral("installing");
    case State::Update:     return QStringLiteral("update");
    case State::Installed:  return QStringLiteral("installed");
    case State::External:   return QStringLiteral("external");
    case State::Download:   return QStringLiteral("download");
    }
    return QStringLiteral("download");
}

QString TranslationStateManager::stateToLabel(State s) const
{
    switch (s) {
    case State::Broken:     return tr("ONARIM GEREKL\u0130");
    case State::Completed:  return tr("Yama Ba\u015Far\u0131yla Kuruldu");
    case State::Update:     return tr("G\u00FCncelleme Mevcut");
    case State::Installed:  return tr("T\u00FCrk\u00E7e Yama Kurulu");
    case State::Installing: return tr("Haz\u0131rlan\u0131yor...");
    case State::External:
        if (m_externalSource.contains(QLatin1String("hangar")))
            return tr("Hangar \u00C7eviri'de \u0130ndir");
        return tr("ApexYama'da \u0130ndir");
    case State::Download:   return tr("T\u00FCrk\u00E7e Yama \u0130ndir");
    }
    return tr("T\u00FCrk\u00E7e Yama \u0130ndir");
}

QString TranslationStateManager::stateToAccessibleText(State s) const
{
    switch (s) {
    case State::Broken:     return tr("Onar\u0131m gerekli");
    case State::Completed:  return tr("Kurulum tamamland\u0131");
    case State::Update:     return tr("G\u00FCncelleme mevcut");
    case State::Installed:  return tr("Kurulu");
    case State::Installing: return tr("Kuruluyor");
    case State::External:
        if (m_externalSource.contains(QLatin1String("hangar")))
            return tr("Hangar \u00C7eviri'den indir");
        return tr("ApexYama'dan indir");
    case State::Download:   return tr("T\u00FCrk\u00E7e Yama \u0130ndir");
    }
    return tr("T\u00FCrk\u00E7e Yama \u0130ndir");
}

} // namespace makine
