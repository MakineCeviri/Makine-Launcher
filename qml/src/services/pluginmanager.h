#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QString>
#include <QStringList>
#include <QLoggingCategory>
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

public:
    explicit PluginManager(QObject* parent = nullptr);
    ~PluginManager() override;

    QVariantList plugins() const;
    int pluginCount() const;

    Q_INVOKABLE void discoverPlugins();
    Q_INVOKABLE bool enablePlugin(const QString& pluginId);
    Q_INVOKABLE bool disablePlugin(const QString& pluginId);
    Q_INVOKABLE QVariantMap pluginInfo(const QString& pluginId) const;
    Q_INVOKABLE bool isPluginEnabled(const QString& pluginId) const;
    Q_INVOKABLE bool isPluginLoaded(const QString& pluginId) const;

    void loadEnabledPlugins();
    void shutdownAll();

signals:
    void pluginsChanged();
    void pluginError(const QString& pluginId, const QString& error);

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

    bool loadManifest(const QString& dirPath, PluginEntry& entry);
    bool loadPlugin(PluginEntry& entry);
    void unloadPlugin(PluginEntry& entry);
    void saveEnabledList();
    QStringList loadEnabledList() const;

    std::vector<PluginEntry> m_plugins;
};
