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
}

// ===== ENTRY POINT: Install =====

void InstallFlowService::startInstall(const QString& gameId, const QString& gameName,
                                       const QString& externalUrl)
{
    m_pendingUpdateFlow = false;

    // External partner redirect (e.g. ApexYama) — scheme-validated
    if (!externalUrl.isEmpty()) {
        QUrl url(externalUrl);
        if (url.scheme() != QStringLiteral("https")) {
            qCWarning(lcInstallFlow) << "Blocked non-HTTPS external URL:" << externalUrl;
            emit installError(gameId, tr("Invalid external URL"));
            return;
        }
        QDesktopServices::openUrl(url);
        emit externalRedirect(externalUrl);
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

    doUpdate(gameId, QString(), QStringList());
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

    if (m_pendingUpdateFlow) {
        doUpdate(gId, QString(), QStringList());
    } else {
        continueWithDetail(gId, gName);
    }
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
    doInstall(gameId, variant, selectedIds);
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

    doInstall(gameId, QString(), QStringList());
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

    doInstall(gameId, variant, QStringList());
}

void InstallFlowService::onVariantCancelled()
{
    m_pendingVariantData.clear();
    m_pendingUpdateFlow = false;
}

// ===== Download gate: install =====

void InstallFlowService::doInstall(const QString& gameId, const QString& variant,
                                    const QStringList& options)
{
    m_pendingDownload.reset();
    m_pendingUpdateFlow = false;

    // External source redirect — scheme-validated
    QVariantMap catalog = m_gameService->getCatalogEntry(gameId);
    QString extUrl = catalog.value(QStringLiteral("externalUrl")).toString();
    if (!extUrl.isEmpty()) {
        QUrl url(extUrl);
        if (url.scheme() != QStringLiteral("https")) {
            qCWarning(lcInstallFlow) << "Blocked non-HTTPS external URL:" << extUrl;
            emit installError(gameId, tr("Invalid external URL"));
            return;
        }
        QDesktopServices::openUrl(url);
        emit externalRedirect(extUrl);
        return;
    }

    // Local package already exists — install directly
    if (m_gameService->hasLocalPackage(gameId)) {
        m_gameService->installTranslation(gameId, variant, options);
        emit installStarted(gameId);
        return;
    }

    // Check catalog for R2 download URL
    QString dataUrl = catalog.value(QStringLiteral("dataUrl")).toString();
    if (dataUrl.isEmpty() || !dataUrl.startsWith(QStringLiteral("https://cdn.makineceviri.org"))) {
        // No valid CDN package — try local install
        m_gameService->installTranslation(gameId, variant, options);
        emit installStarted(gameId);
        return;
    }

    // Resolve dirName and start download
    QString dirName = resolveDirName(gameId, catalog);

    m_pendingDownload = PendingDownload{gameId, variant, options};
    m_downloader->downloadPackage(gameId, dataUrl, dirName);
}

// ===== Download gate: update =====

void InstallFlowService::doUpdate(const QString& gameId, const QString& variant,
                                   const QStringList& options)
{
    m_pendingDownload.reset();
    m_pendingUpdateFlow = true;

    if (m_gameService->hasLocalPackage(gameId)) {
        m_gameService->updateTranslation(gameId, variant, options);
        m_pendingUpdateFlow = false;
        return;
    }

    // Need to download from R2 — CDN domain validated
    QVariantMap catalog = m_gameService->getCatalogEntry(gameId);
    QString dataUrl = catalog.value(QStringLiteral("dataUrl")).toString();

    if (dataUrl.isEmpty() || !dataUrl.startsWith(QStringLiteral("https://cdn.makineceviri.org"))) {
        m_gameService->updateTranslation(gameId, variant, options);
        m_pendingUpdateFlow = false;
        return;
    }

    QString dirName = resolveDirName(gameId, catalog);
    m_pendingDownload = PendingDownload{gameId, variant, options};
    m_downloader->downloadPackage(gameId, dataUrl, dirName);
}

// ===== Download callbacks =====

void InstallFlowService::onDownloadReady(const QString& appId)
{
    if (!m_pendingDownload || m_pendingDownload->gameId != appId)
        return;

    PendingDownload pending = *m_pendingDownload;
    bool isUpdate = m_pendingUpdateFlow;
    m_pendingDownload.reset();
    m_pendingUpdateFlow = false;

    // Reload LocalPackageManager so CoreBridge picks up the new package
    m_coreBridge->refreshPackageManifest();

    if (isUpdate) {
        m_gameService->updateTranslation(pending.gameId, pending.variant, pending.selectedOptions);
    } else {
        m_gameService->installTranslation(pending.gameId, pending.variant, pending.selectedOptions);
        emit installStarted(pending.gameId);
    }
}

void InstallFlowService::onDownloadFailed(const QString& appId, const QString& error)
{
    if (!m_pendingDownload || m_pendingDownload->gameId != appId)
        return;

    QString gameId = m_pendingDownload->gameId;
    m_pendingDownload.reset();
    m_pendingUpdateFlow = false;
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
