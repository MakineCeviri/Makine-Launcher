/**
 * @file settingsmanager.cpp
 * @brief Settings Manager Implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "settingsmanager.h"
#include "imagecachemanager.h"
#include "apppaths.h"
#include "profiler.h"
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QTranslator>
#include <QQmlEngine>
#include <QQuickWindow>
#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dpapi.h>
#endif

namespace makine {

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent)
    , m_settings("MakineCeviri", "Makine-Launcher")
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

void SettingsManager::setGraphicsBackend(const QString& value)
{
    if (m_graphicsBackend != value) {
        m_graphicsBackend = value;
        m_settings.setValue("performance/graphicsBackend", value);
        emit graphicsBackendChanged();
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

void SettingsManager::setAccentPreset(const QString& value)
{
    if (m_accentPreset != value) {
        m_accentPreset = value;
        m_settings.setValue("appearance/accentPreset", value);
        emit accentPresetChanged();
        emit settingsChanged();
    }
}

void SettingsManager::setUiScale(const QString& value)
{
    if (m_uiScale != value) {
        m_uiScale = value;
        m_settings.setValue("appearance/uiScale", value);
        emit uiScaleChanged();
        emit settingsChanged();
    }
}

QString SettingsManager::appliedUiScale() const
{
    return m_settings.value("appearance/_appliedUiScale", "auto").toString();
}

QVariantList SettingsManager::accentPresets() const
{
    struct Preset {
        const char* id;
        const char* name;
        const char* colors[5]; // lightest, light, base, dark, darkest
    };

    static const Preset presets[] = {
        {"purple",   "Mor",             {"#C4B5FD", "#A78BFA", "#8B5CF6", "#7C3AED", "#6D28D9"}},
        {"teal",     "Turkuaz",         {"#5EEAD4", "#2DD4BF", "#14B8A6", "#0D9488", "#0F766E"}},
        {"green",    "Ye\xc5\x9fil",    {"#86EFAC", "#4ADE80", "#22C55E", "#16A34A", "#15803D"}},
        {"rose",     "Pembe",           {"#F9A8D4", "#F472B6", "#EC4899", "#DB2777", "#BE185D"}},
        {"red",      "K\xc4\xb1rm\xc4\xb1z\xc4\xb1",  {"#FCA5A5", "#F87171", "#EF4444", "#DC2626", "#B91C1C"}},
        {"sky",      "G\xc3\xb6k Mavisi",{"#7DD3FC", "#38BDF8", "#0EA5E9", "#0284C7", "#0369A1"}},
        {"indigo",   "\xc4\xb0ndigo",   {"#C7D2FE", "#A5B4FC", "#818CF8", "#6366F1", "#4F46E5"}},
        {"black",    "Siyah",           {"#D4D4D8", "#A1A1AA", "#71717A", "#52525B", "#3F3F46"}},
    };

    QVariantList result;
    for (const auto& p : presets) {
        QVariantMap map;
        map["id"] = QString::fromUtf8(p.id);
        map["name"] = QString::fromUtf8(p.name);
        QVariantList colors;
        for (const auto& c : p.colors)
            colors.append(QString::fromUtf8(c));
        map["colors"] = colors;
        result.append(map);
    }
    return result;
}

void SettingsManager::setAppLanguage(const QString& value)
{
    if (m_appLanguage != value) {
        m_appLanguage = value;
        m_settings.setValue("general/appLanguage", value);

        // Load new translation at runtime
        static std::unique_ptr<QTranslator> currentTranslator;
        auto *app = QCoreApplication::instance();

        if (currentTranslator) {
            app->removeTranslator(currentTranslator.get());
            currentTranslator.reset();
        }

        // "tr" is the source language, no translation file needed
        if (value != "tr") {
            auto translator = std::make_unique<QTranslator>();
            QString path = QCoreApplication::applicationDirPath() + "/i18n";
            if (translator->load("makine_" + value, path)) {
                app->installTranslator(translator.get());
                currentTranslator = std::move(translator);
            } else {
                // Try from qrc
                if (translator->load(":/i18n/makine_" + value)) {
                    app->installTranslator(translator.get());
                    currentTranslator = std::move(translator);
                }
            }
        }

        emit appLanguageChanged();
        emit settingsChanged();
    }
}

void SettingsManager::setTranslationDataPath(const QString& value)
{
    if (m_translationDataPath != value) {
        m_translationDataPath = value;

        // Store encrypted via DPAPI
        QByteArray encrypted = protectData(value.toUtf8());
        if (!encrypted.isEmpty()) {
            m_settings.setValue("paths/translationData_enc", encrypted.toBase64());
            m_settings.remove("paths/translationData"); // Remove plaintext
        } else {
            // DPAPI unavailable (non-Windows), fall back to plaintext
            m_settings.setValue("paths/translationData", value);
        }

        emit translationDataPathChanged();
        emit settingsChanged();
    }
}

void SettingsManager::setOnboardingCompleted(bool value)
{
    if (m_onboardingCompleted != value) {
        m_onboardingCompleted = value;
        m_settings.setValue("general/onboardingCompleted", value);
        emit onboardingCompletedChanged();
        emit settingsChanged();
    }
}

void SettingsManager::resetToDefaults()
{
    setAutoDetectGames(false);
    setStartWithWindows(false);
    setMinimizeToTray(false);
    setShowNotifications(false);
    setHardwareAcceleration(true);
    setUseGlobalCache(true);
    setEnableAnimations(true);
    setGraphicsBackend("vulkan");
    setTranslationLanguage("tr");
    setIsDarkMode(true);
    setAccentPreset("purple");
    setUiScale("auto");
    emit settingsResetCompleted();
}

void SettingsManager::clearCache()
{
    MAKINE_ZONE_NAMED("SettingsManager::clearCache");
    bool ok = true;

    // Clear organized cache directory
    QDir cacheDir(AppPaths::cacheDir());
    if (cacheDir.exists()) {
        ok = cacheDir.removeRecursively();
        cacheDir.mkpath(".");
    }

    // Clear Qt standard cache location
    const QString qtCachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir qtCacheDir(qtCachePath);
    if (qtCacheDir.exists()) {
        qtCacheDir.removeRecursively();
        qtCacheDir.mkpath(".");
    }

    // Clear image cache (temp dir)
    auto* imgCache = qApp->findChild<ImageCacheManager*>();
    if (imgCache)
        imgCache->clearCache();

    emit cacheClearCompleted(ok, ok ? tr("Önbellek temizlendi") : tr("Önbellek temizlenemedi"));
}

void SettingsManager::loadSettings()
{
    m_autoDetectGames = m_settings.value("general/autoDetectGames", false).toBool();
    m_startWithWindows = m_settings.value("general/startWithWindows", false).toBool();
    m_minimizeToTray = m_settings.value("general/minimizeToTray", false).toBool();
    m_showNotifications = m_settings.value("general/showNotifications", false).toBool();
    m_hardwareAcceleration = m_settings.value("performance/hardwareAcceleration", true).toBool();
    m_useGlobalCache = m_settings.value("performance/useGlobalCache", true).toBool();
    m_enableAnimations = m_settings.value("performance/enableAnimations", true).toBool();
    m_graphicsBackend = m_settings.value("performance/graphicsBackend", "vulkan").toString();
    // Migrate: "auto" from earlier versions → vulkan
    if (m_graphicsBackend == "auto") {
        m_graphicsBackend = "vulkan";
        m_settings.setValue("performance/graphicsBackend", m_graphicsBackend);
    }
    m_translationLanguage = m_settings.value("translation/language", "tr").toString();
    m_isDarkMode = m_settings.value("appearance/isDarkMode", true).toBool();
    m_accentPreset = m_settings.value("appearance/accentPreset", "purple").toString();
    m_uiScale = m_settings.value("appearance/uiScale", "auto").toString();
    m_onboardingCompleted = m_settings.value("general/onboardingCompleted", false).toBool();
    m_appLanguage = m_settings.value("general/appLanguage", "tr").toString();
    // translationDataPath: prefer DPAPI-encrypted, migrate from plaintext
    if (m_settings.contains("paths/translationData_enc")) {
        QByteArray encrypted = QByteArray::fromBase64(
            m_settings.value("paths/translationData_enc").toByteArray());
        QByteArray decrypted = unprotectData(encrypted);
        if (!decrypted.isEmpty()) {
            m_translationDataPath = QString::fromUtf8(decrypted);
        } else {
            // Decryption failed — use default
            m_translationDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/translation_data");
        }
    } else {
        // Legacy plaintext or first run — load and migrate
        m_translationDataPath = m_settings.value("paths/translationData",
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/translation_data")).toString();

        // Migrate to encrypted storage
        QByteArray encrypted = protectData(m_translationDataPath.toUtf8());
        if (!encrypted.isEmpty()) {
            m_settings.setValue("paths/translationData_enc", encrypted.toBase64());
            m_settings.remove("paths/translationData");
        }
    }
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
    m_settings.setValue("performance/graphicsBackend", m_graphicsBackend);
    m_settings.setValue("translation/language", m_translationLanguage);
    m_settings.setValue("appearance/isDarkMode", m_isDarkMode);
    m_settings.setValue("appearance/accentPreset", m_accentPreset);
    m_settings.setValue("appearance/uiScale", m_uiScale);
    m_settings.setValue("general/onboardingCompleted", m_onboardingCompleted);
    m_settings.setValue("general/appLanguage", m_appLanguage);

    // Save translationDataPath with DPAPI encryption
    QByteArray encrypted = protectData(m_translationDataPath.toUtf8());
    if (!encrypted.isEmpty()) {
        m_settings.setValue("paths/translationData_enc", encrypted.toBase64());
        m_settings.remove("paths/translationData");
    } else {
        m_settings.setValue("paths/translationData", m_translationDataPath);
    }

    m_settings.sync();
}

QString SettingsManager::qtVersion() const
{
    return QString::fromLatin1(qVersion());
}

QString SettingsManager::activeGraphicsApi() const
{
    switch (QQuickWindow::graphicsApi()) {
    case QSGRendererInterface::VulkanRhi:  return QStringLiteral("Vulkan");
    case QSGRendererInterface::Direct3D11Rhi: return QStringLiteral("Direct3D 11");
    case QSGRendererInterface::OpenGLRhi:  return QStringLiteral("OpenGL");
    case QSGRendererInterface::MetalRhi:   return QStringLiteral("Metal");
    default: return QStringLiteral("Unknown");
    }
}

QByteArray SettingsManager::protectData(const QByteArray& plaintext)
{
#ifdef Q_OS_WIN
    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plaintext.data()));
    input.cbData = static_cast<DWORD>(plaintext.size());

    DATA_BLOB output{};
    if (CryptProtectData(&input, nullptr, nullptr, nullptr, nullptr,
                         CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        QByteArray result(reinterpret_cast<const char*>(output.pbData),
                          static_cast<int>(output.cbData));
        SecureZeroMemory(output.pbData, output.cbData);
        LocalFree(output.pbData);
        return result;
    }
    return {};
#else
    Q_UNUSED(plaintext)
    return {};
#endif
}

QByteArray SettingsManager::unprotectData(const QByteArray& encrypted)
{
#ifdef Q_OS_WIN
    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(encrypted.data()));
    input.cbData = static_cast<DWORD>(encrypted.size());

    DATA_BLOB output{};
    if (CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                           CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        QByteArray result(reinterpret_cast<const char*>(output.pbData),
                          static_cast<int>(output.cbData));
        SecureZeroMemory(output.pbData, output.cbData);
        LocalFree(output.pbData);
        return result;
    }
    return {};
#else
    Q_UNUSED(encrypted)
    return {};
#endif
}

void SettingsManager::setupAutoStart(bool enable)
{
#ifdef Q_OS_WIN
    QSettings bootSettings("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
                           QSettings::NativeFormat);

    if (enable) {
        const QString appPath = QCoreApplication::applicationFilePath().replace("/", "\\");
        bootSettings.setValue("MakineLauncher", QString("\"%1\" --minimized").arg(appPath));
    } else {
        bootSettings.remove("MakineLauncher");
    }
#else
    Q_UNUSED(enable)
#endif
}

} // namespace makine
