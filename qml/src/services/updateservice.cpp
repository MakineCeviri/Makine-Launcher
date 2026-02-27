/**
 * @file updateservice.cpp
 * @brief Application update lifecycle management
 * @copyright (c) 2026 MakineAI Team
 */

#include "updateservice.h"
#include "selfupdater.h"
#include "cdnconfig.h"
#include "profiler.h"
#include "apppaths.h"
#include "crashreporter.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QSettings>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>

namespace makineai {

static constexpr const char* kUpdateJsonUrl = cdn::kUpdateJson;

// Only check once per 24 hours to avoid unnecessary requests
static constexpr qint64 kCheckIntervalSecs = 24 * 60 * 60;

UpdateService::UpdateService(QObject *parent)
    : QObject(parent)
{
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
    QSettings settings(QStringLiteral("MakineAI"), QStringLiteral("MakineAI"));
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

#ifdef MAKINEAI_DEV_TOOLS
    QString urlStr = qEnvironmentVariable("MAKINEAI_UPDATE_URL");
    if (urlStr.isEmpty())
        urlStr = QString::fromLatin1(kUpdateJsonUrl);
    else
        qDebug() << "UpdateService: Using override URL:" << urlStr;
#else
    QString urlStr = QString::fromLatin1(kUpdateJsonUrl);
#endif

    qDebug() << "UpdateService: Checking for updates at" << urlStr;

    QNetworkRequest request{QUrl{urlStr}};
    request.setRawHeader("User-Agent", "MakineAI-UpdateService");
    request.setTransferTimeout(15000);

    auto *reply = m_nam.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onCheckFinished(reply);
    });
}

void UpdateService::onCheckFinished(QNetworkReply* reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "UpdateService: Check failed:" << reply->errorString();
        setError(reply->errorString());
        setState(Idle);
        return;
    }

    const QByteArray data = reply->readAll();
    qDebug() << "UpdateService: Received" << data.size() << "bytes";
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        setError(QStringLiteral("JSON parse error: %1").arg(parseError.errorString()));
        setState(Idle);
        return;
    }

    const QJsonObject obj = doc.object();
    const QString version = obj.value(QStringLiteral("version")).toString();
    const QString url = obj.value(QStringLiteral("url")).toString();
    const QString checksum = obj.value(QStringLiteral("checksum")).toString();
    const qint64 size = obj.value(QStringLiteral("size")).toInteger();
    const QString notes = obj.value(QStringLiteral("notes")).toString();

    if (version.isEmpty() || url.isEmpty()) {
        setError(QStringLiteral("Invalid update.json: missing version or url"));
        setState(Idle);
        return;
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
    remoteRaw.remove(QRegularExpression(QStringLiteral("^v")));
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
    QSettings settings(QStringLiteral("MakineAI"), QStringLiteral("MakineAI"));
    settings.setValue(QStringLiteral("update/lastCheckTime"), QDateTime::currentDateTime());
    settings.setValue(QStringLiteral("update/cachedHasUpdate"), hasUpdate);
    settings.setValue(QStringLiteral("update/cachedVersion"), version);
    settings.setValue(QStringLiteral("update/cachedUrl"), url);

    qDebug() << "UpdateService: Remote" << remoteRaw << "vs Current" << currentRaw
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

    // Validate download URL domain against allowlist
    static const QStringList allowedHosts = {
        QString::fromLatin1(cdn::kDomain),
        QStringLiteral("makineceviri.net"),
#ifdef MAKINEAI_DEV_TOOLS
        QStringLiteral("localhost"),
        QStringLiteral("127.0.0.1"),
#endif
    };
    QUrl dlUrlCheck{m_downloadUrl};
    QString host = dlUrlCheck.host().toLower();
    bool hostAllowed = false;
    for (const auto& allowed : allowedHosts) {
        if (host == allowed || host.endsWith(QLatin1Char('.') + allowed)) {
            hostAllowed = true;
            break;
        }
    }
    if (!hostAllowed) {
        setError(QStringLiteral("Download blocked: untrusted host '%1'").arg(host));
        return;
    }

    // Prepare temp directory
    QString tempDir = AppPaths::updateTempDir();
    QDir().mkpath(tempDir);

    QString fileName = QUrl(m_downloadUrl).fileName();
    if (fileName.isEmpty())
        fileName = QStringLiteral("MakineAI.exe");
    m_installerPath = tempDir + QStringLiteral("/") + fileName;

    // Remove old file if exists
    QFile::remove(m_installerPath);

    m_downloadFile = std::make_unique<QFile>(m_installerPath);
    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        setError(QStringLiteral("Cannot create file: %1").arg(m_downloadFile->errorString()));
        m_downloadFile.reset();
        return;
    }

    setError({});
    m_progress = 0.0;
    emit progressChanged();
    setState(Downloading);

    QUrl dlUrl{m_downloadUrl};
    QNetworkRequest request{dlUrl};
    request.setRawHeader("User-Agent", "MakineAI-UpdateService");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

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
            setError(QStringLiteral("Integrity check unavailable: no checksum in update.json"));
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
        setError(QStringLiteral("Cannot open downloaded file for verification"));
        QFile::remove(filePath);
        setState(Available);
        return;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    QString actualHash = hash.result().toHex().toLower();
    file.close();

    if (actualHash != m_expectedChecksum) {
        setError(QStringLiteral("Checksum verification failed"));
        QFile::remove(filePath);
        setState(Available);
        return;
    }

#ifdef Q_OS_WIN
#ifndef MAKINEAI_DEV_TOOLS
    // Verify Authenticode signature
    if (!SelfUpdater::verifySignature(filePath)) {
        setError(QStringLiteral("Signature verification failed. The file may be tampered."));
        QFile::remove(filePath);
        setState(Available);
        return;
    }
#else
    qDebug() << "UpdateService: Skipping Authenticode check (dev build)";
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
        setError(QStringLiteral("Update file missing"));
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

} // namespace makineai
