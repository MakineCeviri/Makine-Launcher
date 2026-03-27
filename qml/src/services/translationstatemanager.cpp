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
                                        const QString& externalUrl)
{
    // Exact cascade from TranslationActionButton.qml lines 31-45
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

    if (newState == m_state)
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

QString TranslationStateManager::stateToLabel(State s)
{
    switch (s) {
    case State::Broken:     return tr("ONARIM GEREKL\u0130");
    case State::Completed:  return tr("Yama Ba\u015Far\u0131yla Kuruldu");
    case State::Update:     return tr("G\u00FCncelleme Mevcut");
    case State::Installed:  return tr("T\u00FCrk\u00E7e Yama Kurulu");
    case State::Installing: return tr("Haz\u0131rlan\u0131yor...");
    case State::External:   return tr("ApexYama'da \u0130ndir");
    case State::Download:   return tr("T\u00FCrk\u00E7e Yama \u0130ndir");
    }
    return tr("T\u00FCrk\u00E7e Yama \u0130ndir");
}

QString TranslationStateManager::stateToAccessibleText(State s)
{
    switch (s) {
    case State::Broken:     return tr("Onarım gerekli");
    case State::Completed:  return tr("Kurulum tamamlandı");
    case State::Update:     return tr("Güncelleme mevcut");
    case State::Installed:  return tr("Kurulu");
    case State::Installing: return tr("Kuruluyor");
    case State::External:   return tr("ApexYama'dan indir");
    case State::Download:   return tr("Türkçe Yama İndir");
    }
    return tr("Türkçe Yama İndir");
}

} // namespace makine
