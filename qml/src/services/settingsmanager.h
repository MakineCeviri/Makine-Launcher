/**
 * @file settingsmanager.h
 * @brief Application settings management
 * @copyright (c) 2026 MakineAI Team
 */

#pragma once

#include <QObject>
#include <QString>
#include <QSettings>
#include <QQmlEngine>

namespace makineai {

/**
 * @brief Settings Manager - Manages application settings
 *
 * Provides:
 * - Persistent settings storage
 * - Theme preferences
 * - Translation options
 * - Performance settings
 */
class SettingsManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // General settings
    Q_PROPERTY(bool autoDetectGames READ autoDetectGames WRITE setAutoDetectGames NOTIFY autoDetectGamesChanged)
    Q_PROPERTY(bool startWithWindows READ startWithWindows WRITE setStartWithWindows NOTIFY startWithWindowsChanged)
    Q_PROPERTY(bool minimizeToTray READ minimizeToTray WRITE setMinimizeToTray NOTIFY minimizeToTrayChanged)
    Q_PROPERTY(bool showNotifications READ showNotifications WRITE setShowNotifications NOTIFY showNotificationsChanged)

    // Performance settings
    Q_PROPERTY(bool hardwareAcceleration READ hardwareAcceleration WRITE setHardwareAcceleration NOTIFY hardwareAccelerationChanged)
    Q_PROPERTY(bool useGlobalCache READ useGlobalCache WRITE setUseGlobalCache NOTIFY useGlobalCacheChanged)
    Q_PROPERTY(bool enableAnimations READ enableAnimations WRITE setEnableAnimations NOTIFY enableAnimationsChanged)

    // Translation settings
    Q_PROPERTY(QString translationLanguage READ translationLanguage WRITE setTranslationLanguage NOTIFY translationLanguageChanged)

    // Theme settings
    Q_PROPERTY(bool isDarkMode READ isDarkMode WRITE setIsDarkMode NOTIFY isDarkModeChanged)

    // Onboarding
    Q_PROPERTY(bool onboardingCompleted READ onboardingCompleted WRITE setOnboardingCompleted NOTIFY onboardingCompletedChanged)

    // Language
    Q_PROPERTY(QString appLanguage READ appLanguage WRITE setAppLanguage NOTIFY appLanguageChanged)

public:
    explicit SettingsManager(QObject *parent = nullptr);
    ~SettingsManager() override;

    static SettingsManager* create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    // General settings
    bool autoDetectGames() const { return m_autoDetectGames; }
    void setAutoDetectGames(bool value);

    bool startWithWindows() const { return m_startWithWindows; }
    void setStartWithWindows(bool value);

    bool minimizeToTray() const { return m_minimizeToTray; }
    void setMinimizeToTray(bool value);

    bool showNotifications() const { return m_showNotifications; }
    void setShowNotifications(bool value);

    // Performance settings
    bool hardwareAcceleration() const { return m_hardwareAcceleration; }
    void setHardwareAcceleration(bool value);

    bool useGlobalCache() const { return m_useGlobalCache; }
    void setUseGlobalCache(bool value);

    bool enableAnimations() const { return m_enableAnimations; }
    void setEnableAnimations(bool value);

    // Translation settings
    QString translationLanguage() const { return m_translationLanguage; }
    void setTranslationLanguage(const QString& value);

    // Theme settings
    bool isDarkMode() const { return m_isDarkMode; }
    void setIsDarkMode(bool value);

    // Onboarding
    bool onboardingCompleted() const { return m_onboardingCompleted; }
    void setOnboardingCompleted(bool value);

    // Language
    QString appLanguage() const { return m_appLanguage; }
    void setAppLanguage(const QString& value);
    Q_INVOKABLE QStringList availableLanguages() const;

    // Q_INVOKABLE methods
    Q_INVOKABLE void resetToDefaults();
    Q_INVOKABLE void clearCache();

signals:
    void autoDetectGamesChanged();
    void startWithWindowsChanged();
    void minimizeToTrayChanged();
    void showNotificationsChanged();
    void hardwareAccelerationChanged();
    void useGlobalCacheChanged();
    void enableAnimationsChanged();
    void translationLanguageChanged();
    void isDarkModeChanged();
    void onboardingCompletedChanged();
    void appLanguageChanged();
    void settingsChanged();

private:
    void loadSettings();
    void saveSettings();
    void setupAutoStart(bool enable);

    QSettings m_settings;

    // General
    bool m_autoDetectGames{true};
    bool m_startWithWindows{false};
    bool m_minimizeToTray{true};
    bool m_showNotifications{true};

    // Performance
    bool m_hardwareAcceleration{true};
    bool m_useGlobalCache{true};
    bool m_enableAnimations{true};

    // Translation
    QString m_translationLanguage{"tr"};

    // Theme
    bool m_isDarkMode{true};

    // Onboarding
    bool m_onboardingCompleted{false};

    // Language
    QString m_appLanguage{"tr"};
};

} // namespace makineai
