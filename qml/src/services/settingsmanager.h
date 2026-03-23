/**
 * @file settingsmanager.h
 * @brief Application settings management
 * @copyright (c) 2026 MakineCeviri Team
 */

#pragma once

#include <QObject>
#include <QString>
#include <QSettings>
#include <QQmlEngine>
#include <QVariantMap>

namespace makine {

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

    // General settings
    Q_PROPERTY(bool autoDetectGames READ autoDetectGames WRITE setAutoDetectGames NOTIFY autoDetectGamesChanged)
    Q_PROPERTY(bool startWithWindows READ startWithWindows WRITE setStartWithWindows NOTIFY startWithWindowsChanged)
    Q_PROPERTY(bool minimizeToTray READ minimizeToTray WRITE setMinimizeToTray NOTIFY minimizeToTrayChanged)
    Q_PROPERTY(bool showNotifications READ showNotifications WRITE setShowNotifications NOTIFY showNotificationsChanged)
    // Performance settings
    Q_PROPERTY(bool hardwareAcceleration READ hardwareAcceleration WRITE setHardwareAcceleration NOTIFY hardwareAccelerationChanged)
    Q_PROPERTY(bool useGlobalCache READ useGlobalCache WRITE setUseGlobalCache NOTIFY useGlobalCacheChanged)
    Q_PROPERTY(bool enableAnimations READ enableAnimations WRITE setEnableAnimations NOTIFY enableAnimationsChanged)
    Q_PROPERTY(QString graphicsBackend READ graphicsBackend WRITE setGraphicsBackend NOTIFY graphicsBackendChanged)

    // Translation settings
    Q_PROPERTY(QString translationLanguage READ translationLanguage WRITE setTranslationLanguage NOTIFY translationLanguageChanged)

    // Theme settings
    Q_PROPERTY(bool isDarkMode READ isDarkMode WRITE setIsDarkMode NOTIFY isDarkModeChanged)
    Q_PROPERTY(QString accentPreset READ accentPreset WRITE setAccentPreset NOTIFY accentPresetChanged)

    // UI Scale (requires restart)
    Q_PROPERTY(QString uiScale READ uiScale WRITE setUiScale NOTIFY uiScaleChanged)

    // Onboarding
    Q_PROPERTY(bool onboardingCompleted READ onboardingCompleted WRITE setOnboardingCompleted NOTIFY onboardingCompletedChanged)

    // Language
    Q_PROPERTY(QString appLanguage READ appLanguage WRITE setAppLanguage NOTIFY appLanguageChanged)

    // Paths
    Q_PROPERTY(QString translationDataPath READ translationDataPath WRITE setTranslationDataPath NOTIFY translationDataPathChanged)

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

    QString graphicsBackend() const { return m_graphicsBackend; }
    void setGraphicsBackend(const QString& value);
    // Translation settings
    QString translationLanguage() const { return m_translationLanguage; }
    void setTranslationLanguage(const QString& value);

    // Theme settings
    bool isDarkMode() const { return m_isDarkMode; }
    void setIsDarkMode(bool value);

    QString accentPreset() const { return m_accentPreset; }
    void setAccentPreset(const QString& value);

    // UI Scale
    QString uiScale() const { return m_uiScale; }
    void setUiScale(const QString& value);
    Q_INVOKABLE QString appliedUiScale() const;

    // Onboarding
    bool onboardingCompleted() const { return m_onboardingCompleted; }
    void setOnboardingCompleted(bool value);

    // Language
    QString appLanguage() const { return m_appLanguage; }
    void setAppLanguage(const QString& value);
    // Paths
    QString translationDataPath() const { return m_translationDataPath; }
    void setTranslationDataPath(const QString& value);

    // Q_INVOKABLE methods
    Q_INVOKABLE QVariantList accentPresets() const;
    Q_INVOKABLE void resetToDefaults();
    Q_INVOKABLE void clearCache();
    Q_INVOKABLE QString qtVersion() const;
    Q_INVOKABLE QString activeGraphicsApi() const;

signals:
    void autoDetectGamesChanged();
    void startWithWindowsChanged();
    void minimizeToTrayChanged();
    void showNotificationsChanged();
    void hardwareAccelerationChanged();
    void useGlobalCacheChanged();
    void enableAnimationsChanged();
    void graphicsBackendChanged();
    void translationLanguageChanged();
    void isDarkModeChanged();
    void accentPresetChanged();
    void uiScaleChanged();
    void onboardingCompletedChanged();
    void appLanguageChanged();
    void translationDataPathChanged();
    void settingsChanged();
    void cacheClearCompleted(bool success, const QString& message);
    void settingsResetCompleted();

private:
    void loadSettings();
    void saveSettings();
    void setupAutoStart(bool enable);

    // DPAPI encryption for sensitive path data (Windows only)
    static QByteArray protectData(const QByteArray& plaintext);
    static QByteArray unprotectData(const QByteArray& encrypted);

    QSettings m_settings;

    // General
    bool m_autoDetectGames{false};
    bool m_startWithWindows{false};
    bool m_minimizeToTray{false};
    bool m_showNotifications{false};
    // Performance
    bool m_hardwareAcceleration{true};
    bool m_useGlobalCache{true};
    bool m_enableAnimations{true};
    QString m_graphicsBackend{"auto"};

    // Translation
    QString m_translationLanguage{"tr"};

    // Theme
    bool m_isDarkMode{true};
    QString m_accentPreset{"purple"};
    QString m_uiScale{"auto"};

    // Onboarding
    bool m_onboardingCompleted{false};

    // Language
    QString m_appLanguage{"tr"};

    // Paths
    QString m_translationDataPath;
};

} // namespace makine
