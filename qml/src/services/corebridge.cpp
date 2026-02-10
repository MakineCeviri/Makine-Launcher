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
#include <QFileInfo>
#include <QDirIterator>

#ifdef MAKINEAI_UI_ONLY
#include "vdfparser.h"
#include "localpackagemanager.h"
#include <QSettings>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#endif

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

// ========== REAL IMPLEMENTATIONS FOR UI-ONLY BUILD (Pure Qt) ==========

// ========== Steam Scanner ==========

void CoreBridge::doScanSteamReal()
{
    emit scanProgress(0.05, tr("Steam yolu aranıyor..."));

    // Read Steam path from Windows Registry
    QSettings steamReg("HKEY_CURRENT_USER\\Software\\Valve\\Steam", QSettings::NativeFormat);
    QString steamPath = steamReg.value("SteamPath").toString();

    if (steamPath.isEmpty()) {
        // Try alternate location
        QSettings steamReg64("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Valve\\Steam", QSettings::NativeFormat);
        steamPath = steamReg64.value("InstallPath").toString();
    }

    if (steamPath.isEmpty()) {
        qDebug() << "Steam not found in registry";
        return;
    }

    steamPath = QDir::cleanPath(steamPath);
    qDebug() << "Steam path:" << steamPath;

    // Parse libraryfolders.vdf to find all library folders
    QString vdfPath = steamPath + "/steamapps/libraryfolders.vdf";
    QFile vdfFile(vdfPath);
    if (!vdfFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open libraryfolders.vdf:" << vdfPath;
        return;
    }

    std::string vdfContent = vdfFile.readAll().toStdString();
    vdfFile.close();

    auto vdfRoot = makineai::vdf::parse(vdfContent);
    if (!vdfRoot) {
        qWarning() << "Failed to parse libraryfolders.vdf";
        return;
    }

    // Collect library paths
    QStringList libraryPaths;
    const auto* libraryfolders = vdfRoot->find("libraryfolders");
    if (!libraryfolders) libraryfolders = &(*vdfRoot); // root itself might be the node

    for (const auto& [key, node] : libraryfolders->children) {
        // Keys are "0", "1", "2", etc.
        bool isIndex = false;
        QString qKey = QString::fromStdString(key);
        qKey.toInt(&isIndex);

        if (isIndex || key == "path") {
            QString path;
            if (node.isObject()) {
                path = QString::fromStdString(node.getString("path"));
            } else {
                path = QString::fromStdString(node.value);
            }

            if (!path.isEmpty()) {
                path = QDir::cleanPath(path);
                if (QDir(path + "/steamapps").exists()) {
                    libraryPaths.append(path);
                }
            }
        }
    }

    // Always include main Steam path
    if (!libraryPaths.contains(steamPath) && QDir(steamPath + "/steamapps").exists()) {
        libraryPaths.prepend(steamPath);
    }

    qDebug() << "Found" << libraryPaths.size() << "Steam library folders";

    // Known redistributable AppIDs to filter out
    static const QSet<QString> redistributables = {
        "228980", "228981", "1070560", "1245620",  // remove 1245620 from redist
    };
    // Actually only these are redistributables
    static const QSet<QString> realRedistributables = {
        "228980", "228981", "1070560", "1161040", "1493710",
    };

    int totalGames = 0;
    int processed = 0;

    // First pass: count ACF files
    for (const QString& libPath : libraryPaths) {
        QDir steamappsDir(libPath + "/steamapps");
        totalGames += steamappsDir.entryList({"appmanifest_*.acf"}, QDir::Files).size();
    }

    // Second pass: parse each ACF
    for (const QString& libPath : libraryPaths) {
        QDir steamappsDir(libPath + "/steamapps");
        const auto acfFiles = steamappsDir.entryList({"appmanifest_*.acf"}, QDir::Files);

        for (const QString& acfFile : acfFiles) {
            QFile file(steamappsDir.absoluteFilePath(acfFile));
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;

            std::string acfContent = file.readAll().toStdString();
            file.close();

            auto acfRoot = makineai::vdf::parse(acfContent);
            if (!acfRoot) continue;

            const auto* appState = acfRoot->find("AppState");
            if (!appState) appState = &(*acfRoot);

            QString appId = QString::fromStdString(appState->getString("appid"));
            QString name = QString::fromStdString(appState->getString("name"));
            QString installDir = QString::fromStdString(appState->getString("installdir"));

            // Skip empty or redistributable entries
            if (appId.isEmpty() || name.isEmpty() || installDir.isEmpty()) {
                processed++;
                continue;
            }

            if (realRedistributables.contains(appId)) {
                processed++;
                continue;
            }

            // Build install path
            QString installPath = QDir::cleanPath(libPath + "/steamapps/common/" + installDir);
            if (!QDir(installPath).exists()) {
                processed++;
                continue;
            }

            DetectedGame game;
            game.id = appId;
            game.name = name;
            game.installPath = installPath;
            game.source = "steam";
            game.steamAppId = appId;
            game.headerImageUrl = QString("https://cdn.akamai.steamstatic.com/steam/apps/%1/library_600x900_2x.jpg").arg(appId);
            game.isVerified = false;
            game.hasTranslation = false;

            m_detectedGames.append(game);
            emit gameDetected(game.id, game.name);

            processed++;
            if (processed % 10 == 0 && totalGames > 0) {
                emit scanProgress(0.1 + 0.5 * (static_cast<qreal>(processed) / totalGames),
                    tr("Steam: %1 oyun bulundu...").arg(m_detectedGames.count()));
            }
        }
    }

    qDebug() << "Steam scan complete:" << m_detectedGames.count() << "games found";
}

// ========== Epic Games Scanner ==========

void CoreBridge::doScanEpicReal()
{
    emit scanProgress(0.65, tr("Epic Games taranıyor..."));

    QString manifestDir = "C:/ProgramData/Epic/EpicGamesLauncher/Data/Manifests";
    QDir dir(manifestDir);
    if (!dir.exists()) {
        qDebug() << "Epic Games manifest directory not found";
        return;
    }

    const auto itemFiles = dir.entryList({"*.item"}, QDir::Files);
    for (const QString& itemFile : itemFiles) {
        QFile file(dir.absoluteFilePath(itemFile));
        if (!file.open(QIODevice::ReadOnly)) continue;

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
        file.close();

        if (err.error != QJsonParseError::NoError) continue;

        QJsonObject obj = doc.object();
        QString displayName = obj["DisplayName"].toString();
        QString installLocation = obj["InstallLocation"].toString();
        QString catalogItemId = obj["CatalogItemId"].toString();

        if (displayName.isEmpty() || installLocation.isEmpty()) continue;
        if (!QDir(installLocation).exists()) continue;

        DetectedGame game;
        game.id = "epic_" + catalogItemId;
        game.name = displayName;
        game.installPath = QDir::cleanPath(installLocation);
        game.source = "epic";
        game.isVerified = false;
        game.hasTranslation = false;

        m_detectedGames.append(game);
        emit gameDetected(game.id, game.name);
    }

    qDebug() << "Epic scan: found" << itemFiles.size() << "manifests";
}

// ========== GOG Scanner ==========

void CoreBridge::doScanGogReal()
{
    emit scanProgress(0.75, tr("GOG Galaxy taranıyor..."));

    QSettings gogReg("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\GOG.com\\Games", QSettings::NativeFormat);
    const auto gameKeys = gogReg.childGroups();

    for (const QString& gameKey : gameKeys) {
        gogReg.beginGroup(gameKey);

        QString gameName = gogReg.value("gameName").toString();
        QString gamePath = gogReg.value("path").toString();

        gogReg.endGroup();

        if (gameName.isEmpty() || gamePath.isEmpty()) continue;
        if (!QDir(gamePath).exists()) continue;

        DetectedGame game;
        game.id = "gog_" + gameKey;
        game.name = gameName;
        game.installPath = QDir::cleanPath(gamePath);
        game.source = "gog";
        game.isVerified = false;
        game.hasTranslation = false;

        m_detectedGames.append(game);
        emit gameDetected(game.id, game.name);
    }

    qDebug() << "GOG scan: found" << gameKeys.size() << "entries";
}

// ========== Engine Detection ==========

QString CoreBridge::detectEngineReal(const QString& gamePath)
{
    QDir dir(gamePath);
    if (!dir.exists()) return "Unknown";

    // Unity: UnityPlayer.dll or GameAssembly.dll or *_Data/globalgamemanagers
    if (QFile::exists(gamePath + "/UnityPlayer.dll") ||
        QFile::exists(gamePath + "/GameAssembly.dll")) {
        return "Unity";
    }
    // Check for *_Data/globalgamemanagers
    {
        const auto entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& entry : entries) {
            if (entry.endsWith("_Data")) {
                if (QFile::exists(gamePath + "/" + entry + "/globalgamemanagers")) {
                    return "Unity";
                }
            }
        }
    }

    // Unreal: *.pak files in Content/ or Engine/
    if (QDir(gamePath + "/Engine").exists()) {
        return "Unreal";
    }
    {
        QDirIterator it(gamePath, {"*.pak"}, QDir::Files, QDirIterator::Subdirectories);
        int pakCount = 0;
        while (it.hasNext() && pakCount < 3) {
            it.next();
            pakCount++;
        }
        if (pakCount > 0) {
            // Check if it looks Unreal-specific
            if (QDir(gamePath + "/Content").exists() ||
                !dir.entryList({"*.uproject"}, QDir::Files).isEmpty()) {
                return "Unreal";
            }
            // Could be other engines, but .pak is Unreal-typical
            return "Unreal";
        }
    }

    // Bethesda: *.ba2, *.esm, *.esp
    {
        bool hasBa2 = !dir.entryList({"*.ba2"}, QDir::Files).isEmpty();
        bool hasEsm = !dir.entryList({"*.esm"}, QDir::Files).isEmpty();
        bool hasEsp = !dir.entryList({"*.esp"}, QDir::Files).isEmpty();
        // Also check Data/ subfolder
        QDir dataDir(gamePath + "/Data");
        if (dataDir.exists()) {
            hasBa2 = hasBa2 || !dataDir.entryList({"*.ba2"}, QDir::Files).isEmpty();
            hasEsm = hasEsm || !dataDir.entryList({"*.esm"}, QDir::Files).isEmpty();
            hasEsp = hasEsp || !dataDir.entryList({"*.esp"}, QDir::Files).isEmpty();
        }
        if (hasBa2 || (hasEsm && hasEsp)) {
            return "Bethesda";
        }
    }

    // Ren'Py: renpy/ directory, *.rpa, *.rpyc
    if (QDir(gamePath + "/renpy").exists() ||
        QDir(gamePath + "/game/renpy").exists()) {
        return "RenPy";
    }
    {
        QDirIterator it(gamePath, {"*.rpa", "*.rpyc"}, QDir::Files, QDirIterator::Subdirectories);
        if (it.hasNext()) return "RenPy";
    }

    // RPG Maker MV/MZ: www/ directory, rpg_core.js
    if (QDir(gamePath + "/www").exists() ||
        QFile::exists(gamePath + "/www/js/rpg_core.js") ||
        QFile::exists(gamePath + "/js/rpg_core.js")) {
        return "RPGMaker";
    }
    // RPG Maker VX Ace: *.rvdata2
    {
        QDirIterator it(gamePath, {"*.rvdata2"}, QDir::Files);
        if (it.hasNext()) return "RPGMaker";
    }

    // GameMaker: data.win
    if (QFile::exists(gamePath + "/data.win")) {
        return "GameMaker";
    }

    // Godot: *.pck
    {
        const auto pckFiles = dir.entryList({"*.pck"}, QDir::Files);
        if (!pckFiles.isEmpty()) return "Godot";
    }

    // Source Engine: hl2.exe or source engine common files
    if (QDir(gamePath + "/hl2").exists() ||
        QFile::exists(gamePath + "/hl2.exe") ||
        QDir(gamePath + "/platform").exists()) {
        // Check for Source-specific files
        if (!dir.entryList({"*.vpk"}, QDir::Files).isEmpty()) {
            return "Source";
        }
    }

    return "Unknown";
}

// ========== Public API Methods ==========

void CoreBridge::scanAllLibraries()
{
    emit scanStarted();
    m_detectedGames.clear();

    // Initialize LocalPackageManager if needed
    if (!m_localPkgManager) {
        m_localPkgManager = new LocalPackageManager(this);
        // Connect install signals
        connect(m_localPkgManager, &LocalPackageManager::installProgress,
                this, &CoreBridge::packageInstallProgress);
        connect(m_localPkgManager, &LocalPackageManager::installCompleted,
                this, &CoreBridge::packageInstallCompleted);
    }

    // Load translation packages
    QSettings settings("MakineAI", "MakineAI");
    QString translationPath = settings.value("paths/translationData",
        "C:/cedra/translation_data/mc-main").toString();
    m_localPkgManager->loadFromPath(translationPath);

    (void)QtConcurrent::run([this]() {
        emit scanProgress(0.0, tr("Steam kütüphanesi taranıyor..."));
        doScanSteamReal();

        emit scanProgress(0.6, tr("Epic Games taranıyor..."));
        doScanEpicReal();

        emit scanProgress(0.75, tr("GOG Galaxy taranıyor..."));
        doScanGogReal();

        // Detect engines for all found games
        emit scanProgress(0.85, tr("Oyun motorları tespit ediliyor..."));
        for (int i = 0; i < m_detectedGames.size(); ++i) {
            auto& game = m_detectedGames[i];
            if (game.engine.isEmpty() || game.engine == "Unknown") {
                game.engine = detectEngineReal(game.installPath);
            }
            // Check translation availability
            if (m_localPkgManager && m_localPkgManager->hasPackage(game.steamAppId)) {
                game.hasTranslation = true;
            }
        }

        emit scanProgress(1.0, tr("%1 oyun bulundu").arg(m_detectedGames.count()));
        emit scanCompleted(m_detectedGames.count());
    });
}

void CoreBridge::scanSteamLibrary()
{
    emit scanStarted();
    m_detectedGames.clear();
    (void)QtConcurrent::run([this]() {
        emit scanProgress(0.0, tr("Steam kütüphanesi taranıyor..."));
        doScanSteamReal();
        // Detect engines
        for (auto& game : m_detectedGames) {
            game.engine = detectEngineReal(game.installPath);
        }
        emit scanProgress(1.0, tr("%1 Steam oyunu bulundu").arg(m_detectedGames.count()));
        emit scanCompleted(m_detectedGames.count());
    });
}

void CoreBridge::scanEpicLibrary()
{
    emit scanStarted();
    m_detectedGames.clear();
    (void)QtConcurrent::run([this]() {
        doScanEpicReal();
        for (auto& game : m_detectedGames) {
            game.engine = detectEngineReal(game.installPath);
        }
        emit scanProgress(1.0, tr("%1 Epic oyunu bulundu").arg(m_detectedGames.count()));
        emit scanCompleted(m_detectedGames.count());
    });
}

void CoreBridge::scanGogLibrary()
{
    emit scanStarted();
    m_detectedGames.clear();
    (void)QtConcurrent::run([this]() {
        doScanGogReal();
        for (auto& game : m_detectedGames) {
            game.engine = detectEngineReal(game.installPath);
        }
        emit scanProgress(1.0, tr("%1 GOG oyunu bulundu").arg(m_detectedGames.count()));
        emit scanCompleted(m_detectedGames.count());
    });
}

QString CoreBridge::detectEngine(const QString& gamePath)
{
    return detectEngineReal(gamePath);
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


// ========== Package Management (via LocalPackageManager) ==========

bool CoreBridge::hasTranslationPackage(const QString& gameId)
{
    if (m_localPkgManager) {
        return m_localPkgManager->hasPackage(gameId);
    }
    return false;
}

std::optional<TranslationPackageQt> CoreBridge::getPackageForGame(const QString& gameId)
{
    if (!m_localPkgManager) return std::nullopt;

    auto pkg = m_localPkgManager->getPackage(gameId);
    if (!pkg) return std::nullopt;

    TranslationPackageQt qtPkg;
    qtPkg.packageId = pkg->packageId;
    qtPkg.gameId = pkg->steamAppId;
    qtPkg.gameName = pkg->gameName;
    qtPkg.version = pkg->version;
    qtPkg.sizeBytes = pkg->sizeBytes;
    qtPkg.requiresRuntime = false;
    return qtPkg;
}

void CoreBridge::installPackage(const QString& packageId, const QString& gamePath)
{
    if (!m_localPkgManager) {
        emit packageInstallCompleted(false, tr("Paket yöneticisi başlatılamadı"));
        return;
    }

    // packageId here is the steamAppId or the internal package ID
    // Try to find by steamAppId first
    if (m_localPkgManager->hasPackage(packageId)) {
        m_localPkgManager->installPackage(packageId, gamePath);
    } else {
        // Try to resolve from detected games
        for (const auto& game : m_detectedGames) {
            if (game.id == packageId && m_localPkgManager->hasPackage(game.steamAppId)) {
                m_localPkgManager->installPackage(game.steamAppId, gamePath);
                return;
            }
        }
        emit packageInstallCompleted(false, tr("Paket bulunamadı: %1").arg(packageId));
    }
}

bool CoreBridge::isPackageInstalled(const QString& gameId)
{
    if (m_localPkgManager) {
        return m_localPkgManager->isInstalled(gameId);
    }
    return false;
}

bool CoreBridge::uninstallPackage(const QString& gameId, const QString& gamePath)
{
    if (m_localPkgManager) {
        return m_localPkgManager->uninstallPackage(gameId, gamePath);
    }
    return false;
}

void CoreBridge::refreshPackageManifest()
{
    int count = m_localPkgManager ? m_localPkgManager->packageCount() : 0;
    emit packageManifestRefreshed(count);
}

#else  // FULL BUILD WITH CORE LIBRARY

// ========== FULL IMPLEMENTATIONS WITH CORE LIBRARY ==========

void CoreBridge::scanAllLibraries()
{
    emit scanStarted();
    m_detectedGames.clear();

    (void)QtConcurrent::run([this]() {
        emit scanProgress(0.0, tr("Steam kütüphanesi taranıyor..."));
        doScanSteam();

        emit scanProgress(0.33, tr("Epic Games taranıyor..."));
        doScanEpic();

        emit scanProgress(0.66, tr("GOG Galaxy taranıyor..."));
        doScanGog();

        emit scanProgress(1.0, tr("%1 oyun bulundu").arg(m_detectedGames.count()));
        emit scanCompleted(m_detectedGames.count());
    });
}

void CoreBridge::scanSteamLibrary()
{
    emit scanStarted();
    (void)QtConcurrent::run([this]() {
        emit scanProgress(0.0, tr("Steam kütüphanesi taranıyor..."));
        doScanSteam();
        emit scanProgress(1.0, tr("%1 Steam oyunu bulundu").arg(m_detectedGames.count()));
        emit scanCompleted(m_detectedGames.count());
    });
}

void CoreBridge::scanEpicLibrary()
{
    emit scanStarted();
    (void)QtConcurrent::run([this]() {
        emit scanProgress(0.0, tr("Epic Games taranıyor..."));
        doScanEpic();
        emit scanProgress(1.0, tr("%1 Epic oyunu bulundu").arg(m_detectedGames.count()));
        emit scanCompleted(m_detectedGames.count());
    });
}

void CoreBridge::scanGogLibrary()
{
    emit scanStarted();
    (void)QtConcurrent::run([this]() {
        emit scanProgress(0.0, tr("GOG Galaxy taranıyor..."));
        doScanGog();
        emit scanProgress(1.0, tr("%1 GOG oyunu bulundu").arg(m_detectedGames.count()));
        emit scanCompleted(m_detectedGames.count());
    });
}

void CoreBridge::doScanSteam()
{
    // Safety guard: Ensure Core is initialized before scanning
    if (!s_coreInitialized) {
        qWarning() << "CoreBridge::doScanSteam - Core not initialized, skipping scan";
        emit scanError(tr("Core kütüphanesi başlatılamadı. Lütfen uygulamayı yeniden başlatın."));
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
        emit extractionError(tr("Desteklenmeyen motor: %1").arg(engine));
        return;
    }

    emit extractionProgress(0.1, tr("Dosyalar taranıyor..."));

    ExtractionOptions options;
    options.minLength = 2;
    options.maxLength = 10000;

    auto result = handler->extractStrings(path, options);
    if (!result) {
        emit extractionError(QString::fromStdString(result.error().message()));
        return;
    }

    emit extractionProgress(0.5, tr("Metinler işleniyor..."));

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
            emit extractionProgress(progress, tr("İşleniyor: %1/%2").arg(processed).arg(total));
        }
    }

    emit extractionProgress(1.0, tr("%1 metin çıkarıldı").arg(m_extractedStrings.count()));
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
        emit patchError(tr("Desteklenmeyen motor: %1").arg(engine));
        return;
    }

    emit patchProgress(0.1, tr("Yedek oluşturuluyor..."));

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

    emit patchProgress(0.3, tr("Çeviriler uygulanıyor..."));

    PatchOptions options;
    options.createBackup = true;

    auto result = handler->applyTranslations(path, coreTranslations, options);
    if (!result) {
        emit patchError(QString::fromStdString(result.error().message()));
        return;
    }

    emit patchProgress(1.0, tr("%1 çeviri uygulandı").arg(result->appliedCount));
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
        emit backupError(tr("Desteklenmeyen motor: %1").arg(engine));
        return QString();
    }

    auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    std::string backupId = std::to_string(timestamp);
    auto result = handler->createBackup(path, backupId);

    if (!result || !result->success) {
        QString error = result ? QString::fromStdString(result->errorMessage) : tr("Yedek oluşturulamadı");
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
        emit backupError(tr("Desteklenmeyen motor: %1").arg(engine));
        return false;
    }

    auto result = handler->restoreBackup(path, backupId.toStdString());
    if (!result || !result->success) {
        QString error = result ? QString::fromStdString(result->errorMessage) : tr("Yedek geri yüklenemedi");
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
    auto result = TranslationMemoryService::clearAll();
    if (!result) {
        qWarning() << "Failed to clear TM:" << QString::fromStdString(result.error().message());
    }
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
    // Clear the in-memory cache; full database clear requires
    // GlossaryService::clearAll() which is not yet implemented.
    GlossaryService::instance().clearCache();
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

        int total = static_cast<int>(results.size());
        double avgScore = results.empty() ? 100.0 : totalScore / results.size();
        emit qaBatchCompleted(passed, total, avgScore);
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
            emit packageInstallCompleted(false, tr("Paket bulunamadı"));
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
            emit packageInstallCompleted(true, tr("Paket başarıyla kuruldu: %1 dosya").arg(installResult->filesPatched));
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
