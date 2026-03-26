/**
 * @file batchoperationservice.cpp
 * @brief Batch translation operations implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "batchoperationservice.h"
#include "profiler.h"
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcBatchOps, "makine.batch")

namespace makine {

BatchOperationService::BatchOperationService(QObject *parent)
    : QObject(parent)
{
    m_coreBridge = CoreBridge::instance();
}

BatchOperationService::~BatchOperationService()
{
    disconnectCurrentItem();
}

void BatchOperationService::disconnectCurrentItem()
{
    disconnect(m_itemCompletionConn);
    disconnect(m_itemProgressConn);
}

BatchOperationService* BatchOperationService::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    return new BatchOperationService();
}

qreal BatchOperationService::overallProgress() const
{
    if (m_totalItems == 0) return 0.0;

    // Overall = (completed items + current item progress) / total
    qreal completedPortion = static_cast<qreal>(m_completedItems) / m_totalItems;
    qreal currentPortion = qBound(0.0, m_currentItemProgress, 1.0) / m_totalItems;
    return qBound(0.0, completedPortion + currentPortion, 1.0);
}

QVariantList BatchOperationService::queue() const
{
    QVariantList list;
    list.reserve(m_queue.size());
    for (const auto& item : m_queue) {
        list.append(QVariantMap{
            {"gameId", item.gameId},
            {"gameName", item.gameName},
            {"status", static_cast<int>(item.status)},
            {"progress", item.progress},
            {"errorMessage", item.errorMessage}
        });
    }
    return list;
}

QVariantList BatchOperationService::results() const
{
    QVariantList list;
    list.reserve(m_results.size());
    for (const auto& item : m_results) {
        list.append(QVariantMap{
            {"gameId", item.gameId},
            {"gameName", item.gameName},
            {"success", item.status == BatchItemStatus::Completed},
            {"skipped", item.status == BatchItemStatus::Skipped},
            {"errorMessage", item.errorMessage}
        });
    }
    return list;
}

void BatchOperationService::startBatch(BatchOperationType type, const QVariantList& gameIds)
{
    MAKINE_ZONE_NAMED("BatchOp::startBatch");
    if (m_isRunning.load()) {
        emit batchError("A batch operation is already running");
        return;
    }

    if (gameIds.isEmpty()) {
        emit batchError("No games selected");
        return;
    }

    m_queue.clear();
    m_results.clear();
    m_currentOperation = type;
    m_currentIndex = -1;
    m_totalItems = 0;
    m_completedItems = 0;
    m_failedItems = 0;
    m_skippedItems = 0;
    m_cancelRequested = false;

    // Build queue from game IDs
    for (const auto& idVar : gameIds) {
        QString gameId = idVar.toString();
        if (gameId.isEmpty()) continue;

        QueueItem item;
        item.gameId = gameId;
        item.status = BatchItemStatus::Pending;

        // Resolve game info from CoreBridge detected games
        if (m_coreBridge) {
            for (const auto& detected : m_coreBridge->detectedGames()) {
                if (detected.id == gameId) {
                    item.gameName = detected.name;
                    item.installPath = detected.installPath;
                    break;
                }
            }
        }
        if (item.gameName.isEmpty()) item.gameName = gameId;

        m_queue.push_back(std::move(item));
    }

    m_totalItems = static_cast<int>(m_queue.size());

    if (m_totalItems == 0) {
        emit batchError("No valid games found in selection");
        return;
    }

    m_isRunning = true;
    emit isRunningChanged();
    emit queueChanged();

    QString opName;
    switch (type) {
        case BatchOperationType::Install: opName = tr("Toplu yükleme"); break;
        case BatchOperationType::Update: opName = tr("Toplu güncelleme"); break;
        case BatchOperationType::Remove: opName = tr("Toplu kaldırma"); break;
        default: opName = tr("Toplu işlem"); break;
    }

    m_statusMessage = tr("%1 başlatılıyor (%2 oyun)...").arg(opName).arg(m_totalItems);
    emit statusMessageChanged();

    qCDebug(lcBatchOps) << "Batch operation started:" << opName << "for" << m_totalItems << "games";

    // Start processing first item
    processNextItem();
}

void BatchOperationService::processNextItem()
{
    MAKINE_ZONE_NAMED("BatchOp::processNextItem");
    if (m_cancelRequested.load()) {
        // Mark remaining as skipped
        for (size_t i = m_currentIndex + 1; i < m_queue.size(); ++i) {
            m_queue[i].status = BatchItemStatus::Skipped;
            m_results.push_back(m_queue[i]);
            m_skippedItems++;
        }
        finishBatch();
        return;
    }

    m_currentIndex++;
    if (m_currentIndex >= static_cast<int>(m_queue.size())) {
        finishBatch();
        return;
    }

    auto& item = m_queue[m_currentIndex];
    item.status = BatchItemStatus::InProgress;

    m_currentGameId = item.gameId;
    m_currentGameName = item.gameName;
    m_currentItemProgress = 0;
    emit currentItemChanged();
    emit itemProgressChanged();
    emit queueChanged();

    m_statusMessage = QString("%1/%2: %3")
        .arg(m_currentIndex + 1)
        .arg(m_totalItems)
        .arg(item.gameName);
    emit statusMessageChanged();

    // Delegate to CoreBridge based on operation type
    if (!m_coreBridge) {
        onItemCompleted(item.gameId, false, "CoreBridge not available");
        return;
    }

    // Disconnect previous item connections before creating new ones
    disconnectCurrentItem();

    // Connect to CoreBridge completion signal for this item
    m_itemCompletionConn = connect(m_coreBridge, &CoreBridge::packageInstallCompleted,
        this, [this, gameId = item.gameId](bool success, const QString& message) {
            disconnectCurrentItem();
            onItemCompleted(gameId, success, message);
        });

    m_itemProgressConn = connect(m_coreBridge, &CoreBridge::packageInstallProgress,
        this, [this](double progress, const QString& status) {
            onItemProgress(progress, status);
        });

    switch (m_currentOperation) {
        case BatchOperationType::Install:
        case BatchOperationType::Update:
            m_coreBridge->installPackage(item.gameId, item.installPath);
            break;
        case BatchOperationType::Remove:
            // Remove not yet in CoreBridge — complete immediately
            disconnectCurrentItem();
            onItemCompleted(item.gameId, false, "Remove not yet implemented");
            break;
        default:
            disconnectCurrentItem();
            onItemCompleted(item.gameId, false, "Unknown operation");
            break;
    }
}

void BatchOperationService::onItemProgress(qreal progress, const QString& status)
{
    m_currentItemProgress = progress;
    emit itemProgressChanged();
    emit progressChanged();

    if (!status.isEmpty()) {
        m_statusMessage = QString("%1/%2: %3 — %4")
            .arg(m_currentIndex + 1)
            .arg(m_totalItems)
            .arg(m_currentGameName)
            .arg(status);
        emit statusMessageChanged();
    }
}

void BatchOperationService::onItemCompleted(const QString& gameId, bool success, const QString& message)
{
    MAKINE_ZONE_NAMED("BatchOp::onItemCompleted");
    if (m_currentIndex < 0 || m_currentIndex >= static_cast<int>(m_queue.size())) return;

    auto& item = m_queue[m_currentIndex];
    if (item.gameId != gameId) return;

    if (success) {
        item.status = BatchItemStatus::Completed;
        m_completedItems++;
    } else {
        item.status = BatchItemStatus::Failed;
        item.errorMessage = message;
        m_failedItems++;
        m_completedItems++;
    }

    item.progress = 1.0;
    m_currentItemProgress = 1.0;

    m_results.push_back(item);

    emit progressChanged();
    emit resultsChanged();
    emit gameCompleted(gameId, success, message);

    qCDebug(lcBatchOps) << "Batch item" << (success ? "completed" : "failed") << ":" << item.gameName;

    // Yield to event loop (process pending UI updates) then continue
    QTimer::singleShot(0, this, &BatchOperationService::processNextItem);
}

void BatchOperationService::finishBatch()
{
    MAKINE_ZONE_NAMED("BatchOp::finishBatch");
    int succeeded = 0;
    int failed = 0;
    int skipped = 0;

    for (const auto& item : m_results) {
        switch (item.status) {
            case BatchItemStatus::Completed: succeeded++; break;
            case BatchItemStatus::Failed: failed++; break;
            case BatchItemStatus::Skipped: skipped++; break;
            default: break;
        }
    }

    m_isRunning = false;
    m_currentGameId.clear();
    m_currentGameName.clear();

    m_statusMessage = tr("Tamamlandı: %1 başarılı, %2 başarısız, %3 atlandı")
        .arg(succeeded).arg(failed).arg(skipped);
    emit statusMessageChanged();

    emit isRunningChanged();
    emit currentItemChanged();
    emit batchCompleted(succeeded, failed, skipped);

    qCDebug(lcBatchOps) << "Batch operation completed:" << succeeded << "succeeded,"
             << failed << "failed," << skipped << "skipped";
}

void BatchOperationService::cancel()
{
    if (!m_isRunning.load()) return;

    m_cancelRequested = true;
    m_statusMessage = tr("İptal ediliyor...");
    emit statusMessageChanged();

    qCDebug(lcBatchOps) << "Batch operation cancel requested";
}

void BatchOperationService::clearResults()
{
    if (m_isRunning.load()) return;

    m_results.clear();
    m_queue.clear();
    m_totalItems = 0;
    m_completedItems = 0;
    m_failedItems = 0;
    m_skippedItems = 0;

    emit resultsChanged();
    emit queueChanged();
    emit progressChanged();
}

} // namespace makine
