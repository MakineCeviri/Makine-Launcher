/**
 * @file installflowservice.h
 * @brief Install/update state machine (replaces InstallFlowController.qml JS)
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Manages the full install chain:
 *   external redirect → anti-cheat → detail loading → options → variants → download gate → install
 * Also handles update flow (no backup, overwrite only).
 */

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVariantList>
#include <optional>

namespace makine {

class GameService;
class TranslationDownloader;
class ManifestSyncService;
class CoreBridge;

class InstallFlowService : public QObject {
    Q_OBJECT

public:
    explicit InstallFlowService(GameService* gameService,
                                TranslationDownloader* downloader,
                                ManifestSyncService* manifestSync,
                                CoreBridge* coreBridge,
                                QObject* parent = nullptr);

    // Entry points
    Q_INVOKABLE void startInstall(const QString& gameId, const QString& gameName,
                                   const QString& externalUrl);
    Q_INVOKABLE void startUpdate(const QString& gameId, const QString& gameName);

    // Dialog callbacks
    Q_INVOKABLE void onAntiCheatContinue(const QString& gameId, const QString& gameName);
    Q_INVOKABLE void onOptionsConfirmed(const QStringList& selectedIds);
    Q_INVOKABLE void onVariantSelected(const QString& variant);
    Q_INVOKABLE void onOptionsCancelled();
    Q_INVOKABLE void onVariantCancelled();

    // Download callbacks
    Q_INVOKABLE void onDownloadReady(const QString& appId);
    Q_INVOKABLE void onDownloadFailed(const QString& appId, const QString& error);

    // Package detail callback
    Q_INVOKABLE void onPackageDetailEnriched(const QString& appId);

signals:
    void showAntiCheatWarning(const QVariantMap& data);
    void showInstallOptions(const QVariantMap& data);
    void showVariantSelection(const QVariantMap& data);
    void externalRedirect(const QString& url);
    void installStarted(const QString& gameId);
    void installError(const QString& gameId, const QString& error);

private:
    void continueAfterAntiCheat(const QString& gameId, const QString& gameName);
    void continueWithDetail(const QString& gameId, const QString& gameName);
    void continueAfterNotes(const QString& gameId, const QString& gameName);
    void continueAfterOptions(const QString& gameId, const QString& variant);
    void doInstall(const QString& gameId, const QString& variant, const QStringList& options);
    void doUpdate(const QString& gameId, const QString& variant, const QStringList& options);
    bool ensurePackageDetail(const QString& gameId, const QString& gameName);
    QString resolveDirName(const QString& gameId, const QVariantMap& catalog) const;

    GameService* m_gameService;
    TranslationDownloader* m_downloader;
    ManifestSyncService* m_manifestSync;
    CoreBridge* m_coreBridge;

    // Pending state
    QString m_pendingGameId;
    QString m_pendingGameName;
    bool m_pendingUpdateFlow{false};

    // Pending download state for R2 packages
    struct PendingDownload {
        QString gameId;
        QString variant;
        QStringList selectedOptions;
    };
    std::optional<PendingDownload> m_pendingDownload;

    // Pending install options (for onOptionsConfirmed callback)
    QVariantMap m_pendingInstallOptionsData;

    // Pending variant data (for onVariantSelected callback)
    QVariantMap m_pendingVariantData;
};

} // namespace makine
