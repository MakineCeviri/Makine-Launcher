/**
 * @file translationservice.cpp
 * @brief Translation Service Implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "translationservice.h"
#include <QDebug>
#include <QThread>

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
                m_isActive = false;
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
                m_isActive = false;
                emit isActiveChanged();
            });
}

void TranslationService::startTranslation(const QString& gameId, const QString& gameName, const QString& installPath)
{
    if (m_isActive) {
        qWarning() << "Translation already in progress";
        return;
    }

    m_activeGameId = gameId;
    m_activeGameName = gameName;
    m_activeInstallPath = installPath;
    m_isActive = true;
    m_isPaused = false;

    emit activeGameChanged();
    emit isActiveChanged();
    emit translationStarted(gameId);

    // Validate CoreBridge
    if (!m_coreBridge) {
        setPhase(TranslationPhase::Error);
        setProgress(0, "Sistem hatası: Core bağlantısı yok");
        emit translationError(gameId, "Core bağlantısı kurulamadı");
        m_isActive = false;
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
        m_isActive = false;
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
        m_isActive = false;
        emit isActiveChanged();
        return;
    }

    // Phase 3: Matching translations (from Translation Memory)
    setPhase(TranslationPhase::Matching);
    setProgress(0, "Çeviriler eşleştiriliyor...");

    // Get extracted strings and apply translations from TM
    auto extractedStrings = m_coreBridge->extractedStrings();
    QList<TranslationEntryQt> translatedStrings;

    int matched = 0;
    int total = extractedStrings.count();
    const double MIN_TM_SCORE = 70.0;  // Minimum fuzzy match score

    for (int i = 0; i < total; ++i) {
        // Check for pause state
        while (m_isPaused && m_isActive) {
            QThread::msleep(100);  // Wait 100ms while paused
        }

        // Check if stopped during pause
        if (!m_isActive) {
            return;
        }

        auto& entry = extractedStrings[i];

        if (!entry.sourceText.isEmpty()) {
            // Query Translation Memory for matching translation
            auto tmMatch = m_coreBridge->findBestTMMatch(
                entry.sourceText,
                m_activeGameId,
                MIN_TM_SCORE
            );

            if (tmMatch.has_value()) {
                // Found a TM match - use it
                entry.targetText = tmMatch->targetText;
                entry.qaScore = tmMatch->qualityScore;
                matched++;

                // Emit signal for UI to display match
                emit tmMatchFound(entry.sourceText, entry.targetText, tmMatch->similarity);
            } else {
                // No TM match - apply glossary terms if any
                QString glossaryApplied = m_coreBridge->applyGlossary(entry.sourceText, m_activeGameId);
                if (glossaryApplied != entry.sourceText) {
                    // Glossary terms were applied
                    entry.targetText = glossaryApplied;
                } else {
                    // No translation available - keep source (will need manual translation)
                    entry.targetText = entry.sourceText;
                }
            }

            translatedStrings.append(entry);
        }

        // Update progress every 10 entries or at milestones
        if (i % 10 == 0 || i == total - 1) {
            qreal progress = static_cast<qreal>(i + 1) / total;
            setProgress(progress, QString("Eşleştiriliyor: %1/%2 (%3 bulundu)")
                .arg(i + 1).arg(total).arg(matched));
        }
    }

    qDebug() << "TM Matching completed:" << matched << "/" << total << "translations found";
    emit matchingCompleted(matched, total);

    // Phase 4: Reviewing (QA check)
    setPhase(TranslationPhase::Reviewing);
    setProgress(0, "Kalite kontrolü yapılıyor...");

    int passed = 0;
    int failed = 0;
    int qaTotal = translatedStrings.count();
    QVariantList qaIssues;  // Issue detayları listesi

    for (int i = 0; i < qaTotal; ++i) {
        // Check for pause state
        while (m_isPaused && m_isActive) {
            QThread::msleep(100);  // Wait 100ms while paused
        }

        // Check if stopped during pause
        if (!m_isActive) {
            return;
        }

        auto& entry = translatedStrings[i];

        if (!entry.targetText.isEmpty() && entry.targetText != entry.sourceText) {
            // Run QA check on translated entries
            auto qaResult = m_coreBridge->performQACheck(
                entry.sourceText,
                entry.targetText,
                m_activeGameId,
                true  // checkGlossary
            );

            entry.qaScore = qaResult.score;
            entry.hasIssues = !qaResult.passed;

            if (qaResult.passed) {
                passed++;
            } else {
                failed++;

                // Issue detayını listeye ekle
                QVariantMap issue;
                issue["sourceText"] = entry.sourceText;
                issue["targetText"] = entry.targetText;
                issue["score"] = qaResult.score;
                issue["severity"] = qaResult.score < 30 ? "critical" : (qaResult.score < 60 ? "warning" : "info");
                qaIssues.append(issue);

                qDebug() << "QA failed for:" << entry.sourceText.left(30)
                         << "score:" << qaResult.score;
            }
        } else {
            // Untranslated entries pass QA by default
            passed++;
        }

        // Update progress
        if (i % 20 == 0 || i == qaTotal - 1) {
            qreal progress = static_cast<qreal>(i + 1) / qaTotal;
            setProgress(progress, QString("Kontrol ediliyor: %1/%2").arg(i + 1).arg(qaTotal));
        }
    }

    int avgScore = qaTotal > 0 ? (passed * 100) / qaTotal : 100;
    qDebug() << "QA completed: passed=" << passed << "failed=" << failed << "avgScore=" << avgScore << "issues:" << qaIssues.count();
    emit qaCompleted(passed, failed, avgScore, qaIssues);

    setProgress(1.0, QString("Kalite kontrolü tamamlandı (Skor: %1)").arg(avgScore));

    // Phase 5: Patching
    setPhase(TranslationPhase::Applying);
    setProgress(0, "Çeviriler uygulanıyor...");

    // Filter out entries with critical QA issues (score < 50)
    QList<TranslationEntryQt> approvedTranslations;
    for (const auto& entry : translatedStrings) {
        if (entry.qaScore >= 50) {
            approvedTranslations.append(entry);
        }
    }

    qDebug() << "Applying" << approvedTranslations.count() << "approved translations";

    // Apply translations using CoreBridge
    m_coreBridge->applyTranslations(m_activeInstallPath, m_activeEngine, approvedTranslations);
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

    // Keep active for a moment then reset
    m_isActive = false;
    emit isActiveChanged();
}

void TranslationService::stopTranslation()
{
    if (!m_isActive) return;

    m_isActive = false;
    m_isPaused = false;
    m_phase = TranslationPhase::Idle;
    m_progress = 0;
    m_statusMessage.clear();

    emit isActiveChanged();
    emit phaseChanged();
    emit progressChanged();
    emit statusMessageChanged();
}

void TranslationService::pauseTranslation()
{
    if (!m_isActive || m_isPaused) return;
    m_isPaused = true;
    setProgress(m_progress, m_statusMessage + " (Duraklatıldı)");
    emit isPausedChanged();
}

void TranslationService::resumeTranslation()
{
    if (!m_isActive || !m_isPaused) return;
    m_isPaused = false;
    // Remove "(Duraklatıldı)" suffix if present
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
