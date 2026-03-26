/**
 * @file batchoperationservice.h
 * @brief Batch translation operations service
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Manages batch install, update, and remove operations for
 * multiple games. Provides queue management, per-game progress,
 * and overall progress tracking.
 */

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>
#include <QVariantMap>
#include <QTimer>

#include <atomic>
#include <vector>

#include "corebridge.h"

namespace makine {

/**
 * @brief Batch operation type
 */
enum class BatchOperationType {
    Install,
    Update,
    Remove,
    CheckUpdates
};

/**
 * @brief Status of a single item in the batch queue
 */
enum class BatchItemStatus {
    Pending,
    InProgress,
    Completed,
    Failed,
    Skipped
};

/**
 * @brief Batch Operation Service - manages multi-game operations
 */
class BatchOperationService : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isRunning READ isRunning NOTIFY isRunningChanged)
    Q_PROPERTY(int totalItems READ totalItems NOTIFY queueChanged)
    Q_PROPERTY(int completedItems READ completedItems NOTIFY progressChanged)
    Q_PROPERTY(int failedItems READ failedItems NOTIFY progressChanged)
    Q_PROPERTY(qreal overallProgress READ overallProgress NOTIFY progressChanged)
    Q_PROPERTY(QString currentGameName READ currentGameName NOTIFY currentItemChanged)
    Q_PROPERTY(QString currentGameId READ currentGameId NOTIFY currentItemChanged)
    Q_PROPERTY(qreal currentItemProgress READ currentItemProgress NOTIFY itemProgressChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QVariantList queue READ queue NOTIFY queueChanged)
    Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)
    Q_PROPERTY(bool canCancel READ canCancel NOTIFY isRunningChanged)

public:
    explicit BatchOperationService(QObject *parent = nullptr);
    ~BatchOperationService() override;

    static BatchOperationService* create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    // Properties
    bool isRunning() const { return m_isRunning.load(); }
    int totalItems() const { return m_totalItems; }
    int completedItems() const { return m_completedItems; }
    int failedItems() const { return m_failedItems; }
    qreal overallProgress() const;
    QString currentGameName() const { return m_currentGameName; }
    QString currentGameId() const { return m_currentGameId; }
    qreal currentItemProgress() const { return m_currentItemProgress; }
    QString statusMessage() const { return m_statusMessage; }
    QVariantList queue() const;
    QVariantList results() const;
    bool canCancel() const { return m_isRunning.load(); }

    /**
     * @brief Cancel the current batch operation
     */
    Q_INVOKABLE void cancel();

    /**
     * @brief Clear completed results
     */
    Q_INVOKABLE void clearResults();

signals:
    void isRunningChanged();
    void queueChanged();
    void progressChanged();
    void currentItemChanged();
    void itemProgressChanged();
    void statusMessageChanged();
    void resultsChanged();
    void batchCompleted(int succeeded, int failed, int skipped);
    void batchError(const QString& error);
    void gameCompleted(const QString& gameId, bool success, const QString& message);

private:
    struct QueueItem {
        QString gameId;
        QString gameName;
        QString installPath;
        BatchItemStatus status{BatchItemStatus::Pending};
        QString errorMessage;
        qreal progress{0};
    };

    void startBatch(BatchOperationType type, const QVariantList& gameIds);
    void processNextItem();
    void onItemProgress(qreal progress, const QString& status);
    void onItemCompleted(const QString& gameId, bool success, const QString& message);
    void finishBatch();

    void disconnectCurrentItem();

    CoreBridge* m_coreBridge{nullptr};
    QMetaObject::Connection m_itemCompletionConn;
    QMetaObject::Connection m_itemProgressConn;
    std::vector<QueueItem> m_queue;
    std::vector<QueueItem> m_results;
    BatchOperationType m_currentOperation{BatchOperationType::Install};
    int m_currentIndex{-1};

    std::atomic<bool> m_isRunning{false};
    std::atomic<bool> m_cancelRequested{false};
    int m_totalItems{0};
    int m_completedItems{0};
    int m_failedItems{0};
    int m_skippedItems{0};
    qreal m_currentItemProgress{0};
    QString m_currentGameId;
    QString m_currentGameName;
    QString m_statusMessage;
};

} // namespace makine
