/**
 * @file updatechecker.cpp
 * @brief GitHub release update checker with auto-update implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "updatechecker.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QSettings>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QProcess>
#include <QCryptographicHash>

namespace makineai {

static constexpr const char* kGitHubApiUrl =
    "https://api.github.com/repos/jlceaser/MakineAI/releases/latest";

// Only check once per 24 hours to avoid rate limiting
static constexpr qint64 kCheckIntervalSecs = 24 * 60 * 60;

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
}

UpdateChecker::~UpdateChecker()
{
    cancelDownload();
}

UpdateChecker* UpdateChecker::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(jsEngine)
    auto *instance = new UpdateChecker;
    QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
    return instance;
}

void UpdateChecker::checkForUpdatesIfNeeded()
{
    QSettings settings(QStringLiteral("MakineAI"), QStringLiteral("MakineAI"));
    QDateTime lastCheck = settings.value(QStringLiteral("update/lastCheckTime")).toDateTime();

    if (lastCheck.isValid()) {
        qint64 elapsed = lastCheck.secsTo(QDateTime::currentDateTime());
        if (elapsed < kCheckIntervalSecs) {
            // Restore cached result if available
            QString cachedVersion = settings.value(QStringLiteral("update/cachedVersion")).toString();
            QString cachedUrl = settings.value(QStringLiteral("update/cachedUrl")).toString();
            bool cachedHasUpdate = settings.value(QStringLiteral("update/cachedHasUpdate"), false).toBool();

            if (cachedHasUpdate && !cachedVersion.isEmpty()) {
                m_updateAvailable = true;
                m_latestVersion = cachedVersion;
                m_downloadUrl = cachedUrl;
                emit updateAvailableChanged();
                emit latestVersionChanged();
                emit downloadUrlChanged();
                setStatus(cachedVersion, QStringLiteral("updateAvailable"));
            } else {
                setStatus(QStringLiteral(""), QStringLiteral("upToDate"));
            }

            qDebug() << "UpdateChecker: Skipping check, last checked" << elapsed << "seconds ago";
            return;
        }
    }

    checkForUpdates();
}

void UpdateChecker::checkForUpdates()
{
    if (m_checking)
        return;

    m_checking = true;
    emit checkingChanged();
    setStatus(QStringLiteral(""), QStringLiteral("checking"));

    QNetworkRequest request(QUrl(QString::fromLatin1(kGitHubApiUrl)));
    request.setRawHeader("Accept", "application/vnd.github.v3+json");
    request.setRawHeader("User-Agent", "MakineAI-UpdateChecker");
    request.setTransferTimeout(15000); // 15s timeout

    auto *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onReplyFinished(reply);
    });
}

void UpdateChecker::onReplyFinished(QNetworkReply* reply)
{
    reply->deleteLater();

    m_checking = false;
    emit checkingChanged();

    if (reply->error() != QNetworkReply::NoError) {
        setStatus(QStringLiteral(""), QStringLiteral("error"));
        emit checkFailed(reply->errorString());
        return;
    }

    const QByteArray data = reply->readAll();
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        emit checkFailed(QStringLiteral("JSON parse error: %1").arg(parseError.errorString()));
        return;
    }

    const QJsonObject obj = doc.object();
    const QString tagName = obj.value(QStringLiteral("tag_name")).toString();
    const QString htmlUrl = obj.value(QStringLiteral("html_url")).toString();

    if (tagName.isEmpty()) {
        emit checkFailed(QStringLiteral("No tag_name in response"));
        return;
    }

    // Parse assets for installer and checksums
    m_installerUrl.clear();
    m_checksumsUrl.clear();
    m_installerSize = 0;

    const QJsonArray assets = obj.value(QStringLiteral("assets")).toArray();
    for (const auto& asset : assets) {
        QJsonObject a = asset.toObject();
        QString name = a.value(QStringLiteral("name")).toString();
        if (name.endsWith(QStringLiteral("-setup.exe"))) {
            m_installerUrl = a.value(QStringLiteral("browser_download_url")).toString();
            m_installerSize = a.value(QStringLiteral("size")).toInteger();
            emit installerSizeChanged();
        }
        if (name == QStringLiteral("checksums.txt")) {
            m_checksumsUrl = a.value(QStringLiteral("browser_download_url")).toString();
        }
    }

    // Parse version: strip "v" prefix, separate pre-release suffix
    QString remoteRaw = tagName;
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

    // Cache the result with timestamp
    QSettings settings(QStringLiteral("MakineAI"), QStringLiteral("MakineAI"));
    settings.setValue(QStringLiteral("update/lastCheckTime"), QDateTime::currentDateTime());
    settings.setValue(QStringLiteral("update/cachedHasUpdate"), hasUpdate);
    settings.setValue(QStringLiteral("update/cachedVersion"), tagName);
    settings.setValue(QStringLiteral("update/cachedUrl"), htmlUrl);

    if (hasUpdate) {
        m_updateAvailable = true;
        m_latestVersion = tagName;
        m_downloadUrl = htmlUrl;
        emit updateAvailableChanged();
        emit latestVersionChanged();
        emit downloadUrlChanged();
        setStatus(tagName, QStringLiteral("updateAvailable"));
    } else {
        setStatus(QStringLiteral(""), QStringLiteral("upToDate"));
    }

    emit checkCompleted(hasUpdate, tagName, htmlUrl);
}

void UpdateChecker::downloadUpdate()
{
    if (m_downloading || m_installerUrl.isEmpty())
        return;

    // Clear previous state
    setDownloadError({});
    m_readyToInstall = false;
    emit readyToInstallChanged();

    // Prepare temp directory
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                      + QStringLiteral("/MakineAI-update");
    QDir().mkpath(tempDir);

    QString fileName = QUrl(m_installerUrl).fileName();
    m_installerPath = tempDir + QStringLiteral("/") + fileName;

    // Remove old file if exists
    QFile::remove(m_installerPath);

    m_downloadFile = new QFile(m_installerPath, this);
    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        setDownloadError(QStringLiteral("Cannot create file: %1").arg(m_downloadFile->errorString()));
        delete m_downloadFile;
        m_downloadFile = nullptr;
        return;
    }

    m_downloading = true;
    m_downloadProgress = 0.0;
    emit downloadingChanged();
    emit downloadProgressChanged();

    QUrl dlUrl{m_installerUrl};
    QNetworkRequest request{dlUrl};
    request.setRawHeader("User-Agent", "MakineAI-UpdateChecker");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    m_downloadReply = m_networkManager->get(request);

    connect(m_downloadReply, &QNetworkReply::readyRead, this, [this]() {
        if (m_downloadFile && m_downloadReply)
            m_downloadFile->write(m_downloadReply->readAll());
    });

    connect(m_downloadReply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
        if (total > 0) {
            m_downloadProgress = static_cast<qreal>(received) / static_cast<qreal>(total);
            emit downloadProgressChanged();
        }
    });

    connect(m_downloadReply, &QNetworkReply::finished, this, [this]() {
        auto *reply = m_downloadReply;
        m_downloadReply = nullptr;

        if (m_downloadFile) {
            m_downloadFile->close();
            delete m_downloadFile;
            m_downloadFile = nullptr;
        }

        if (!reply) return;
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            m_downloading = false;
            emit downloadingChanged();
            setDownloadError(reply->errorString());
            QFile::remove(m_installerPath);
            return;
        }

        // If checksums URL is available, verify integrity
        if (!m_checksumsUrl.isEmpty()) {
            verifyAndFinalize(m_installerPath);
        } else {
            // No checksums available, accept as-is
            m_downloading = false;
            m_downloadProgress = 1.0;
            m_readyToInstall = true;
            emit downloadingChanged();
            emit downloadProgressChanged();
            emit readyToInstallChanged();
            emit downloadCompleted();
        }
    });
}

void UpdateChecker::verifyAndFinalize(const QString& installerPath)
{
    QUrl csUrl{m_checksumsUrl};
    QNetworkRequest request{csUrl};
    request.setRawHeader("User-Agent", "MakineAI-UpdateChecker");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    auto *csReply = m_networkManager->get(request);
    connect(csReply, &QNetworkReply::finished, this, [this, csReply, installerPath]() {
        csReply->deleteLater();

        if (csReply->error() != QNetworkReply::NoError) {
            // Checksums fetch failed, still allow install with warning
            qWarning() << "UpdateChecker: Could not fetch checksums:" << csReply->errorString();
            m_downloading = false;
            m_downloadProgress = 1.0;
            m_readyToInstall = true;
            emit downloadingChanged();
            emit downloadProgressChanged();
            emit readyToInstallChanged();
            emit downloadCompleted();
            return;
        }

        // Parse checksums.txt: each line is "SHA256_HASH  FILENAME"
        QString checksumsContent = QString::fromUtf8(csReply->readAll());
        QString installerFileName = QFileInfo(installerPath).fileName();

        QString expectedHash;
        const QStringList lines = checksumsContent.split(QLatin1Char('\n'));
        for (const auto& line : lines) {
            if (line.contains(installerFileName)) {
                expectedHash = line.split(QRegularExpression(QStringLiteral("\\s+"))).first().toLower();
                break;
            }
        }

        if (!expectedHash.isEmpty()) {
            // Calculate SHA256 of downloaded file
            QFile file(installerPath);
            if (file.open(QIODevice::ReadOnly)) {
                QCryptographicHash hash(QCryptographicHash::Sha256);
                hash.addData(&file);
                QString actualHash = hash.result().toHex().toLower();
                file.close();

                if (actualHash != expectedHash) {
                    m_downloading = false;
                    emit downloadingChanged();
                    setDownloadError(QStringLiteral("Checksum verification failed"));
                    QFile::remove(installerPath);
                    return;
                }
            }
        }

        m_downloading = false;
        m_downloadProgress = 1.0;
        m_readyToInstall = true;
        emit downloadingChanged();
        emit downloadProgressChanged();
        emit readyToInstallChanged();
        emit downloadCompleted();
    });
}

void UpdateChecker::installUpdate()
{
    if (!m_readyToInstall || m_installerPath.isEmpty())
        return;

    if (!QFile::exists(m_installerPath)) {
        setDownloadError(QStringLiteral("Installer file not found"));
        m_readyToInstall = false;
        emit readyToInstallChanged();
        return;
    }

    emit installStarted();

    QStringList args{
        QStringLiteral("/SILENT"),
        QStringLiteral("/CLOSEAPPLICATIONS"),
        QStringLiteral("/RESTARTAPPLICATIONS")
    };

    bool started = QProcess::startDetached(m_installerPath, args);
    if (started) {
        QCoreApplication::quit();
    } else {
        setDownloadError(QStringLiteral("Failed to start installer"));
    }
}

void UpdateChecker::cancelDownload()
{
    if (m_downloadReply) {
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
    }

    if (m_downloadFile) {
        m_downloadFile->close();
        delete m_downloadFile;
        m_downloadFile = nullptr;
    }

    if (!m_installerPath.isEmpty())
        QFile::remove(m_installerPath);

    if (m_downloading) {
        m_downloading = false;
        m_downloadProgress = 0.0;
        emit downloadingChanged();
        emit downloadProgressChanged();
    }
}

int UpdateChecker::compareVersions(const QString& v1, const QString& v2)
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

void UpdateChecker::setStatus(const QString& text, const QString& type)
{
    if (m_statusText != text) {
        m_statusText = text;
        emit statusTextChanged();
    }
    if (m_statusType != type) {
        m_statusType = type;
        emit statusTypeChanged();
    }
}

void UpdateChecker::setDownloadError(const QString& error)
{
    if (m_downloadError != error) {
        m_downloadError = error;
        emit downloadErrorChanged();
    }
}

} // namespace makineai
