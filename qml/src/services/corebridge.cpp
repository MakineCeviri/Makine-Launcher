/**
 * @file corebridge.cpp
 * @brief Core Bridge Implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "corebridge.h"

#include <QDebug>
#include <QDir>
#include <QThread>
#include <QStandardPaths>
#include <QSet>

#ifndef MAKINEAI_UI_ONLY
#include <makineai/core.hpp>
#include <makineai/game_detector.hpp>
#include <makineai/handlers/engine_handler.hpp>
#include <makineai/handlers/unity_handler.hpp>
#include <makineai/handlers/unreal_handler.hpp>
#include <makineai/handlers/renpy_handler.hpp>
#include <makineai/handlers/rpgmaker_handler.hpp>
#include <makineai/handlers/gamemaker_handler.hpp>
#include <makineai/translation_memory.hpp>
#include <makineai/glossary_service.hpp>
#include <makineai/qa_service.hpp>
#include <makineai/package_manager.hpp>
#endif

namespace makineai {

CoreBridge* CoreBridge::s_instance = nullptr;

#ifndef MAKINEAI_UI_ONLY
static bool s_coreInitialized = false;

// Initialize Core singleton on first use
// Returns true if Core is ready to use, false otherwise
static bool ensureCoreInitialized() {
    if (s_coreInitialized) return true;

    try {
        auto& core = Core::instance();
        if (!core.isInitialized()) {
            qDebug() << "Initializing MakineAI Core...";
            auto result = core.initialize();
            if (result) {
                qDebug() << "Core initialized successfully in" << result->initDuration.count() << "ms";
                s_coreInitialized = true;
                return true;
            } else {
                qCritical() << "Core initialization FAILED:"
                           << QString::fromStdString(result.error().message());
                return false;
            }
        } else {
            s_coreInitialized = true;
            return true;
        }
    } catch (const std::exception& e) {
        qCritical() << "Core initialization threw exception:" << e.what();
        return false;
    } catch (...) {
        qCritical() << "Core initialization threw unknown exception";
        return false;
    }
}
#endif

CoreBridge::CoreBridge(QObject *parent)
    : QObject(parent)
{
    s_instance = this;

#ifndef MAKINEAI_UI_ONLY
    // Initialize Core on construction
    ensureCoreInitialized();
#endif
}

CoreBridge::~CoreBridge()
{
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

CoreBridge* CoreBridge::instance()
{
    if (!s_instance) {
        s_instance = new CoreBridge();
    }
    return s_instance;
}

#ifdef MAKINEAI_UI_ONLY

// ========== STUB IMPLEMENTATIONS FOR UI-ONLY BUILD ==========

void CoreBridge::scanAllLibraries()
{
    emit scanStarted();
    m_detectedGames.clear();

    // Add verified games for UI testing
    (void)QtConcurrent::run([this]() {
        emit scanProgress(0.1, "Onaylı oyunlar yükleniyor...");
        QThread::msleep(200);

        // Verified games list (30 games)
        struct VerifiedGame {
            QString steamAppId;
            QString name;
        };

        QList<VerifiedGame> verifiedGames = {
            {"1174180", "Red Dead Redemption 2"},
            {"1245620", "Elden Ring"},
            {"3159330", "Assassin's Creed Shadows"},
            {"2208920", "Assassin's Creed Valhalla"},
            {"2358720", "Black Myth: Wukong"},
            {"3035570", "Assassin's Creed Mirage"},
            {"582160", "Assassin's Creed Origins"},
            {"812140", "Assassin's Creed Odyssey"},
            {"1716740", "Starfield"},
            {"3611110", "Alan Wake 2"},
            {"949230", "Cities: Skylines II"},
            {"208650", "Batman: Arkham Knight"},
            {"1708010", "The Expanse: A Telltale Series"},
            {"990080", "Hogwarts Legacy"},
            {"2221920", "Immortals Fenyx Rising"},
            {"1123770", "Curse of the Dead Gods"},
            {"668580", "Atomic Heart"},
            {"373420", "Divinity: Original Sin Enhanced Edition"},
            {"291650", "Pillars of Eternity"},
            {"1065310", "Evil West"},
            {"435150", "Divinity: Original Sin 2"},
            {"1238000", "Mass Effect: Andromeda"},
            {"1151340", "Fallout 76"},
            {"1313140", "Cult of the Lamb"},
            {"916440", "Anno 1800"},
            {"22330", "The Elder Scrolls IV: Oblivion Remastered"},
            {"1903340", "Clair Obscur: Expedition 33"},
            {"2677660", "Indiana Jones and the Great Circle"},
            {"2651280", "Marvel's Spider-Man 2"},
        };

        emit scanProgress(0.3, "Steam kütüphanesi taranıyor...");
        QThread::msleep(200);

        int count = 0;
        int total = verifiedGames.size();

        for (const auto& vg : verifiedGames) {
            DetectedGame game;
            game.id = vg.steamAppId;
            game.name = vg.name;
            game.installPath = QString("C:/Games/%1").arg(vg.name);
            game.source = "steam";
            game.steamAppId = vg.steamAppId;
            game.engine = "Unity";
            game.headerImageUrl = QString("https://cdn.akamai.steamstatic.com/steam/apps/%1/library_600x900_2x.jpg").arg(vg.steamAppId);
            game.isVerified = true;
            game.hasTranslation = true;
            m_detectedGames.append(game);

            count++;
            if (count % 5 == 0) {
                emit scanProgress(0.3 + 0.6 * (static_cast<qreal>(count) / total),
                    QString("%1 oyun yüklendi...").arg(count));
                QThread::msleep(50);
            }
        }

        emit scanProgress(1.0, QString("%1 onaylı oyun bulundu").arg(m_detectedGames.count()));
        emit scanCompleted(m_detectedGames.count());
    });
}

void CoreBridge::scanSteamLibrary()
{
    emit scanStarted();
    emit scanProgress(1.0, "Steam tarama (UI test modu)");
    emit scanCompleted(0);
}

void CoreBridge::scanEpicLibrary()
{
    emit scanStarted();
    emit scanProgress(1.0, "Epic tarama (UI test modu)");
    emit scanCompleted(0);
}

void CoreBridge::scanGogLibrary()
{
    emit scanStarted();
    emit scanProgress(1.0, "GOG tarama (UI test modu)");
    emit scanCompleted(0);
}

QString CoreBridge::detectEngine(const QString& gamePath)
{
    Q_UNUSED(gamePath)
    return "Unity";  // Default for UI testing
}

void CoreBridge::extractStrings(const QString& gamePath, const QString& engine)
{
    Q_UNUSED(gamePath)
    Q_UNUSED(engine)

    emit extractionStarted();
    m_extractedStrings.clear();

    (void)QtConcurrent::run([this]() {
        emit extractionProgress(0.2, "Dosyalar taranıyor...");
        QThread::msleep(500);

        // Sample strings for UI testing
        QStringList samples = {
            "Press any key to continue",
            "New Game",
            "Continue",
            "Settings",
            "Exit",
            "Are you sure you want to quit?",
            "Loading...",
            "Save Game",
            "Load Game"
        };

        emit extractionProgress(0.5, "Metinler işleniyor...");

        int index = 0;
        for (const auto& text : samples) {
            TranslationEntryQt entry;
            entry.entryKey = QString("key_%1").arg(index);
            entry.sourceText = text;
            entry.filePath = "strings.txt";
            entry.category = "ui";
            m_extractedStrings.append(entry);
            index++;
        }

        QThread::msleep(500);
        emit extractionProgress(1.0, QString("%1 metin çıkarıldı").arg(m_extractedStrings.count()));
        emit extractionCompleted(m_extractedStrings.count());
    });
}

void CoreBridge::applyTranslations(const QString& gamePath, const QString& engine,
                                   const QList<TranslationEntryQt>& translations)
{
    Q_UNUSED(gamePath)
    Q_UNUSED(engine)

    emit patchStarted();

    (void)QtConcurrent::run([this, translations]() {
        emit patchProgress(0.3, "Yedek oluşturuluyor...");
        QThread::msleep(500);

        emit patchProgress(0.7, "Çeviriler uygulanıyor...");
        QThread::msleep(500);

        emit patchProgress(1.0, QString("%1 çeviri uygulandı").arg(translations.count()));
        emit patchCompleted(translations.count());
    });
}

QString CoreBridge::createBackup(const QString& gamePath, const QString& engine)
{
    Q_UNUSED(gamePath)
    Q_UNUSED(engine)

    QString backupId = QString::number(QDateTime::currentMSecsSinceEpoch());
    emit backupCreated(backupId);
    return backupId;
}

bool CoreBridge::restoreBackup(const QString& gamePath, const QString& engine,
                               const QString& backupId)
{
    Q_UNUSED(gamePath)
    Q_UNUSED(engine)
    Q_UNUSED(backupId)

    emit backupRestored();
    return true;
}

QList<TMMatchQt> CoreBridge::findTMMatches(
    const QString& sourceText,
    const QString& gameId,
    const QString& engineType,
    int limit,
    double minScore)
{
    Q_UNUSED(sourceText)
    Q_UNUSED(gameId)
    Q_UNUSED(engineType)
    Q_UNUSED(limit)
    Q_UNUSED(minScore)

    return {};  // Empty for UI testing
}

std::optional<TMMatchQt> CoreBridge::findBestTMMatch(
    const QString& sourceText,
    const QString& gameId,
    double minScore)
{
    Q_UNUSED(sourceText)
    Q_UNUSED(gameId)
    Q_UNUSED(minScore)

    return std::nullopt;
}

bool CoreBridge::addTMEntry(
    const QString& sourceText,
    const QString& targetText,
    const QString& gameId,
    const QString& context)
{
    Q_UNUSED(sourceText)
    Q_UNUSED(targetText)
    Q_UNUSED(gameId)
    Q_UNUSED(context)

    emit tmEntryAdded(true);
    return true;
}

void CoreBridge::findBatchTMMatches(
    const QStringList& sourceTexts,
    const QString& gameId,
    double minScore)
{
    Q_UNUSED(sourceTexts)
    Q_UNUSED(gameId)
    Q_UNUSED(minScore)

    emit tmBatchCompleted(0, sourceTexts.size());
}

void CoreBridge::clearTM()
{
    // No-op in UI-only mode
}

QList<GlossaryTermQt> CoreBridge::getAllGlossaryTerms()
{
    return {};
}

QList<GlossaryTermQt> CoreBridge::getGlossaryTermsForGame(const QString& gameId)
{
    Q_UNUSED(gameId)
    return {};
}

QString CoreBridge::applyGlossary(const QString& text, const QString& gameId)
{
    Q_UNUSED(gameId)
    return text;
}

QList<GlossaryTermQt> CoreBridge::findTermsInText(const QString& text, const QString& gameId)
{
    Q_UNUSED(text)
    Q_UNUSED(gameId)
    return {};
}

void CoreBridge::clearGlossary()
{
    // No-op in UI-only mode
}

QAResultQt CoreBridge::performQACheck(
    const QString& sourceText,
    const QString& targetText,
    const QString& gameId,
    bool checkGlossary)
{
    Q_UNUSED(sourceText)
    Q_UNUSED(targetText)
    Q_UNUSED(gameId)
    Q_UNUSED(checkGlossary)

    QAResultQt result;
    result.score = 100;
    result.passed = true;
    result.hasCriticalIssues = false;
    return result;
}

void CoreBridge::performBatchQA(
    const QList<QPair<QString, QString>>& entries,
    const QString& gameId)
{
    Q_UNUSED(entries)
    Q_UNUSED(gameId)

    emit qaBatchCompleted(entries.size(), entries.size(), 100.0);
}

bool CoreBridge::hasTranslationPackage(const QString& gameId)
{
    // All verified games have translation packages
    static const QSet<QString> verifiedGameIds = {
        "2358720", "1245620", "1174180", "3159330", "2208920",
        "3035570", "582160", "812140", "1716740", "3611110",
        "949230", "208650", "1708010", "990080", "2221920",
        "1123770", "668580", "373420", "291650", "1065310",
        "435150", "1238000", "1151340", "1313140", "916440",
        "22330", "1903340", "2677660", "2651280"
    };
    return verifiedGameIds.contains(gameId);
}

std::optional<TranslationPackageQt> CoreBridge::getPackageForGame(const QString& gameId)
{
    if (gameId == "1245620") {
        TranslationPackageQt pkg;
        pkg.packageId = "eldenring-tr-1.0";
        pkg.gameId = "1245620";
        pkg.gameName = "Elden Ring";
        pkg.version = "1.0.0";
        pkg.sizeBytes = 52428800;  // 50 MB
        pkg.requiresRuntime = true;
        return pkg;
    }
    return std::nullopt;
}

void CoreBridge::installPackage(const QString& packageId, const QString& gamePath)
{
    Q_UNUSED(gamePath)

    (void)QtConcurrent::run([this, packageId]() {
        for (int i = 0; i <= 100; i += 10) {
            emit packageDownloadProgress(i / 100.0, QString("Kuruluyor... %1%").arg(i));
            QThread::msleep(200);
        }
        emit packageInstalled(packageId);
    });
}

bool CoreBridge::isPackageInstalled(const QString& gameId)
{
    Q_UNUSED(gameId)
    return false;
}

bool CoreBridge::uninstallPackage(const QString& gameId, const QString& gamePath)
{
    Q_UNUSED(gameId)
    Q_UNUSED(gamePath)
    return true;
}

void CoreBridge::refreshPackageManifest()
{
    emit packageManifestRefreshed(5);  // Simulate 5 packages
}

#else  // FULL BUILD WITH CORE LIBRARY

// ========== FULL IMPLEMENTATIONS WITH CORE LIBRARY ==========

void CoreBridge::scanAllLibraries()
{
    emit scanStarted();
    m_detectedGames.clear();

    (void)QtConcurrent::run([this]() {
        emit scanProgress(0.0, "Steam kütüphanesi taranıyor...");
        doScanSteam();

        emit scanProgress(0.33, "Epic Games taranıyor...");
        doScanEpic();

        emit scanProgress(0.66, "GOG Galaxy taranıyor...");
        doScanGog();

        emit scanProgress(1.0, QString("%1 oyun bulundu").arg(m_detectedGames.count()));
        emit scanCompleted(m_detectedGames.count());
    });
}

void CoreBridge::scanSteamLibrary()
{
    emit scanStarted();
    (void)QtConcurrent::run([this]() {
        emit scanProgress(0.0, "Steam kütüphanesi taranıyor...");
        doScanSteam();
        emit scanProgress(1.0, QString("%1 Steam oyunu bulundu").arg(m_detectedGames.count()));
        emit scanCompleted(m_detectedGames.count());
    });
}

void CoreBridge::scanEpicLibrary()
{
    emit scanStarted();
    (void)QtConcurrent::run([this]() {
        emit scanProgress(0.0, "Epic Games taranıyor...");
        doScanEpic();
        emit scanProgress(1.0, QString("%1 Epic oyunu bulundu").arg(m_detectedGames.count()));
        emit scanCompleted(m_detectedGames.count());
    });
}

void CoreBridge::scanGogLibrary()
{
    emit scanStarted();
    (void)QtConcurrent::run([this]() {
        emit scanProgress(0.0, "GOG Galaxy taranıyor...");
        doScanGog();
        emit scanProgress(1.0, QString("%1 GOG oyunu bulundu").arg(m_detectedGames.count()));
        emit scanCompleted(m_detectedGames.count());
    });
}

void CoreBridge::doScanSteam()
{
    // Safety guard: Ensure Core is initialized before scanning
    if (!s_coreInitialized) {
        qWarning() << "CoreBridge::doScanSteam - Core not initialized, skipping scan";
        emit scanError("Core kütüphanesi başlatılamadı. Lütfen uygulamayı yeniden başlatın.");
        return;
    }

    SteamScanner scanner;
    auto result = scanner.scan();

    if (!result) {
        qWarning() << "Steam scan failed:" << QString::fromStdString(result.error().message());
        // Don't emit error for Steam - continue silently if Steam not available
        return;
    }

    for (const auto& game : *result) {
        DetectedGame detected;
        detected.id = QString::fromStdString(game.id.storeId);
        detected.name = QString::fromStdString(game.name);
        detected.installPath = QString::fromStdWString(game.installPath.wstring());
        detected.source = "steam";
        detected.steamAppId = QString::fromStdString(game.id.storeId);

        bool ok = false;
        int appId = detected.steamAppId.toInt(&ok);
        if (ok && appId > 0) {
            detected.headerImageUrl = QString("https://cdn.akamai.steamstatic.com/steam/apps/%1/library_600x900_2x.jpg")
                .arg(appId);
        }

        detected.engine = detectEngine(detected.installPath);
        m_detectedGames.append(detected);
        emit gameDetected(detected.id, detected.name);
    }
}

void CoreBridge::doScanEpic()
{
    // Safety guard: Ensure Core is initialized before scanning
    if (!s_coreInitialized) {
        qWarning() << "CoreBridge::doScanEpic - Core not initialized, skipping scan";
        return;
    }

    EpicScanner scanner;
    auto result = scanner.scan();

    if (!result) {
        qWarning() << "Epic scan failed:" << QString::fromStdString(result.error().message());
        // Silent fail for Epic - continue with other scans
        return;
    }

    for (const auto& game : *result) {
        DetectedGame detected;
        detected.id = QString::fromStdString(game.id.storeId);
        detected.name = QString::fromStdString(game.name);
        detected.installPath = QString::fromStdWString(game.installPath.wstring());
        detected.source = "epic";
        detected.engine = detectEngine(detected.installPath);
        m_detectedGames.append(detected);
        emit gameDetected(detected.id, detected.name);
    }
}

void CoreBridge::doScanGog()
{
    // Safety guard: Ensure Core is initialized before scanning
    if (!s_coreInitialized) {
        qWarning() << "CoreBridge::doScanGog - Core not initialized, skipping scan";
        return;
    }

    GOGScanner scanner;
    auto result = scanner.scan();

    if (!result) {
        qWarning() << "GOG scan failed:" << QString::fromStdString(result.error().message());
        // Silent fail for GOG - continue with other scans
        return;
    }

    for (const auto& game : *result) {
        DetectedGame detected;
        detected.id = QString::fromStdString(game.id.storeId);
        detected.name = QString::fromStdString(game.name);
        detected.installPath = QString::fromStdWString(game.installPath.wstring());
        detected.source = "gog";
        detected.engine = detectEngine(detected.installPath);
        m_detectedGames.append(detected);
        emit gameDetected(detected.id, detected.name);
    }
}

QString CoreBridge::detectEngine(const QString& gamePath)
{
    std::filesystem::path path = gamePath.toStdWString();

    UnityHandler unityHandler;
    if (unityHandler.canHandleGame(path)) return "Unity";

    UnrealHandler unrealHandler;
    if (unrealHandler.canHandleGame(path)) return "Unreal";

    RenpyHandler renpyHandler;
    if (renpyHandler.canHandleGame(path)) return "RenPy";

    RpgMakerHandler rpgMakerHandler;
    if (rpgMakerHandler.canHandleGame(path)) return "RPGMaker";

    GameMakerHandler gameMakerHandler;
    if (gameMakerHandler.canHandleGame(path)) return "GameMaker";

    return "Unknown";
}

void CoreBridge::extractStrings(const QString& gamePath, const QString& engine)
{
    emit extractionStarted();
    m_extractedStrings.clear();
    (void)QtConcurrent::run([this, gamePath, engine]() {
        doExtractStrings(gamePath, engine);
    });
}

void CoreBridge::doExtractStrings(const QString& gamePath, const QString& engine)
{
    std::filesystem::path path = gamePath.toStdWString();
    std::unique_ptr<IEngineHandler> handler;

    if (engine == "Unity") handler = std::make_unique<UnityHandler>();
    else if (engine == "Unreal") handler = std::make_unique<UnrealHandler>();
    else if (engine == "RenPy") handler = std::make_unique<RenpyHandler>();
    else if (engine == "RPGMaker") handler = std::make_unique<RpgMakerHandler>();
    else if (engine == "GameMaker") handler = std::make_unique<GameMakerHandler>();
    else {
        emit extractionError("Desteklenmeyen motor: " + engine);
        return;
    }

    emit extractionProgress(0.1, "Dosyalar taranıyor...");

    ExtractionOptions options;
    options.minLength = 2;
    options.maxLength = 10000;

    auto result = handler->extractStrings(path, options);
    if (!result) {
        emit extractionError(QString::fromStdString(result.error().message()));
        return;
    }

    emit extractionProgress(0.5, "Metinler işleniyor...");

    const auto& extractionResult = *result;
    int processed = 0;
    int total = static_cast<int>(extractionResult.entries.size());

    for (const auto& entry : extractionResult.entries) {
        TranslationEntryQt qtEntry;
        qtEntry.filePath = QString::fromStdString(entry.filePath);
        qtEntry.entryKey = QString::fromStdString(entry.entryKey.value_or(""));
        qtEntry.sourceText = QString::fromStdString(entry.sourceText);
        qtEntry.targetText = QString::fromStdString(entry.targetText.value_or(""));
        qtEntry.context = QString::fromStdString(entry.context.value_or(""));

        if (entry.category.has_value()) {
            switch (*entry.category) {
            case EntryCategory::Dialog: qtEntry.category = "dialog"; break;
            case EntryCategory::UI: qtEntry.category = "ui"; break;
            case EntryCategory::Item: qtEntry.category = "item"; break;
            case EntryCategory::Skill: qtEntry.category = "skill"; break;
            case EntryCategory::System: qtEntry.category = "system"; break;
            case EntryCategory::Narration: qtEntry.category = "narration"; break;
            default: qtEntry.category = "other"; break;
            }
        } else {
            qtEntry.category = "other";
        }

        m_extractedStrings.append(qtEntry);
        processed++;

        if (processed % 100 == 0) {
            qreal progress = 0.5 + 0.5 * (static_cast<qreal>(processed) / total);
            emit extractionProgress(progress, QString("İşleniyor: %1/%2").arg(processed).arg(total));
        }
    }

    emit extractionProgress(1.0, QString("%1 metin çıkarıldı").arg(m_extractedStrings.count()));
    emit extractionCompleted(m_extractedStrings.count());
}

void CoreBridge::applyTranslations(const QString& gamePath, const QString& engine,
                                   const QList<TranslationEntryQt>& translations)
{
    emit patchStarted();
    (void)QtConcurrent::run([this, gamePath, engine, translations]() {
        doApplyTranslations(gamePath, engine, translations);
    });
}

void CoreBridge::doApplyTranslations(const QString& gamePath, const QString& engine,
                                     const QList<TranslationEntryQt>& translations)
{
    std::filesystem::path path = gamePath.toStdWString();
    std::unique_ptr<IEngineHandler> handler;

    if (engine == "Unity") handler = std::make_unique<UnityHandler>();
    else if (engine == "Unreal") handler = std::make_unique<UnrealHandler>();
    else if (engine == "RenPy") handler = std::make_unique<RenpyHandler>();
    else if (engine == "RPGMaker") handler = std::make_unique<RpgMakerHandler>();
    else if (engine == "GameMaker") handler = std::make_unique<GameMakerHandler>();
    else {
        emit patchError("Desteklenmeyen motor: " + engine);
        return;
    }

    emit patchProgress(0.1, "Yedek oluşturuluyor...");

    std::vector<TranslationEntry> coreTranslations;
    for (const auto& qtEntry : translations) {
        TranslationEntry entry;
        entry.filePath = qtEntry.filePath.toStdString();
        entry.entryKey = qtEntry.entryKey.toStdString();
        entry.sourceText = qtEntry.sourceText.toStdString();
        entry.targetText = qtEntry.targetText.toStdString();
        entry.context = qtEntry.context.toStdString();
        coreTranslations.push_back(entry);
    }

    emit patchProgress(0.3, "Çeviriler uygulanıyor...");

    PatchOptions options;
    options.createBackup = true;

    auto result = handler->applyTranslations(path, coreTranslations, options);
    if (!result) {
        emit patchError(QString::fromStdString(result.error().message()));
        return;
    }

    emit patchProgress(1.0, QString("%1 çeviri uygulandı").arg(result->appliedCount));
    emit patchCompleted(result->appliedCount);
}

QString CoreBridge::createBackup(const QString& gamePath, const QString& engine)
{
    std::filesystem::path path = gamePath.toStdWString();
    std::unique_ptr<IEngineHandler> handler;

    if (engine == "Unity") handler = std::make_unique<UnityHandler>();
    else if (engine == "Unreal") handler = std::make_unique<UnrealHandler>();
    else if (engine == "RenPy") handler = std::make_unique<RenpyHandler>();
    else if (engine == "RPGMaker") handler = std::make_unique<RpgMakerHandler>();
    else if (engine == "GameMaker") handler = std::make_unique<GameMakerHandler>();
    else {
        emit backupError("Desteklenmeyen motor: " + engine);
        return QString();
    }

    auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    std::string backupId = std::to_string(timestamp);
    auto result = handler->createBackup(path, backupId);

    if (!result || !result->success) {
        QString error = result ? QString::fromStdString(result->errorMessage) : "Yedek oluşturulamadı";
        emit backupError(error);
        return QString();
    }

    QString qBackupId = QString::fromStdString(result->backupId);
    emit backupCreated(qBackupId);
    return qBackupId;
}

bool CoreBridge::restoreBackup(const QString& gamePath, const QString& engine,
                               const QString& backupId)
{
    std::filesystem::path path = gamePath.toStdWString();
    std::unique_ptr<IEngineHandler> handler;

    if (engine == "Unity") handler = std::make_unique<UnityHandler>();
    else if (engine == "Unreal") handler = std::make_unique<UnrealHandler>();
    else if (engine == "RenPy") handler = std::make_unique<RenpyHandler>();
    else if (engine == "RPGMaker") handler = std::make_unique<RpgMakerHandler>();
    else if (engine == "GameMaker") handler = std::make_unique<GameMakerHandler>();
    else {
        emit backupError("Desteklenmeyen motor: " + engine);
        return false;
    }

    auto result = handler->restoreBackup(path, backupId.toStdString());
    if (!result || !result->success) {
        QString error = result ? QString::fromStdString(result->errorMessage) : "Yedek geri yüklenemedi";
        emit backupError(error);
        return false;
    }

    emit backupRestored();
    return true;
}

// ========== Translation Memory Functions ==========

namespace {

// Helper: Convert core MatchType to string
QString matchTypeToString(MatchType type) {
    switch (type) {
        case MatchType::Exact: return "exact";
        case MatchType::NearExact: return "nearExact";
        case MatchType::Fuzzy: return "fuzzy";
        case MatchType::Poor: return "poor";
        default: return "unknown";
    }
}

} // anonymous namespace

// Helper: Convert core TMMatch to Qt struct
TMMatchQt CoreBridge::convertTMMatch(const TMMatch& match) {
    TMMatchQt qt;
    qt.sourceText = QString::fromStdString(match.entry.sourceText);
    qt.targetText = QString::fromStdString(match.entry.targetText);
    qt.similarity = match.similarity;
    qt.matchType = matchTypeToString(match.matchType);
    qt.gameId = match.entry.gameId ? QString::fromStdString(*match.entry.gameId) : QString();
    qt.context = match.entry.context ? QString::fromStdString(*match.entry.context) : QString();
    qt.qualityScore = match.entry.qualityScore;
    qt.verified = match.entry.verified;
    return qt;
}

// Helper: Convert core GlossaryTerm to Qt struct
GlossaryTermQt CoreBridge::convertGlossaryTerm(const GlossaryTerm& term) {
    GlossaryTermQt qt;
    qt.id = term.id.value_or(0);
    qt.termSource = QString::fromStdString(term.termSource);
    qt.termTarget = QString::fromStdString(term.termTarget);
    qt.caseSensitive = term.caseSensitive;
    qt.exactMatch = term.exactMatch;
    qt.priority = term.priority;
    qt.notes = term.notes ? QString::fromStdString(*term.notes) : QString();
    qt.doNotTranslate = term.doNotTranslate;

    // Convert termType
    if (term.termType) {
        switch (*term.termType) {
            case TermType::Noun: qt.termType = "noun"; break;
            case TermType::Verb: qt.termType = "verb"; break;
            case TermType::UI: qt.termType = "ui"; break;
            case TermType::Item: qt.termType = "item"; break;
            case TermType::Skill: qt.termType = "skill"; break;
            case TermType::Stat: qt.termType = "stat"; break;
            case TermType::Action: qt.termType = "action"; break;
            case TermType::Place: qt.termType = "place"; break;
            case TermType::Character: qt.termType = "character"; break;
            default: qt.termType = "other"; break;
        }
    }

    // Convert domain
    if (term.domain) {
        switch (*term.domain) {
            case TermDomain::General: qt.domain = "general"; break;
            case TermDomain::RPG: qt.domain = "rpg"; break;
            case TermDomain::FPS: qt.domain = "fps"; break;
            case TermDomain::VisualNovel: qt.domain = "visual_novel"; break;
            case TermDomain::Strategy: qt.domain = "strategy"; break;
            case TermDomain::Action: qt.domain = "action"; break;
            case TermDomain::Horror: qt.domain = "horror"; break;
            default: qt.domain = "general"; break;
        }
    }

    return qt;
}

// Helper: Convert core QAIssue to Qt struct
QAIssueQt CoreBridge::convertQAIssue(const QAIssue& issue) {
    QAIssueQt qt;
    qt.code = QString::fromStdString(issue.code);
    qt.message = QString::fromStdString(issue.message);
    qt.penaltyPoints = issue.penaltyPoints;

    switch (issue.severity) {
        case QASeverity::Info: qt.severity = "info"; break;
        case QASeverity::Warning: qt.severity = "warning"; break;
        case QASeverity::Major: qt.severity = "major"; break;
        case QASeverity::Critical: qt.severity = "critical"; break;
        default: qt.severity = "info"; break;
    }

    return qt;
}

// Helper: Convert core QAResult to Qt struct
QAResultQt CoreBridge::convertQAResult(const QAResult& result) {
    QAResultQt qt;
    qt.score = result.score;
    qt.passed = result.passed;
    qt.hasCriticalIssues = result.hasCriticalIssues;

    for (const auto& issue : result.issues) {
        qt.issues.append(convertQAIssue(issue));
    }

    return qt;
}

QList<TMMatchQt> CoreBridge::findTMMatches(
    const QString& sourceText, const QString& gameId,
    const QString& engineType, int limit, double minScore)
{
    QList<TMMatchQt> results;

    // Build match context
    TranslationMemoryService::MatchContext ctx;
    if (!gameId.isEmpty()) ctx.gameId = gameId.toStdString();
    if (!engineType.isEmpty()) ctx.engineType = engineType.toStdString();

    // Call core TM service
    auto matchResult = TranslationMemoryService::findFuzzyMatches(
        sourceText.toStdString(),
        ctx,
        "tr",
        static_cast<size_t>(limit),
        minScore
    );

    if (matchResult) {
        for (const auto& match : *matchResult) {
            results.append(convertTMMatch(match));
        }
    } else {
        qWarning() << "TM findFuzzyMatches failed:" << QString::fromStdString(matchResult.error().message());
    }

    return results;
}

std::optional<TMMatchQt> CoreBridge::findBestTMMatch(
    const QString& sourceText, const QString& gameId, double minScore)
{
    TranslationMemoryService::MatchContext ctx;
    if (!gameId.isEmpty()) ctx.gameId = gameId.toStdString();

    auto matchResult = TranslationMemoryService::findBestMatch(
        sourceText.toStdString(),
        ctx,
        "tr",
        minScore
    );

    if (matchResult && matchResult->has_value()) {
        return convertTMMatch(**matchResult);
    }

    if (!matchResult) {
        qWarning() << "TM findBestMatch failed:" << QString::fromStdString(matchResult.error().message());
    }

    return std::nullopt;
}

bool CoreBridge::addTMEntry(const QString& sourceText, const QString& targetText,
                            const QString& gameId, const QString& context)
{
    TranslationMemoryEntry entry;
    entry.sourceText = sourceText.toStdString();
    entry.targetText = targetText.toStdString();
    entry.sourceHash = TranslationMemoryService::hashText(entry.sourceText);
    entry.sourceLang = "en";
    entry.targetLang = "tr";

    if (!gameId.isEmpty()) entry.gameId = gameId.toStdString();
    if (!context.isEmpty()) entry.context = context.toStdString();

    entry.createdAt = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    entry.updatedAt = entry.createdAt;

    auto result = TranslationMemoryService::addEntry(entry);
    if (!result) {
        qWarning() << "TM addEntry failed:" << QString::fromStdString(result.error().message());
        return false;
    }

    return true;
}

void CoreBridge::findBatchTMMatches(const QStringList& sourceTexts,
                                    const QString& gameId, double minScore)
{
    (void)QtConcurrent::run([this, sourceTexts, gameId, minScore]() {
        TranslationMemoryService::MatchContext ctx;
        if (!gameId.isEmpty()) ctx.gameId = gameId.toStdString();

        std::vector<std::string> texts;
        for (const auto& text : sourceTexts) {
            texts.push_back(text.toStdString());
        }

        auto result = TranslationMemoryService::findBatchMatches(texts, ctx, "tr", minScore);

        int matched = 0;
        int total = sourceTexts.size();

        if (result) {
            for (const auto& [source, match] : *result) {
                if (match.has_value()) {
                    matched++;
                }
            }
        } else {
            qWarning() << "TM findBatchMatches failed:" << QString::fromStdString(result.error().message());
        }

        emit tmBatchCompleted(matched, total);
    });
}

void CoreBridge::clearTM()
{
    // TODO: Implement when TranslationMemoryService::clear() is available
}

// ========== Glossary Functions ==========

QList<GlossaryTermQt> CoreBridge::getAllGlossaryTerms()
{
    QList<GlossaryTermQt> results;

    auto& glossary = GlossaryService::instance();
    auto termsResult = glossary.getAllTerms();

    if (termsResult) {
        for (const auto& term : *termsResult) {
            results.append(convertGlossaryTerm(term));
        }
    } else {
        qWarning() << "Glossary getAllTerms failed:" << QString::fromStdString(termsResult.error().message());
    }

    return results;
}

QList<GlossaryTermQt> CoreBridge::getGlossaryTermsForGame(const QString& gameId)
{
    QList<GlossaryTermQt> results;

    auto& glossary = GlossaryService::instance();
    auto termsResult = glossary.getTermsForGame(gameId.toStdString());

    if (termsResult) {
        for (const auto& term : *termsResult) {
            results.append(convertGlossaryTerm(term));
        }
    } else {
        qWarning() << "Glossary getTermsForGame failed:" << QString::fromStdString(termsResult.error().message());
    }

    return results;
}

QString CoreBridge::applyGlossary(const QString& text, const QString& gameId)
{
    auto& glossary = GlossaryService::instance();

    std::optional<std::string> optGameId;
    if (!gameId.isEmpty()) {
        optGameId = gameId.toStdString();
    }

    auto result = glossary.applyGlossary(text.toStdString(), std::nullopt, optGameId);

    if (result) {
        return QString::fromStdString(*result);
    } else {
        qWarning() << "Glossary applyGlossary failed:" << QString::fromStdString(result.error().message());
        return text;
    }
}

QList<GlossaryTermQt> CoreBridge::findTermsInText(const QString& text, const QString& gameId)
{
    QList<GlossaryTermQt> results;

    auto& glossary = GlossaryService::instance();

    std::optional<std::string> optGameId;
    if (!gameId.isEmpty()) {
        optGameId = gameId.toStdString();
    }

    auto matchesResult = glossary.findTermsInText(text.toStdString(), std::nullopt, optGameId);

    if (matchesResult) {
        for (const auto& match : *matchesResult) {
            results.append(convertGlossaryTerm(match.term));
        }
    } else {
        qWarning() << "Glossary findTermsInText failed:" << QString::fromStdString(matchesResult.error().message());
    }

    return results;
}

void CoreBridge::clearGlossary()
{
    // TODO: Implement when GlossaryService::clear() is available
}

// ========== QA Functions ==========

QAResultQt CoreBridge::performQACheck(const QString& sourceText, const QString& targetText,
                                       const QString& gameId, bool checkGlossary)
{
    std::optional<std::string> optGameId;
    if (!gameId.isEmpty()) {
        optGameId = gameId.toStdString();
    }

    auto result = QAService::performFullQA(
        sourceText.toStdString(),
        targetText.toStdString(),
        optGameId,
        std::nullopt,  // domain
        checkGlossary
    );

    return convertQAResult(result);
}

void CoreBridge::performBatchQA(const QList<QPair<QString, QString>>& entries,
                                const QString& gameId)
{
    (void)QtConcurrent::run([this, entries, gameId]() {
        std::optional<std::string> optGameId;
        if (!gameId.isEmpty()) {
            optGameId = gameId.toStdString();
        }

        // Build batch map
        std::map<int64_t, std::pair<std::string, std::string>> batchEntries;
        int64_t idx = 0;
        for (const auto& [source, target] : entries) {
            batchEntries[idx++] = {source.toStdString(), target.toStdString()};
        }

        auto results = QAService::batchQA(batchEntries, optGameId, std::nullopt, false);

        int passed = 0;
        int failed = 0;
        double totalScore = 0.0;

        for (const auto& [id, result] : results) {
            if (result.passed) {
                passed++;
            } else {
                failed++;
            }
            totalScore += result.score;
        }

        int avgScore = results.empty() ? 100 : static_cast<int>(totalScore / results.size());
        emit qaBatchCompleted(passed, failed, avgScore);
    });
}

// ========== Package Functions ==========

bool CoreBridge::hasTranslationPackage(const QString& gameId)
{
    // Search in detected games first
    for (const auto& game : m_detectedGames) {
        if (game.id == gameId) {
            GameInfo coreGame;
            coreGame.name = game.name.toStdString();
            coreGame.installPath = game.installPath.toStdString();
            coreGame.id.storeId = game.steamAppId.toStdString();
            coreGame.id.store = GameStore::Steam;

            auto& pm = Core::instance().packageManager();
            return pm.hasTranslation(coreGame);
        }
    }

    return false;
}

std::optional<TranslationPackageQt> CoreBridge::getPackageForGame(const QString& gameId)
{
    // Search in detected games
    for (const auto& game : m_detectedGames) {
        if (game.id == gameId) {
            GameInfo coreGame;
            coreGame.name = game.name.toStdString();
            coreGame.installPath = game.installPath.toStdString();
            coreGame.id.storeId = game.steamAppId.toStdString();
            coreGame.id.store = GameStore::Steam;

            auto& pm = Core::instance().packageManager();
            auto result = pm.findPackage(coreGame);

            if (result) {
                TranslationPackageQt pkg;
                pkg.packageId = QString::fromStdString(result->packageId);
                pkg.gameId = QString::fromStdString(result->gameId);
                pkg.gameName = QString::fromStdString(result->gameName);
                pkg.version = QString::fromStdString(result->version);
                pkg.downloadUrl = QString::fromStdString(result->downloadUrl);
                pkg.sizeBytes = static_cast<qint64>(result->sizeBytes);
                pkg.requiresRuntime = result->requiresRuntime;

                return pkg;
            }
        }
    }

    return std::nullopt;
}

void CoreBridge::installPackage(const QString& packageId, const QString& gamePath)
{
    (void)QtConcurrent::run([this, packageId, gamePath]() {
        auto& pm = Core::instance().packageManager();

        auto pkgResult = pm.getPackage(packageId.toStdString());
        if (!pkgResult) {
            qWarning() << "Package not found:" << packageId;
            emit packageInstallCompleted(false, "Paket bulunamadı");
            return;
        }

        // Build GameInfo
        GameInfo game;
        game.installPath = gamePath.toStdString();

        auto installResult = pm.install(*pkgResult, game, [this](uint32_t current, uint32_t total, std::string_view status) {
            double progress = total > 0 ? static_cast<double>(current) / static_cast<double>(total) : 0.0;
            emit packageInstallProgress(progress, QString::fromUtf8(status.data(), static_cast<int>(status.size())));
        });

        if (installResult) {
            emit packageInstallCompleted(true, QString("Paket başarıyla kuruldu: %1 dosya").arg(installResult->filesPatched));
        } else {
            emit packageInstallCompleted(false, QString::fromStdString(installResult.error().message()));
        }
    });
}

bool CoreBridge::isPackageInstalled(const QString& gameId)
{
    auto& pm = Core::instance().packageManager();
    return pm.isInstalled(gameId.toStdString());
}

bool CoreBridge::uninstallPackage(const QString& gameId, const QString& gamePath)
{
    GameInfo game;
    game.installPath = gamePath.toStdString();

    auto& pm = Core::instance().packageManager();
    auto result = pm.uninstall(game);

    if (result) {
        return true;
    }

    qWarning() << "Uninstall failed:" << QString::fromStdString(result.error().message());
    return false;
}

void CoreBridge::refreshPackageManifest()
{
    emit packageManifestRefreshed(0);
}

#endif  // MAKINEAI_UI_ONLY

} // namespace makineai
