/**
 * @file translationservice.cpp
 * @brief Translation Service Implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "translationservice.h"
#include <QDebug>
#include <QThread>
#include <QtConcurrent>

namespace makineai {

TranslationService::TranslationService(QObject *parent)
    : QObject(parent)
{
    setupCoreBridge();
}

TranslationService::~TranslationService() = default;

TranslationService* TranslationService::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    return new TranslationService();
}

void TranslationService::setupCoreBridge()
{
    m_coreBridge = CoreBridge::instance();
    if (!m_coreBridge) {
        qCritical() << "Failed to get CoreBridge instance!";
        return;
    }

    // Extraction signals
    connect(m_coreBridge, &CoreBridge::extractionProgress,
            this, &TranslationService::onExtractionProgress);
    connect(m_coreBridge, &CoreBridge::extractionCompleted,
            this, &TranslationService::onExtractionCompleted);
    connect(m_coreBridge, &CoreBridge::extractionError,
            this, [this](const QString& error) {
                setPhase(TranslationPhase::Error);
                setProgress(0, error);
                emit translationError(m_activeGameId, error);
                m_isActive.store(false, std::memory_order_relaxed);
                emit isActiveChanged();
            });

    // Patching signals
    connect(m_coreBridge, &CoreBridge::patchProgress,
            this, &TranslationService::onPatchProgress);
    connect(m_coreBridge, &CoreBridge::patchCompleted,
            this, &TranslationService::onPatchCompleted);
    connect(m_coreBridge, &CoreBridge::patchError,
            this, [this](const QString& error) {
                setPhase(TranslationPhase::Error);
                setProgress(0, error);
                emit translationError(m_activeGameId, error);
                m_isActive.store(false, std::memory_order_relaxed);
                emit isActiveChanged();
            });
}

void TranslationService::startTranslation(const QString& gameId, const QString& gameName, const QString& installPath)
{
    if (m_isActive.load(std::memory_order_relaxed)) {
        qWarning() << "Translation already in progress";
        return;
    }

    m_activeGameId = gameId;
    m_activeGameName = gameName;
    m_activeInstallPath = installPath;
    m_isActive.store(true, std::memory_order_relaxed);
    m_isPaused.store(false, std::memory_order_relaxed);

    emit activeGameChanged();
    emit isActiveChanged();
    emit translationStarted(gameId);

    // Validate CoreBridge
    if (!m_coreBridge) {
        setPhase(TranslationPhase::Error);
        setProgress(0, "Sistem hatası: Core bağlantısı yok");
        emit translationError(gameId, "Core bağlantısı kurulamadı");
        m_isActive.store(false, std::memory_order_relaxed);
        emit isActiveChanged();
        return;
    }

    // Phase 1: Detecting engine
    setPhase(TranslationPhase::Detecting);
    setProgress(0, "Oyun motoru tespit ediliyor...");

    // Detect engine using CoreBridge
    m_activeEngine = m_coreBridge->detectEngine(installPath);

    if (m_activeEngine == "Unknown") {
        setPhase(TranslationPhase::Error);
        setProgress(0, "Oyun motoru tespit edilemedi");
        emit translationError(gameId, "Desteklenmeyen oyun motoru");
        m_isActive.store(false, std::memory_order_relaxed);
        emit isActiveChanged();
        return;
    }

    qDebug() << "Detected engine:" << m_activeEngine << "for game:" << gameName;
    setProgress(1.0, QString("Motor tespit edildi: %1").arg(m_activeEngine));

    // Phase 2: Extracting strings
    setPhase(TranslationPhase::Extracting);
    setProgress(0, "Metinler çıkarılıyor...");

    // Start extraction using CoreBridge
    m_coreBridge->extractStrings(installPath, m_activeEngine);
}

void TranslationService::onExtractionProgress(qreal progress, const QString& status)
{
    if (m_phase == TranslationPhase::Extracting) {
        setProgress(progress, status);
    }
}

void TranslationService::onExtractionCompleted(int count)
{
    qDebug() << "Extraction completed:" << count << "strings";

    if (count == 0) {
        setPhase(TranslationPhase::Error);
        setProgress(0, "Çevrilecek metin bulunamadı");
        emit translationError(m_activeGameId, "Oyunda çevrilecek metin bulunamadı");
        m_isActive.store(false, std::memory_order_relaxed);
        emit isActiveChanged();
        return;
    }

    // Launch matching and QA on worker thread to keep UI responsive
    setPhase(TranslationPhase::Matching);
    setProgress(0, "Çeviriler eşleştiriliyor...");

    (void)QtConcurrent::run([this, count]() {
        runMatchingAndQA(count);
    });
}

void TranslationService::runMatchingAndQA(int /*extractedCount*/)
{
    // --- Phase 3: Matching translations (TM) --- (runs on worker thread)
    auto extractedStrings = m_coreBridge->extractedStrings();
    QList<TranslationEntryQt> translatedStrings;

    int matched = 0;
    int total = extractedStrings.count();
    const double MIN_TM_SCORE = 70.0;

    for (int i = 0; i < total; ++i) {
        // Cooperative pause: yield while paused
        while (m_isPaused.load(std::memory_order_relaxed) &&
               m_isActive.load(std::memory_order_relaxed)) {
            QThread::msleep(50);
        }
        if (!m_isActive.load(std::memory_order_relaxed)) return;

        auto& entry = extractedStrings[i];

        if (!entry.sourceText.isEmpty()) {
            auto tmMatch = m_coreBridge->findBestTMMatch(
                entry.sourceText, m_activeGameId, MIN_TM_SCORE);

            if (tmMatch.has_value()) {
                entry.targetText = tmMatch->targetText;
                entry.qaScore = tmMatch->qualityScore;
                matched++;

                QMetaObject::invokeMethod(this, [this, src = entry.sourceText,
                        tgt = tmMatch->targetText, sim = tmMatch->similarity]() {
                    emit tmMatchFound(src, tgt, sim);
                }, Qt::QueuedConnection);
            } else {
                QString glossaryApplied = m_coreBridge->applyGlossary(
                    entry.sourceText, m_activeGameId);
                entry.targetText = (glossaryApplied != entry.sourceText)
                    ? glossaryApplied : entry.sourceText;
            }
            translatedStrings.append(entry);
        }

        if (i % 10 == 0 || i == total - 1) {
            const qreal prog = static_cast<qreal>(i + 1) / total;
            const int m = matched;
            QMetaObject::invokeMethod(this, [this, prog, i, total, m]() {
                setProgress(prog, QString("Eşleştiriliyor: %1/%2 (%3 bulundu)")
                    .arg(i + 1).arg(total).arg(m));
            }, Qt::QueuedConnection);
        }
    }

    qDebug() << "TM Matching completed:" << matched << "/" << total << "translations found";
    QMetaObject::invokeMethod(this, [this, matched, total]() {
        emit matchingCompleted(matched, total);
        setPhase(TranslationPhase::Reviewing);
        setProgress(0, "Kalite kontrolü yapılıyor...");
    }, Qt::QueuedConnection);

    // --- Phase 4: QA check --- (still on worker thread)
    int passed = 0;
    int failed = 0;
    int qaTotal = translatedStrings.count();
    QVariantList qaIssues;

    for (int i = 0; i < qaTotal; ++i) {
        while (m_isPaused.load(std::memory_order_relaxed) &&
               m_isActive.load(std::memory_order_relaxed)) {
            QThread::msleep(50);
        }
        if (!m_isActive.load(std::memory_order_relaxed)) return;

        auto& entry = translatedStrings[i];

        if (!entry.targetText.isEmpty() && entry.targetText != entry.sourceText) {
            auto qaResult = m_coreBridge->performQACheck(
                entry.sourceText, entry.targetText, m_activeGameId, true);

            entry.qaScore = qaResult.score;
            entry.hasIssues = !qaResult.passed;

            if (qaResult.passed) {
                passed++;
            } else {
                failed++;
                QVariantMap issue;
                issue["sourceText"] = entry.sourceText;
                issue["targetText"] = entry.targetText;
                issue["score"] = qaResult.score;
                issue["severity"] = qaResult.score < 30 ? "critical"
                    : (qaResult.score < 60 ? "warning" : "info");
                qaIssues.append(issue);
            }
        } else {
            passed++;
        }

        if (i % 20 == 0 || i == qaTotal - 1) {
            const qreal prog = static_cast<qreal>(i + 1) / qaTotal;
            QMetaObject::invokeMethod(this, [this, prog, i, qaTotal]() {
                setProgress(prog, QString("Kontrol ediliyor: %1/%2")
                    .arg(i + 1).arg(qaTotal));
            }, Qt::QueuedConnection);
        }
    }

    int avgScore = qaTotal > 0 ? (passed * 100) / qaTotal : 100;
    qDebug() << "QA completed: passed=" << passed << "failed=" << failed
             << "avgScore=" << avgScore << "issues:" << qaIssues.count();

    // Filter approved translations
    QList<TranslationEntryQt> approvedTranslations;
    for (const auto& entry : translatedStrings) {
        if (entry.qaScore >= 50) {
            approvedTranslations.append(entry);
        }
    }

    // Switch to main thread for phase transitions and patching
    QMetaObject::invokeMethod(this, [this, passed, failed, avgScore, qaIssues,
            approvedTranslations = std::move(approvedTranslations)]() {
        emit qaCompleted(passed, failed, avgScore, qaIssues);
        setProgress(1.0, QString("Kalite kontrolü tamamlandı (Skor: %1)").arg(avgScore));

        // Phase 5: Patching
        setPhase(TranslationPhase::Applying);
        setProgress(0, "Çeviriler uygulanıyor...");

        qDebug() << "Applying" << approvedTranslations.count() << "approved translations";
        m_coreBridge->applyTranslations(m_activeInstallPath, m_activeEngine, approvedTranslations);
    }, Qt::QueuedConnection);
}

void TranslationService::onPatchProgress(qreal progress, const QString& status)
{
    if (m_phase == TranslationPhase::Applying) {
        setProgress(progress, status);
    }
}

void TranslationService::onPatchCompleted(int count)
{
    qDebug() << "Patching completed:" << count << "strings applied";

    // Phase 6: Completed
    setPhase(TranslationPhase::Completed);
    setProgress(1.0, QString("Çeviri tamamlandı! %1 metin uygulandı").arg(count));

    emit translationCompleted(m_activeGameId);

    m_isActive.store(false, std::memory_order_relaxed);
    emit isActiveChanged();
}

void TranslationService::stopTranslation()
{
    if (!m_isActive.load(std::memory_order_relaxed)) return;

    m_isActive.store(false, std::memory_order_relaxed);
    m_isPaused.store(false, std::memory_order_relaxed);
    m_phase = TranslationPhase::Idle;
    m_progress = 0;
    m_statusMessage.clear();

    emit isActiveChanged();
    emit isPausedChanged();
    emit phaseChanged();
    emit progressChanged();
    emit statusMessageChanged();
}

void TranslationService::pauseTranslation()
{
    if (!m_isActive.load(std::memory_order_relaxed) ||
         m_isPaused.load(std::memory_order_relaxed)) return;
    m_isPaused.store(true, std::memory_order_relaxed);
    setProgress(m_progress, m_statusMessage + " (Duraklatıldı)");
    emit isPausedChanged();
}

void TranslationService::resumeTranslation()
{
    if (!m_isActive.load(std::memory_order_relaxed) ||
        !m_isPaused.load(std::memory_order_relaxed)) return;
    m_isPaused.store(false, std::memory_order_relaxed);
    QString msg = m_statusMessage;
    if (msg.endsWith(" (Duraklatıldı)")) {
        msg = msg.left(msg.length() - 15);
        setProgress(m_progress, msg);
    }
    emit isPausedChanged();
}

void TranslationService::quickTranslate(const QString& gameId, const QString& gameName, const QString& installPath)
{
    // Quick translate is the same as regular translation for now
    // In the future, this could skip certain steps (like QA review)
    startTranslation(gameId, gameName, installPath);
}

void TranslationService::setPhase(TranslationPhase phase)
{
    if (m_phase != phase) {
        m_phase = phase;
        emit phaseChanged();
    }
}

void TranslationService::setProgress(qreal progress, const QString& message)
{
    m_progress = qBound(0.0, progress, 1.0);
    m_statusMessage = message;
    emit progressChanged();
    emit statusMessageChanged();
}

} // namespace makineai
