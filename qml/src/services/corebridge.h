/**
 * @file corebridge.h
 * @brief Bridge between QML services and C++ Core library
 * @copyright (c) 2026 MakineAI Team
 */

#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent>

#include <optional>
#include <memory>

namespace makineai {

class LocalPackageManager;

/**
 * @brief Detected game info from core scanners
 */
struct DetectedGame {
    QString id;
    QString name;
    QString installPath;
    QString source;        // steam, epic, gog, manual
    QString engine;        // Unity, Unreal, RenPy, RPGMaker, GameMaker
    QString steamAppId;
    QString headerImageUrl;
    bool isVerified{false};
    bool hasTranslation{false};
    QString translationStatus;
};

/**
 * @brief Translation entry from core handlers
 */
struct TranslationEntryQt {
    QString filePath;
    QString entryKey;
    QString sourceText;
    QString targetText;
    QString context;
    QString category;
    int qaScore{100};
    bool hasIssues{false};
    int lineNumber{0};
};

/**
 * @brief Translation Memory match from core
 */
struct TMMatchQt {
    QString sourceText;
    QString targetText;
    double similarity;
    QString matchType;     // exact, nearExact, fuzzy, poor
    QString gameId;
    QString context;
    int qualityScore{100};
    bool verified{false};
};

/**
 * @brief Glossary term from core
 */
struct GlossaryTermQt {
    qint64 id;
    QString termSource;
    QString termTarget;
    QString termType;
    QString domain;
    bool caseSensitive{false};
    bool exactMatch{false};
    int priority{50};
    QString notes;
    bool doNotTranslate{false};
};

/**
 * @brief QA issue from core
 */
struct QAIssueQt {
    QString code;
    QString message;
    QString severity;      // info, warning, major, critical
    int penaltyPoints;
};

/**
 * @brief QA result from core
 */
struct QAResultQt {
    int score{100};
    bool passed{true};
    bool hasCriticalIssues{false};
    QList<QAIssueQt> issues;
};

/**
 * @brief Translation package info from core
 */
struct TranslationPackageQt {
    QString packageId;
    QString gameId;
    QString gameName;
    QString version;
    QString downloadUrl;
    qint64 sizeBytes;
    bool requiresRuntime{false};
};

/**
 * @brief Core Bridge - Interface to C++ Core Library
 *
 * Provides async operations for:
 * - Game library scanning (Steam, Epic, GOG)
 * - Engine detection
 * - String extraction from game files
 * - Translation application
 * - Backup/restore operations
 */
class CoreBridge : public QObject
{
    Q_OBJECT

public:
    explicit CoreBridge(QObject *parent = nullptr);
    ~CoreBridge() override;

    // Singleton access
    static CoreBridge* instance();

    // ========== Game Detection ==========

    /**
     * @brief Scan all game libraries asynchronously
     */
    void scanAllLibraries();

    /**
     * @brief Scan Steam library only
     */
    void scanSteamLibrary();

    /**
     * @brief Scan Epic Games library only
     */
    void scanEpicLibrary();

    /**
     * @brief Scan GOG Galaxy library only
     */
    void scanGogLibrary();

    /**
     * @brief Detect engine for a specific game directory
     */
    QString detectEngine(const QString& gamePath);

    /**
     * @brief Get detected games list
     */
    QList<DetectedGame> detectedGames() const { return m_detectedGames; }

    // ========== Translation Operations ==========

    /**
     * @brief Extract strings from game files
     */
    void extractStrings(const QString& gamePath, const QString& engine);

    /**
     * @brief Apply translations to game files
     */
    void applyTranslations(const QString& gamePath, const QString& engine,
                          const QList<TranslationEntryQt>& translations);

    /**
     * @brief Create backup of game files
     */
    QString createBackup(const QString& gamePath, const QString& engine);

    /**
     * @brief Restore game files from backup
     */
    bool restoreBackup(const QString& gamePath, const QString& engine,
                       const QString& backupId);

    // ========== Extracted Strings ==========

    /**
     * @brief Get extracted strings
     */
    QList<TranslationEntryQt> extractedStrings() const { return m_extractedStrings; }

    // ========== Translation Memory ==========

    /**
     * @brief Find fuzzy matches in Translation Memory
     */
    QList<TMMatchQt> findTMMatches(
        const QString& sourceText,
        const QString& gameId = QString(),
        const QString& engineType = QString(),
        int limit = 5,
        double minScore = 40.0
    );

    /**
     * @brief Find best TM match for text
     */
    std::optional<TMMatchQt> findBestTMMatch(
        const QString& sourceText,
        const QString& gameId = QString(),
        double minScore = 40.0
    );

    /**
     * @brief Add entry to Translation Memory
     */
    bool addTMEntry(
        const QString& sourceText,
        const QString& targetText,
        const QString& gameId = QString(),
        const QString& context = QString()
    );

    /**
     * @brief Batch find TM matches for multiple strings
     */
    void findBatchTMMatches(
        const QStringList& sourceTexts,
        const QString& gameId = QString(),
        double minScore = 40.0
    );

    /**
     * @brief Clear all Translation Memory entries
     */
    void clearTM();

    // ========== Glossary ==========

    /**
     * @brief Get all glossary terms
     */
    QList<GlossaryTermQt> getAllGlossaryTerms();

    /**
     * @brief Get glossary terms for a specific game
     */
    QList<GlossaryTermQt> getGlossaryTermsForGame(const QString& gameId);

    /**
     * @brief Apply glossary to text
     */
    QString applyGlossary(
        const QString& text,
        const QString& gameId = QString()
    );

    /**
     * @brief Find glossary terms in text
     */
    QList<GlossaryTermQt> findTermsInText(
        const QString& text,
        const QString& gameId = QString()
    );

    /**
     * @brief Clear all glossary terms
     */
    void clearGlossary();

    // ========== QA Service ==========

    /**
     * @brief Perform QA check on translation
     */
    QAResultQt performQACheck(
        const QString& sourceText,
        const QString& targetText,
        const QString& gameId = QString(),
        bool checkGlossary = false
    );

    /**
     * @brief Batch QA check
     */
    void performBatchQA(
        const QList<QPair<QString, QString>>& entries,
        const QString& gameId = QString()
    );

    // ========== Package Manager ==========

    /**
     * @brief Check if translation package exists for game
     */
    bool hasTranslationPackage(const QString& gameId);

    /**
     * @brief Get translation package info for game
     */
    std::optional<TranslationPackageQt> getPackageForGame(const QString& gameId);

    /**
     * @brief Download and install translation package
     */
    void installPackage(const QString& packageId, const QString& gamePath);

    /**
     * @brief Check if package is installed for game
     */
    bool isPackageInstalled(const QString& gameId);

    /**
     * @brief Uninstall translation package
     */
    bool uninstallPackage(const QString& gameId, const QString& gamePath);

    /**
     * @brief Refresh package manifest from server
     */
    void refreshPackageManifest();

signals:
    // Scanning signals
    void scanStarted();
    void scanProgress(qreal progress, const QString& status);
    void scanCompleted(int gameCount);
    void scanError(const QString& error);
    void gameDetected(const QString& gameId, const QString& gameName);

    // Extraction signals
    void extractionStarted();
    void extractionProgress(qreal progress, const QString& status);
    void extractionCompleted(int stringCount);
    void extractionError(const QString& error);

    // Patching signals
    void patchStarted();
    void patchProgress(qreal progress, const QString& status);
    void patchCompleted(int appliedCount);
    void patchError(const QString& error);

    // Backup signals
    void backupCreated(const QString& backupId);
    void backupRestored();
    void backupError(const QString& error);

    // TM signals
    void tmMatchFound(int index, const TMMatchQt& match);
    void tmBatchCompleted(int matchedCount, int totalCount);
    void tmEntryAdded(bool success);

    // QA signals
    void qaCheckCompleted(int index, const QAResultQt& result);
    void qaBatchCompleted(int passedCount, int totalCount, double avgScore);

    // Package signals
    void packageManifestRefreshed(int packageCount);
    void packageDownloadProgress(qreal progress, const QString& status);
    void packageInstalled(const QString& packageId);
    void packageInstallError(const QString& error);
    void packageInstallProgress(double progress, const QString& status);
    void packageInstallCompleted(bool success, const QString& message);

private:
#ifndef MAKINEAI_UI_ONLY
    void doScanSteam();
    void doScanEpic();
    void doScanGog();
    void doExtractStrings(const QString& gamePath, const QString& engine);
    void doApplyTranslations(const QString& gamePath, const QString& engine,
                            const QList<TranslationEntryQt>& translations);
    // TM helpers
    TMMatchQt convertTMMatch(const struct TMMatch& match);

    // Glossary helpers
    GlossaryTermQt convertGlossaryTerm(const struct GlossaryTerm& term);

    // QA helpers
    QAResultQt convertQAResult(const struct QAResult& result);
    QAIssueQt convertQAIssue(const struct QAIssue& issue);

    // Package helpers
    TranslationPackageQt convertPackage(const struct TranslationPackage& pkg);
#endif

    static CoreBridge* s_instance;
    QList<DetectedGame> m_detectedGames;
    QList<TranslationEntryQt> m_extractedStrings;
#ifdef MAKINEAI_UI_ONLY
    // Real scanning helpers for UI_ONLY build (pure Qt, no vcpkg deps)
    void doScanSteamReal();
    void doScanEpicReal();
    void doScanGogReal();
    QString detectEngineReal(const QString& gamePath);
    LocalPackageManager* m_localPkgManager{nullptr};
#else
    std::unique_ptr<class PackageManager> m_packageManager;
    bool m_packageManagerInitialized{false};
#endif
};

} // namespace makineai
