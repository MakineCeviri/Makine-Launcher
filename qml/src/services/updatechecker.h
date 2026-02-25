/**
 * @file updatechecker.h
 * @brief GitHub release update checker with auto-update support
 * @copyright (c) 2026 MakineAI Team
 *
 * Checks GitHub releases API for new versions.
 * Downloads installer and runs silent Inno Setup update.
 */

#pragma once

#include <QObject>
#include <QString>
#include <QQmlEngine>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QDateTime>
#include <QFile>
#include <memory>

namespace makineai {

class UpdateChecker : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY updateAvailableChanged)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY latestVersionChanged)
    Q_PROPERTY(QString downloadUrl READ downloadUrl NOTIFY downloadUrlChanged)
    Q_PROPERTY(bool checking READ checking NOTIFY checkingChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString statusType READ statusType NOTIFY statusTypeChanged)

    Q_PROPERTY(bool downloading READ downloading NOTIFY downloadingChanged)
    Q_PROPERTY(qreal downloadProgress READ downloadProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(bool readyToInstall READ readyToInstall NOTIFY readyToInstallChanged)
    Q_PROPERTY(QString downloadError READ downloadError NOTIFY downloadErrorChanged)
    Q_PROPERTY(qint64 installerSize READ installerSize NOTIFY installerSizeChanged)

public:
    explicit UpdateChecker(QObject *parent = nullptr);
    ~UpdateChecker() override;

    static UpdateChecker* create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    bool updateAvailable() const { return m_updateAvailable; }
    QString latestVersion() const { return m_latestVersion; }
    QString downloadUrl() const { return m_downloadUrl; }
    bool checking() const { return m_checking; }
    QString statusText() const { return m_statusText; }
    QString statusType() const { return m_statusType; }

    bool downloading() const { return m_downloading; }
    qreal downloadProgress() const { return m_downloadProgress; }
    bool readyToInstall() const { return m_readyToInstall; }
    QString downloadError() const { return m_downloadError; }
    qint64 installerSize() const { return m_installerSize; }

    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE void checkForUpdatesIfNeeded();
    Q_INVOKABLE void downloadUpdate();
    Q_INVOKABLE void installUpdate();
    Q_INVOKABLE void cancelDownload();

    /**
     * @brief Compare two semantic version strings
     * @return >0 if v1 > v2, <0 if v1 < v2, 0 if equal
     */
    Q_INVOKABLE static int compareVersions(const QString& v1, const QString& v2);

signals:
    void updateAvailableChanged();
    void latestVersionChanged();
    void downloadUrlChanged();
    void checkingChanged();
    void statusTextChanged();
    void statusTypeChanged();
    void checkCompleted(bool hasUpdate, const QString& version, const QString& url);
    void checkFailed(const QString& error);

    void downloadingChanged();
    void downloadProgressChanged();
    void readyToInstallChanged();
    void downloadErrorChanged();
    void installerSizeChanged();
    void downloadCompleted();
    void installStarted();

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    void setStatus(const QString& text, const QString& type);
    void setDownloadError(const QString& error);
    void verifyAndFinalize(const QString& installerPath);

    QNetworkAccessManager m_networkManager;
    bool m_updateAvailable{false};
    QString m_latestVersion;
    QString m_downloadUrl;
    bool m_checking{false};
    QString m_statusText;
    QString m_statusType;  // "upToDate", "updateAvailable", "checking", "error", ""

    // Auto-update state
    bool m_downloading{false};
    qreal m_downloadProgress{0.0};
    bool m_readyToInstall{false};
    QString m_downloadError;
    qint64 m_installerSize{0};
    QString m_installerUrl;
    QString m_checksumsUrl;
    QString m_installerPath;
    QNetworkReply* m_downloadReply{nullptr};  // Non-owning. Managed by QNetworkAccessManager.
    std::unique_ptr<QFile> m_downloadFile;
};

} // namespace makineai
