/**
 * @file updateservice.h
 * @brief Application update lifecycle — single QML facade
 * @copyright (c) 2026 MakineCeviri Team
 *
 * State-enum driven API. C++ computes all derived display
 * properties; QML only binds, never switches on state.
 */

#pragma once

#include <QObject>
#include <QString>
#include <QQmlEngine>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <memory>

namespace makine {

class UpdateService : public QObject
{
    Q_OBJECT

    // ── Primary state ──
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString version READ version NOTIFY versionChanged)
    Q_PROPERTY(QString releaseNotes READ releaseNotes NOTIFY versionChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(qint64 totalBytes READ totalBytes NOTIFY versionChanged)

    // ── Computed display properties (C++ → QML, no logic in QML) ──
    Q_PROPERTY(QString statusText READ statusText NOTIFY displayChanged)
    Q_PROPERTY(QString navLabel READ navLabel NOTIFY displayChanged)
    Q_PROPERTY(QString navIcon READ navIcon NOTIFY displayChanged)
    Q_PROPERTY(bool indicatorVisible READ indicatorVisible NOTIFY displayChanged)
    Q_PROPERTY(bool actionable READ actionable NOTIFY displayChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY displayChanged)

public:
    enum State {
        Idle,
        Checking,
        Available,
        Downloading,
        Verifying,
        Ready,
        Installing
    };
    Q_ENUM(State)

    explicit UpdateService(QObject *parent = nullptr);
    ~UpdateService() override;

    static UpdateService* create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    // ── Primary state accessors ──
    State state() const { return m_state; }
    qreal progress() const { return m_progress; }
    QString version() const { return m_version; }
    QString releaseNotes() const { return m_releaseNotes; }
    QString error() const { return m_error; }
    qint64 totalBytes() const { return m_totalBytes; }

    // ── Computed display accessors ──
    QString statusText() const;
    QString navLabel() const;
    QString navIcon() const;
    bool indicatorVisible() const;
    bool actionable() const;
    bool busy() const;

    // ── QML-invokable actions ──
    Q_INVOKABLE void check();
    Q_INVOKABLE void download();
    Q_INVOKABLE void install();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void dismiss();

    /// Called from main.cpp on --post-update launch (static)
    static void handlePostUpdate();

    /// Semantic version comparison
    Q_INVOKABLE static int compareVersions(const QString& v1, const QString& v2);

signals:
    void stateChanged();
    void progressChanged();
    void versionChanged();
    void errorChanged();
    void displayChanged();   // statusText, indicatorVisible, busy

private:
    void setState(State s);
    void setError(const QString& msg);
    void onCheckFinished(QNetworkReply* reply);
    void verifyAndFinalize(const QString& filePath);

    // GitHub dev channel (private repo, token from gh CLI)
    void checkGitHub();
    void onGitHubCheckFinished(QNetworkReply* reply);
    void downloadGitHubAsset();
    static QString readGitHubToken();

    QNetworkAccessManager m_nam;
    State m_state{Idle};
    qreal m_progress{0.0};
    QString m_version;
    QString m_releaseNotes;
    QString m_error;
    qint64 m_totalBytes{0};

    // Internal download state
    QString m_downloadUrl;
    QString m_expectedChecksum;
    QString m_installerPath;
    QNetworkReply* m_downloadReply{nullptr};
    std::unique_ptr<QFile> m_downloadFile;

    // GitHub dev channel state
    QString m_githubToken;
    int m_githubAssetId{0};
};

} // namespace makine
