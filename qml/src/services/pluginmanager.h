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

#include <makineai/plugin/plugin_api.h>

Q_DECLARE_LOGGING_CATEGORY(lcPlugin)

class PluginManager : public QObject {
    Q_OBJECT

    Q_PROPERTY(QVariantList plugins READ plugins NOTIFY pluginsChanged)
    Q_PROPERTY(int pluginCount READ pluginCount NOTIFY pluginsChanged)
    Q_PROPERTY(bool checking READ isChecking NOTIFY checkingChanged)
    Q_PROPERTY(bool installing READ isInstalling NOTIFY installingChanged)
    Q_PROPERTY(double installProgress READ installProgress NOTIFY installProgressChanged)
    Q_PROPERTY(bool restartRequired READ restartRequired NOTIFY restartRequiredChanged)

public:
    explicit PluginManager(QObject* parent = nullptr);
    ~PluginManager() override;

    QVariantList plugins() const;
    int pluginCount() const;
    bool isChecking() const { return m_checking; }
    bool isInstalling() const { return m_installing; }
    double installProgress() const { return m_installProgress; }
    bool restartRequired() const { return m_restartRequired; }

    Q_INVOKABLE void discoverPlugins();
    Q_INVOKABLE bool enablePlugin(const QString& pluginId);
    Q_INVOKABLE bool disablePlugin(const QString& pluginId);
    Q_INVOKABLE QVariantMap pluginInfo(const QString& pluginId) const;
    Q_INVOKABLE bool isPluginEnabled(const QString& pluginId) const;
    Q_INVOKABLE bool isPluginLoaded(const QString& pluginId) const;

    // Install / Update / Uninstall
    Q_INVOKABLE void installPlugin(const QString& pluginId, const QString& downloadUrl);
    Q_INVOKABLE void uninstallPlugin(const QString& pluginId, bool removeData = false);
    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE bool hasUpdate(const QString& pluginId) const;
    Q_INVOKABLE QString availableVersion(const QString& pluginId) const;

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

#ifdef Q_OS_WIN
        HMODULE hModule = nullptr;
#endif
        MakineAiFn_GetInfo fnGetInfo = nullptr;
        MakineAiFn_Initialize fnInitialize = nullptr;
        MakineAiFn_Shutdown fnShutdown = nullptr;
        MakineAiFn_IsReady fnIsReady = nullptr;
        MakineAiFn_GetLastError fnGetLastError = nullptr;

        QVariantMap toVariantMap() const;
    };

    // CDN index entry
    struct CdnPluginEntry {
        QString id;
        QString version;
        QString downloadUrl;
        QString sha256;
        qint64 size = 0;
    };

    bool loadManifest(const QString& dirPath, PluginEntry& entry);
    bool loadPlugin(PluginEntry& entry);
    void unloadPlugin(PluginEntry& entry);
    void saveEnabledList();
    QStringList loadEnabledList() const;

    // CDN operations
    void parseCdnIndex(const QByteArray& data);
    void downloadPlugin(const CdnPluginEntry& cdn);
    bool extractPlugin(const QString& zipPath, const QString& pluginId);
    bool verifyChecksum(const QString& filePath, const QString& expected);
    int compareVersions(const QString& a, const QString& b) const;

    std::vector<PluginEntry> m_plugins;
    std::vector<CdnPluginEntry> m_cdnIndex;
    QNetworkAccessManager* m_net = nullptr;
    bool m_checking = false;
    bool m_installing = false;
    double m_installProgress = 0.0;
    bool m_restartRequired = false;
};
