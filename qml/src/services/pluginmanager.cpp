// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#include "pluginmanager.h"
#include "apppaths.h"
#include "cdnconfig.h"
#include "gameservice.h"
#include "networksecurity.h"
#include "pluginprocess.h"
#include "pluginregistry.h"
#include "processscanner.h"

#include <makine/plugin/abi.h>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSaveFile>
#include <QScreen>
#include <QSettings>
#include <QTimer>

#include <functional>
#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dpapi.h>
#endif

using namespace makine;

namespace {

constexpr int kMaxLogLines = 500;
constexpr qint64 kMaxLogFileBytes = 1024 * 1024;
constexpr qint64 kCrashWindowMs = 5 * 60 * 1000;
constexpr int kMaxCrashes = 3;
constexpr int kRestartDelaysMs[] = {1000, 5000, 15000};
constexpr qint64 kMaxPackageBytes = 200LL * 1024 * 1024;

const QString kEnabledKey = QStringLiteral("plugins/enabled");
const QString kDeveloperModeKey = QStringLiteral("plugins/developerMode");

QString key(const char* group, const QString& id)
{
    return QStringLiteral("plugins/%1/%2").arg(QLatin1String(group), id);
}

QString capabilityLabel(const QString& id)
{
    if (id == QLatin1String("network"))          return PluginManager::tr("İnternete bağlanır");
    if (id == QLatin1String("screen-capture"))   return PluginManager::tr("Ekran görüntüsü alır");
    if (id == QLatin1String("overlay"))          return PluginManager::tr("Oyunun üzerine pencere çizer");
    if (id == QLatin1String("game-files-read"))  return PluginManager::tr("Oyun klasörlerini okur");
    if (id == QLatin1String("game-files-write")) return PluginManager::tr("Oyun dosyalarını değiştirir");
    if (id == QLatin1String("process-inject"))   return PluginManager::tr("Çalışan oyuna bağlanır");
    return id;
}

QVariantList capabilityList(const QList<PluginCapability>& caps, const QStringList& skip = {})
{
    QVariantList list;
    for (const auto& c : caps)
        if (!skip.contains(c.id))
            list.append(QVariantMap{{"id", c.id}, {"label", capabilityLabel(c.id)}, {"reason", c.reason}});
    return list;
}

// Plugins speak physical pixels of the virtual desktop (the host is per-monitor
// DPI aware, so DXGI and BitBlt need no scaling); QML speaks Qt's logical pixels.
// Qt keeps each screen's top-left at its native position and scales only inside
// the screen, so the conversion is per screen around that origin.
QRect toPhysical(const QRect& logical)
{
    const QScreen* screen = QGuiApplication::screenAt(logical.center());
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (!screen) return logical;
    const qreal dpr = screen->devicePixelRatio();
    const QPoint origin = screen->geometry().topLeft();
    const QPointF topLeft = QPointF(origin) + QPointF(logical.topLeft() - origin) * dpr;
    return QRect(qRound(topLeft.x()), qRound(topLeft.y()),
                 qRound(logical.width() * dpr), qRound(logical.height() * dpr));
}

QRect toLogical(const QRect& physical)
{
    for (const QScreen* screen : QGuiApplication::screens()) {
        const qreal dpr = screen->devicePixelRatio();
        const QPoint origin = screen->geometry().topLeft();
        if (!QRect(origin, screen->geometry().size() * dpr).contains(physical.center())) continue;
        const QPointF topLeft = QPointF(origin) + QPointF(physical.topLeft() - origin) / dpr;
        return QRect(qRound(topLeft.x()), qRound(topLeft.y()),
                     qRound(physical.width() / dpr), qRound(physical.height() / dpr));
    }
    return physical;
}

QString appVersion()
{
    return QCoreApplication::applicationVersion();
}

// Every Makine release carries "-beta"; a plugin asking for 0.2.0 means the 0.2.0
// line, so the launcher's own pre-release tag is not held against it.
bool launcherAtLeast(const QString& minLauncher)
{
    return pluginmanifest::compareVersions(appVersion().section(u'-', 0, 0), minLauncher) >= 0;
}

QString describeStartFailure(int code, const QString& message, const QJsonValue& data)
{
    if (code == -32000) {
        switch (data.toObject().value(u"winError").toInt()) {
        case 126: return PluginManager::tr("Eklentinin ihtiyaç duyduğu bir DLL eksik; paket hatalı.");
        case 193: return PluginManager::tr("Eklenti 64 bit Windows için derlenmemiş.");
        case 2:
        case 3:   return PluginManager::tr("Eklenti dosyası bulunamadı.");
        case 0:   return PluginManager::tr("Eklenti Makine eklentisi değil: %1").arg(message);
        default:  return PluginManager::tr("Eklenti yüklenemedi (Windows hata kodu %1).")
                            .arg(data.toObject().value(u"winError").toInt());
        }
    }
    if (code == MAKINE_E_UNSUPPORTED) return PluginManager::tr("Bu eklenti Makine'nin bu sürümüyle çalışmıyor.");
    if (code == MAKINE_E_TIMEOUT) return PluginManager::tr("Eklenti zamanında başlamadı.");
    return PluginManager::tr("Eklenti başlatılamadı: %1").arg(message);
}

#ifdef Q_OS_WIN
// DPAPI, bound to the current Windows user; the plugin id is the entropy so one
// plugin's blob cannot be replayed as another's.
QByteArray dpapi(const QByteArray& data, const QByteArray& entropy, bool protect)
{
    DATA_BLOB in{static_cast<DWORD>(data.size()), reinterpret_cast<BYTE*>(const_cast<char*>(data.data()))};
    DATA_BLOB ent{static_cast<DWORD>(entropy.size()), reinterpret_cast<BYTE*>(const_cast<char*>(entropy.data()))};
    DATA_BLOB out{};
    const BOOL ok = protect
        ? CryptProtectData(&in, L"Makine plugin setting", &ent, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)
        : CryptUnprotectData(&in, nullptr, &ent, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out);
    if (!ok) return {};
    QByteArray result(reinterpret_cast<const char*>(out.pbData), static_cast<qsizetype>(out.cbData));
    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
    return result;
}
#else
QByteArray dpapi(const QByteArray&, const QByteArray&, bool) { return {}; }
#endif

} // namespace

PluginManager::PluginManager(QObject* parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
    security::installTlsPinning(m_net);
    m_registry = new PluginRegistry(m_net, this);
    connect(m_registry, &PluginRegistry::loadingChanged, this, &PluginManager::catalogChanged);
    connect(m_registry, &PluginRegistry::failed, this, &PluginManager::catalogChanged);
    connect(m_registry, &PluginRegistry::updated, this, [this] {
        emit catalogChanged();
        emit pluginsChanged();   // update flags
    });
}

PluginManager::~PluginManager() = default;

void PluginManager::setGameService(GameService* service)
{
    m_gameService = service;
    if (!service) return;
    connect(service, &GameService::translationInstallCompleted, this,
            [this](const QString& gameId, bool success, const QString&) {
        if (success) broadcast(QStringLiteral("translation.installed"), {{"gameId", gameId}});
    });
    connect(service, &GameService::translationUninstalled, this,
            [this](const QString& gameId, bool success, const QString&) {
        if (success) broadcast(QStringLiteral("translation.uninstalled"), {{"gameId", gameId}});
    });
}

void PluginManager::setProcessScanner(ProcessScanner* scanner)
{
    m_processScanner = scanner;
    if (!scanner) return;
    const auto onChange = [this] {
        const QString now = m_processScanner->gameRunning() ? m_processScanner->runningGameId() : QString();
        if (now == m_runningGameId) return;
        if (!m_runningGameId.isEmpty())
            broadcast(QStringLiteral("game.stopped"), {{"gameId", m_runningGameId}});
        m_runningGameId = now;
        if (!now.isEmpty())
            broadcast(QStringLiteral("game.started"),
                      {{"gameId", now}, {"name", m_processScanner->runningGameName()}});
    };
    connect(scanner, &ProcessScanner::runningGameChanged, this, onChange);
    connect(scanner, &ProcessScanner::gameRunningChanged, this, onChange);
}

// ── Lifecycle ───────────────────────────────────────────────────────────────

void PluginManager::start()
{
    const QString root = AppPaths::pluginsDir();
    QDir().mkpath(root);
    plugininstaller::cleanUp(root);
    // An update cut short by exit: put the old version back if the new one never
    // landed; otherwise the new one still has to prove that it starts.
    const QString previousRoot = root + QStringLiteral("/.previous");
    for (const QString& id : QDir(previousRoot).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        if (!QFileInfo::exists(root + u'/' + id)) plugininstaller::rollback(root, id);
    for (const auto& folder : plugininstaller::scan(root))
        loadFolder(folder);
    for (auto& [id, p] : m_plugins) {
        if (QFileInfo::exists(previousRoot + u'/' + id)) {
            if (p.enabled) p.updatePending = true;
            else plugininstaller::dropPrevious(root, id);
        }
        if (p.enabled) startPlugin(p);
    }
    emit pluginsChanged();
    refreshCatalog();
}

void PluginManager::shutdownAll()
{
    m_shuttingDown = true;
    for (auto& [id, p] : m_plugins)
        if (p.process) p.process->stop(1500);
    for (auto& [id, p] : m_plugins)
        if (p.process) p.process->waitForExit(2000);
}

void PluginManager::loadFolder(const plugininstaller::InstalledFolder& folder)
{
    const QString id = folder.manifest ? folder.manifest->id : QFileInfo(folder.dir).fileName();
    Installed& p = m_plugins[id];
    const QSettings s;
    p.id = id;
    p.dir = folder.dir;
    p.manifest = folder.manifest;
    p.enabled = s.value(kEnabledKey).toStringList().contains(id);
    p.verified = s.value(key("verified", id)).toBool();
    p.granted = s.value(key("granted", id)).toStringList();
    loadSettings(p);
    if (!p.manifest)
        setState(p, QStringLiteral("invalid"),
                 tr("Eklenti dosyaları okunamadı; kaldırıp yeniden kurun. (%1)").arg(folder.error));
    else if (!p.enabled) setState(p, QStringLiteral("disabled"));
}

void PluginManager::reloadOne(const QString& id)
{
    for (const auto& folder : plugininstaller::scan(AppPaths::pluginsDir()))
        if (QFileInfo(folder.dir).fileName() == id) loadFolder(folder);
}

void PluginManager::startPlugin(Installed& p)
{
    if (!p.enabled || p.process || !p.manifest || m_shuttingDown) return;
    const PluginManifest& m = *p.manifest;

    if (!launcherAtLeast(m.minLauncher))
        return setState(p, QStringLiteral("incompatible"),
                        tr("Bu eklenti Makine %1 veya daha yeni bir sürüm istiyor.").arg(m.minLauncher));
    for (const auto& c : m.capabilities)
        if (!p.granted.contains(c.id)) return setState(p, QStringLiteral("needs-consent"));
    const QString exe = hostExe();
    if (!QFileInfo::exists(exe))
        return setState(p, QStringLiteral("error"),
                        tr("makine-plugin-host.exe bulunamadı; Makine'yi yeniden kurun."));

    const QString dataDir = AppPaths::pluginDataDir() + u'/' + p.id;
    QDir().mkpath(dataDir);

    auto* proc = new PluginProcess(exe, p.dir + u'/' + m.entry, this);
    p.process = proc;
    const QString id = p.id;
    connect(proc, &PluginProcess::ready, this, [this, id, proc] { onReady(id, proc); });
    connect(proc, &PluginProcess::startFailed, this,
            [this, id, proc](int code, const QString& message, const QJsonValue& data) {
        onStartFailed(id, proc, code, message, data);
    });
    connect(proc, &PluginProcess::finished, this,
            [this, id, proc](bool requested, int) { onFinished(id, proc, requested); });
    connect(proc, &PluginProcess::callRequested, this,
            [this, id](const QJsonValue& callId, const QString& method, const QJsonValue& params) {
        onCall(id, callId, method, params);
    });
    connect(proc, &PluginProcess::eventEmitted, this,
            [this, id, proc](const QString& event, const QJsonValue& payload) {
        Installed* q = find(id);
        if (q && q->process == proc) handleRequest(*q, QJsonValue::Undefined, event, payload.toObject());
    });
    connect(proc, &PluginProcess::logLine, this, [this, id](int level, const QString& line) {
        if (Installed* q = find(id)) appendLog(*q, level, line);
    });

    const QJsonObject init{
        {"pluginId", p.id},
        {"version", m.version},
        {"launcherVersion", appVersion()},
        {"abi", static_cast<int>(MAKINE_PLUGIN_ABI_VERSION)},
        {"locale", "tr"},
        {"dataDir", QDir::toNativeSeparators(dataDir)},
        {"pluginDir", QDir::toNativeSeparators(p.dir)},
        {"settings", settingValues(p)},
    };
    setState(p, QStringLiteral("starting"));
    appendLog(p, MAKINE_LOG_INFO, tr("Başlatılıyor: %1 %2").arg(m.name, m.version));
    proc->start(init);
}

void PluginManager::stopThen(Installed& p, std::function<void()> next)
{
    if (!p.process) {
        if (next) next();
        return;
    }
    p.afterStop = std::move(next);
    setState(p, QStringLiteral("stopping"));
    p.process->stop();
}

void PluginManager::onReady(const QString& id, PluginProcess* proc)
{
    Installed* p = find(id);
    if (!p || p->process != proc) return;
    setState(*p, QStringLiteral("running"));
    appendLog(*p, MAKINE_LOG_INFO, tr("Çalışıyor."));
    if (p->updatePending) {
        p->updatePending = false;
        plugininstaller::dropPrevious(AppPaths::pluginsDir(), id);
    }
}

void PluginManager::onStartFailed(const QString& id, PluginProcess* proc, int code,
                                  const QString& message, const QJsonValue& data)
{
    Installed* p = find(id);
    if (!p || p->process != proc) return;
    p->process = nullptr;
    proc->deleteLater();
    hideOverlayOf(id);

    const QString reason = describeStartFailure(code, message, data);
    appendLog(*p, MAKINE_LOG_ERROR, QStringLiteral("%1 [%2: %3]").arg(reason).arg(code).arg(message));

    if (p->updatePending) {
        p->updatePending = false;
        if (plugininstaller::rollback(AppPaths::pluginsDir(), id)) {
            reloadOne(id);
            emit pluginsChanged();
            const QString note = tr("Yeni sürüm başlatılamadı (%1); önceki sürüme dönüldü.").arg(reason);
            if (Installed* q = find(id)) {
                appendLog(*q, MAKINE_LOG_WARN, note);
                startPlugin(*q);
            }
            emit pluginError(id, note);
            return;
        }
    }
    setState(*p, QStringLiteral("error"), reason);
    emit pluginError(id, reason);
}

void PluginManager::onFinished(const QString& id, PluginProcess* proc, bool requested)
{
    Installed* p = find(id);
    if (!p || p->process != proc) return;   // already handled by onStartFailed
    p->process = nullptr;
    proc->deleteLater();
    hideOverlayOf(id);
    for (auto it = m_requests.begin(); it != m_requests.end();) {
        if (it->pluginId != id) {
            ++it;
            continue;
        }
        const int requestId = it.key();
        it = m_requests.erase(it);
        emit requestCancelled(requestId);
    }

    if (requested || m_shuttingDown) {
        appendLog(*p, MAKINE_LOG_INFO, tr("Durduruldu."));
        setState(*p, p->enabled ? QStringLiteral("starting") : QStringLiteral("disabled"));
        if (auto next = std::exchange(p->afterStop, {})) {
            next();   // may erase *p
            return;
        }
        if (p->enabled) startPlugin(*p);
        return;
    }

    // Crash: restart with back-off, give up after kMaxCrashes in the window.
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    p->crashTimes.append(now);
    p->crashTimes.removeIf([now](qint64 t) { return now - t > kCrashWindowMs; });
    appendLog(*p, MAKINE_LOG_ERROR, tr("Eklenti beklenmedik şekilde kapandı."));
    if (p->crashTimes.size() >= kMaxCrashes) {
        const QString reason = tr("Eklenti 5 dakika içinde %1 kez çöktü ve durduruldu.").arg(p->crashTimes.size());
        setState(*p, QStringLiteral("crashed"), reason);
        emit pluginError(id, reason);
        return;
    }
    setState(*p, QStringLiteral("starting"), tr("Eklenti çöktü; yeniden başlatılıyor."));
    const int delay = kRestartDelaysMs[qMin(p->crashTimes.size(), qsizetype(std::size(kRestartDelaysMs))) - 1];
    QTimer::singleShot(delay, this, [this, id] {
        Installed* q = find(id);
        if (q && q->enabled && !q->process && q->state == QLatin1String("starting")) startPlugin(*q);
    });
}

// ── Queries ─────────────────────────────────────────────────────────────────

PluginManager::Installed* PluginManager::find(const QString& id)
{
    const auto it = m_plugins.find(id);
    return it == m_plugins.end() ? nullptr : &it->second;
}

const PluginManager::Installed* PluginManager::find(const QString& id) const
{
    const auto it = m_plugins.find(id);
    return it == m_plugins.end() ? nullptr : &it->second;
}

QVariantMap PluginManager::toVariant(const Installed& p) const
{
    QVariantMap map{
        {"id", p.id},
        {"name", p.id},
        {"enabled", p.enabled},
        {"verified", p.verified},
        {"state", p.state},
        {"statusText", p.statusText},
        {"lastError", p.lastError},
        {"capabilities", QVariantList()},
        {"missingCapabilities", QVariantList()},
        {"actions", QVariantList()},
        {"hasSettings", false},
        {"updateAvailable", false},
        {"availableVersion", QString()},
    };
    if (!p.manifest) return map;

    const PluginManifest& m = *p.manifest;
    map["name"] = m.name;
    map["version"] = m.version;
    map["description"] = m.description;
    map["publisher"] = m.publisher;
    map["homepage"] = m.homepage;
    map["license"] = m.license;
    map["capabilities"] = capabilityList(m.capabilities);
    map["missingCapabilities"] = capabilityList(m.capabilities, p.granted);
    QVariantList actions;
    for (const auto& a : m.actions)
        actions.append(QVariantMap{{"id", a.id}, {"label", a.label}, {"description", a.description}});
    map["actions"] = actions;
    map["hasSettings"] = !m.settings.isEmpty();
    if (const auto* e = m_registry->find(p.id);
        e && pluginmanifest::compareVersions(e->version, m.version) > 0) {
        map["updateAvailable"] = true;
        map["availableVersion"] = e->version;
    }
    return map;
}

QVariantList PluginManager::plugins() const
{
    QVariantList list;
    for (const auto& [id, p] : m_plugins) list.append(toVariant(p));
    return list;
}

int PluginManager::updateCount() const
{
    int n = 0;
    for (const auto& [id, p] : m_plugins)
        if (const auto* e = m_registry->find(id);
            e && p.manifest && pluginmanifest::compareVersions(e->version, p.manifest->version) > 0)
            ++n;
    return n;
}

QVariantMap PluginManager::plugin(const QString& id) const
{
    const Installed* p = find(id);
    return p ? toVariant(*p) : QVariantMap{};
}

QVariantList PluginManager::catalog() const
{
    QVariantList list;
    for (const auto& e : m_registry->entries()) {
        const Installed* p = find(e.id);
        const bool installed = p && p->manifest;
        QString incompatible;
        if (!pluginmanifest::isSupportedAbi(e.abi)
            || !launcherAtLeast(e.minLauncher))
            incompatible = tr("Makine'yi güncelleyin (en az %1 gerekiyor).").arg(e.minLauncher);
        list.append(QVariantMap{
            {"id", e.id},
            {"name", e.name},
            {"version", e.version},
            {"description", e.description},
            {"publisher", e.publisher},
            {"homepage", e.homepage},
            {"size", e.size},
            {"capabilities", capabilityList(e.capabilities)},
            {"installed", installed},
            {"installedVersion", installed ? p->manifest->version : QString()},
            {"updateAvailable", installed && pluginmanifest::compareVersions(e.version, p->manifest->version) > 0},
            {"compatible", incompatible.isEmpty()},
            {"incompatibleReason", incompatible},
        });
    }
    return list;
}

bool PluginManager::catalogLoading() const { return m_registry->isLoading(); }
QString PluginManager::catalogError() const { return m_registry->lastError(); }

bool PluginManager::developerMode() const
{
    return QSettings().value(kDeveloperModeKey, false).toBool();
}

void PluginManager::setDeveloperMode(bool on)
{
    if (developerMode() == on) return;
    QSettings().setValue(kDeveloperModeKey, on);
    emit developerModeChanged();
}

QStringList PluginManager::logLines(const QString& id) const
{
    const Installed* p = find(id);
    return p ? p->log : QStringList{};
}

QVariantList PluginManager::settingsCategories() const
{
    QVariantList list;
    for (const auto& [id, p] : m_plugins) {
        if (!p.enabled || !p.manifest || (p.manifest->settings.isEmpty() && p.manifest->actions.isEmpty()))
            continue;
        list.append(QVariantMap{
            {"name", p.manifest->name},
            {"description", p.manifest->description.isEmpty() ? tr("Eklenti ayarları") : p.manifest->description},
            {"isPlugin", true},
            {"pluginId", id},
        });
    }
    return list;
}

QString PluginManager::hostExe() const
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/makine-plugin-host.exe");
}

// ── User actions ────────────────────────────────────────────────────────────

void PluginManager::refreshCatalog()
{
    m_registry->refresh();
}

void PluginManager::setEnabled(const QString& id, bool enabled)
{
    Installed* p = find(id);
    if (!p || !p->manifest || p->enabled == enabled) return;
    p->enabled = enabled;
    saveEnabled();
    if (enabled) {
        p->crashTimes.clear();
        startPlugin(*p);
    } else if (p->process) {
        stopThen(*p, nullptr);
    } else {
        setState(*p, QStringLiteral("disabled"));
    }
    emit pluginsChanged();   // the sidebar lists enabled plugins only
}

void PluginManager::restart(const QString& id)
{
    Installed* p = find(id);
    if (!p || !p->enabled) return;
    p->crashTimes.clear();
    if (!p->process) return startPlugin(*p);
    stopThen(*p, [this, id] {
        if (Installed* q = find(id)) startPlugin(*q);
    });
}

void PluginManager::grantCapabilities(const QString& id)
{
    Installed* p = find(id);
    if (!p || !p->manifest) return;
    for (const auto& c : p->manifest->capabilities)
        if (!p->granted.contains(c.id)) p->granted.append(c.id);
    QSettings().setValue(key("granted", id), p->granted);
    if (p->enabled) startPlugin(*p);
    emit pluginsChanged();
}

void PluginManager::install(const QString& id)
{
    if (!m_busyId.isEmpty()) return;
    const PluginRegistryEntry* found = m_registry->find(id);
    if (!found) {
        emit pluginError(id, tr("Eklenti katalogda bulunamadı."));
        return;
    }
    const PluginRegistryEntry entry = *found;   // the registry may refresh meanwhile
    if (!pluginmanifest::isSupportedAbi(entry.abi)
        || !launcherAtLeast(entry.minLauncher)) {
        emit pluginError(id, tr("Bu eklenti için Makine'yi güncelleyin."));
        return;
    }

    setBusy(id, 0.0);
    QNetworkRequest req{QUrl(entry.url)};
    req.setRawHeader("User-Agent", cdn::kUserAgent);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto* reply = m_net->get(req);
    connect(reply, &QNetworkReply::downloadProgress, this, [this, reply, entry](qint64 received, qint64) {
        if (received > entry.size) reply->abort();
        else setBusy(entry.id, 0.9 * double(received) / double(entry.size));
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, entry] {
        reply->deleteLater();
        const auto fail = [this, &entry](const QString& message) {
            setBusy({}, 0.0);
            emit pluginError(entry.id, message);
        };
        if (reply->error() != QNetworkReply::NoError)
            return fail(tr("İndirilemedi: %1").arg(reply->errorString()));
        const QByteArray data = reply->readAll();
        if (data.size() != entry.size
            || QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex() != entry.sha256.toLatin1())
            return fail(tr("İndirilen paket doğrulanamadı; kurulum yapılmadı."));
        QString error;
        auto staged = plugininstaller::stage(data, AppPaths::pluginsDir() + QStringLiteral("/.staging"), &error);
        if (!staged) return fail(tr("Paket açılamadı: %1").arg(error));
        if (staged->manifest.id != entry.id || staged->manifest.version != entry.version) {
            QDir(staged->dir).removeRecursively();
            return fail(tr("Paket katalogdaki eklentiyle eşleşmiyor; kurulum yapılmadı."));
        }
        QStringList grant;
        for (const auto& c : entry.capabilities) grant.append(c.id);
        replaceInstalled(*staged, true, grant);
    });
}

void PluginManager::installFromFile(const QUrl& file)
{
    if (!developerMode()) {
        emit pluginError({}, tr("Dosyadan eklenti kurmak için önce Geliştirici modunu açın."));
        return;
    }
    if (!m_busyId.isEmpty()) return;
    const QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly) || f.size() > kMaxPackageBytes) {
        emit pluginError({}, tr("Eklenti dosyası açılamadı: %1").arg(QFileInfo(path).fileName()));
        return;
    }
    QString error;
    auto staged = plugininstaller::stage(f.readAll(), AppPaths::pluginsDir() + QStringLiteral("/.staging"), &error);
    if (!staged) {
        emit pluginError({}, tr("Eklenti paketi geçersiz: %1").arg(error));
        return;
    }
    // Never replace a working version with one this launcher cannot run.
    if (!launcherAtLeast(staged->manifest.minLauncher)) {
        QDir(staged->dir).removeRecursively();
        emit pluginError(staged->manifest.id, tr("Bu eklenti Makine %1 veya daha yeni bir sürüm istiyor.")
                                                  .arg(staged->manifest.minLauncher));
        return;
    }
    // Unverified: nothing is granted, the user sees the capabilities and decides.
    replaceInstalled(*staged, false, {});
}

void PluginManager::replaceInstalled(const plugininstaller::StagedPlugin& staged, bool verified,
                                     const QStringList& grant)
{
    const QString id = staged.manifest.id;
    Installed* existing = find(id);
    const bool hadWorkingVersion = existing && existing->manifest;
    const bool enable = existing ? existing->enabled : true;
    setBusy(id, 0.95);

    auto commit = [this, staged, verified, grant, id, hadWorkingVersion, enable] {
        QString error;
        if (!plugininstaller::commit(staged, AppPaths::pluginsDir(), &error)) {
            QDir(staged.dir).removeRecursively();
            setBusy({}, 0.0);
            emit pluginError(id, tr("Kurulamadı: %1").arg(error));
            if (Installed* p = find(id); p && p->enabled) startPlugin(*p);
            return;
        }
        QSettings s;
        s.setValue(key("verified", id), verified);
        QStringList granted = s.value(key("granted", id)).toStringList();
        for (const auto& g : grant)
            if (!granted.contains(g)) granted.append(g);
        s.setValue(key("granted", id), granted);

        reloadOne(id);
        Installed* p = find(id);
        if (!p) return setBusy({}, 0.0);
        p->enabled = enable;
        p->crashTimes.clear();
        saveEnabled();
        // Keep the replaced version only while the new one has yet to prove it starts.
        p->updatePending = hadWorkingVersion && enable;
        if (hadWorkingVersion && !enable) plugininstaller::dropPrevious(AppPaths::pluginsDir(), id);
        setBusy({}, 0.0);
        appendLog(*p, MAKINE_LOG_INFO, tr("Kuruldu: %1").arg(p->manifest->version));
        emit pluginInstalled(id);
        if (p->enabled) startPlugin(*p);
        else setState(*p, QStringLiteral("disabled"));
        emit pluginsChanged();
        emit catalogChanged();
    };

    if (existing) stopThen(*existing, commit);
    else commit();
}

void PluginManager::uninstall(const QString& id, bool removeData)
{
    Installed* p = find(id);
    if (!p) return;
    stopThen(*p, [this, id, removeData] {
        QString error;
        if (!plugininstaller::remove(AppPaths::pluginsDir(), id, &error)) {
            emit pluginError(id, tr("Kaldırılamadı: %1").arg(error));
            if (Installed* q = find(id); q && q->enabled) startPlugin(*q);
            return;
        }
        QSettings s;
        s.remove(key("verified", id));
        s.remove(key("granted", id));
        s.remove(key("attach", id));
        if (removeData) {
            QDir(AppPaths::pluginDataDir() + u'/' + id).removeRecursively();
            // The log holds whatever the plugin wrote, so it goes with the data.
            const QString log = AppPaths::logsDir() + QStringLiteral("/plugins/") + id + QStringLiteral(".log");
            QFile::remove(log);
            QFile::remove(log + QStringLiteral(".1"));
        }
        m_plugins.erase(id);
        saveEnabled();
        emit pluginsChanged();
        emit catalogChanged();
    });
}

void PluginManager::runAction(const QString& id, const QString& actionId)
{
    Installed* p = find(id);
    if (!p || !p->manifest) return;
    const PluginAction* action = nullptr;
    for (const auto& a : p->manifest->actions)
        if (a.id == actionId) action = &a;
    if (!action) return;
    if (!p->process || !p->process->isReady()) {
        emit actionFinished(id, actionId, false, tr("Eklenti çalışmıyor."));
        return;
    }
    // Long timeout: an action may wait for the user (a region pick, a consent).
    p->process->invoke(action->method, QJsonObject{},
                       [this, id, actionId](const QJsonValue& result, const QJsonObject& error) {
        if (!error.isEmpty()) {
            emit actionFinished(id, actionId, false, error.value(u"message").toString());
            return;
        }
        const QString message = result.isString() ? result.toString()
                                                  : result.toObject().value(u"message").toString();
        emit actionFinished(id, actionId, true, message);
    }, 120000);
}

// ── Settings ────────────────────────────────────────────────────────────────

void PluginManager::loadSettings(Installed& p)
{
    QFile f(AppPaths::pluginDataDir() + u'/' + p.id + QStringLiteral("/settings.json"));
    const QJsonObject o = f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll()).object() : QJsonObject{};
    p.values = o.value(u"values").toObject();
    p.secrets = o.value(u"secrets").toObject();
}

void PluginManager::saveSettings(const Installed& p) const
{
    const QString dir = AppPaths::pluginDataDir() + u'/' + p.id;
    QDir().mkpath(dir);
    QSaveFile f(dir + QStringLiteral("/settings.json"));
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(QJsonDocument(QJsonObject{{"values", p.values}, {"secrets", p.secrets}}).toJson());
    f.commit();
}

QJsonObject PluginManager::settingValues(const Installed& p) const
{
    QJsonObject out;
    if (!p.manifest) return out;
    for (const QJsonObject& def : p.manifest->settings) {
        const QString k = def.value(u"key").toString();
        if (def.value(u"type").toString() == QLatin1String("secret")) {
            const QByteArray blob = QByteArray::fromBase64(p.secrets.value(k).toString().toLatin1());
            out.insert(k, blob.isEmpty() ? QString() : QString::fromUtf8(dpapi(blob, p.id.toUtf8(), false)));
        } else if (p.values.contains(k) && pluginmanifest::isValidSettingValue(def, p.values.value(k))) {
            out.insert(k, p.values.value(k));
        } else if (def.contains(u"default")) {
            out.insert(k, def.value(u"default"));
        }
    }
    return out;
}

QVariantList PluginManager::settings(const QString& id) const
{
    const Installed* p = find(id);
    if (!p || !p->manifest) return {};
    const QJsonObject values = settingValues(*p);
    QVariantList list;
    for (const QJsonObject& def : p->manifest->settings) {
        QVariantMap item = def.toVariantMap();
        const QString k = def.value(u"key").toString();
        if (def.value(u"type").toString() == QLatin1String("secret"))
            item["hasValue"] = p->secrets.contains(k);
        else
            item["value"] = values.value(k).toVariant();
        list.append(item);
    }
    return list;
}

bool PluginManager::storeSetting(Installed& p, const QString& k, const QJsonValue& v)
{
    const QJsonObject def = p.manifest ? p.manifest->setting(k) : QJsonObject{};
    if (def.isEmpty()) return false;
    if (def.value(u"type").toString() == QLatin1String("secret")) {
        const QString text = v.toString();
        if (text.isEmpty()) {
            p.secrets.remove(k);
        } else {
            const QByteArray blob = dpapi(text.toUtf8(), p.id.toUtf8(), true);
            if (blob.isEmpty()) return false;
            p.secrets.insert(k, QString::fromLatin1(blob.toBase64()));
        }
    } else {
        QJsonValue value = v;
        // Path settings arrive from QML file dialogs as file:// URLs.
        if (def.value(u"type").toString() == QLatin1String("path") && v.toString().startsWith(QLatin1String("file:")))
            value = QDir::toNativeSeparators(QUrl(v.toString()).toLocalFile());
        if (!pluginmanifest::isValidSettingValue(def, value)) return false;
        p.values.insert(k, value);
    }
    saveSettings(p);
    emit settingsChanged(p.id);
    return true;
}

bool PluginManager::setSetting(const QString& id, const QString& k, const QVariant& value)
{
    Installed* p = find(id);
    if (!p || !storeSetting(*p, k, QJsonValue::fromVariant(value))) return false;
    if (p->process && p->manifest->events.contains(QStringLiteral("settings.changed")))
        p->process->sendEvent(QStringLiteral("settings.changed"),
                              QJsonObject{{"key", k}, {"value", settingValues(*p).value(k)}});
    return true;
}

// ── Plugin → launcher ───────────────────────────────────────────────────────

void PluginManager::onCall(const QString& id, const QJsonValue& callId, const QString& method,
                           const QJsonValue& params)
{
    Installed* p = find(id);
    if (!p || !p->process || !p->manifest) return;
    handleRequest(*p, callId, method, params.toObject());
}

QJsonObject PluginManager::attachResult(const QString& gameId) const
{
    const QVariantMap game = m_gameService ? m_gameService->getGameById(gameId) : QVariantMap{};
    return {{"allowed", true},
            {"gameId", gameId},
            {"name", game.value("name").toString()},
            {"installPath", QDir::toNativeSeparators(game.value("installPath").toString())}};
}

void PluginManager::handleRequest(Installed& p, const QJsonValue& callId, const QString& method,
                                  const QJsonObject& params)
{
    const bool isCall = !callId.isUndefined();
    PluginProcess* proc = p.process;
    const auto reply = [&](const QJsonValue& result) {
        if (isCall && proc) proc->respond(callId, result);
    };
    const auto fail = [&](int code, const QString& message) {
        if (isCall && proc) proc->respondError(callId, code, message);
        else appendLog(p, MAKINE_LOG_WARN, QStringLiteral("%1: %2").arg(method, message));
    };
    const PluginManifest& m = *p.manifest;
    const auto needs = [&](const char* capability) {
        if (m.hasCapability(QLatin1String(capability))) return true;
        fail(MAKINE_E_DENIED, QStringLiteral("capability %1 is not declared in manifest.json").arg(capability));
        return false;
    };

    if (method == QLatin1String("settings.get")) {
        const QJsonObject all = settingValues(p);
        const QString k = params.value(u"key").toString();
        if (!all.contains(k)) return fail(MAKINE_E_NOT_FOUND, QStringLiteral("unknown setting %1").arg(k));
        return reply(all.value(k));
    }
    if (method == QLatin1String("settings.all")) return reply(settingValues(p));
    if (method == QLatin1String("settings.set")) {
        const QString k = params.value(u"key").toString();
        if (m.setting(k).isEmpty()) return fail(MAKINE_E_NOT_FOUND, QStringLiteral("unknown setting %1").arg(k));
        if (!storeSetting(p, k, params.value(u"value")))
            return fail(MAKINE_E_INVALID_ARGUMENT, QStringLiteral("value does not fit setting %1").arg(k));
        return reply(true);
    }
    if (method == QLatin1String("ui.notify")) {
        QString kind = params.value(u"kind").toString(QStringLiteral("info"));
        if (!QStringList{"info", "success", "warning", "error"}.contains(kind)) kind = QStringLiteral("info");
        emit notify(p.id, m.name, params.value(u"title").toString().left(100),
                    params.value(u"message").toString().left(500), kind);
        return reply(true);
    }
    if (method == QLatin1String("ui.setStatus")) {
        const QString text = params.value(u"text").toString().left(200);
        if (text != p.statusText) {
            p.statusText = text;
            emit pluginChanged(p.id);
        }
        return reply(true);
    }
    if (method == QLatin1String("games.running")) {
        if (!m_processScanner || !m_processScanner->gameRunning()) return reply(QJsonValue::Null);
        return reply(QJsonObject{{"gameId", m_processScanner->runningGameId()},
                                 {"name", m_processScanner->runningGameName()}});
    }
    if (method == QLatin1String("games.list")) {
        if (!needs("game-files-read")) return;
        QJsonArray games;
        if (m_gameService)
            for (const auto& g : m_gameService->games()) {
                const QVariantMap game = g.toMap();
                games.append(QJsonObject{{"id", game.value("id").toString()},
                                         {"name", game.value("name").toString()},
                                         {"engine", game.value("engine").toString()},
                                         {"installPath", QDir::toNativeSeparators(game.value("installPath").toString())}});
            }
        return reply(games);
    }
    if (method == QLatin1String("games.requestAttach")) {
        if (!needs("process-inject")) return;
        if (!isCall) return;
        const QString gameId = params.value(u"gameId").toString();
        const QVariantMap game = m_gameService ? m_gameService->getGameById(gameId) : QVariantMap{};
        if (game.isEmpty()) return fail(MAKINE_E_NOT_FOUND, QStringLiteral("unknown game %1").arg(gameId));
        // Anti-cheat: never attach, whatever the user says. A ban is not undoable.
        const bool protectedGame = m_gameService->checkAntiCheat(gameId).value("hasAntiCheat").toBool()
            || (m_processScanner && m_processScanner->runningGameId() == gameId && m_processScanner->hasAntiCheat());
        if (protectedGame)
            return fail(MAKINE_E_DENIED, tr("Bu oyunda hile koruması var; eklentiler bağlanamaz."));
        if (QSettings().value(key("attach", p.id)).toStringList().contains(gameId))
            return reply(attachResult(gameId));
        const int requestId = m_nextRequestId++;
        m_requests.insert(requestId, {QStringLiteral("attach"), p.id, callId, gameId});
        emit attachRequested(requestId, p.id, m.name, game.value("name").toString());
        return;
    }
    if (method == QLatin1String("overlay.pickRegion")) {
        if (!needs("overlay")) return;
        if (!isCall) return;
        for (const auto& r : std::as_const(m_requests))
            if (r.kind == QLatin1String("region"))
                return fail(MAKINE_E_BUSY, QStringLiteral("a region pick is already open"));
        const int requestId = m_nextRequestId++;
        m_requests.insert(requestId, {QStringLiteral("region"), p.id, callId, {}});
        emit regionRequested(requestId, p.id, m.name);
        return;
    }
    if (method == QLatin1String("overlay.show")) {
        if (!needs("overlay")) return;
        if (m_overlay.visible && m_overlay.owner != p.id)
            return fail(MAKINE_E_BUSY, QStringLiteral("another plugin owns the overlay"));
        m_overlay.owner = p.id;
        if (params.contains(u"width") && params.contains(u"height"))
            m_overlay.region = toLogical(QRect(params.value(u"x").toInt(), params.value(u"y").toInt(),
                                               params.value(u"width").toInt(), params.value(u"height").toInt()));
        m_overlay.text = params.value(u"text").toString().left(4000);
        m_overlay.sourceText = params.value(u"sourceText").toString().left(4000);
        m_overlay.visible = true;
        emit overlayChanged();
        return reply(true);
    }
    if (method == QLatin1String("overlay.hide")) {
        if (!needs("overlay")) return;
        hideOverlayOf(p.id);
        return reply(true);
    }
    fail(MAKINE_E_NOT_FOUND, QStringLiteral("unknown method %1").arg(method));
}

void PluginManager::answerAttach(int requestId, bool allow, bool remember)
{
    const UserRequest req = m_requests.take(requestId);
    Installed* p = find(req.pluginId);
    if (!p || !p->process || req.kind != QLatin1String("attach")) return;
    if (!allow) return p->process->respondError(req.callId, MAKINE_E_DENIED, QStringLiteral("the user declined"));
    if (remember) {
        QSettings s;
        QStringList games = s.value(key("attach", p->id)).toStringList();
        if (!games.contains(req.gameId)) games.append(req.gameId);
        s.setValue(key("attach", p->id), games);
    }
    p->process->respond(req.callId, attachResult(req.gameId));
}

void PluginManager::answerRegion(int requestId, int x, int y, int width, int height)
{
    const UserRequest req = m_requests.take(requestId);
    Installed* p = find(req.pluginId);
    if (!p || !p->process || req.kind != QLatin1String("region")) return;
    if (width <= 0 || height <= 0)
        return p->process->respondError(req.callId, MAKINE_E_DENIED, QStringLiteral("the user cancelled"));
    m_overlay.region = QRect(x, y, width, height);
    emit overlayChanged();
    const QRect physical = toPhysical(m_overlay.region);
    p->process->respond(req.callId, QJsonObject{{"x", physical.x()}, {"y", physical.y()},
                                                {"width", physical.width()}, {"height", physical.height()}});
}

void PluginManager::broadcast(const QString& event, const QJsonObject& payload)
{
    for (auto& [id, p] : m_plugins)
        if (p.process && p.process->isReady() && p.manifest && p.manifest->events.contains(event))
            p.process->sendEvent(event, payload);
}

void PluginManager::hideOverlayOf(const QString& id)
{
    if (m_overlay.owner != id || !m_overlay.visible) return;
    m_overlay.visible = false;
    m_overlay.owner.clear();
    emit overlayChanged();
}

// ── Helpers ─────────────────────────────────────────────────────────────────

void PluginManager::setBusy(const QString& id, double progress)
{
    m_busyId = id;
    m_busyProgress = progress;
    emit busyChanged();
}

void PluginManager::setState(Installed& p, const QString& state, const QString& error)
{
    p.state = state;
    p.lastError = error;
    if (state != QLatin1String("running")) p.statusText.clear();
    emit pluginChanged(p.id);
}

void PluginManager::saveEnabled() const
{
    QStringList enabled;
    for (const auto& [id, p] : m_plugins)
        if (p.enabled) enabled.append(id);
    QSettings().setValue(kEnabledKey, enabled);
}

void PluginManager::appendLog(Installed& p, int level, const QString& line)
{
    static const char* const kLevels[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    const char* levelName = kLevels[qBound(0, level, 3)];
    const QString text = line.left(2000);
    p.log.append(QStringLiteral("%1 %2 %3").arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                                                QLatin1String(levelName), text));
    if (p.log.size() > kMaxLogLines) p.log.remove(0, p.log.size() - kMaxLogLines);

    const QString dir = AppPaths::logsDir() + QStringLiteral("/plugins");
    const QString path = dir + u'/' + p.id + QStringLiteral(".log");
    if (QFileInfo(path).size() > kMaxLogFileBytes) {
        QFile::remove(path + QStringLiteral(".1"));
        QFile::rename(path, path + QStringLiteral(".1"));
    }
    QDir().mkpath(dir);
    QFile f(path);
    if (f.open(QIODevice::Append | QIODevice::Text))
        f.write(QStringLiteral("%1 %2 %3\n").arg(QDateTime::currentDateTime().toString(Qt::ISODate),
                                                 QLatin1String(levelName), text).toUtf8());
    qCDebug(lcPlugin).noquote() << p.id << levelName << text;
    emit logChanged(p.id);
}
