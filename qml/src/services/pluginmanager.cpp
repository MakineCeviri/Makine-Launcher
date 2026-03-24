#include "pluginmanager.h"
#include "apppaths.h"
#include "cdnconfig.h"
#include "networksecurity.h"

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
#include <QRegularExpression>
#include <QDesktopServices>

#ifndef MAKINE_UI_ONLY
#include "mkpkformat.h"
#endif

#ifdef Q_OS_WIN
#include <wintrust.h>
#include <softpub.h>
#endif

using namespace makine;

Q_LOGGING_CATEGORY(lcPlugin, "makine.plugin")

PluginManager::PluginManager(QObject* parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
    security::installTlsPinning(m_net);
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

    // Parse settings definitions from manifest
    for (const auto& s : obj["settings"].toArray()) {
        auto sObj = s.toObject();
        QVariantMap setting;
        setting["key"]     = sObj["key"].toString();
        setting["type"]    = sObj["type"].toString();
        setting["label"]   = sObj["label"].toString();
        setting["default"] = sObj["default"].toVariant();
        QVariantList opts;
        for (const auto& o : sObj["options"].toArray())
            opts.append(o.toString());
        setting["options"] = opts;
        entry.settingsDefs.append(setting);
    }

    // Validate required fields
    if (entry.id.isEmpty() || entry.name.isEmpty() || entry.entryDll.isEmpty()) {
        qCWarning(lcPlugin) << "Manifest missing required fields:" << manifestPath;
        return false;
    }

    // Check API version compatibility
    if (entry.apiVersion > MAKINE_PLUGIN_API_VERSION) {
        qCWarning(lcPlugin) << "Plugin" << entry.id
                            << "requires API v" << entry.apiVersion
                            << "(launcher supports v" << MAKINE_PLUGIN_API_VERSION << ")";
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
    entry.fnGetInfo     = reinterpret_cast<MakineFn_GetInfo>(
                              GetProcAddress(entry.hModule, "makine_get_info"));
    entry.fnInitialize  = reinterpret_cast<MakineFn_Initialize>(
                              GetProcAddress(entry.hModule, "makine_initialize"));
    entry.fnShutdown    = reinterpret_cast<MakineFn_Shutdown>(
                              GetProcAddress(entry.hModule, "makine_shutdown"));
    entry.fnIsReady     = reinterpret_cast<MakineFn_IsReady>(
                              GetProcAddress(entry.hModule, "makine_is_ready"));
    entry.fnGetLastError = reinterpret_cast<MakineFn_GetLastError>(
                              GetProcAddress(entry.hModule, "makine_get_last_error"));

    // Optional: settings exports
    entry.fnGetSetting = reinterpret_cast<PluginEntry::GetSettingFn>(
                              GetProcAddress(entry.hModule, "makine_get_setting"));
    entry.fnSetSetting = reinterpret_cast<PluginEntry::SetSettingFn>(
                              GetProcAddress(entry.hModule, "makine_set_setting"));

    // Optional: OCR+translate exports
    entry.fnCaptureOcrTranslate = reinterpret_cast<PluginEntry::CaptureOcrTranslateFn>(
                              GetProcAddress(entry.hModule, "makine_capture_ocr_translate"));
    entry.fnGetLastOcrText = reinterpret_cast<PluginEntry::GetLastOcrTextFn>(
                              GetProcAddress(entry.hModule, "makine_get_last_ocr_text"));

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

    MakineError err = entry.fnInitialize(dataPath.toUtf8().constData());
    if (err != MAKINE_OK) {
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
            // Load DLL immediately — no restart needed
            if (!p.loaded) {
                if (!loadPlugin(p))
                    emit pluginError(p.id, p.lastError);
            }
            emit pluginsChanged();
            qCInfo(lcPlugin) << "Enabled plugin:" << pluginId;
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
            // Unload DLL immediately — no restart needed
            if (p.loaded)
                unloadPlugin(p);
            emit pluginsChanged();
            qCInfo(lcPlugin) << "Disabled plugin:" << pluginId;
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

QString PluginManager::callPluginOcr(const QString& pluginId, int x, int y, int w, int h)
{
    for (const auto& p : m_plugins) {
        if (p.id == pluginId && p.loaded && p.fnCaptureOcrTranslate) {
            const char* result = p.fnCaptureOcrTranslate(nullptr, x, y, w, h);
            return result ? QString::fromUtf8(result) : QString();
        }
    }
    return {};
}

QString PluginManager::getPluginLastOcrText(const QString& pluginId) const
{
    for (const auto& p : m_plugins) {
        if (p.id == pluginId && p.loaded && p.fnGetLastOcrText) {
            const char* result = p.fnGetLastOcrText();
            return result ? QString::fromUtf8(result) : QString();
        }
    }
    return {};
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
        {"settings",         settingsDefs},
        {"hasSettings",      !settingsDefs.isEmpty()},
    };
}

// ── Update Check (from registry repo on GitHub) ──

void PluginManager::checkForUpdates()
{
    if (m_checking) return;



    m_checking = true;
    emit checkingChanged();

    // Fetch index.json from registry repo (raw content via GitHub)
    const QString url = QStringLiteral("https://raw.githubusercontent.com/%1/main/index.json")
                            .arg(QString::fromLatin1(kRegistryRepo));
    QNetworkRequest req{QUrl(url)};
    req.setRawHeader("User-Agent", "Makine-Launcher");
    auto* reply = m_net->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_checking = false;
        emit checkingChanged();

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcPlugin) << "Failed to fetch plugin index:" << reply->errorString();
            return;
        }

        parsePluginIndex(reply->readAll());
    });
}

void PluginManager::parsePluginIndex(const QByteArray& data)
{
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        qCWarning(lcPlugin) << "Plugin index parse error:" << err.errorString();
        return;
    }

    m_remoteIndex.clear();
    const auto arr = doc.object()["plugins"].toArray();
    for (const auto& item : arr) {
        auto obj = item.toObject();
        RemotePluginEntry remote;
        remote.id          = obj["id"].toString();
        remote.version     = obj["version"].toString();
        remote.githubRepo  = obj["githubRepo"].toString();
        remote.downloadUrl = obj["downloadUrl"].toString();
        remote.sha256      = obj["sha256"].toString();
        remote.size        = obj["size"].toInteger(0);

        // Security: only accept plugins from trusted GitHub org
        if (!remote.githubRepo.isEmpty()
            && !remote.githubRepo.startsWith(QString::fromLatin1(kTrustedGitHubOrg) + "/")) {
            qCWarning(lcPlugin) << "Rejected untrusted plugin:" << remote.id
                                << "from repo:" << remote.githubRepo;
            continue;
        }

        m_remoteIndex.push_back(std::move(remote));
    }

    qCInfo(lcPlugin) << "Plugin index:" << m_remoteIndex.size() << "trusted plugin(s) available";

    // Check installed plugins for updates
    bool anyUpdate = false;
    for (auto& p : m_plugins) {
        for (const auto& remote : m_remoteIndex) {
            if (remote.id == p.id && compareVersions(remote.version, p.version) > 0) {
                p.updateAvailable = true;
                p.availableVersion = remote.version;
                anyUpdate = true;
                qCInfo(lcPlugin) << "Update available:" << p.id
                                 << p.version << "->" << remote.version;
                emit updateAvailable(p.id, remote.version);
            }
        }
    }

    if (anyUpdate)
        emit pluginsChanged();
}

// ── GitHub Release Fetch ──

void PluginManager::fetchGitHubRelease(const QString& repo, const QString& pluginId)
{


    // Security: validate repo format and trusted org
    if (!repo.startsWith(QString::fromLatin1(kTrustedGitHubOrg) + "/")) {
        emit pluginError(pluginId, QStringLiteral("Untrusted repository: ") + repo);
        return;
    }

    // Only allow alphanumeric, dash, underscore, slash in repo name
    static const QRegularExpression repoPattern(QStringLiteral("^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$"));
    if (!repoPattern.match(repo).hasMatch()) {
        emit pluginError(pluginId, QStringLiteral("Invalid repository name"));
        return;
    }

    const QString apiUrl = QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(repo);
    QNetworkRequest req{QUrl(apiUrl)};
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("User-Agent", "Makine-Launcher");

    auto* reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, pluginId]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcPlugin) << "GitHub API error for" << pluginId << ":" << reply->errorString();
            return;
        }

        const auto doc = QJsonDocument::fromJson(reply->readAll());
        const auto assets = doc.object()["assets"].toArray();

        for (const auto& asset : assets) {
            auto obj = asset.toObject();
            const QString name = obj["name"].toString();
            if (name.endsWith(QStringLiteral(".makine"))) {
                const QString downloadUrl = obj["browser_download_url"].toString();

                // Security: verify download URL is from github.com
                QUrl parsed(downloadUrl);
                if (parsed.host() != QStringLiteral("github.com")
                    && !parsed.host().endsWith(QStringLiteral(".github.com"))
                    && !parsed.host().endsWith(QStringLiteral(".githubusercontent.com"))) {
                    qCWarning(lcPlugin) << "Rejected non-GitHub download URL:" << downloadUrl;
                    emit pluginError(pluginId, QStringLiteral("Download URL not from GitHub"));
                    return;
                }

                installPlugin(pluginId, downloadUrl);
                return;
            }
        }

        emit pluginError(pluginId, QStringLiteral("No .makine asset found in latest release"));
    });
}

// ── Install ──

void PluginManager::installPlugin(const QString& pluginId, const QString& downloadUrl)
{
    if (m_installing) return;

    // Find remote entry (prefer passed URL, fallback to index)
    QString url = downloadUrl;
    QString expectedHash;
    if (url.isEmpty()) {
        for (const auto& remote : m_remoteIndex) {
            if (remote.id == pluginId) {
                // If GitHub repo is set and no direct URL, fetch from GitHub
                if (remote.downloadUrl.isEmpty() && !remote.githubRepo.isEmpty()) {
                    fetchGitHubRelease(remote.githubRepo, pluginId);
                    return;
                }
                url = remote.downloadUrl;
                expectedHash = remote.sha256;
                break;
            }
        }
    }

    if (url.isEmpty()) {
        emit pluginError(pluginId, QStringLiteral("No download URL for plugin"));
        return;
    }

    // Security: validate URL scheme
    QUrl parsed(url);
    if (parsed.scheme() != QStringLiteral("https")) {
        emit pluginError(pluginId, QStringLiteral("Only HTTPS downloads allowed"));
        return;
    }

    // Security: only allow downloads from trusted domains
    const QString host = parsed.host();
    bool trustedHost = host == QStringLiteral("github.com")
                    || host.endsWith(QStringLiteral(".github.com"))
                    || host.endsWith(QStringLiteral(".githubusercontent.com"))
                    || host == QStringLiteral("cdn.makineceviri.org");
    if (!trustedHost) {
        emit pluginError(pluginId, QStringLiteral("Untrusted download domain: ") + host);
        return;
    }



    m_installing = true;
    m_installingPluginId = pluginId;
    m_installProgress = 0.0;
    emit installingChanged();
    emit installProgressChanged();

    QNetworkRequest req{parsed};
    req.setRawHeader("User-Agent", "Makine-Launcher");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    auto* reply = m_net->get(req);

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
        m_installingPluginId.clear();
        m_installProgress = 1.0;
        emit installingChanged();
        emit installProgressChanged();

        if (reply->error() != QNetworkReply::NoError) {
            emit pluginError(pluginId, reply->errorString());
            return;
        }

        // Security: check response size (max 100MB)
        const auto data = reply->readAll();
        if (data.size() > 100 * 1024 * 1024) {
            emit pluginError(pluginId, QStringLiteral("Download exceeds 100MB limit"));
            return;
        }

        // Save to temp
        const QString tempPath = AppPaths::downloadTempDir() + "/" + pluginId + ".makine";
        QDir().mkpath(AppPaths::downloadTempDir());
        QFile file(tempPath);
        if (!file.open(QIODevice::WriteOnly)) {
            emit pluginError(pluginId, QStringLiteral("Cannot write temp file"));
            return;
        }
        file.write(data);
        file.close();

        // Security: verify SHA-256 checksum (mandatory for index plugins)
        if (!expectedHash.isEmpty() && !verifyChecksum(tempPath, expectedHash)) {
            QFile::remove(tempPath);
            emit pluginError(pluginId, QStringLiteral("Checksum verification failed — download may be tampered"));
            return;
        }

        // Security: validate zip contents before extraction
        if (!validateZipContents(tempPath, pluginId)) {
            QFile::remove(tempPath);
            return; // error already emitted
        }

        // Unload DLL before overwriting (Windows locks loaded DLLs)
        bool wasEnabled = false;
        for (auto& p : m_plugins) {
            if (p.id == pluginId) {
                wasEnabled = p.enabled;
                if (p.loaded) unloadPlugin(p);
                break;
            }
        }

        // Extract (overwrites existing files)
        if (!extractPlugin(tempPath, pluginId)) {
            QFile::remove(tempPath);
            emit pluginError(pluginId, QStringLiteral("Extraction failed"));
            return;
        }

        QFile::remove(tempPath);

        // Re-discover and auto-enable (fresh install or update)
        discoverPlugins();
        enablePlugin(pluginId);
        emit pluginInstalled(pluginId);
        qCInfo(lcPlugin) << "Installed plugin:" << pluginId;
    });
}

// ── Install from local .makine file ──

void PluginManager::installFromFile(const QString& filePath)
{
    QFileInfo fi(filePath);
    if (!fi.exists()) {
        emit pluginError({}, QStringLiteral("File not found: ") + filePath);
        return;
    }

    QString tempId = fi.completeBaseName();

    if (!validateZipContents(filePath, tempId))
        return;

    // Unload if already installed (DLL lock prevention)
    bool wasEnabled = false;
    for (auto& p : m_plugins) {
        if (p.id == tempId || p.dirPath.endsWith("/" + tempId)) {
            wasEnabled = p.enabled;
            if (p.loaded) unloadPlugin(p);
            break;
        }
    }

    if (!extractPlugin(filePath, tempId)) {
        emit pluginError(tempId, QStringLiteral("Extraction failed"));
        return;
    }

    // Re-discover and auto-enable
    discoverPlugins();
    enablePlugin(tempId);
    emit pluginInstalled(tempId);
    qCInfo(lcPlugin) << "Installed plugin from file:" << filePath;
}

// ── Query: last error for a plugin ──

QString PluginManager::lastPluginError(const QString& pluginId) const
{
    for (const auto& p : m_plugins)
        if (p.id == pluginId)
            return p.lastError;
    return {};
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

// ── Security: Path Safety ──

bool PluginManager::isPathSafe(const QString& path, const QString& allowedRoot)
{
    // Resolve to canonical path, prevent path traversal (../ attacks)
    QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    if (canonical.isEmpty())
        return false; // path doesn't exist yet, check the absolute version
    const QString absPath = info.absoluteFilePath();

    QFileInfo rootInfo(allowedRoot);
    const QString canonicalRoot = rootInfo.canonicalFilePath().isEmpty()
                                  ? rootInfo.absoluteFilePath()
                                  : rootInfo.canonicalFilePath();

    return absPath.startsWith(canonicalRoot + "/") || absPath == canonicalRoot;
}

// ── Security: Package Content Validation ──

bool PluginManager::validateZipContents(const QString& packagePath, const QString& pluginId)
{
    // Read and validate the .makine header
    QFile file(packagePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit pluginError(pluginId, QStringLiteral("Cannot open package file"));
        return false;
    }

    const QByteArray header = file.read(5);
    file.close();

    if (header.size() < 5) {
        emit pluginError(pluginId, QStringLiteral("Package too small"));
        return false;
    }

    // Validate MKPK magic bytes
    if (header[0] != 'M' || header[1] != 'K' || header[2] != 'P' || header[3] != 'K') {
        emit pluginError(pluginId, QStringLiteral("Invalid .makine package (bad magic bytes)"));
        return false;
    }

    // Plugins must be v2 (unencrypted)
    if (static_cast<uint8_t>(header[4]) != 2) {
        emit pluginError(pluginId, QStringLiteral("Plugin packages must be MKPK v2 (unencrypted)"));
        return false;
    }

    // File size sanity check (max 100MB)
    QFileInfo fi(packagePath);
    if (fi.size() > 100 * 1024 * 1024) {
        emit pluginError(pluginId, QStringLiteral("Package exceeds 100MB limit"));
        return false;
    }

    return true;
}

// ── Security: DLL Signature Verification ──

bool PluginManager::verifyDllSignature(const QString& dllPath)
{
#ifdef Q_OS_WIN
    // Use WinVerifyTrust to check Authenticode signature
    WINTRUST_FILE_INFO fileInfo = {};
    fileInfo.cbStruct = sizeof(fileInfo);
    fileInfo.pcwszFilePath = reinterpret_cast<LPCWSTR>(dllPath.utf16());

    GUID policyGuid = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    WINTRUST_DATA trustData = {};
    trustData.cbStruct = sizeof(trustData);
    trustData.dwUIChoice = WTD_UI_NONE;
    trustData.fdwRevocationChecks = WTD_REVOKE_NONE;
    trustData.dwUnionChoice = WTD_CHOICE_FILE;
    trustData.pFile = &fileInfo;
    trustData.dwStateAction = WTD_STATEACTION_VERIFY;

    LONG result = WinVerifyTrust(nullptr, &policyGuid, &trustData);

    // Cleanup
    trustData.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &policyGuid, &trustData);

    if (result == ERROR_SUCCESS) {
        qCInfo(lcPlugin) << "DLL signature valid:" << dllPath;
        return true;
    }

    qCWarning(lcPlugin) << "DLL signature check failed (code:" << result << "):" << dllPath;
    // Don't block unsigned plugins — just warn.
    // Official plugins will always be signed; community plugins may not be.
    return true; // Allow unsigned for now, log the warning
#else
    Q_UNUSED(dllPath);
    return true;
#endif
}

// ── Security: Checksum Verification ──

bool PluginManager::verifyChecksum(const QString& filePath, const QString& expected)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
        return false;

    const QString actual = hash.result().toHex();
    if (actual != expected.toLower()) {
        qCWarning(lcPlugin) << "Checksum mismatch:"
                            << "expected:" << expected
                            << "actual:" << actual;
        return false;
    }

    qCDebug(lcPlugin) << "Checksum verified:" << filePath;
    return true;
}

// ── Extraction ──

bool PluginManager::extractPlugin(const QString& packagePath, const QString& pluginId)
{
    const QString destDir = AppPaths::pluginsDir() + "/" + pluginId;
    QDir().mkpath(destDir);

#ifndef MAKINE_UI_ONLY
    // Use .makine format (MKPK v2: zstd + tar, no encryption)
    QFile file(packagePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcPlugin) << "Cannot open package:" << packagePath;
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    makine::mkpk::MkpkError mkpkErr{""};
    int count = makine::mkpk::process_mkpkg(
        reinterpret_cast<const uint8_t*>(data.constData()),
        static_cast<size_t>(data.size()),
        destDir.toStdWString(),
        &mkpkErr);

    if (count < 0) {
        qCWarning(lcPlugin) << "Plugin extraction failed:" << QString::fromStdString(mkpkErr.message);
        QDir(destDir).removeRecursively();
        return false;
    }

    qCInfo(lcPlugin) << "Extracted" << count << "files for plugin" << pluginId;
#else
    // UI-only build: fallback to cmake tar extraction
    QProcess proc;
    proc.setWorkingDirectory(destDir);
    proc.start(QStringLiteral("cmake"), {"-E", "tar", "xf", packagePath});
    if (!proc.waitForFinished(30000) || proc.exitCode() != 0) {
        qCWarning(lcPlugin) << "Extract failed for" << pluginId;
        QDir(destDir).removeRecursively();
        return false;
    }
#endif

    // Post-extraction: verify manifest.json exists
    if (!QFile::exists(destDir + "/manifest.json")) {
        qCWarning(lcPlugin) << "Extracted plugin missing manifest.json";
        QDir(destDir).removeRecursively();
        return false;
    }

    return true;
}

// ── Version Comparison ──

// ── Plugin Settings ──

QVariantList PluginManager::pluginSettings(const QString& pluginId) const
{
    for (const auto& p : m_plugins)
        if (p.id == pluginId)
            return p.settingsDefs;
    return {};
}

QString PluginManager::getPluginSetting(const QString& pluginId, const QString& key) const
{
    for (const auto& p : m_plugins) {
        if (p.id == pluginId && p.fnGetSetting) {
            const char* val = p.fnGetSetting(key.toUtf8().constData());
            return val ? QString::fromUtf8(val) : QString();
        }
    }
    return {};
}

void PluginManager::setPluginSetting(const QString& pluginId, const QString& key, const QString& value)
{
    for (const auto& p : m_plugins) {
        if (p.id == pluginId && p.fnSetSetting) {
            p.fnSetSetting(key.toUtf8().constData(), value.toUtf8().constData());
            emit pluginsChanged(); // trigger UI refresh
            return;
        }
    }
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

// ── Settings Discovery (moved from SettingsScreen.qml) ──

QVariantList PluginManager::settingsCategories() const
{
    QVariantList result;
    for (const auto& p : m_plugins) {
        if (p.loaded && !p.settingsDefs.isEmpty()) {
            result.append(QVariantMap{
                {"name",        p.name},
                {"description", p.description.isEmpty()
                                    ? tr("Eklenti ayarları")
                                    : p.description},
                {"isPlugin",    true},
                {"pluginId",    p.id}
            });
        }
    }
    return result;
}

QVariantList PluginManager::pluginsWithSettings() const
{
    QVariantList result;
    for (const auto& p : m_plugins) {
        if (p.loaded && !p.settingsDefs.isEmpty())
            result.append(p.toVariantMap());
    }
    return result;
}

// ── Community Plugin Discovery (GitHub Topic Search) ──

void PluginManager::fetchCommunityPlugins()
{
    if (m_loadingCommunity) return;



    m_loadingCommunity = true;
    emit loadingCommunityChanged();

    // Search GitHub for repos with "makine-plugin" topic, sorted by stars
    const QString url = QStringLiteral(
        "https://api.github.com/search/repositories?"
        "q=topic:%1+fork:false&sort=stars&order=desc&per_page=%2")
        .arg(QString::fromLatin1(kGitHubTopic))
        .arg(kCommunityMaxDisplay);

    QNetworkRequest req{QUrl(url)};
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("User-Agent", "Makine-Launcher");

    auto* reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_loadingCommunity = false;
        emit loadingCommunityChanged();

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcPlugin) << "GitHub topic search failed:" << reply->errorString();
            return;
        }

        const auto doc = QJsonDocument::fromJson(reply->readAll());
        const auto items = doc.object()["items"].toArray();

        m_communityPlugins.clear();
        for (const auto& item : items) {
            auto repo = item.toObject();
            const QString fullName = repo["full_name"].toString();

            // Skip official MakineCeviri repos — they show in the official section
            if (fullName.startsWith(QString::fromLatin1(kTrustedGitHubOrg) + "/"))
                continue;

            m_communityPlugins.append(QVariantMap{
                {"name",          repo["name"].toString()},
                {"fullName",      fullName},
                {"description",   repo["description"].toString()},
                {"stars",         repo["stargazers_count"].toInt()},
                {"url",           repo["html_url"].toString()},
                {"owner",         repo["owner"].toObject()["login"].toString()},
                {"ownerAvatar",   repo["owner"].toObject()["avatar_url"].toString()},
                {"language",      repo["language"].toString()},
                {"updatedAt",     repo["updated_at"].toString()},
            });
        }

        qCInfo(lcPlugin) << "Found" << m_communityPlugins.size() << "community plugin(s) on GitHub";
        emit communityPluginsChanged();
    });
}

void PluginManager::openCommunityPage()
{
    const QString url = QStringLiteral("https://github.com/topics/%1")
                            .arg(QString::fromLatin1(kGitHubTopic));
    QDesktopServices::openUrl(QUrl(url));
}
