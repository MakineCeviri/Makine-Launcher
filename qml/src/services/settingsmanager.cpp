/**
 * @file settingsmanager.cpp
 * @brief Settings Manager Implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "settingsmanager.h"
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace makineai {

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent)
    , m_settings("MakineAI", "MakineAI")
{
    loadSettings();
}

SettingsManager::~SettingsManager()
{
    saveSettings();
}

SettingsManager* SettingsManager::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    return new SettingsManager();
}

void SettingsManager::setAutoDetectGames(bool value)
{
    if (m_autoDetectGames != value) {
        m_autoDetectGames = value;
        m_settings.setValue("general/autoDetectGames", value);
        emit autoDetectGamesChanged();
        emit settingsChanged();
    }
}

void SettingsManager::setStartWithWindows(bool value)
{
    if (m_startWithWindows != value) {
        m_startWithWindows = value;
        m_settings.setValue("general/startWithWindows", value);
        setupAutoStart(value);
        emit startWithWindowsChanged();
        emit settingsChanged();
    }
}

void SettingsManager::setMinimizeToTray(bool value)
{
    if (m_minimizeToTray != value) {
        m_minimizeToTray = value;
        m_settings.setValue("general/minimizeToTray", value);
        emit minimizeToTrayChanged();
        emit settingsChanged();
    }
}

void SettingsManager::setShowNotifications(bool value)
{
    if (m_showNotifications != value) {
        m_showNotifications = value;
        m_settings.setValue("general/showNotifications", value);
        emit showNotificationsChanged();
        emit settingsChanged();
    }
}

void SettingsManager::setHardwareAcceleration(bool value)
{
    if (m_hardwareAcceleration != value) {
        m_hardwareAcceleration = value;
        m_settings.setValue("performance/hardwareAcceleration", value);
        emit hardwareAccelerationChanged();
        emit settingsChanged();
    }
}

void SettingsManager::setUseGlobalCache(bool value)
{
    if (m_useGlobalCache != value) {
        m_useGlobalCache = value;
        m_settings.setValue("performance/useGlobalCache", value);
        emit useGlobalCacheChanged();
        emit settingsChanged();
    }
}

void SettingsManager::setEnableAnimations(bool value)
{
    if (m_enableAnimations != value) {
        m_enableAnimations = value;
        m_settings.setValue("performance/enableAnimations", value);
        emit enableAnimationsChanged();
        emit settingsChanged();
    }
}

void SettingsManager::setTranslationLanguage(const QString& value)
{
    if (m_translationLanguage != value) {
        m_translationLanguage = value;
        m_settings.setValue("translation/language", value);
        emit translationLanguageChanged();
        emit settingsChanged();
    }
}

void SettingsManager::setIsDarkMode(bool value)
{
    if (m_isDarkMode != value) {
        m_isDarkMode = value;
        m_settings.setValue("appearance/isDarkMode", value);
        emit isDarkModeChanged();
        emit settingsChanged();
    }
}

void SettingsManager::resetToDefaults()
{
    setAutoDetectGames(true);
    setStartWithWindows(false);
    setMinimizeToTray(true);
    setShowNotifications(true);
    setHardwareAcceleration(true);
    setUseGlobalCache(true);
    setEnableAnimations(true);
    setTranslationLanguage("tr");
    setIsDarkMode(true);
}

void SettingsManager::clearCache()
{
    const QString cachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir cacheDir(cachePath);

    if (cacheDir.exists()) {
        cacheDir.removeRecursively();
        cacheDir.mkpath(".");
    }
}

void SettingsManager::loadSettings()
{
    m_autoDetectGames = m_settings.value("general/autoDetectGames", true).toBool();
    m_startWithWindows = m_settings.value("general/startWithWindows", false).toBool();
    m_minimizeToTray = m_settings.value("general/minimizeToTray", true).toBool();
    m_showNotifications = m_settings.value("general/showNotifications", true).toBool();
    m_hardwareAcceleration = m_settings.value("performance/hardwareAcceleration", true).toBool();
    m_useGlobalCache = m_settings.value("performance/useGlobalCache", true).toBool();
    m_enableAnimations = m_settings.value("performance/enableAnimations", true).toBool();
    m_translationLanguage = m_settings.value("translation/language", "tr").toString();
    m_isDarkMode = m_settings.value("appearance/isDarkMode", true).toBool();
}

void SettingsManager::saveSettings()
{
    m_settings.setValue("general/autoDetectGames", m_autoDetectGames);
    m_settings.setValue("general/startWithWindows", m_startWithWindows);
    m_settings.setValue("general/minimizeToTray", m_minimizeToTray);
    m_settings.setValue("general/showNotifications", m_showNotifications);
    m_settings.setValue("performance/hardwareAcceleration", m_hardwareAcceleration);
    m_settings.setValue("performance/useGlobalCache", m_useGlobalCache);
    m_settings.setValue("performance/enableAnimations", m_enableAnimations);
    m_settings.setValue("translation/language", m_translationLanguage);
    m_settings.setValue("appearance/isDarkMode", m_isDarkMode);
    m_settings.sync();
}

void SettingsManager::setupAutoStart(bool enable)
{
#ifdef Q_OS_WIN
    QSettings bootSettings("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
                           QSettings::NativeFormat);

    if (enable) {
        const QString appPath = QCoreApplication::applicationFilePath().replace("/", "\\");
        bootSettings.setValue("MakineAI", QString("\"%1\" --minimized").arg(appPath));
    } else {
        bootSettings.remove("MakineAI");
    }
#else
    Q_UNUSED(enable)
#endif
}

} // namespace makineai
