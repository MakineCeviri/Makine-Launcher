/**
 * @file translationservice.h
 * @brief Translation workflow backend service
 * @copyright (c) 2026 MakineAI Team
 */

#pragma once

#include <QObject>
#include <QString>
#include <QQmlEngine>

#include <atomic>

#include "corebridge.h"
#include "../makineai_metatypes.h"

namespace makineai {

/**
 * @brief Translation Service - Manages translation process
 *
 * Provides:
 * - Translation initiation and control
 * - Progress tracking
 * - Phase management
 * - Recipe handling
 */
class TranslationService : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool isActive READ isActive NOTIFY isActiveChanged)
    Q_PROPERTY(bool isPaused READ isPaused NOTIFY isPausedChanged)
    Q_PROPERTY(bool isProcessing READ isProcessing NOTIFY phaseChanged)
    Q_PROPERTY(int phase READ phase NOTIFY phaseChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString activeGameId READ activeGameId NOTIFY activeGameChanged)
    Q_PROPERTY(QString activeGameName READ activeGameName NOTIFY activeGameChanged)

public:
    explicit TranslationService(QObject *parent = nullptr);
    ~TranslationService() override;

    static TranslationService* create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    // Properties
    bool isActive() const { return m_isActive.load(std::memory_order_relaxed); }
    bool isPaused() const { return m_isPaused.load(std::memory_order_relaxed); }
    bool isProcessing() const { return static_cast<int>(m_phase) >= 1 && static_cast<int>(m_phase) <= 5; }
    int phase() const { return static_cast<int>(m_phase); }
    qreal progress() const { return m_progress; }
    QString statusMessage() const { return m_statusMessage; }
    QString activeGameId() const { return m_activeGameId; }
    QString activeGameName() const { return m_activeGameName; }

    // Q_INVOKABLE methods for QML
    Q_INVOKABLE void startTranslation(const QString& gameId, const QString& gameName, const QString& installPath);
    Q_INVOKABLE void stopTranslation();
    Q_INVOKABLE void pauseTranslation();
    Q_INVOKABLE void resumeTranslation();
    Q_INVOKABLE void quickTranslate(const QString& gameId, const QString& gameName, const QString& installPath);

signals:
    void isActiveChanged();
    void isPausedChanged();
    void phaseChanged();
    void progressChanged();
    void statusMessageChanged();
    void activeGameChanged();
    void translationStarted(const QString& gameId);
    void translationCompleted(const QString& gameId);
    void translationError(const QString& gameId, const QString& error);
    void matchingCompleted(int matched, int total);
    void qaCompleted(int passed, int failed, int avgScore, const QVariantList& issues);
    void tmMatchFound(const QString& source, const QString& target, double similarity);

private:
    void setPhase(TranslationPhase phase);
    void setProgress(qreal progress, const QString& message);
    void setupCoreBridge();
    void onExtractionProgress(qreal progress, const QString& status);
    void onExtractionCompleted(int count);
    void onPatchProgress(qreal progress, const QString& status);
    void onPatchCompleted(int count);

    void runMatchingAndQA(int extractedCount);

    CoreBridge* m_coreBridge{nullptr};
    std::atomic<bool> m_isActive{false};
    TranslationPhase m_phase{TranslationPhase::Idle};
    qreal m_progress{0};
    QString m_statusMessage;
    QString m_activeGameId;
    QString m_activeGameName;
    QString m_activeInstallPath;
    QString m_activeEngine;
    std::atomic<bool> m_isPaused{false};
};

} // namespace makineai
