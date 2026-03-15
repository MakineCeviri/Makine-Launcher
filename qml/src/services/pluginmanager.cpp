#include "pluginmanager.h"
#include "apppaths.h"
#include "cdnconfig.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>
#include <QNetworkReply>
#include <QCryptographicHash>
#include <QProcess>

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
        {"enabled",          enabled},
        {"loaded",           loaded},
        {"updateAvailable",  updateAvailable},
        {"availableVersion", availableVersion},
        {"lastError",        lastError},
        {"features",         QVariant(features)},
        {"capabilities",     QVariant(capabilities)},
    };
}

// ── CDN Update Check ──

void PluginManager::checkForUpdates()
{
    if (m_checking) return;

    if (!m_net)
        m_net = new QNetworkAccessManager(this);

    m_checking = true;
    emit checkingChanged();

    const QString url = QString::fromLatin1(makineai::cdn::kAssetsBase)
                        + QStringLiteral("plugins/index.json");
    auto* reply = m_net->get(QNetworkRequest(QUrl(url)));

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_checking = false;
        emit checkingChanged();

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcPlugin) << "Failed to fetch plugin index:" << reply->errorString();
            return;
        }

        parseCdnIndex(reply->readAll());
    });
}

void PluginManager::parseCdnIndex(const QByteArray& data)
{
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        qCWarning(lcPlugin) << "Plugin index parse error:" << err.errorString();
        return;
    }

    m_cdnIndex.clear();
    const auto arr = doc.object()["plugins"].toArray();
    for (const auto& item : arr) {
        auto obj = item.toObject();
        CdnPluginEntry cdn;
        cdn.id          = obj["id"].toString();
        cdn.version     = obj["version"].toString();
        cdn.downloadUrl = obj["downloadUrl"].toString();
        cdn.sha256      = obj["sha256"].toString();
        cdn.size        = obj["size"].toInteger(0);
        m_cdnIndex.push_back(std::move(cdn));
    }

    qCInfo(lcPlugin) << "CDN index:" << m_cdnIndex.size() << "plugin(s) available";

    // Check installed plugins for updates
    bool anyUpdate = false;
    for (auto& p : m_plugins) {
        for (const auto& cdn : m_cdnIndex) {
            if (cdn.id == p.id && compareVersions(cdn.version, p.version) > 0) {
                p.updateAvailable = true;
                p.availableVersion = cdn.version;
                anyUpdate = true;
                qCInfo(lcPlugin) << "Update available:" << p.id
                                 << p.version << "->" << cdn.version;
                emit updateAvailable(p.id, cdn.version);
            }
        }
    }

    if (anyUpdate)
        emit pluginsChanged();
}

// ── Install ──

void PluginManager::installPlugin(const QString& pluginId, const QString& downloadUrl)
{
    if (m_installing) return;

    // Find CDN entry (prefer passed URL, fallback to index)
    QString url = downloadUrl;
    QString expectedHash;
    if (url.isEmpty()) {
        for (const auto& cdn : m_cdnIndex) {
            if (cdn.id == pluginId) {
                url = cdn.downloadUrl;
                expectedHash = cdn.sha256;
                break;
            }
        }
    }

    if (url.isEmpty()) {
        emit pluginError(pluginId, QStringLiteral("No download URL for plugin"));
        return;
    }

    if (!m_net)
        m_net = new QNetworkAccessManager(this);

    m_installing = true;
    m_installProgress = 0.0;
    emit installingChanged();
    emit installProgressChanged();

    auto* reply = m_net->get(QNetworkRequest(QUrl(url)));

    connect(reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
        if (total > 0) {
            m_installProgress = static_cast<double>(received) / static_cast<double>(total);
            emit installProgressChanged();
        }
    });

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, pluginId, expectedHash]() {
        reply->deleteLater();
        m_installing = false;
        m_installProgress = 1.0;
        emit installingChanged();
        emit installProgressChanged();

        if (reply->error() != QNetworkReply::NoError) {
            emit pluginError(pluginId, reply->errorString());
            return;
        }

        // Save to temp
        const QString tempPath = AppPaths::downloadTempDir() + "/" + pluginId + ".zip";
        QDir().mkpath(AppPaths::downloadTempDir());
        QFile file(tempPath);
        if (!file.open(QIODevice::WriteOnly)) {
            emit pluginError(pluginId, QStringLiteral("Cannot write temp file"));
            return;
        }
        file.write(reply->readAll());
        file.close();

        // Verify checksum
        if (!expectedHash.isEmpty() && !verifyChecksum(tempPath, expectedHash)) {
            QFile::remove(tempPath);
            emit pluginError(pluginId, QStringLiteral("Checksum verification failed"));
            return;
        }

        // Extract
        if (!extractPlugin(tempPath, pluginId)) {
            QFile::remove(tempPath);
            emit pluginError(pluginId, QStringLiteral("Extraction failed"));
            return;
        }

        QFile::remove(tempPath);

        // Re-discover to pick up the new plugin
        discoverPlugins();
        emit pluginInstalled(pluginId);
        qCInfo(lcPlugin) << "Installed plugin:" << pluginId;
    });
}

// ── Uninstall ──

void PluginManager::uninstallPlugin(const QString& pluginId, bool removeData)
{
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        if (it->id == pluginId) {
            if (it->loaded) {
                unloadPlugin(*it);
                m_restartRequired = true;
                emit restartRequiredChanged();
            }

            // Remove plugin directory
            QDir pluginDir(it->dirPath);
            if (pluginDir.exists())
                pluginDir.removeRecursively();

            // Optionally remove plugin data
            if (removeData) {
                QDir dataDir(AppPaths::pluginDataDir() + "/" + pluginId);
                if (dataDir.exists())
                    dataDir.removeRecursively();
            }

            m_plugins.erase(it);
            saveEnabledList();
            emit pluginsChanged();
            emit pluginUninstalled(pluginId);
            qCInfo(lcPlugin) << "Uninstalled plugin:" << pluginId
                             << (removeData ? "(data removed)" : "(data kept)");
            return;
        }
    }
}

// ── Query Helpers ──

bool PluginManager::hasUpdate(const QString& pluginId) const
{
    for (const auto& p : m_plugins)
        if (p.id == pluginId)
            return p.updateAvailable;
    return false;
}

QString PluginManager::availableVersion(const QString& pluginId) const
{
    for (const auto& p : m_plugins)
        if (p.id == pluginId)
            return p.availableVersion;
    return {};
}

// ── Utilities ──

bool PluginManager::extractPlugin(const QString& zipPath, const QString& pluginId)
{
    const QString destDir = AppPaths::pluginsDir() + "/" + pluginId;
    QDir().mkpath(destDir);

    // Use Qt's built-in tar/zip via QProcess (cmake -E tar)
    QProcess proc;
    proc.setWorkingDirectory(destDir);
    proc.start(QStringLiteral("cmake"), {"-E", "tar", "xf", zipPath});
    if (!proc.waitForFinished(30000)) {
        qCWarning(lcPlugin) << "Extract timeout for" << pluginId;
        return false;
    }
    if (proc.exitCode() != 0) {
        qCWarning(lcPlugin) << "Extract failed:" << proc.readAllStandardError();
        return false;
    }
    return true;
}

bool PluginManager::verifyChecksum(const QString& filePath, const QString& expected)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    const QString actual = hash.result().toHex();

    if (actual != expected.toLower()) {
        qCWarning(lcPlugin) << "Checksum mismatch:" << actual << "!=" << expected;
        return false;
    }
    return true;
}

int PluginManager::compareVersions(const QString& a, const QString& b) const
{
    const auto partsA = a.split('.');
    const auto partsB = b.split('.');
    const int len = qMax(partsA.size(), partsB.size());

    for (int i = 0; i < len; ++i) {
        int va = (i < partsA.size()) ? partsA[i].toInt() : 0;
        int vb = (i < partsB.size()) ? partsB[i].toInt() : 0;
        if (va != vb) return va - vb;
    }
    return 0;
}
