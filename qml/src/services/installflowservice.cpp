// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

/**
 * @file installflowservice.cpp
 * @brief Install/update state machine implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "installflowservice.h"
#include "gameservice.h"
#include "translationdownloader.h"
#include "manifestsyncservice.h"
#include "corebridge.h"
#include "crashreporter.h"
#include "cdnconfig.h"

#include <QDesktopServices>
#include <QUrl>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcInstallFlow, "makine.installflow")

namespace makine {

InstallFlowService::InstallFlowService(GameService* gameService,
                                       TranslationDownloader* downloader,
                                       ManifestSyncService* manifestSync,
                                       CoreBridge* coreBridge,
                                       QObject* parent)
    : QObject(parent)
    , m_gameService(gameService)
    , m_downloader(downloader)
    , m_manifestSync(manifestSync)
    , m_coreBridge(coreBridge)
{
    // The index carries no recipe, so an install whose detail fetch failed
    // would run as a plain copy: Valhalla's 146 .acv files dropped in the game
    // root, reported as installed. Stop and say why instead.
    connect(m_manifestSync, &ManifestSyncService::packageDetailReady, this,
        [this](const QString& appId) {
            if (appId != m_pendingGameId || !m_manifestSync->getPackageDetail(appId).isEmpty())
                return;
            m_pendingGameId.clear();
            m_pendingGameName.clear();
            m_pendingUpdateFlow = false;
            qCWarning(lcInstallFlow) << "Package detail unavailable for" << appId
                                     << "— not installing without its recipe";
            emit m_gameService->translationInstallCompleted(appId, false,
                tr("Kurulum bilgileri alınamadı, yama kurulmadı.\n"
                   "İnternet bağlantınızı kontrol edip tekrar deneyin."));
        });
}

// ===== ENTRY POINT: Install =====

void InstallFlowService::startInstall(const QString& gameId, const QString& gameName)
{
    m_pendingUpdateFlow = false;

    // "Yakında": no package of ours to fetch. Not a failure — nothing starts,
    // nothing is reported.
    if (m_gameService->getCatalogEntry(gameId).value(QStringLiteral("comingSoon")).toBool()
        && !m_gameService->hasLocalPackage(gameId)) {
        qCInfo(lcInstallFlow) << "No package yet for" << gameId << "— install not started";
        return;
    }

    // Anti-cheat check
    QVariantMap antiCheat = m_gameService->checkAntiCheat(gameId);
    if (antiCheat.value(QStringLiteral("hasAntiCheat")).toBool()) {
        QVariantList systems = antiCheat.value(QStringLiteral("systems")).toList();
        if (!systems.isEmpty()) {
            QVariantMap data;
            data[QStringLiteral("gameName")] = gameName;
            data[QStringLiteral("detectedSystems")] = systems;
            emit showAntiCheatWarning(data);
            return;
        }
    }

    continueAfterAntiCheat(gameId, gameName);
}

// ===== ENTRY POINT: Update =====

void InstallFlowService::startUpdate(const QString& gameId, const QString& gameName)
{
    m_pendingUpdateFlow = true;

    if (!ensurePackageDetail(gameId, gameName))
        return;

    // Through the variant gate, not straight to the update: a package whose
    // recipe paths live inside a variant folder cannot be updated without
    // knowing which one. Passing an empty variant sent the install searching
    // from the package root, where it finds one copy per variant and refuses
    // to guess — the user saw "Adım 1 hata" for a package that is intact.
    //
    // And through the options gate before it, as an install goes. An options
    // recipe (Elden Ring, Dark Souls II) updated with no option picked: the
    // update removed the patch, then refused the install as "Bu yama otomatik
    // kurulamıyor (kurulum yöntemi: options)". The record does not keep the
    // options chosen at install time, so the user picks them again.
    continueWithDetail(gameId, gameName);
}

// ===== Anti-cheat continue =====

void InstallFlowService::onAntiCheatContinue(const QString& gameId, const QString& gameName)
{
    m_gameService->acknowledgeAntiCheat(gameId);
    continueAfterAntiCheat(gameId, gameName);
}

void InstallFlowService::continueAfterAntiCheat(const QString& gameId, const QString& gameName)
{
    if (!ensurePackageDetail(gameId, gameName))
        return;

    continueWithDetail(gameId, gameName);
}

bool InstallFlowService::ensurePackageDetail(const QString& gameId, const QString& gameName)
{
    if (m_coreBridge->isPackageDetailLoaded(gameId))
        return true;

    if (m_coreBridge->ensurePackageDetail(gameId))
        return true;

    // Need network fetch — save state and wait
    m_pendingGameId = gameId;
    m_pendingGameName = gameName;
    m_manifestSync->fetchPackageDetail(gameId);
    return false;
}

void InstallFlowService::continueWithDetail(const QString& gameId, const QString& gameName)
{
    // Install notes shown in AboutSection — skip blocking dialog
    continueAfterNotes(gameId, gameName);
}

// ===== Package detail arrived from network =====

void InstallFlowService::onPackageDetailEnriched(const QString& appId)
{
    if (m_pendingGameId != appId)
        return;

    QString gId = m_pendingGameId;
    QString gName = m_pendingGameName;
    m_pendingGameId.clear();
    m_pendingGameName.clear();

    // Install and update both go through the options gate (see startUpdate).
    continueWithDetail(gId, gName);
}

// ===== Install options =====

void InstallFlowService::continueAfterNotes(const QString& gameId, const QString& gameName)
{
    QVariantList options = m_gameService->getInstallOptions(gameId);
    if (!options.isEmpty()) {
        QVariantMap data;
        data[QStringLiteral("gameId")] = gameId;
        data[QStringLiteral("options")] = options;
        data[QStringLiteral("specialDialog")] = m_gameService->getSpecialDialog(gameId);
        data[QStringLiteral("gameName")] = gameName;
        m_pendingInstallOptionsData = data;
        emit showInstallOptions(data);
        return;
    }

    continueAfterOptions(gameId, QString());
}

void InstallFlowService::onOptionsConfirmed(const QStringList& selectedIds)
{
    if (m_pendingInstallOptionsData.isEmpty())
        return;

    QString variant = m_pendingInstallOptionsData.value(QStringLiteral("variant")).toString();
    QString gameId = m_pendingInstallOptionsData.value(QStringLiteral("gameId")).toString();
    m_pendingInstallOptionsData.clear();
    dispatch(gameId, variant, selectedIds);
}

void InstallFlowService::onOptionsCancelled()
{
    m_pendingInstallOptionsData.clear();
    m_pendingUpdateFlow = false;
}

// ===== Variant selection =====

void InstallFlowService::continueAfterOptions(const QString& gameId, const QString& variant)
{
    Q_UNUSED(variant)
    QVariantList variants = m_gameService->getVariants(gameId);
    if (!variants.isEmpty()) {
        QVariantMap data;
        data[QStringLiteral("gameId")] = gameId;
        data[QStringLiteral("variants")] = variants;
        data[QStringLiteral("variantType")] = m_gameService->getVariantType(gameId);
        m_pendingVariantData = data;
        emit showVariantSelection(data);
        return;
    }

    dispatch(gameId, QString(), QStringList());
}

void InstallFlowService::onVariantSelected(const QString& variant)
{
    if (m_pendingVariantData.isEmpty())
        return;

    QString gameId = m_pendingVariantData.value(QStringLiteral("gameId")).toString();
    m_pendingVariantData.clear();

    // Check for variant-specific install options
    QVariantList variantOptions = m_gameService->getVariantInstallOptions(gameId, variant);
    if (!variantOptions.isEmpty()) {
        QVariantMap data;
        data[QStringLiteral("gameId")] = gameId;
        data[QStringLiteral("options")] = variantOptions;
        data[QStringLiteral("specialDialog")] = m_gameService->getVariantSpecialDialog(gameId, variant);
        data[QStringLiteral("gameName")] = QString(); // Will be empty — same as QML behavior
        data[QStringLiteral("variant")] = variant;
        m_pendingInstallOptionsData = data;
        emit showInstallOptions(data);
        return;
    }

    dispatch(gameId, variant, QStringList());
}

void InstallFlowService::onVariantCancelled()
{
    m_pendingVariantData.clear();
    m_pendingUpdateFlow = false;
}

// ===== Download gates =====

void InstallFlowService::dispatch(const QString& gameId, const QString& variant,
                                   const QStringList& options)
{
    if (m_pendingUpdateFlow)
        doUpdate(gameId, variant, options);
    else
        doInstall(gameId, variant, options);
}

void InstallFlowService::doInstall(const QString& gameId, const QString& variant,
                                    const QStringList& options)
{
    // Don't drop other in-flight downloads here — only this gameId's slot
    // gets replaced when we register below (B2-04).
    m_pendingDownloads.remove(gameId);
    m_pendingUpdateFlow = false;

    QVariantMap catalog = m_gameService->getCatalogEntry(gameId);

    // Local package already exists — install directly
    if (m_gameService->hasLocalPackage(gameId)) {
        m_gameService->installTranslation(gameId, variant, options);
        emit installStarted(gameId);
        return;
    }

    // Check catalog for R2 download URL
    QString dataUrl = catalog.value(QStringLiteral("dataUrl")).toString();
    if (dataUrl.isEmpty() || !cdn::isCdnUrl(dataUrl)) {
        // No valid CDN package — try local install
        m_gameService->installTranslation(gameId, variant, options);
        emit installStarted(gameId);
        return;
    }

    // Resolve dirName and start download
    QString dirName = resolveDirName(gameId, catalog);
    // The catalogue entry calls the package size "downloadSize" (catalogstore.cpp); asking
    // for "size" read 0 and the free-space check never scaled with the package.
    const qint64 expectedSize = catalog.value(QStringLiteral("downloadSize")).toLongLong();
    const QString expectedChecksum =
        catalog.value(QStringLiteral("checksum")).toString();

    m_pendingDownloads.insert(gameId, PendingDownload{gameId, variant, options, /*isUpdate=*/false});
    m_downloader->downloadPackage(gameId, dataUrl, dirName, expectedSize,
                                  expectedChecksum);
}

// ===== Download gate: update =====

void InstallFlowService::doUpdate(const QString& gameId, const QString& variant,
                                   const QStringList& options)
{
    m_pendingDownloads.remove(gameId);
    m_pendingUpdateFlow = true;

    if (m_gameService->hasLocalPackage(gameId)) {
        m_gameService->updateTranslation(gameId, variant, options);
        m_pendingUpdateFlow = false;
        return;
    }

    // Need to download from R2 — CDN domain validated
    QVariantMap catalog = m_gameService->getCatalogEntry(gameId);
    QString dataUrl = catalog.value(QStringLiteral("dataUrl")).toString();

    if (dataUrl.isEmpty() || !cdn::isCdnUrl(dataUrl)) {
        m_gameService->updateTranslation(gameId, variant, options);
        m_pendingUpdateFlow = false;
        return;
    }

    QString dirName = resolveDirName(gameId, catalog);
    // The catalogue entry calls the package size "downloadSize" (catalogstore.cpp); asking
    // for "size" read 0 and the free-space check never scaled with the package.
    const qint64 expectedSize = catalog.value(QStringLiteral("downloadSize")).toLongLong();
    const QString expectedChecksum =
        catalog.value(QStringLiteral("checksum")).toString();
    m_pendingDownloads.insert(gameId, PendingDownload{gameId, variant, options, /*isUpdate=*/true});
    m_downloader->downloadPackage(gameId, dataUrl, dirName, expectedSize,
                                  expectedChecksum);
}

// ===== Download callbacks =====

void InstallFlowService::onDownloadReady(const QString& appId)
{
    auto it = m_pendingDownloads.find(appId);
    if (it == m_pendingDownloads.end())
        return;

    PendingDownload pending = it.value();
    m_pendingDownloads.erase(it);

    // Reload LocalPackageManager so CoreBridge picks up the new package
    m_coreBridge->refreshPackageManifest();

    if (pending.isUpdate) {
        m_gameService->updateTranslation(pending.gameId, pending.variant, pending.selectedOptions);
    } else {
        m_gameService->installTranslation(pending.gameId, pending.variant, pending.selectedOptions);
        emit installStarted(pending.gameId);
    }
}

void InstallFlowService::onDownloadFailed(const QString& appId, const QString& error)
{
    auto it = m_pendingDownloads.find(appId);
    if (it == m_pendingDownloads.end())
        return;

    QString gameId = it.value().gameId;
    m_pendingDownloads.erase(it);
    emit installError(gameId, error);
}

// ===== Helpers =====

QString InstallFlowService::resolveDirName(const QString& gameId, const QVariantMap& catalog) const
{
    QString dirName = catalog.value(QStringLiteral("dirName")).toString();
    if (dirName.isEmpty())
        dirName = m_coreBridge->getPackageDirName(gameId);
    if (dirName.isEmpty()) {
        dirName = catalog.value(QStringLiteral("gameName")).toString();
        if (dirName.isEmpty())
            dirName = catalog.value(QStringLiteral("name")).toString();
        if (dirName.isEmpty())
            dirName = gameId;
    }
    return dirName;
}

} // namespace makine
