#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QString>
#include <QStringList>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <vector>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include <makine/plugin/plugin_api.h>

Q_DECLARE_LOGGING_CATEGORY(lcPlugin)

class PluginManager : public QObject {
    Q_OBJECT

    Q_PROPERTY(QVariantList plugins READ plugins NOTIFY pluginsChanged)
    Q_PROPERTY(int pluginCount READ pluginCount NOTIFY pluginsChanged)
    Q_PROPERTY(bool checking READ isChecking NOTIFY checkingChanged)
    Q_PROPERTY(bool installing READ isInstalling NOTIFY installingChanged)
    Q_PROPERTY(QString installingPluginId READ installingPluginId NOTIFY installingChanged)
    Q_PROPERTY(double installProgress READ installProgress NOTIFY installProgressChanged)
    Q_PROPERTY(bool restartRequired READ restartRequired NOTIFY restartRequiredChanged)
    Q_PROPERTY(QVariantList communityPlugins READ communityPlugins NOTIFY communityPluginsChanged)
    Q_PROPERTY(bool loadingCommunity READ isLoadingCommunity NOTIFY loadingCommunityChanged)

public:
    explicit PluginManager(QObject* parent = nullptr);
    ~PluginManager() override;

    QVariantList plugins() const;
    int pluginCount() const;
    bool isChecking() const { return m_checking; }
    bool isInstalling() const { return m_installing; }
    QString installingPluginId() const { return m_installingPluginId; }
    double installProgress() const { return m_installProgress; }
    bool restartRequired() const { return m_restartRequired; }
    QVariantList communityPlugins() const { return m_communityPlugins; }
    bool isLoadingCommunity() const { return m_loadingCommunity; }

    Q_INVOKABLE void discoverPlugins();
    Q_INVOKABLE bool enablePlugin(const QString& pluginId);
    Q_INVOKABLE bool disablePlugin(const QString& pluginId);
    Q_INVOKABLE QVariantMap pluginInfo(const QString& pluginId) const;
    Q_INVOKABLE bool isPluginEnabled(const QString& pluginId) const;
    Q_INVOKABLE bool isPluginLoaded(const QString& pluginId) const;

    // Install / Update / Uninstall
    Q_INVOKABLE void installPlugin(const QString& pluginId, const QString& downloadUrl = {});
    Q_INVOKABLE void installFromFile(const QString& filePath);
    Q_INVOKABLE void uninstallPlugin(const QString& pluginId, bool removeData = false);
    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE bool hasUpdate(const QString& pluginId) const;
    Q_INVOKABLE QString availableVersion(const QString& pluginId) const;
    Q_INVOKABLE QString lastPluginError(const QString& pluginId) const;

    // Plugin settings (reads manifest settings definitions + calls DLL get/set)
    Q_INVOKABLE QVariantList pluginSettings(const QString& pluginId) const;
    Q_INVOKABLE QString getPluginSetting(const QString& pluginId, const QString& key) const;
    Q_INVOKABLE void setPluginSetting(const QString& pluginId, const QString& key, const QString& value);

    // Settings discovery (moved from SettingsScreen.qml JS loops)
    Q_INVOKABLE QVariantList settingsCategories() const;
    Q_INVOKABLE QVariantList pluginsWithSettings() const;

    // Community discovery (GitHub topic search)
    Q_INVOKABLE void fetchCommunityPlugins();
    Q_INVOKABLE void openCommunityPage();

    // OCR plugin calls
    Q_INVOKABLE QString callPluginOcr(const QString& pluginId, int x, int y, int w, int h);
    Q_INVOKABLE QString getPluginLastOcrText(const QString& pluginId) const;

    void loadEnabledPlugins();
    void shutdownAll();

signals:
    void pluginsChanged();
    void pluginError(const QString& pluginId, const QString& error);
    void checkingChanged();
    void installingChanged();
    void installProgressChanged();
    void restartRequiredChanged();
    void pluginInstalled(const QString& pluginId);
    void pluginUninstalled(const QString& pluginId);
    void updateAvailable(const QString& pluginId, const QString& newVersion);
    void communityPluginsChanged();
    void loadingCommunityChanged();

private:
    struct PluginEntry {
        QString id;
        QString name;
        QString version;
        QString description;
        QString author;
        QString license;
        QString category;
        QString entryDll;
        QString dirPath;
        QStringList features;
        QStringList capabilities;
        QString size;
        int apiVersion = 0;
        bool enabled = false;
        bool loaded = false;
        bool updateAvailable = false;
        QString availableVersion;
        QString lastError;
        QVariantList settingsDefs; // from manifest.json "settings" array

        // Optional DLL exports for settings
        using GetSettingFn = const char* (*)(const char*);
        using SetSettingFn = void (*)(const char*, const char*);
        GetSettingFn fnGetSetting = nullptr;
        SetSettingFn fnSetSetting = nullptr;

        // Optional DLL exports for OCR+translate
        using CaptureOcrTranslateFn = const char* (*)(void*, int, int, int, int);
        using GetLastOcrTextFn = const char* (*)(void);
        CaptureOcrTranslateFn fnCaptureOcrTranslate = nullptr;
        GetLastOcrTextFn fnGetLastOcrText = nullptr;

#ifdef Q_OS_WIN
        HMODULE hModule = nullptr;
#endif
        MakineFn_GetInfo fnGetInfo = nullptr;
        MakineFn_Initialize fnInitialize = nullptr;
        MakineFn_Shutdown fnShutdown = nullptr;
        MakineFn_IsReady fnIsReady = nullptr;
        MakineFn_GetLastError fnGetLastError = nullptr;

        QVariantMap toVariantMap() const;
    };

    // Remote plugin entry (from plugin index)
    struct RemotePluginEntry {
        QString id;
        QString version;
        QString githubRepo;     // e.g. "MakineCeviri/makine-plugin-live"
        QString downloadUrl;    // GitHub release asset URL
        QString sha256;
        qint64 size = 0;
    };

    bool loadManifest(const QString& dirPath, PluginEntry& entry);
    bool loadPlugin(PluginEntry& entry);
    void unloadPlugin(PluginEntry& entry);
    void saveEnabledList();
    QStringList loadEnabledList() const;

    // Remote operations
    void parsePluginIndex(const QByteArray& data);
    void fetchGitHubRelease(const QString& repo, const QString& pluginId);
    bool extractPlugin(const QString& zipPath, const QString& pluginId);
    int compareVersions(const QString& a, const QString& b) const;

    // Security
    bool verifyChecksum(const QString& filePath, const QString& expected);
    bool verifyDllSignature(const QString& dllPath);
    bool validateZipContents(const QString& zipPath, const QString& pluginId);
    bool isPathSafe(const QString& path, const QString& allowedRoot);

    std::vector<PluginEntry> m_plugins;
    std::vector<RemotePluginEntry> m_remoteIndex;
    QVariantList m_communityPlugins;
    QNetworkAccessManager* m_net = nullptr;
    bool m_checking = false;
    bool m_installing = false;
    QString m_installingPluginId;
    double m_installProgress = 0.0;
    bool m_restartRequired = false;
    bool m_loadingCommunity = false;

    // GitHub configuration
    static constexpr const char* kTrustedGitHubOrg = "MakineCeviri";
    static constexpr const char* kGitHubTopic = "makine-plugin";
    static constexpr const char* kRegistryRepo = "MakineCeviri/Makine-LauncherPlugins";
    static constexpr int kCommunityMaxDisplay = 3;
};
