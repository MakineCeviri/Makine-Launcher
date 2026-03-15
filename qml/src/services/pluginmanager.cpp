#include "pluginmanager.h"
#include "apppaths.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>

using namespace makineai;

Q_LOGGING_CATEGORY(lcPlugin, "makineai.plugin")

PluginManager::PluginManager(QObject* parent)
    : QObject(parent)
{
}

PluginManager::~PluginManager()
{
    shutdownAll();
}

// ── QML Properties ──

QVariantList PluginManager::plugins() const
{
    QVariantList list;
    list.reserve(static_cast<int>(m_plugins.size()));
    for (const auto& p : m_plugins)
        list.append(p.toVariantMap());
    return list;
}

int PluginManager::pluginCount() const
{
    return static_cast<int>(m_plugins.size());
}

// ── Discovery ──

void PluginManager::discoverPlugins()
{
    m_plugins.clear();

    const QString pluginsPath = AppPaths::pluginsDir();
    QDir dir(pluginsPath);
    if (!dir.exists()) {
        qCDebug(lcPlugin) << "Plugins directory does not exist:" << pluginsPath;
        emit pluginsChanged();
        return;
    }

    const auto enabledList = loadEnabledList();
    const auto entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const auto& entry : entries) {
        const QString pluginDir = pluginsPath + "/" + entry;
        PluginEntry plugin;
        if (loadManifest(pluginDir, plugin)) {
            plugin.enabled = enabledList.contains(plugin.id);
            qCDebug(lcPlugin) << "Discovered plugin:" << plugin.id
                              << "v" << plugin.version
                              << (plugin.enabled ? "(enabled)" : "(disabled)");
            m_plugins.push_back(std::move(plugin));
        }
    }

    qCInfo(lcPlugin) << "Discovered" << m_plugins.size() << "plugin(s)";
    emit pluginsChanged();
}

// ── Manifest Parsing ──

bool PluginManager::loadManifest(const QString& dirPath, PluginEntry& entry)
{
    const QString manifestPath = dirPath + "/manifest.json";
    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcPlugin) << "Cannot open manifest:" << manifestPath;
        return false;
    }

    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qCWarning(lcPlugin) << "Manifest parse error in" << manifestPath
                            << ":" << parseError.errorString();
        return false;
    }

    const auto obj = doc.object();

    entry.id          = obj["id"].toString();
    entry.name        = obj["name"].toString();
    entry.version     = obj["version"].toString();
    entry.description = obj["description"].toString();
    entry.author      = obj["author"].toString();
    entry.license     = obj["license"].toString();
    entry.category    = obj["category"].toString();
    entry.entryDll    = obj["entry"].toString();
    entry.apiVersion  = obj["apiVersion"].toInt(0);
    entry.dirPath     = dirPath;
    entry.size        = obj["size"].toString();

    for (const auto& f : obj["features"].toArray())
        entry.features.append(f.toString());

    for (const auto& c : obj["capabilities"].toArray())
        entry.capabilities.append(c.toString());

    // Validate required fields
    if (entry.id.isEmpty() || entry.name.isEmpty() || entry.entryDll.isEmpty()) {
        qCWarning(lcPlugin) << "Manifest missing required fields:" << manifestPath;
        return false;
    }

    // Check API version compatibility
    if (entry.apiVersion > MAKINEAI_PLUGIN_API_VERSION) {
        qCWarning(lcPlugin) << "Plugin" << entry.id
                            << "requires API v" << entry.apiVersion
                            << "(launcher supports v" << MAKINEAI_PLUGIN_API_VERSION << ")";
        entry.lastError = QStringLiteral("Incompatible API version");
        return false;
    }

    return true;
}

// ── DLL Loading ──

bool PluginManager::loadPlugin(PluginEntry& entry)
{
    if (entry.loaded)
        return true;

#ifdef Q_OS_WIN
    const QString dllPath = entry.dirPath + "/" + entry.entryDll;
    if (!QFile::exists(dllPath)) {
        entry.lastError = "DLL not found: " + entry.entryDll;
        qCWarning(lcPlugin) << entry.lastError;
        return false;
    }

    entry.hModule = LoadLibraryW(reinterpret_cast<LPCWSTR>(dllPath.utf16()));
    if (!entry.hModule) {
        entry.lastError = "LoadLibrary failed (error " + QString::number(GetLastError()) + ")";
        qCWarning(lcPlugin) << "Plugin" << entry.id << ":" << entry.lastError;
        return false;
    }

    // Resolve required exported symbols
    entry.fnGetInfo     = reinterpret_cast<MakineAiFn_GetInfo>(
                              GetProcAddress(entry.hModule, "makineai_get_info"));
    entry.fnInitialize  = reinterpret_cast<MakineAiFn_Initialize>(
                              GetProcAddress(entry.hModule, "makineai_initialize"));
    entry.fnShutdown    = reinterpret_cast<MakineAiFn_Shutdown>(
                              GetProcAddress(entry.hModule, "makineai_shutdown"));
    entry.fnIsReady     = reinterpret_cast<MakineAiFn_IsReady>(
                              GetProcAddress(entry.hModule, "makineai_is_ready"));
    entry.fnGetLastError = reinterpret_cast<MakineAiFn_GetLastError>(
                              GetProcAddress(entry.hModule, "makineai_get_last_error"));

    if (!entry.fnGetInfo || !entry.fnInitialize || !entry.fnShutdown
        || !entry.fnIsReady || !entry.fnGetLastError) {
        entry.lastError = "Missing required exports in " + entry.entryDll;
        qCWarning(lcPlugin) << "Plugin" << entry.id << ":" << entry.lastError;
        FreeLibrary(entry.hModule);
        entry.hModule = nullptr;
        return false;
    }

    // Initialize the plugin with its data directory
    const QString dataPath = AppPaths::pluginDataDir() + "/" + entry.id;
    QDir().mkpath(dataPath);

    MakineAiError err = entry.fnInitialize(dataPath.toUtf8().constData());
    if (err != MAKINEAI_OK) {
        const char* errMsg = entry.fnGetLastError();
        entry.lastError = errMsg ? QString::fromUtf8(errMsg) : QStringLiteral("Initialize failed");
        qCWarning(lcPlugin) << "Plugin" << entry.id << "init failed:" << entry.lastError;
        FreeLibrary(entry.hModule);
        entry.hModule = nullptr;
        return false;
    }

    entry.loaded = true;
    qCInfo(lcPlugin) << "Loaded plugin:" << entry.id << "v" << entry.version;
    return true;
#else
    entry.lastError = "Plugin loading not supported on this platform";
    return false;
#endif
}

void PluginManager::unloadPlugin(PluginEntry& entry)
{
    if (!entry.loaded)
        return;

    if (entry.fnShutdown)
        entry.fnShutdown();

#ifdef Q_OS_WIN
    if (entry.hModule) {
        FreeLibrary(entry.hModule);
        entry.hModule = nullptr;
    }
#endif

    entry.fnGetInfo = nullptr;
    entry.fnInitialize = nullptr;
    entry.fnShutdown = nullptr;
    entry.fnIsReady = nullptr;
    entry.fnGetLastError = nullptr;
    entry.loaded = false;

    qCInfo(lcPlugin) << "Unloaded plugin:" << entry.id;
}

// ── Lifecycle ──

void PluginManager::loadEnabledPlugins()
{
    for (auto& p : m_plugins) {
        if (p.enabled && !p.loaded) {
            if (!loadPlugin(p))
                emit pluginError(p.id, p.lastError);
        }
    }
    emit pluginsChanged();
}

void PluginManager::shutdownAll()
{
    for (auto& p : m_plugins)
        unloadPlugin(p);
}

// ── Enable/Disable ──

bool PluginManager::enablePlugin(const QString& pluginId)
{
    for (auto& p : m_plugins) {
        if (p.id == pluginId) {
            p.enabled = true;
            saveEnabledList();
            emit pluginsChanged();
            qCInfo(lcPlugin) << "Enabled plugin:" << pluginId << "(restart required)";
            return true;
        }
    }
    return false;
}

bool PluginManager::disablePlugin(const QString& pluginId)
{
    for (auto& p : m_plugins) {
        if (p.id == pluginId) {
            p.enabled = false;
            saveEnabledList();
            emit pluginsChanged();
            qCInfo(lcPlugin) << "Disabled plugin:" << pluginId << "(restart required)";
            return true;
        }
    }
    return false;
}

// ── Queries ──

QVariantMap PluginManager::pluginInfo(const QString& pluginId) const
{
    for (const auto& p : m_plugins)
        if (p.id == pluginId)
            return p.toVariantMap();
    return {};
}

bool PluginManager::isPluginEnabled(const QString& pluginId) const
{
    for (const auto& p : m_plugins)
        if (p.id == pluginId)
            return p.enabled;
    return false;
}

bool PluginManager::isPluginLoaded(const QString& pluginId) const
{
    for (const auto& p : m_plugins)
        if (p.id == pluginId)
            return p.loaded;
    return false;
}

// ── Persistence ──

void PluginManager::saveEnabledList()
{
    QSettings s;
    QStringList enabled;
    for (const auto& p : m_plugins)
        if (p.enabled)
            enabled.append(p.id);
    s.setValue("plugins/enabled", enabled);
}

QStringList PluginManager::loadEnabledList() const
{
    QSettings s;
    return s.value("plugins/enabled").toStringList();
}

// ── Serialization ──

QVariantMap PluginManager::PluginEntry::toVariantMap() const
{
    return {
        {"id",           id},
        {"name",         name},
        {"version",      version},
        {"description",  description},
        {"author",       author},
        {"license",      license},
        {"category",     category},
        {"size",         size},
        {"apiVersion",   apiVersion},
        {"enabled",      enabled},
        {"loaded",       loaded},
        {"lastError",    lastError},
        {"features",     QVariant(features)},
        {"capabilities", QVariant(capabilities)},
    };
}
