/**
 * @file updateservice.cpp
 * @brief Application update lifecycle management
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "updateservice.h"
#include "selfupdater.h"
#include "cdnconfig.h"
#include "networksecurity.h"
#include "profiler.h"
#include "apppaths.h"
#include "crashreporter.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QSettings>
#include <QDateTime>
#include <QLoggingCategory>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <memory>

Q_LOGGING_CATEGORY(lcUpdateService, "makine.update")

namespace makine {

static constexpr const char* kUpdateJsonUrl = cdn::kUpdateJson;

// Only check once per 24 hours to avoid unnecessary requests
static constexpr qint64 kCheckIntervalSecs = 24 * 60 * 60;

UpdateService::UpdateService(QObject *parent)
    : QObject(parent)
{
    security::installTlsPinning(&m_nam);
}

UpdateService::~UpdateService()
{
    cancel();
}

UpdateService* UpdateService::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(jsEngine)
    Q_UNUSED(qmlEngine)
    auto *instance = new UpdateService;
    QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
    return instance;
}

// ---------------------------------------------------------------------------
// Post-update cleanup (called from main.cpp on --post-update launch)
// ---------------------------------------------------------------------------

void UpdateService::handlePostUpdate()
{
    // Remove .old EXE left by self-swap
    QFile::remove(QCoreApplication::applicationFilePath() + QStringLiteral(".old"));

    // Remove temp download directory
    QDir(AppPaths::updateTempDir()).removeRecursively();

    // Clear update check cache so we don't immediately re-show
    // "update available" for the version we just installed
    QSettings settings(QStringLiteral("MakineCeviri"), QStringLiteral("Makine-Launcher"));
    settings.remove(QStringLiteral("update/lastCheckTime"));
    settings.remove(QStringLiteral("update/cachedHasUpdate"));
    settings.remove(QStringLiteral("update/cachedVersion"));
    settings.remove(QStringLiteral("update/cachedUrl"));
}

// ---------------------------------------------------------------------------
// Version check
// ---------------------------------------------------------------------------

void UpdateService::check()
{
    MAKINE_ZONE_NAMED("UpdateService::check");
    CrashReporter::addBreadcrumb("update", "UpdateService::check");
    if (m_state == Checking || m_state == Downloading)
        return;

    setState(Checking);
    setError({});

#ifdef MAKINE_DEV_TOOLS
    // Dev builds: prefer GitHub Releases API (private repo, token from gh CLI)
    m_githubToken = readGitHubToken();
    if (!m_githubToken.isEmpty()) {
        checkGitHub();
        return;
    }
    qCDebug(lcUpdateService) << "UpdateService: No GitHub token, falling back to CDN";

    QString urlStr = qEnvironmentVariable("MAKINE_UPDATE_URL");
    if (urlStr.isEmpty())
        urlStr = QString::fromLatin1(kUpdateJsonUrl);
    else
        qCDebug(lcUpdateService) << "UpdateService: Using override URL:" << urlStr;
#else
    QString urlStr = QString::fromLatin1(kUpdateJsonUrl);
#endif

    qCDebug(lcUpdateService) << "UpdateService: Checking for updates at" << urlStr;

    QNetworkRequest request{QUrl{urlStr}};
    request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) Makine-Launcher/0.1");
    request.setTransferTimeout(15000);

    auto *reply = m_nam.get(request);

    // Abort if response exceeds 1 MB (update.json is a few hundred bytes normally)
    connect(reply, &QNetworkReply::downloadProgress, this, [reply](qint64 received, qint64) {
        if (received > 1 * 1024 * 1024) {
            qCWarning(lcUpdateService) << "UpdateService: update.json response too large, aborting";
            reply->abort();
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onCheckFinished(reply);
    });
}

void UpdateService::onCheckFinished(QNetworkReply* reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qCDebug(lcUpdateService) << "UpdateService: Check failed:" << reply->errorString();
        setError(reply->errorString());
        setState(Idle);
        return;
    }

    const QByteArray data = reply->readAll();
    qCDebug(lcUpdateService) << "UpdateService: Received" << data.size() << "bytes";
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        setError(QStringLiteral("Güncelleme bilgisi okunamadı: %1").arg(parseError.errorString()));
        setState(Idle);
        return;
    }

    const QJsonObject obj = doc.object();
    const QString version = obj.value(QStringLiteral("version")).toString();
    const QString url = obj.value(QStringLiteral("url")).toString();
    const QString checksum = obj.value(QStringLiteral("checksum")).toString();
    const qint64 size = obj.value(QStringLiteral("size")).toInteger();
    const QString notes = obj.value(QStringLiteral("notes")).toString();
    const QString channel = obj.value(QStringLiteral("channel")).toString();

    if (version.isEmpty() || url.isEmpty()) {
        setError(QStringLiteral("Güncelleme verisi eksik veya hatalı"));
        setState(Idle);
        return;
    }

    // Channel filtering: dev updates only visible to dev builds
    if (channel == QStringLiteral("dev")) {
#ifndef MAKINE_DEV_TOOLS
        qCDebug(lcUpdateService) << "UpdateService: Ignoring dev channel update (production build)";
        setState(Idle);
        return;
#endif
    }

    // Store metadata
    m_downloadUrl = url;
    m_totalBytes = size;

    // Strip "sha256:" prefix if present
    m_expectedChecksum = checksum;
    if (m_expectedChecksum.startsWith(QStringLiteral("sha256:"), Qt::CaseInsensitive))
        m_expectedChecksum = m_expectedChecksum.mid(7);
    m_expectedChecksum = m_expectedChecksum.toLower();

    m_releaseNotes = notes;

    // Parse version: strip "v" prefix, separate pre-release suffix
    QString remoteRaw = version;
    if (remoteRaw.startsWith(QLatin1Char('v')) || remoteRaw.startsWith(QLatin1Char('V')))
        remoteRaw = remoteRaw.mid(1);
    QString currentRaw = QCoreApplication::applicationVersion();

    auto splitPreRelease = [](const QString& raw) -> std::pair<QString, QString> {
        int dashIdx = raw.indexOf(QLatin1Char('-'));
        if (dashIdx > 0)
            return {raw.left(dashIdx), raw.mid(dashIdx + 1)};
        return {raw, {}};
    };

    auto [remoteVer, remotePre] = splitPreRelease(remoteRaw);
    auto [currentVer, currentPre] = splitPreRelease(currentRaw);

    int cmp = compareVersions(remoteVer, currentVer);
    bool hasUpdate = (cmp > 0) || (cmp == 0 && currentPre.size() > 0 && remotePre.isEmpty());

    // Cache result with timestamp
    QSettings settings(QStringLiteral("MakineCeviri"), QStringLiteral("Makine-Launcher"));
    settings.setValue(QStringLiteral("update/lastCheckTime"), QDateTime::currentDateTime());
    settings.setValue(QStringLiteral("update/cachedHasUpdate"), hasUpdate);
    settings.setValue(QStringLiteral("update/cachedVersion"), version);
    settings.setValue(QStringLiteral("update/cachedUrl"), url);

    qCDebug(lcUpdateService) << "UpdateService: Remote" << remoteRaw << "vs Current" << currentRaw
             << "-> hasUpdate:" << hasUpdate;

    if (hasUpdate) {
        m_version = version;
        emit versionChanged();
        setState(Available);
    } else {
        setState(Idle);
    }
}

// ---------------------------------------------------------------------------
// Download
// ---------------------------------------------------------------------------

void UpdateService::download()
{
    MAKINE_ZONE_NAMED("UpdateService::download");
    CrashReporter::addBreadcrumb("update", "UpdateService::download");
    if (m_state != Available || m_downloadUrl.isEmpty())
        return;

    // GitHub asset download (private repo): use API with auth + redirect
#ifdef MAKINE_DEV_TOOLS
    if (m_githubAssetId > 0 && !m_githubToken.isEmpty()) {
        downloadGitHubAsset();
        return;
    }
#endif

    // Validate download URL domain against allowlist
    static const QStringList allowedHosts = {
        QString::fromLatin1(cdn::kDomain),
        QStringLiteral("makineceviri.org"),
#ifdef MAKINE_DEV_TOOLS
        QStringLiteral("localhost"),
        QStringLiteral("127.0.0.1"),
#endif
    };
    QUrl dlUrlCheck{m_downloadUrl};
    QString host = dlUrlCheck.host().toLower();
    bool hostAllowed = false;
    for (const auto& allowed : allowedHosts) {
        if (host == allowed) {
            hostAllowed = true;
            break;
        }
    }
    if (!hostAllowed) {
        setError(QStringLiteral("İndirme engellendi: güvenilmeyen sunucu '%1'").arg(host));
        return;
    }

    // Prepare temp directory
    QString tempDir = AppPaths::updateTempDir();
    QDir().mkpath(tempDir);

    QString fileName = QUrl(m_downloadUrl).fileName();
    if (fileName.isEmpty())
        fileName = QStringLiteral("Makine-Launcher.exe");
    m_installerPath = tempDir + QStringLiteral("/") + fileName;

    // Remove old file if exists
    QFile::remove(m_installerPath);

    m_downloadFile = std::make_unique<QFile>(m_installerPath);
    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        setError(QStringLiteral("Dosya oluşturulamadı: %1").arg(m_downloadFile->errorString()));
        m_downloadFile.reset();
        return;
    }

    setError({});
    m_progress = 0.0;
    emit progressChanged();
    setState(Downloading);

    QUrl dlUrl{m_downloadUrl};
    QNetworkRequest request{dlUrl};
    request.setRawHeader("User-Agent", QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) Makine-Launcher/%1").arg(QCoreApplication::applicationVersion()).toUtf8());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::SameOriginRedirectPolicy);

    m_downloadReply = m_nam.get(request);

    connect(m_downloadReply, &QNetworkReply::readyRead, this, [this]() {
        if (m_downloadFile && m_downloadReply)
            m_downloadFile->write(m_downloadReply->readAll());
    });

    connect(m_downloadReply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
        if (total > 0) {
            m_progress = static_cast<qreal>(received) / static_cast<qreal>(total);
            emit progressChanged();
            emit displayChanged();  // navLabel shows "%XX"
        }
    });

    connect(m_downloadReply, &QNetworkReply::finished, this, [this]() {
        auto *reply = m_downloadReply;
        m_downloadReply = nullptr;

        if (m_downloadFile) {
            m_downloadFile->close();
            m_downloadFile.reset();
        }

        if (!reply) return;
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            setError(reply->errorString());
            QFile::remove(m_installerPath);
            setState(Available); // Allow retry
            return;
        }

        // Verify integrity
        if (!m_expectedChecksum.isEmpty()) {
            verifyAndFinalize(m_installerPath);
        } else {
            // No checksum available — fail closed
            setError(QStringLiteral("Bütünlük doğrulaması yapılamadı: checksum bilgisi eksik"));
            QFile::remove(m_installerPath);
            setState(Available);
        }
    });
}

void UpdateService::verifyAndFinalize(const QString& filePath)
{
    MAKINE_ZONE_NAMED("UpdateService::verifyAndFinalize");
    setState(Verifying);

    // SHA256 verification
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(QStringLiteral("İndirilen dosya doğrulama için açılamadı"));
        QFile::remove(filePath);
        setState(Available);
        return;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    QString actualHash = hash.result().toHex().toLower();
    file.close();

    if (actualHash != m_expectedChecksum) {
        setError(QStringLiteral("Dosya bütünlüğü doğrulanamadı"));
        QFile::remove(filePath);
        setState(Available);
        return;
    }

#ifdef Q_OS_WIN
#ifndef MAKINE_DEV_TOOLS
    // Verify Authenticode signature
    if (!SelfUpdater::verifySignature(filePath)) {
        setError(QStringLiteral("Dijital imza doğrulanamadı. Dosya değiştirilmiş olabilir."));
        QFile::remove(filePath);
        setState(Available);
        return;
    }
#else
    qCDebug(lcUpdateService) << "UpdateService: Skipping Authenticode check (dev build)";
#endif
#endif

    m_progress = 1.0;
    emit progressChanged();
    setState(Ready);
}

// ---------------------------------------------------------------------------
// Install
// ---------------------------------------------------------------------------

void UpdateService::install()
{
    if (m_state != Ready || m_installerPath.isEmpty())
        return;

    if (!QFile::exists(m_installerPath)) {
        setError(QStringLiteral("Güncelleme dosyası bulunamadı"));
        setState(Available);
        return;
    }

    setState(Installing);
    // Does not return
    SelfUpdater::swapAndRestart(m_installerPath);
}

// ---------------------------------------------------------------------------
// Cancel / Dismiss
// ---------------------------------------------------------------------------

void UpdateService::cancel()
{
    if (m_downloadReply) {
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
    }

    if (m_downloadFile) {
        m_downloadFile->close();
        m_downloadFile.reset();
    }

    if (!m_installerPath.isEmpty())
        QFile::remove(m_installerPath);

    if (m_state == Downloading || m_state == Verifying) {
        m_progress = 0.0;
        emit progressChanged();
        setState(Available); // Can retry
    }
}

void UpdateService::dismiss()
{
    if (m_state == Available) {
        setState(Idle);
    }
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Computed display properties (C++ → QML, zero logic in QML)
// ---------------------------------------------------------------------------

QString UpdateService::statusText() const
{
    switch (m_state) {
    case Checking:    return tr("Kontrol ediliyor...");
    case Available:   return tr("Yeni s\u00FCr\u00FCm mevcut: %1").arg(m_version);
    case Downloading: return tr("\u0130ndiriliyor... %1%").arg(qRound(m_progress * 100));
    case Verifying:   return tr("Do\u011Frulan\u0131yor...");
    case Ready:       return tr("G\u00FCncelleme kurulmaya haz\u0131r");
    case Installing:  return tr("G\u00FCncelleme kuruluyor...");
    case Idle:
        return m_error.isEmpty() ? tr("G\u00FCncel s\u00FCr\u00FCmdesiniz")
                                 : tr("Kontrol ba\u015Far\u0131s\u0131z oldu");
    }
    return {};
}

QString UpdateService::navLabel() const
{
    switch (m_state) {
    case Idle:        return tr("G\u00FCncel");  // Always "Güncel" — errors shown in Settings only
    case Checking:    return tr("G\u00FCncel");
    case Available:   return tr("v%1 mevcut").arg(m_version);
    case Downloading: return tr("%%%1").arg(qRound(m_progress * 100));
    case Verifying:   return tr("Do\u011Frulan\u0131yor");
    case Ready:       return tr("Kurulmaya haz\u0131r");
    case Installing:  return tr("Kuruluyor");
    }
    return {};
}

QString UpdateService::navIcon() const
{
    switch (m_state) {
    case Available:   return QStringLiteral("\uE896");  // download arrow
    case Ready:       return QStringLiteral("\uE72C");  // checkmark
    case Installing:  return QStringLiteral("\uE823");  // clock
    default:          return {};
    }
}

bool UpdateService::indicatorVisible() const
{
    return m_state != Idle && m_state != Checking;
}

bool UpdateService::actionable() const
{
    return m_state == Available || m_state == Downloading
        || m_state == Verifying || m_state == Ready;
}

bool UpdateService::busy() const
{
    return m_state == Checking || m_state == Downloading
        || m_state == Verifying || m_state == Installing;
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

int UpdateService::compareVersions(const QString& v1, const QString& v2)
{
    if (v1.isEmpty() || v2.isEmpty())
        return 0;

    const QStringList parts1 = v1.split(QLatin1Char('.'));
    const QStringList parts2 = v2.split(QLatin1Char('.'));

    const int maxLen = qMax(parts1.size(), parts2.size());
    for (int i = 0; i < maxLen; ++i) {
        const int p1 = (i < parts1.size()) ? parts1[i].toInt() : 0;
        const int p2 = (i < parts2.size()) ? parts2[i].toInt() : 0;
        if (p1 > p2) return 1;
        if (p1 < p2) return -1;
    }
    return 0;
}

void UpdateService::setState(State s)
{
    if (m_state != s) {
        m_state = s;
        emit stateChanged();
        emit displayChanged();
    }
}

void UpdateService::setError(const QString& msg)
{
    if (m_error != msg) {
        m_error = msg;
        emit errorChanged();
        emit displayChanged();
    }
}

// ---------------------------------------------------------------------------
// GitHub dev channel (private repo support)
// ---------------------------------------------------------------------------

QString UpdateService::readGitHubToken()
{
    // 1. Environment variable
    QString token = qEnvironmentVariable("GITHUB_TOKEN");
    if (!token.isEmpty())
        return token;

    // 2. gh CLI config (Windows: %APPDATA%/GitHub CLI/hosts.yml)
#ifdef Q_OS_WIN
    QString configPath = qEnvironmentVariable("APPDATA")
        + QStringLiteral("/GitHub CLI/hosts.yml");
#else
    QString configPath = QDir::homePath()
        + QStringLiteral("/.config/gh/hosts.yml");
#endif

    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    // Simple line-by-line parsing for oauth_token value
    bool inGitHub = false;
    while (!file.atEnd()) {
        QString line = QString::fromUtf8(file.readLine());
        if (line.contains(QStringLiteral("github.com")))
            inGitHub = true;
        if (inGitHub && line.contains(QStringLiteral("oauth_token:"))) {
            token = line.mid(line.indexOf(QLatin1Char(':')) + 1).trimmed();
            break;
        }
    }
    return token;
}

void UpdateService::checkGitHub()
{
    static const QString kGitHubApiUrl =
        QStringLiteral("https://api.github.com/repos/MakineCeviri/Makine-Launcher/releases/latest");

    qCDebug(lcUpdateService) << "UpdateService: Checking GitHub Releases API";

    QNetworkRequest request{QUrl{kGitHubApiUrl}};
    request.setRawHeader("Authorization", ("Bearer " + m_githubToken).toUtf8());
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "Makine-Launcher");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setTransferTimeout(15000);

    auto *reply = m_nam.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onGitHubCheckFinished(reply);
    });
}

void UpdateService::onGitHubCheckFinished(QNetworkReply* reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qCDebug(lcUpdateService) << "UpdateService: GitHub API failed:" << reply->errorString();
        setError(reply->errorString());
        setState(Idle);
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    const QJsonObject obj = doc.object();

    // Parse GitHub release fields
    QString tagName = obj.value(QStringLiteral("tag_name")).toString();
    QString body = obj.value(QStringLiteral("body")).toString();
    QJsonArray assets = obj.value(QStringLiteral("assets")).toArray();

    if (tagName.isEmpty() || assets.isEmpty()) {
        qCDebug(lcUpdateService) << "UpdateService: No GitHub release or assets found";
        setState(Idle);
        return;
    }

    // Find the .exe asset
    int assetId = 0;
    qint64 assetSize = 0;
    for (const auto& a : assets) {
        QJsonObject asset = a.toObject();
        QString name = asset.value(QStringLiteral("name")).toString();
        if (name.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
            assetId = asset.value(QStringLiteral("id")).toInt();
            assetSize = asset.value(QStringLiteral("size")).toInteger();
            break;
        }
    }

    if (assetId == 0) {
        qCDebug(lcUpdateService) << "UpdateService: No .exe asset in GitHub release";
        setState(Idle);
        return;
    }

    // Extract SHA256 from release body (format: "SHA256:hexhash")
    QString checksum;
    for (const auto& line : body.split(QLatin1Char('\n'))) {
        QString trimmed = line.trimmed();
        if (trimmed.startsWith(QStringLiteral("SHA256:"), Qt::CaseInsensitive)) {
            checksum = trimmed.mid(7).trimmed().toLower();
            break;
        }
    }

    // Parse version from tag: "v0.1.0-pre-alpha-dev" -> "0.1.0-pre-alpha-dev"
    QString version = tagName;
    if (version.startsWith(QLatin1Char('v')))
        version = version.mid(1);

    // Store metadata
    m_githubAssetId = assetId;
    m_totalBytes = assetSize;
    m_expectedChecksum = checksum;
    m_releaseNotes = body;

    // Compare versions
    QString currentRaw = QCoreApplication::applicationVersion();

    auto splitPreRelease = [](const QString& raw) -> std::pair<QString, QString> {
        int dashIdx = raw.indexOf(QLatin1Char('-'));
        if (dashIdx > 0)
            return {raw.left(dashIdx), raw.mid(dashIdx + 1)};
        return {raw, {}};
    };

    auto [remoteVer, remotePre] = splitPreRelease(version);
    auto [currentVer, currentPre] = splitPreRelease(currentRaw);

    int cmp = compareVersions(remoteVer, currentVer);
    // For dev channel: any different pre-release suffix = update available
    bool hasUpdate = (cmp > 0)
        || (cmp == 0 && remotePre != currentPre && !remotePre.isEmpty());

    // Dev builds: same version but different binary → compare SHA-256
    if (!hasUpdate && !checksum.isEmpty()) {
        const QString exePath = QCoreApplication::applicationFilePath();
        QFile exeFile(exePath);
        if (exeFile.open(QIODevice::ReadOnly)) {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            hash.addData(&exeFile);
            QString localHash = QString::fromLatin1(hash.result().toHex()).toLower();
            if (localHash != checksum) {
                hasUpdate = true;
                qCDebug(lcUpdateService) << "UpdateService: Same version but different binary"
                         << "(local:" << localHash.left(16) << "remote:" << checksum.left(16) << ")";
            }
        }
    }

    qCDebug(lcUpdateService) << "UpdateService: GitHub" << version << "vs" << currentRaw
             << "-> hasUpdate:" << hasUpdate;

    if (hasUpdate) {
        m_version = tagName;
        emit versionChanged();
        setState(Available);
    } else {
        setState(Idle);
    }
}

void UpdateService::downloadGitHubAsset()
{
    MAKINE_ZONE_NAMED("UpdateService::downloadGitHubAsset");

    // Step 1: Request asset via API with auth — GitHub returns 302 to presigned S3 URL
    QString apiUrl = QStringLiteral(
        "https://api.github.com/repos/MakineCeviri/Makine-Launcher/releases/assets/%1")
        .arg(m_githubAssetId);

    QNetworkRequest request{QUrl{apiUrl}};
    request.setRawHeader("Authorization", ("Bearer " + m_githubToken).toUtf8());
    request.setRawHeader("Accept", "application/octet-stream");
    request.setRawHeader("User-Agent", "Makine-Launcher");
    // Manual redirect: we need to follow without forwarding auth header
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);

    setError({});
    m_progress = 0.0;
    emit progressChanged();
    setState(Downloading);

    auto *reply = m_nam.get(request);
    connect(reply, &QNetworkReply::redirected, this, [this, reply](const QUrl &url) {
        // Mark so the parallel finished-handler skips this reply (B2-06 race)
        reply->setProperty("makineRedirected", true);
        reply->deleteLater();

        // Step 2: Follow redirect to presigned S3 URL (no auth needed)
        m_downloadUrl = url.toString();
        qCDebug(lcUpdateService) << "UpdateService: GitHub redirect ->" << url.host();

        // Prepare temp file
        QString tempDir = AppPaths::updateTempDir();
        QDir().mkpath(tempDir);
        m_installerPath = tempDir + QStringLiteral("/Makine-Launcher.exe");
        QFile::remove(m_installerPath);

        m_downloadFile = std::make_unique<QFile>(m_installerPath);
        if (!m_downloadFile->open(QIODevice::WriteOnly)) {
            setError(QStringLiteral("Cannot create file: %1").arg(m_downloadFile->errorString()));
            m_downloadFile.reset();
            setState(Available);
            return;
        }

        // Download from presigned URL (unauthenticated)
        QNetworkRequest dlReq{url};
        dlReq.setRawHeader("User-Agent", "Makine-Launcher");
        dlReq.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                           QNetworkRequest::NoLessSafeRedirectPolicy);

        m_downloadReply = m_nam.get(dlReq);

        connect(m_downloadReply, &QNetworkReply::readyRead, this, [this]() {
            if (m_downloadFile && m_downloadReply)
                m_downloadFile->write(m_downloadReply->readAll());
        });

        connect(m_downloadReply, &QNetworkReply::downloadProgress, this,
                [this](qint64 received, qint64 total) {
            if (total > 0) {
                m_progress = static_cast<qreal>(received) / static_cast<qreal>(total);
                emit progressChanged();
                emit displayChanged();
            }
        });

        connect(m_downloadReply, &QNetworkReply::finished, this, [this]() {
            auto *r = m_downloadReply;
            m_downloadReply = nullptr;

            if (m_downloadFile) {
                m_downloadFile->close();
                m_downloadFile.reset();
            }
            if (!r) return;
            r->deleteLater();

            if (r->error() != QNetworkReply::NoError) {
                setError(r->errorString());
                QFile::remove(m_installerPath);
                setState(Available);
                return;
            }

            if (!m_expectedChecksum.isEmpty()) {
                verifyAndFinalize(m_installerPath);
            } else {
                // Dev builds: allow without checksum
                qCDebug(lcUpdateService) << "UpdateService: No checksum in release, skipping verify";
                m_progress = 1.0;
                emit progressChanged();
                setState(Ready);
            }
        });
    });

    // Handle case where GitHub doesn't redirect (error)
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        // The redirected lambda already owns this reply lifetime; bail out so
        // we don't double-deleteLater or race against the new download reply.
        if (reply->property("makineRedirected").toBool())
            return;
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 302) {
            if (m_state == Downloading) {
                setError(QStringLiteral("GitHub asset download failed: %1").arg(reply->errorString()));
                setState(Available);
            }
            reply->deleteLater();
        }
    });
}

} // namespace makine
