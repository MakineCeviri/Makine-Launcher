// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

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
#include <QHash>
#include <QPair>
#include <QSet>

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
    Q_INVOKABLE void startInstall(const QString& gameId, const QString& gameName);
    Q_INVOKABLE void startUpdate(const QString& gameId, const QString& gameName);
    /**
     * @brief Install one of the game's patches ("" = the default; alternatives:
     * index.json "alternatives"). Its detail comes from ?pkg=, its package
     * and folder are its own, and the install record keeps which one it is.
     */
    Q_INVOKABLE void startInstallPackage(const QString& gameId, const QString& gameName,
                                         const QString& pkg);
    /**
     * @brief Replace the installed patch with another: uninstall (the usual
     * flow, backups restored), then install  pkg. One patch per game.
     */
    Q_INVOKABLE void startSwitchPackage(const QString& gameId, const QString& gameName,
                                        const QString& pkg);

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
    void installStarted(const QString& gameId);
    void installError(const QString& gameId, const QString& error);

private:
    void beginInstall(const QString& gameId, const QString& gameName);
    /// An install that will not happen: the entry goes back to the installed patch
    void dropPendingPackage(const QString& gameId);
    void continueAfterAntiCheat(const QString& gameId, const QString& gameName);
    void continueWithDetail(const QString& gameId, const QString& gameName);
    void continueAfterNotes(const QString& gameId, const QString& gameName);
    void continueAfterOptions(const QString& gameId, const QString& variant);
    // Install and update share every gate in front of them; only the last
    // step differs. Routing through here is what keeps an update from
    // skipping a gate the install path asks for.
    void dispatch(const QString& gameId, const QString& variant, const QStringList& options);
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

    // Pending download state for R2 packages — keyed by gameId so concurrent
    // installs don't trample each other's PendingDownload slot (B2-04).
    struct PendingDownload {
        QString gameId;
        QString variant;
        QStringList selectedOptions;
        bool isUpdate;
    };
    QHash<QString, PendingDownload> m_pendingDownloads;

    // Pending install options (for onOptionsConfirmed callback)
    QVariantMap m_pendingInstallOptionsData;

    // Pending variant data (for onVariantSelected callback)
    QVariantMap m_pendingVariantData;

    // Patch switches waiting for their uninstall: gameId -> {gameName, pkg}
    QHash<QString, QPair<QString, QString>> m_pendingSwitches;
    // Installs waiting for "Yine de kur" (GameService::installNeedsConfirmation)
    QSet<QString> m_heldInstalls;
};

} // namespace makine
