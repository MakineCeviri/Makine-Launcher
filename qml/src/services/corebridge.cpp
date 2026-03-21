/**
 * @file corebridge.cpp
 * @brief Core Bridge Implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "corebridge.h"
#include "profiler.h"
#include "appprotection.h"
#include "apppaths.h"
#include "crashreporter.h"

#include <QLoggingCategory>
#include <QDir>
#include <QThread>
#include <QStandardPaths>
#include <QSet>
#include <QFileInfo>
#include <QDirIterator>
#include <QStorageInfo>

#include "vdfparser.h"
#include "localpackagemanager.h"
#include "operationjournal.h"
#include <QSettings>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>

#include <optional>
#include <string>

#ifndef MAKINE_UI_ONLY
#include <makine/core.hpp>
#endif

Q_LOGGING_CATEGORY(lcCoreBridge, "makine.bridge")

namespace makine {

CoreBridge* CoreBridge::s_instance = nullptr;

#ifndef MAKINE_UI_ONLY
static bool s_coreInitialized = false;

// Initialize Core singleton on first use
// Returns true if Core is ready to use, false otherwise
static bool ensureCoreInitialized() {
    if (s_coreInitialized) return true;

    try {
        auto& core = Core::instance();
        if (!core.isInitialized()) {
            qCDebug(lcCoreBridge) << "Initializing Makine Core...";
            auto result = core.initialize();
            if (result) {
                qCDebug(lcCoreBridge) << "Core initialized successfully in" << result->initDuration.count() << "ms";
                CrashReporter::addBreadcrumb("core", "Core initialized successfully");
                s_coreInitialized = true;
                return true;
            } else {
                qCCritical(lcCoreBridge) << "Core initialization FAILED:"
                           << QString::fromStdString(result.error().message());
                CrashReporter::captureMessage("Core initialization failed", "error");
                return false;
            }
        } else {
            s_coreInitialized = true;
            return true;
        }
    } catch (const std::exception& e) {
        qCCritical(lcCoreBridge) << "Core initialization threw exception:" << e.what();
        return false;
    } catch (...) {
        qCCritical(lcCoreBridge) << "Core initialization threw unknown exception";
        return false;
    }
}
#endif

CoreBridge::CoreBridge(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
    // Core library init deferred to first use (scanAllLibraries)
    // to avoid blocking startup
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

void CoreBridge::setJournal(OperationJournal* journal)
{
    m_journal = journal;
    if (m_localPkgManager)
        m_localPkgManager->setJournal(journal);
}

// ========== GAME SCANNING (Pure Qt) ==========

namespace {
// Check if a directory contains at least one .exe within maxDepth levels.
// GTA SA DE has exe at Gameface/Binaries/Win64/SanAndreas.exe (depth 3).
bool hasExecutable(const QString& dirPath, int maxDepth = 3)
{
    QDirIterator it(dirPath, {QStringLiteral("*.exe")}, QDir::Files,
                    QDirIterator::Subdirectories);
    QDir base(dirPath);
    while (it.hasNext()) {
        it.next();
        QString rel = base.relativeFilePath(it.filePath());
        int depth = rel.count(QLatin1Char('/')) + rel.count(QLatin1Char('\\'));
        if (depth <= maxDepth)
            return true;
    }
    return false;
}
} // namespace

void CoreBridge::setCustomGamePaths(const QStringList& paths)
{
    m_customGamePaths = paths;
}

// ========== Steam Scanner ==========

void CoreBridge::doScanSteamReal(QList<DetectedGame>& outGames)
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
        qCDebug(lcCoreBridge) << "Steam not found in registry";
        return;
    }

    steamPath = QDir::cleanPath(steamPath);
    qCDebug(lcCoreBridge) << "Steam path:" << steamPath;

    // Parse libraryfolders.vdf to find all library folders
    QString vdfPath = steamPath + "/steamapps/libraryfolders.vdf";
    QFile vdfFile(vdfPath);
    if (!vdfFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(lcCoreBridge) << "Cannot open libraryfolders.vdf:" << vdfPath;
        return;
    }

    if (vdfFile.size() > static_cast<qint64>(makine::vdf::detail::kMaxVdfFileSize)) {
        qCWarning(lcCoreBridge) << "VDF file too large, skipping:" << vdfPath;
        return;
    }
    std::string vdfContent = vdfFile.readAll().toStdString();
    vdfFile.close();

    auto vdfRoot = makine::vdf::parse(vdfContent);
    if (!vdfRoot) {
        qCWarning(lcCoreBridge) << "Failed to parse libraryfolders.vdf";
        return;
    }

    // Collect library paths (deduplicated)
    QSet<QString> libraryPathSet;
    const auto* libraryfolders = vdfRoot->find("libraryfolders");
    if (!libraryfolders) libraryfolders = &(*vdfRoot); // root itself might be the node

    for (const auto& [key, node] : libraryfolders->children) {
        // Keys are "0", "1", "2", etc. — each is an object with a "path" child
        bool isIndex = false;
        QString::fromStdString(key).toInt(&isIndex);
        if (!isIndex) continue;

        if (!node.isObject()) continue;
        QString path = QString::fromStdString(node.getString("path"));
        if (path.isEmpty()) continue;

        path = QDir::cleanPath(path);
        if (QDir(path + "/steamapps").exists()) {
            libraryPathSet.insert(path);
        }
    }

    // Always include main Steam path
    if (QDir(steamPath + "/steamapps").exists()) {
        libraryPathSet.insert(steamPath);
    }

    QStringList libraryPaths = libraryPathSet.values();

    qCDebug(lcCoreBridge) << "Found" << libraryPaths.size() << "Steam library folders";

    // Known redistributable AppIDs to filter out
    static const QSet<QString> realRedistributables = {
        "228980", "228981", "1070560", "1161040", "1493710",
    };

    // Track seen appIds to avoid duplicates across library folders
    QSet<QString> seenAppIds;

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

            auto acfRoot = makine::vdf::parse(acfContent);
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

            // Skip duplicate appIds (same game in multiple library folders)
            if (seenAppIds.contains(appId)) {
                processed++;
                continue;
            }
            seenAppIds.insert(appId);

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
            game.isVerified = false;
            game.hasTranslation = false;

            outGames.append(game);
            emit gameDetected(game.id, game.name);

            processed++;
            if (processed % 50 == 0 && totalGames > 0) {
                emit scanProgress(0.1 + 0.5 * (static_cast<qreal>(processed) / totalGames),
                    tr("Steam: %1 oyun bulundu...").arg(outGames.count()));
            }
        }
    }

    qCDebug(lcCoreBridge) << "Steam scan complete:" << outGames.count() << "games found";
}

// ========== Epic Games Scanner ==========

void CoreBridge::doScanEpicReal(QList<DetectedGame>& outGames)
{
    emit scanProgress(0.65, tr("Epic Games taranıyor..."));

    // Find Epic manifest dir on any drive (ProgramData is on the system drive)
    QString systemDrive = QString::fromLocal8Bit(qgetenv("SystemDrive")); // e.g. "C:"
    if (systemDrive.isEmpty()) systemDrive = "C:";

    QString manifestDir = systemDrive + "/ProgramData/Epic/EpicGamesLauncher/Data/Manifests";
    QDir dir(manifestDir);
    if (!dir.exists()) {
        qCDebug(lcCoreBridge) << "Epic Games manifest directory not found at" << manifestDir;
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

        outGames.append(game);
        emit gameDetected(game.id, game.name);
    }

    qCDebug(lcCoreBridge) << "Epic scan: found" << itemFiles.size() << "manifests";
}

// ========== GOG Scanner ==========

void CoreBridge::doScanGogReal(QList<DetectedGame>& outGames)
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

        outGames.append(game);
        emit gameDetected(game.id, game.name);
    }

    qCDebug(lcCoreBridge) << "GOG scan: found" << gameKeys.size() << "entries";
}

// ========== Engine Detection ==========

QString CoreBridge::detectEngineReal(const QString& gamePath)
{
    MAKINE_ZONE_NAMED("CoreBridge::detectEngine");
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
        QDirIterator pakIter(gamePath, {"*.pak"}, QDir::Files, QDirIterator::Subdirectories);
        int pakCount = 0;
        while (pakIter.hasNext() && pakCount < 3) {
            pakIter.next();
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
        QDirIterator renpyIter(gamePath, {"*.rpa", "*.rpyc"}, QDir::Files, QDirIterator::Subdirectories);
        if (renpyIter.hasNext()) return "RenPy";
    }

    // RPG Maker MV/MZ: www/ directory, rpg_core.js
    if (QDir(gamePath + "/www").exists() ||
        QFile::exists(gamePath + "/www/js/rpg_core.js") ||
        QFile::exists(gamePath + "/js/rpg_core.js")) {
        return "RPGMaker";
    }
    // RPG Maker VX Ace: *.rvdata2
    {
        QDirIterator rpgmakerIter(gamePath, {"*.rvdata2"}, QDir::Files);
        if (rpgmakerIter.hasNext()) return "RPGMaker";
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

// ========== Filesystem Scanner ==========

QStringList CoreBridge::knownGameDirectories() const
{
    QStringList dirs;

    // Enumerate all mounted drives dynamically
    QStringList drives;
    for (const auto& vol : QStorageInfo::mountedVolumes()) {
        if (!vol.isReady() || vol.isReadOnly()) continue;
        QString root = vol.rootPath();
        if (root.size() >= 2) drives.append(root.left(2)); // "C:", "D:", etc.
    }

    static const QStringList knownFolders = {
        QStringLiteral("Games"),
        QStringLiteral("Oyunlar"),
        QStringLiteral("Program Files/Rockstar Games"),
    };

    for (const auto& drive : drives) {
        for (const auto& folder : knownFolders) {
            QString path = QDir::cleanPath(drive + QLatin1Char('/') + folder);
            if (QDir(path).exists())
                dirs.append(path);
        }
    }

    // User-configured additional paths
    dirs.append(m_customGamePaths);

    return dirs;
}

void CoreBridge::doScanFilesystemReal(QList<DetectedGame>& outGames,
                                       const QSet<QString>& knownPaths)
{
    const QStringList gameDirs = knownGameDirectories();
    if (gameDirs.isEmpty()) return;

    qCDebug(lcCoreBridge) << "Filesystem scan: checking" << gameDirs.size() << "directories";

    for (const QString& baseDir : gameDirs) {
        QDir dir(baseDir);
        if (!dir.exists()) continue;

        const auto entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& folderName : entries) {
            QString fullPath = QDir::cleanPath(dir.absoluteFilePath(folderName));

            // Skip if already detected by store scanners
            if (knownPaths.contains(fullPath.toLower()))
                continue;

            // Must contain at least one .exe to qualify as a game
            if (!hasExecutable(fullPath))
                continue;

            DetectedGame game;
            game.name = folderName;
            game.installPath = fullPath;
            game.source = QStringLiteral("filesystem");

            outGames.append(game);
            qCDebug(lcCoreBridge) << "Filesystem: found" << folderName << "at" << fullPath;
        }
    }
}

// ========== Windows Registry Scanner ==========

void CoreBridge::doScanRegistryReal(QList<DetectedGame>& outGames,
                                     const QSet<QString>& knownPaths)
{
    // Scan both 64-bit and 32-bit uninstall registry paths
    static const QStringList regPaths = {
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall"),
    };

    // Publisher patterns that indicate a game (case-insensitive substring match)
    static const QStringList gamePublishers = {
        QStringLiteral("rockstar"), QStringLiteral("2k games"), QStringLiteral("take-two"),
        QStringLiteral("capcom"), QStringLiteral("square enix"), QStringLiteral("bandai namco"),
        QStringLiteral("sega"), QStringLiteral("ubisoft"), QStringLiteral("electronic arts"),
        QStringLiteral("bethesda"), QStringLiteral("cd projekt"), QStringLiteral("techland"),
        QStringLiteral("fromsoftware"), QStringLiteral("devolver"), QStringLiteral("team17"),
        QStringLiteral("paradox"), QStringLiteral("koei tecmo"), QStringLiteral("naughty dog"),
        QStringLiteral("insomniac"), QStringLiteral("sucker punch"), QStringLiteral("guerrilla"),
        QStringLiteral("remedy"), QStringLiteral("playdead"), QStringLiteral("annapurna"),
        QStringLiteral("thq nordic"), QStringLiteral("deep silver"),
        QStringLiteral("focus entertainment"), QStringLiteral("focus home"),
        QStringLiteral("warner bros"), QStringLiteral("wb games"),
        QStringLiteral("sony"), QStringLiteral("playstation pc"),
        QStringLiteral("xbox game studios"), QStringLiteral("microsoft game studios"),
        QStringLiteral("rare ltd"),
    };

    // Non-game patterns to skip (only when publisher is NOT a known game publisher)
    static const QStringList skipPatterns = {
        QStringLiteral("visual c++"), QStringLiteral("redistributable"),
        QStringLiteral(".net framework"), QStringLiteral("directx"),
        QStringLiteral("nvidia driver"), QStringLiteral("amd driver"),
        QStringLiteral("intel driver"), QStringLiteral("update for"),
        QStringLiteral("hotfix"), QStringLiteral("service pack"),
        QStringLiteral("sdk"), QStringLiteral("runtime"),
        QStringLiteral("chrome"), QStringLiteral("firefox"),
        QStringLiteral("edge browser"), QStringLiteral("antivirus"),
        QStringLiteral("office"), QStringLiteral("adobe"),
        QStringLiteral("java runtime"), QStringLiteral("python"), QStringLiteral("node.js"),
    };

    int found = 0;

    for (const QString& regPath : regPaths) {
        QSettings reg(regPath, QSettings::NativeFormat);

        for (const QString& group : reg.childGroups()) {
            reg.beginGroup(group);

            QString displayName = reg.value(QStringLiteral("DisplayName")).toString().trimmed();
            QString installLocation = reg.value(QStringLiteral("InstallLocation")).toString().trimmed();
            QString publisher = reg.value(QStringLiteral("Publisher")).toString().trimmed();

            reg.endGroup();

            // Must have both name and install path
            if (displayName.isEmpty() || installLocation.isEmpty())
                continue;

            // Install location must exist
            installLocation = QDir::cleanPath(installLocation);
            if (!QDir(installLocation).exists())
                continue;

            // Skip if already known
            if (knownPaths.contains(installLocation.toLower()))
                continue;

            // Filter: must be from a known game publisher or have game keywords
            QString lowerPublisher = publisher.toLower();
            QString lowerName = displayName.toLower();

            bool isGamePublisher = false;
            for (const auto& gp : gamePublishers) {
                if (lowerPublisher.contains(gp)) {
                    isGamePublisher = true;
                    break;
                }
            }

            bool hasGameKeyword = lowerName.contains(QLatin1String("game")) ||
                                  lowerName.contains(QLatin1String("edition")) ||
                                  lowerName.contains(QLatin1String("remastered")) ||
                                  lowerName.contains(QLatin1String("definitive"));

            if (!isGamePublisher && !hasGameKeyword)
                continue;

            // Skip non-games — only when publisher is NOT a known game publisher
            if (!isGamePublisher) {
                bool isSkipped = false;
                for (const auto& sp : skipPatterns) {
                    if (lowerName.contains(sp) || lowerPublisher.contains(sp)) {
                        isSkipped = true;
                        break;
                    }
                }
                if (isSkipped) continue;
            }

            // Must contain an executable to be a game
            if (!hasExecutable(installLocation))
                continue;

            DetectedGame game;
            game.name = displayName;
            game.installPath = installLocation;
            game.source = QStringLiteral("registry");

            outGames.append(game);
            ++found;
            qCDebug(lcCoreBridge) << "Registry: found" << displayName
                     << "by" << publisher << "at" << installLocation;
        }
    }

    qCDebug(lcCoreBridge) << "Registry scan: found" << found << "games";
}

// ========== Public API Methods ==========

void CoreBridge::scanAllLibraries()
{
    MAKINE_ZONE_NAMED("CoreBridge::scanAllLibraries");
    INTEGRITY_GATE();
    CrashReporter::addBreadcrumb("core", "CoreBridge::scanAllLibraries");
    emit scanStarted();
    m_detectedGames.clear();

    // Initialize LocalPackageManager if needed
    if (!m_localPkgManager) {
        m_localPkgManager = new LocalPackageManager(this);
        if (m_journal) m_localPkgManager->setJournal(m_journal);
        // Connect install signals
        connect(m_localPkgManager, &LocalPackageManager::installProgress,
                this, &CoreBridge::packageInstallProgress);
        connect(m_localPkgManager, &LocalPackageManager::installCompleted,
                this, &CoreBridge::packageInstallCompleted);
    }

    // Capture raw pointer for thread-safe package lookups (manager lives on main thread)
    LocalPackageManager* pkgMgr = m_localPkgManager;

    // Load translation packages from cached index (network-only mode)
    QString indexPath = AppPaths::manifestIndexFile();
    QString packageCache = AppPaths::packagesDir();

    (void)QtConcurrent::run([this, pkgMgr, indexPath, packageCache]() {
        MAKINE_THREAD_NAME("Worker-Scan");
#ifndef MAKINE_UI_ONLY
        // Lazy Core init — runs once in background, doesn't block UI
        ensureCoreInitialized();
#endif

        // Load translation packages from index (lightweight catalog)
        emit scanProgress(0.0, tr("Çeviri paketleri yükleniyor..."));
        if (QFile::exists(indexPath)) {
            pkgMgr->loadFromIndex(indexPath, packageCache);
        } else {
            qCDebug(lcCoreBridge) << "CoreBridge: No cached index yet, waiting for sync...";
        }

        // Collect games in a thread-local list to avoid data race on m_detectedGames
        QList<DetectedGame> games;

        // ── Store scanners ──
        emit scanProgress(0.10, tr("Steam kütüphanesi taranıyor..."));
        doScanSteamReal(games);

        emit scanProgress(0.45, tr("Epic Games taranıyor..."));
        doScanEpicReal(games);

        emit scanProgress(0.60, tr("GOG Galaxy taranıyor..."));
        doScanGogReal(games);

        // ── Filesystem & Registry scanners ──
        // Collect known install paths from store scanners (for dedup)
        QSet<QString> knownPaths;
        knownPaths.reserve(games.size());
        for (const auto& g : games)
            knownPaths.insert(QDir::cleanPath(g.installPath).toLower());

        emit scanProgress(0.75, tr("Dosya sistemi taranıyor..."));
        doScanFilesystemReal(games, knownPaths);

        // Update knownPaths with filesystem results before registry scan
        for (const auto& g : games)
            knownPaths.insert(QDir::cleanPath(g.installPath).toLower());

        emit scanProgress(0.82, tr("Kayıt defteri taranıyor..."));
        doScanRegistryReal(games, knownPaths);

        // ── Cross-scanner deduplication ──
        // Priority: steam > epic > gog > registry > filesystem
        {
            QHash<QString, int> pathIndex;
            QList<DetectedGame> unique;
            unique.reserve(games.size());

            auto sourcePriority = [](const QString& src) -> int {
                if (src == QLatin1String("steam")) return 0;
                if (src == QLatin1String("epic")) return 1;
                if (src == QLatin1String("gog")) return 2;
                if (src == QLatin1String("registry")) return 3;
                if (src == QLatin1String("filesystem")) return 4;
                return 5;
            };

            for (auto& game : games) {
                QString normPath = QDir::cleanPath(game.installPath).toLower();
                auto it = pathIndex.find(normPath);

                if (it != pathIndex.end()) {
                    auto& existing = unique[it.value()];
                    if (sourcePriority(game.source) < sourcePriority(existing.source)) {
                        existing = std::move(game);
                    }
                    continue;
                }

                pathIndex[normPath] = unique.size();
                unique.append(std::move(game));
            }

            int removed = games.size() - unique.size();
            games = std::move(unique);
            if (removed > 0) {
                qCDebug(lcCoreBridge) << "Dedup: removed" << removed << "duplicate entries";
            }
        }

        // ── Engine detection + catalog matching ──
        emit scanProgress(0.90, tr("Oyun motorları tespit ediliyor..."));
        for (auto& game : games) {
            if (game.engine.isEmpty() || game.engine == QLatin1String("Unknown")) {
                game.engine = detectEngineReal(game.installPath);
            }
            // Check translation availability via ID resolution
            if (pkgMgr) {
                QString resolved = resolveToSteamAppId(game.id);

                // For non-Steam games, storeId reverse index may
                // not be populated yet. Fall back to name-based matching.
                if (resolved.isEmpty() && game.source != QLatin1String("steam")) {
                    QDir gameDir(game.installPath);
                    resolved = pkgMgr->findMatchingAppId(gameDir.dirName());
                    // Also try the display name
                    if (resolved.isEmpty() && !game.name.isEmpty()) {
                        resolved = pkgMgr->findMatchingAppId(game.name);
                    }
                    // Last resort: fingerprint-based file matching
                    if (resolved.isEmpty() &&
                        (game.source == QLatin1String("filesystem") ||
                         game.source == QLatin1String("registry"))) {
                        QVariantList candidates = findMatchingGamesFromFiles(game.installPath);
                        if (!candidates.isEmpty()) {
                            QVariantMap best = candidates.first().toMap();
                            if (best.value(QStringLiteral("confidence")).toInt() >= 60) {
                                resolved = best.value(QStringLiteral("steamAppId")).toString();
                                qCDebug(lcCoreBridge) << "Fingerprint match for"
                                         << game.name << "->" << resolved
                                         << "confidence:" << best.value(QStringLiteral("confidence")).toInt();
                            }
                        }
                    }
                }

                if (!resolved.isEmpty()) {
                    game.steamAppId = resolved;
                    // Normalize ID to steamAppId so install/update flows
                    // can resolve without storeId reverse index
                    if (game.source != QLatin1String("steam"))
                        game.id = resolved;
                    if (pkgMgr->hasPackage(resolved)) {
                        game.hasTranslation = true;
                    }
                }
            }
        }

        // ── Second dedup pass: by steamAppId ──
        // After catalog matching, multiple folders may resolve to the same game.
        // e.g. Steam RDR2 + D:\Games\Red Dead Redemption 2 → both steamAppId 1174180
        // Keep highest priority source, drop duplicates.
        {
            QHash<QString, int> appIdIndex;
            QList<DetectedGame> unique;
            unique.reserve(games.size());

            auto sourcePriority = [](const QString& src) -> int {
                if (src == QLatin1String("steam")) return 0;
                if (src == QLatin1String("epic")) return 1;
                if (src == QLatin1String("gog")) return 2;
                if (src == QLatin1String("registry")) return 3;
                if (src == QLatin1String("filesystem")) return 4;
                return 5;
            };

            for (auto& game : games) {
                // Games without steamAppId always pass (can't dedup)
                if (game.steamAppId.isEmpty()) {
                    unique.append(std::move(game));
                    continue;
                }

                auto it = appIdIndex.find(game.steamAppId);
                if (it != appIdIndex.end()) {
                    auto& existing = unique[it.value()];
                    if (sourcePriority(game.source) < sourcePriority(existing.source)) {
                        existing = std::move(game);
                    }
                    qCDebug(lcCoreBridge) << "AppID dedup: dropping duplicate for"
                             << game.steamAppId;
                    continue;
                }

                appIdIndex[game.steamAppId] = unique.size();
                unique.append(std::move(game));
            }

            int removed = games.size() - unique.size();
            games = std::move(unique);
            if (removed > 0) {
                qCDebug(lcCoreBridge) << "AppID dedup: removed" << removed << "duplicates";
            }
        }

        const int count = games.count();

        // Move results to main thread
        QMetaObject::invokeMethod(this, [this, games = std::move(games)]() mutable {
            m_detectedGames = std::move(games);
            buildDetectedGameIndex();
        }, Qt::QueuedConnection);

        emit scanProgress(1.0, tr("%1 oyun bulundu").arg(count));
        emit scanCompleted(count);
    });
}

QString CoreBridge::detectEngine(const QString& gamePath)
{
    return detectEngineReal(gamePath);
}


// ========== ID Resolution ==========

QString CoreBridge::resolveToSteamAppId(const QString& gameId)
{
    if (!m_localPkgManager) return {};

    // Direct match
    if (m_localPkgManager->hasPackage(gameId)) return gameId;

    // Reverse lookup via storeIds
    return m_localPkgManager->resolveGameId(gameId);
}

void CoreBridge::buildDetectedGameIndex()
{
    m_steamAppIdToDetectedIndex.clear();
    for (int i = 0; i < m_detectedGames.size(); ++i) {
        const QString& steamAppId = m_detectedGames[i].steamAppId;
        if (!steamAppId.isEmpty()) {
            m_steamAppIdToDetectedIndex[steamAppId] = i;
        }
        // Also index by game.id for Epic/GOG (epic_xxx, gog_xxx)
        const QString& gameId = m_detectedGames[i].id;
        if (gameId != steamAppId && !gameId.isEmpty()) {
            QString resolved = resolveToSteamAppId(gameId);
            if (!resolved.isEmpty()) {
                m_steamAppIdToDetectedIndex[resolved] = i;
            }
        }
    }
}

// ========== Supported Games Catalog ==========

QVariantList CoreBridge::allSupportedGames() const
{
    MAKINE_ZONE_NAMED("CoreBridge::allSupportedGames");
    if (!m_localPkgManager) return {};

    QVariantList catalog = m_localPkgManager->allPackagesAsList();

    // Enrich each catalog entry with install status using O(1) hash lookup
    for (int i = 0; i < catalog.size(); ++i) {
        QVariantMap entry = catalog[i].toMap();
        const QString steamAppId = entry["steamAppId"].toString();

        auto it = m_steamAppIdToDetectedIndex.find(steamAppId);
        if (it != m_steamAppIdToDetectedIndex.end()) {
            const auto& game = m_detectedGames[it.value()];
            entry["isInstalled"] = true;
            entry["installPath"] = game.installPath;
            entry["id"] = game.id;
            entry["source"] = game.source;
        } else {
            entry["isInstalled"] = false;
            entry["id"] = steamAppId;
        }

        // All catalog games have translation available and are verified
        entry["hasTranslation"] = true;
        entry["isVerified"] = true;
        entry["name"] = entry["gameName"];

        catalog[i] = entry;
    }

    return catalog;
}

// ========== Package Management (via LocalPackageManager) ==========

bool CoreBridge::hasTranslationPackage(const QString& gameId)
{
    MAKINE_ZONE_NAMED("CoreBridge::hasTranslationPackage");
    if (!m_localPkgManager) return false;

    QString resolved = resolveToSteamAppId(gameId);
    return !resolved.isEmpty() && m_localPkgManager->hasPackage(resolved);
}

std::optional<TranslationPackageQt> CoreBridge::getPackageForGame(const QString& gameId)
{
    MAKINE_ZONE_NAMED("CoreBridge::getPackageForGame");
    if (!m_localPkgManager) return std::nullopt;

    QString resolved = resolveToSteamAppId(gameId);
    if (resolved.isEmpty()) return std::nullopt;

    auto pkg = m_localPkgManager->getPackage(resolved);
    if (!pkg) return std::nullopt;

    TranslationPackageQt qtPkg;
    qtPkg.packageId = pkg->packageId;
    qtPkg.gameId = pkg->steamAppId;
    qtPkg.gameName = pkg->gameName;
    qtPkg.version = pkg->version;
    qtPkg.sizeBytes = pkg->sizeBytes;
    qtPkg.requiresRuntime = false;
    qtPkg.contributors = pkg->contributors;
    return qtPkg;
}

QString CoreBridge::getPackageDirName(const QString& steamAppId) const
{
    if (!m_localPkgManager) return {};
    auto pkg = m_localPkgManager->getPackage(steamAppId);
    return pkg ? pkg->dirName : QString{};
}

void CoreBridge::installPackage(const QString& packageId, const QString& gamePath,
                                const QString& variant, const QStringList& selectedOptions)
{
    MAKINE_ZONE_NAMED("CoreBridge::installPackage");
    if (!m_localPkgManager) {
        emit packageInstallCompleted(false, tr("Paket yöneticisi başlatılamadı"));
        return;
    }

    QString resolved = resolveToSteamAppId(packageId);
    if (!resolved.isEmpty() && m_localPkgManager->hasPackage(resolved)) {
        m_localPkgManager->installPackage(resolved, gamePath, variant, selectedOptions);
    } else {
        emit packageInstallCompleted(false, tr("Paket bulunamadı: %1").arg(packageId));
    }
}

void CoreBridge::updatePackage(const QString& packageId, const QString& gamePath,
                               const QString& variant, const QStringList& selectedOptions)
{
    MAKINE_ZONE_NAMED("CoreBridge::updatePackage");
    if (!m_localPkgManager) {
        emit packageInstallCompleted(false, tr("Paket yöneticisi başlatılamadı"));
        return;
    }

    QString resolved = resolveToSteamAppId(packageId);
    if (!resolved.isEmpty() && m_localPkgManager->hasPackage(resolved)) {
        m_localPkgManager->updatePackage(resolved, gamePath, variant, selectedOptions);
    } else {
        emit packageInstallCompleted(false, tr("Paket bulunamadı: %1").arg(packageId));
    }
}

void CoreBridge::cancelInstall()
{
    if (m_localPkgManager)
        m_localPkgManager->cancelInstall();
}

QVariantList CoreBridge::getVariantsForGame(const QString& gameId)
{
    MAKINE_ZONE_NAMED("CoreBridge::getVariantsForGame");
    if (!m_localPkgManager) return {};

    QString resolved = resolveToSteamAppId(gameId);
    if (resolved.isEmpty()) return {};

    return m_localPkgManager->getVariants(resolved);
}

QString CoreBridge::getVariantTypeForGame(const QString& gameId)
{
    MAKINE_ZONE_NAMED("CoreBridge::getVariantTypeForGame");
    if (!m_localPkgManager) return {};

    QString resolved = resolveToSteamAppId(gameId);
    if (resolved.isEmpty()) return {};

    return m_localPkgManager->getVariantType(resolved);
}

QString CoreBridge::getInstallNotesForGame(const QString& gameId)
{
    MAKINE_ZONE_NAMED("CoreBridge::getInstallNotesForGame");
    if (!m_localPkgManager) return {};

    QString resolved = resolveToSteamAppId(gameId);
    if (resolved.isEmpty()) return {};

    auto pkg = m_localPkgManager->getPackage(resolved);
    if (!pkg) return {};

    return pkg->installNotes;
}

QVariantList CoreBridge::getInstallOptionsForGame(const QString& gameId)
{
    MAKINE_ZONE_NAMED("CoreBridge::getInstallOptionsForGame");
    if (!m_localPkgManager) return {};

    QString resolved = resolveToSteamAppId(gameId);
    if (resolved.isEmpty()) return {};

    auto pkg = m_localPkgManager->getPackage(resolved);
    if (!pkg || pkg->installOptions.isEmpty()) return {};

    QVariantList result;
    for (const auto& opt : pkg->installOptions) {
        result.append(QVariantMap{
            {"id", opt.id},
            {"label", opt.label},
            {"description", opt.description},
            {"icon", opt.icon},
            {"defaultSelected", opt.defaultSelected},
            {"subDir", opt.subDir}
        });
    }
    return result;
}

QString CoreBridge::getSpecialDialogForGame(const QString& gameId)
{
    MAKINE_ZONE_NAMED("CoreBridge::getSpecialDialogForGame");
    if (!m_localPkgManager) return {};

    QString resolved = resolveToSteamAppId(gameId);
    if (resolved.isEmpty()) return {};

    auto pkg = m_localPkgManager->getPackage(resolved);
    if (!pkg) return {};

    return pkg->specialDialog;
}

QVariantList CoreBridge::getVariantInstallOptionsForGame(const QString& gameId, const QString& variant)
{
    MAKINE_ZONE_NAMED("CoreBridge::getVariantInstallOptionsForGame");
    if (!m_localPkgManager) return {};

    QString resolved = resolveToSteamAppId(gameId);
    if (resolved.isEmpty()) return {};

    auto pkg = m_localPkgManager->getPackage(resolved);
    if (!pkg || !pkg->variantInstallOptions.contains(variant)) return {};

    QVariantList result;
    for (const auto& opt : pkg->variantInstallOptions[variant].installOptions) {
        result.append(QVariantMap{
            {"id", opt.id},
            {"label", opt.label},
            {"description", opt.description},
            {"icon", opt.icon},
            {"defaultSelected", opt.defaultSelected},
            {"subDir", opt.subDir}
        });
    }
    return result;
}

QString CoreBridge::getVariantSpecialDialogForGame(const QString& gameId, const QString& variant)
{
    MAKINE_ZONE_NAMED("CoreBridge::getVariantSpecialDialogForGame");
    Q_UNUSED(variant);
    // Variant-specific specialDialog not yet supported; return package-level
    return getSpecialDialogForGame(gameId);
}

QStringList CoreBridge::getPackageFileList(const QString& gameId, const QString& variant)
{
    MAKINE_ZONE_NAMED("CoreBridge::getPackageFileList");
    if (!m_localPkgManager) return {};

    QString resolved = resolveToSteamAppId(gameId);
    if (resolved.isEmpty()) return {};

    return m_localPkgManager->getPackageFileList(resolved, variant);
}

QString CoreBridge::findMatchingAppId(const QString& folderName)
{
    MAKINE_ZONE_NAMED("CoreBridge::findMatchingAppId");
    if (!m_localPkgManager) return {};
    return m_localPkgManager->findMatchingAppId(folderName);
}

QVariantList CoreBridge::findMatchingGamesFromFiles(const QString& gamePath)
{
    MAKINE_ZONE_NAMED("CoreBridge::findMatchingGamesFromFiles");
    if (!m_localPkgManager) return {};

    QDir dir(gamePath);
    if (!dir.exists()) return {};

    // Collect exe names and top-level entries from the game folder
    QStringList exeNames;
    QStringList topEntries;
    const auto entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto& info : entries) {
        topEntries.append(info.fileName());
        if (info.isFile() && info.suffix().toLower() == QStringLiteral("exe")) {
            const QString name = info.fileName().toLower();
            // Skip known non-game executables
            if (!name.contains(QStringLiteral("launcher")) &&
                !name.contains(QStringLiteral("crash")) &&
                !name.contains(QStringLiteral("unins")) &&
                !name.contains(QStringLiteral("redist")) &&
                !name.contains(QStringLiteral("setup")) &&
                !name.contains(QStringLiteral("dxsetup")) &&
                !name.contains(QStringLiteral("vcredist")) &&
                !name.contains(QStringLiteral("dotnet"))) {
                exeNames.append(name);
            }
        }
    }

    // Detect engine
    QString engine = detectEngineReal(gamePath);

    const QString folderName = QFileInfo(gamePath).fileName();
    return m_localPkgManager->findMatchingGamesFromFiles(exeNames, engine, topEntries, folderName);
}

bool CoreBridge::hasTranslationUpdate(const QString& gameId)
{
    MAKINE_ZONE_NAMED("CoreBridge::hasTranslationUpdate");
    if (!m_localPkgManager) return false;

    QString resolved = resolveToSteamAppId(gameId);
    if (resolved.isEmpty()) return false;

    // Must be installed to have an update
    auto installed = m_localPkgManager->getInstalledInfo(resolved);
    if (!installed) return false;

    // Compare installed version vs catalog version
    auto pkg = m_localPkgManager->getPackage(resolved);
    if (!pkg) return false;

    return !installed->version.isEmpty()
        && !pkg->version.isEmpty()
        && installed->version != pkg->version;
}

bool CoreBridge::isPackageInstalled(const QString& gameId)
{
    MAKINE_ZONE_NAMED("CoreBridge::isPackageInstalled");
    if (!m_localPkgManager) return false;

    QString resolved = resolveToSteamAppId(gameId);
    return !resolved.isEmpty() && m_localPkgManager->isInstalled(resolved);
}

std::optional<InstalledPackageInfo> CoreBridge::getInstalledInfo(const QString& gameId)
{
    if (!m_localPkgManager) return std::nullopt;
    QString resolved = resolveToSteamAppId(gameId);
    if (resolved.isEmpty()) return std::nullopt;
    return m_localPkgManager->getInstalledInfo(resolved);
}

void CoreBridge::updateInstalledStoreVersion(const QString& gameId, const QString& storeVersion,
                                               const QString& source)
{
    if (!m_localPkgManager) return;
    QString resolved = resolveToSteamAppId(gameId);
    if (resolved.isEmpty()) return;
    m_localPkgManager->updateInstalledStoreVersion(resolved, storeVersion, source);
}

bool CoreBridge::uninstallPackage(const QString& gameId, const QString& gamePath)
{
    MAKINE_ZONE_NAMED("CoreBridge::uninstallPackage");
    if (!m_localPkgManager) return false;

    QString resolved = resolveToSteamAppId(gameId);
    if (resolved.isEmpty()) return false;

    return m_localPkgManager->uninstallPackage(resolved, gamePath);
}

void CoreBridge::refreshPackageManifest()
{
    MAKINE_ZONE_NAMED("CoreBridge::refreshPackageManifest");
    // Lazy-init LocalPackageManager so catalog is available even
    // before the first scanAllLibraries() call
    if (!m_localPkgManager) {
        m_localPkgManager = new LocalPackageManager(this);
        if (m_journal) m_localPkgManager->setJournal(m_journal);
        connect(m_localPkgManager, &LocalPackageManager::installProgress,
                this, &CoreBridge::packageInstallProgress);
        connect(m_localPkgManager, &LocalPackageManager::installCompleted,
                this, &CoreBridge::packageInstallCompleted);
        qCDebug(lcCoreBridge) << "LocalPackageManager created early (via refreshPackageManifest)";
    }

    // Reload from cached index.json (lightweight catalog)
    QString indexPath = AppPaths::manifestIndexFile();
    QString packageCache = AppPaths::packagesDir();
    if (QFile::exists(indexPath)) {
        m_localPkgManager->loadFromIndex(indexPath, packageCache);
    } else {
        qCDebug(lcCoreBridge) << "CoreBridge::refreshPackageManifest: No cached index available";
    }

    // Batch-load all cached per-game details so storeIds + fingerprints
    // are available for Epic/GOG game resolution without network requests
    const QString detailDir = AppPaths::packageDetailDir();
    QDir dir(detailDir);
    if (dir.exists()) {
        const auto detailFiles = dir.entryList({"*.json"}, QDir::Files);
        int enriched = 0;
        for (const QString& fileName : detailFiles) {
            QString appId = fileName.chopped(5);  // remove ".json"
            if (m_localPkgManager->isDetailLoaded(appId))
                continue;

            QFile file(dir.absoluteFilePath(fileName));
            if (file.open(QIODevice::ReadOnly)) {
                m_localPkgManager->enrichPackageDetail(appId, file.readAll());
                ++enriched;
            }
        }
        if (enriched > 0) {
            qCDebug(lcCoreBridge) << "Batch-loaded" << enriched << "cached game details"
                       << "(storeIds + fingerprints now available)";
        }
    }

    int count = m_localPkgManager->packageCount();
    emit packageManifestRefreshed(count);
}

bool CoreBridge::ensurePackageDetail(const QString& steamAppId)
{
    if (!m_localPkgManager) return false;

    // Already enriched in catalog?
    if (m_localPkgManager->isDetailLoaded(steamAppId))
        return true;

    // Check disk cache
    QString cachePath = AppPaths::packageDetailDir() + QStringLiteral("/%1.json").arg(steamAppId);
    if (QFile::exists(cachePath)) {
        QFile file(cachePath);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            enrichPackageFromJson(steamAppId, data);
            return true;
        }
    }

    // Caller needs to trigger ManifestSync.fetchPackageDetail()
    return false;
}

bool CoreBridge::isPackageDetailLoaded(const QString& steamAppId)
{
    if (!m_localPkgManager) return false;
    return m_localPkgManager->isDetailLoaded(steamAppId);
}

void CoreBridge::enrichPackageFromJson(const QString& steamAppId, const QByteArray& jsonData)
{
    if (!m_localPkgManager) return;

    if (m_localPkgManager->enrichPackageDetail(steamAppId, jsonData)) {
        emit packageDetailEnriched(steamAppId);
    }
}

} // namespace makine
