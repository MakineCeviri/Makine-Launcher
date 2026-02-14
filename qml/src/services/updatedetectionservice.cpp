/**
 * @file updatedetectionservice.cpp
 * @brief Two-tier game update detection implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "updatedetectionservice.h"
#include "vdfparser.h"
#include "gameservice.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>

namespace {
constexpr int kMonitorIntervalMs = 30 * 60 * 1000; // 30 minutes
}
#include <QStandardPaths>
#include <QtConcurrent>
#include <QDebug>

namespace makineai {

// =============================================================================
// Engine Profiles — Rule-based file collection
// =============================================================================

EngineProfile UpdateDetectionService::profileForEngine(const QString& engine)
{
    using Rule = EngineProfile::Rule;
    const QString e = engine.toLower();

    if (e.contains("unity")) {
        return {
            {
                Rule{"", "globalgamemanagers", false},        // *_Data/ resolved at collection time
                Rule{"", "GameAssembly.dll", false},
                Rule{"", "UnityPlayer.dll", false},
                Rule{"", "*.assets", true},                   // *_Data/**/*.assets
                Rule{"", "*.dll", false},                     // Managed DLLs in *_Data/Managed/
            },
            {"StreamingAssets", "Saves", "Temp", "Logs"},
            80
        };
    }
    if (e.contains("unreal")) {
        return {
            {
                Rule{"Content/Paks", "*.pak", false},
                Rule{"Content/Localization", "*", true},
                Rule{"", "*.exe", false},
            },
            {"Saved", "Config", "Logs", "CrashReportClient"},
            50
        };
    }
    if (e.contains("renpy") || e.contains("ren'py")) {
        return {
            {
                Rule{"game", "*.rpa", false},
                Rule{"game", "*.rpy", false},
                Rule{"game", "*.rpyc", false},
            },
            {"saves", "tmp", "cache"},
            100
        };
    }
    if (e.contains("rpgmaker") || e.contains("rpg maker")) {
        return {
            {
                Rule{"www/data", "*.json", false},
                Rule{"www/js", "*.js", false},
                Rule{"data", "*.rvdata2", false},
                Rule{"data", "*.json", false},
            },
            {"save", "www/save"},
            100
        };
    }
    if (e.contains("bethesda") || e.contains("creation")) {
        return {
            {
                Rule{"Data", "*.esm", false},
                Rule{"Data", "*.esp", false},
                Rule{"Data/Strings", "*", false},
            },
            {"Saves", "Data/SKSE", "Data/F4SE"},
            60
        };
    }
    if (e.contains("gamemaker") || e.contains("game maker")) {
        return {
            {
                Rule{"", "data.win", false},
                Rule{"", "*.exe", false},
            },
            {"saves"},
            10
        };
    }
    if (e.contains("godot")) {
        return {
            {
                Rule{"", "*.pck", false},
                Rule{"", "*.exe", false},
            },
            {"logs"},
            10
        };
    }

    // Other engines - minimal tracking
    return {
        {
            Rule{"", "*.exe", false},
            Rule{"", "*.dll", false},
        },
        {"logs", "saves", "config", "cache"},
        10
    };
}

// =============================================================================
// File Collection — Proper directory+filter matching
// =============================================================================

QStringList UpdateDetectionService::collectFiles(const QString& installPath,
                                                  const EngineProfile& profile)
{
    QSet<QString> result;
    QDir baseDir(installPath);
    if (!baseDir.exists()) return {};

    auto isIgnoredDir = [&](const QString& relPath) -> bool {
        for (const auto& ignored : profile.ignoredDirs) {
            if (relPath.startsWith(ignored + "/", Qt::CaseInsensitive) ||
                relPath.compare(ignored, Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
        return false;
    };

    for (const auto& rule : profile.rules) {
        QString searchDir;
        if (rule.directory.isEmpty()) {
            searchDir = installPath;
        } else {
            searchDir = installPath + "/" + rule.directory;
        }

        // For Unity: if directory is empty and filter is specific (globalgamemanagers, *.assets),
        // we need to find *_Data subdirectories first
        if (rule.directory.isEmpty() &&
            (rule.nameFilter == "globalgamemanagers" || rule.nameFilter == "*.assets")) {
            // Find *_Data directories
            const auto entries = baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const auto& entry : entries) {
                if (entry.endsWith("_Data")) {
                    QString dataDir = installPath + "/" + entry;
                    QDir dataDirObj(dataDir);

                    if (rule.nameFilter == "globalgamemanagers") {
                        QString target = dataDir + "/globalgamemanagers";
                        if (QFileInfo::exists(target)) {
                            result.insert(baseDir.relativeFilePath(target));
                        }
                    } else {
                        // *.assets - search in _Data and subdirectories
                        auto flags = rule.recurse ? QDirIterator::Subdirectories
                                                  : QDirIterator::NoIteratorFlags;
                        QDirIterator it(dataDir, {rule.nameFilter}, QDir::Files, flags);
                        while (it.hasNext()) {
                            it.next();
                            QString rel = baseDir.relativeFilePath(it.filePath());
                            if (!isIgnoredDir(rel)) {
                                result.insert(rel);
                            }
                        }
                    }
                }
            }
            continue;
        }

        // For Unity Managed DLLs: look in *_Data/Managed/
        if (rule.directory.isEmpty() && rule.nameFilter == "*.dll") {
            // Root-level DLLs
            QDirIterator rootIt(installPath, {"*.dll"}, QDir::Files, QDirIterator::NoIteratorFlags);
            while (rootIt.hasNext()) {
                rootIt.next();
                result.insert(baseDir.relativeFilePath(rootIt.filePath()));
            }
            // Also check *_Data/Managed/
            const auto entries = baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const auto& entry : entries) {
                if (entry.endsWith("_Data")) {
                    QString managedDir = installPath + "/" + entry + "/Managed";
                    if (QDir(managedDir).exists()) {
                        QDirIterator mIt(managedDir, {"*.dll"}, QDir::Files);
                        while (mIt.hasNext()) {
                            mIt.next();
                            result.insert(baseDir.relativeFilePath(mIt.filePath()));
                        }
                    }
                }
            }
            continue;
        }

        if (!QDir(searchDir).exists()) continue;

        auto flags = rule.recurse ? QDirIterator::Subdirectories
                                  : QDirIterator::NoIteratorFlags;
        QDirIterator it(searchDir, {rule.nameFilter}, QDir::Files, flags);
        while (it.hasNext()) {
            it.next();
            QString rel = baseDir.relativeFilePath(it.filePath());
            if (!isIgnoredDir(rel)) {
                result.insert(rel);
            }
        }
    }

    // Sort and limit
    QStringList sorted = result.values();
    sorted.sort();
    if (sorted.size() > profile.maxFiles) {
        sorted.resize(profile.maxFiles);
    }

    return sorted;
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

UpdateDetectionService::UpdateDetectionService(QObject *parent)
    : QObject(parent)
    , m_monitorTimer(new QTimer(this))
{
    m_monitorTimer->setInterval(kMonitorIntervalMs);
    connect(m_monitorTimer, &QTimer::timeout, this, &UpdateDetectionService::checkAllGamesQuick);
    loadStoreVersions();
}

UpdateDetectionService::~UpdateDetectionService() = default;

// =============================================================================
// Tier 1: Store Metadata Readers (all static/thread-safe)
// =============================================================================

QString UpdateDetectionService::readSteamBuildId(const QString& installPath,
                                                   const QString& steamAppId)
{
    if (!steamAppId.isEmpty()) {
        // Direct lookup: find appmanifest_{appId}.acf — O(1)
        QDir dir(installPath);
        if (!dir.cdUp()) return {}; // go to "common"
        if (!dir.cdUp()) return {}; // go to "steamapps"

        QString acfPath = dir.absolutePath() + "/appmanifest_" + steamAppId + ".acf";
        QFile file(acfPath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            auto root = vdf::parse(file.readAll().toStdString());
            file.close();
            if (root) {
                const auto* appState = root->find("AppState");
                if (!appState) appState = &(*root);
                return QString::fromStdString(appState->getString("buildid"));
            }
        }
    }

    // Fallback: scan all ACFs (for manual/unknown appId games)
    QDir dir(installPath);
    if (!dir.cdUp()) return {};
    if (!dir.cdUp()) return {};

    const QString steamappsPath = dir.absolutePath();
    const QString dirName = QDir(installPath).dirName();
    const auto acfFiles = dir.entryList({"appmanifest_*.acf"}, QDir::Files);

    for (const auto& acfFile : acfFiles) {
        QFile file(steamappsPath + "/" + acfFile);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;

        auto root = vdf::parse(file.readAll().toStdString());
        file.close();
        if (!root) continue;

        const auto* appState = root->find("AppState");
        if (!appState) appState = &(*root);

        QString installDir = QString::fromStdString(appState->getString("installdir"));
        if (installDir.compare(dirName, Qt::CaseInsensitive) == 0) {
            return QString::fromStdString(appState->getString("buildid"));
        }
    }
    return {};
}

QString UpdateDetectionService::readEpicVersion(const QString& installPath)
{
    const QString manifestDir = "C:/ProgramData/Epic/EpicGamesLauncher/Data/Manifests";
    QDir dir(manifestDir);
    if (!dir.exists()) return {};

    const QString cleanPath = QDir::cleanPath(installPath);
    const auto itemFiles = dir.entryList({"*.item"}, QDir::Files);
    for (const auto& itemFile : itemFiles) {
        QFile file(dir.absoluteFilePath(itemFile));
        if (!file.open(QIODevice::ReadOnly)) continue;

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
        file.close();

        if (err.error != QJsonParseError::NoError) continue;

        QJsonObject obj = doc.object();
        if (QDir::cleanPath(obj["InstallLocation"].toString()).compare(cleanPath, Qt::CaseInsensitive) == 0) {
            return obj["AppVersionString"].toString();
        }
    }
    return {};
}

QString UpdateDetectionService::readGogVersion(const QString& gameId)
{
    if (!gameId.startsWith("gog_")) return {};
    QString gogKey = gameId.mid(4);

    QSettings gogReg("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\GOG.com\\Games\\" + gogKey,
                     QSettings::NativeFormat);

    QString ver = gogReg.value("ver").toString();
    if (ver.isEmpty()) {
        ver = gogReg.value("buildId").toString();
    }
    return ver;
}

qint64 UpdateDetectionService::readExeMtime(const QString& installPath)
{
    QDir dir(installPath);
    const auto exeFiles = dir.entryList({"*.exe"}, QDir::Files);
    qint64 latestMtime = 0;

    for (const auto& exeFile : exeFiles) {
        QFileInfo info(dir.absoluteFilePath(exeFile));
        qint64 mtime = info.lastModified().toSecsSinceEpoch();
        if (mtime > latestMtime) {
            latestMtime = mtime;
        }
    }
    return latestMtime;
}

// =============================================================================
// Store Version Recording
// =============================================================================

StoreVersionRecord UpdateDetectionService::readCurrentStoreVersion(
    const QString& gameId, const QString& installPath,
    const QString& source, const QString& steamAppId)
{
    StoreVersionRecord record;
    record.gameId = gameId;
    record.recordedAt = QDateTime::currentSecsSinceEpoch();

    if (source == "steam") {
        record.steamBuildId = readSteamBuildId(installPath, steamAppId);
    } else if (source == "epic") {
        record.epicVersionString = readEpicVersion(installPath);
    } else if (source == "gog") {
        record.gogBuildId = readGogVersion(gameId);
    }

    record.exeLastModified = readExeMtime(installPath);
    return record;
}

void UpdateDetectionService::recordStoreVersion(const QString& gameId,
                                                  const QString& installPath,
                                                  const QString& source)
{
    // Get steamAppId from GameService if available
    QString steamAppId;
    if (m_gameService) {
        QVariantMap data = m_gameService->getGameById(gameId);
        steamAppId = data["steamAppId"].toString();
    }

    auto record = readCurrentStoreVersion(gameId, installPath, source, steamAppId);

    {
        QMutexLocker lock(&m_storeVersionsMutex);
        m_storeVersions[gameId] = record;
    }
    saveStoreVersions();

    qDebug() << "Recorded store version for" << gameId
             << "steam:" << record.steamBuildId
             << "epic:" << record.epicVersionString
             << "gog:" << record.gogBuildId
             << "exeMtime:" << record.exeLastModified;
}

void UpdateDetectionService::removeStoreVersion(const QString& gameId)
{
    bool removed = false;
    {
        QMutexLocker lock(&m_storeVersionsMutex);
        removed = m_storeVersions.remove(gameId);
    }
    if (removed) {
        saveStoreVersions();
    }
}

// =============================================================================
// Tier 1: Quick Check — Real background I/O
// =============================================================================

void UpdateDetectionService::checkGameQuick(const QString& gameId)
{
    if (!m_gameService) return;

    QVariantMap gameData = m_gameService->getGameById(gameId);
    if (gameData.isEmpty()) return;

    StoreVersionRecord saved;
    {
        QMutexLocker lock(&m_storeVersionsMutex);
        if (!m_storeVersions.contains(gameId)) return;
        saved = m_storeVersions[gameId]; // copy under lock
    }

    const QString installPath = gameData["installPath"].toString();
    const QString source = gameData["source"].toString();
    const QString gameName = gameData["name"].toString();
    const QString steamAppId = gameData["steamAppId"].toString();

    auto current = readCurrentStoreVersion(gameId, installPath, source, steamAppId);

    bool changed = false;
    QString summary;

    if (!saved.steamBuildId.isEmpty() && !current.steamBuildId.isEmpty()) {
        if (saved.steamBuildId != current.steamBuildId) {
            changed = true;
            summary = tr("Steam build ID changed: %1 -> %2")
                .arg(saved.steamBuildId, current.steamBuildId);
        }
    } else if (!saved.epicVersionString.isEmpty() && !current.epicVersionString.isEmpty()) {
        if (saved.epicVersionString != current.epicVersionString) {
            changed = true;
            summary = tr("Epic version changed: %1 -> %2")
                .arg(saved.epicVersionString, current.epicVersionString);
        }
    } else if (!saved.gogBuildId.isEmpty() && !current.gogBuildId.isEmpty()) {
        if (saved.gogBuildId != current.gogBuildId) {
            changed = true;
            summary = tr("GOG version changed: %1 -> %2")
                .arg(saved.gogBuildId, current.gogBuildId);
        }
    }

    if (!changed && saved.exeLastModified > 0 && current.exeLastModified > 0) {
        if (current.exeLastModified != saved.exeLastModified) {
            changed = true;
            summary = tr("Game executable was modified");
        }
    }

    if (changed) {
        emit gameUpdateDetected(gameId, gameName, summary);
    }
}

void UpdateDetectionService::checkAllGamesQuick()
{
    if (m_isChecking) return;
    if (!m_gameService) return;

    m_isChecking = true;
    emit isCheckingChanged();

    // Snapshot data under lock, then release
    struct GameCheckInfo {
        QString gameId;
        QString installPath;
        QString source;
        QString gameName;
        QString steamAppId;
        StoreVersionRecord saved;
    };

    QList<GameCheckInfo> checkList;
    {
        QMutexLocker lock(&m_storeVersionsMutex);
        for (auto it = m_storeVersions.constBegin(); it != m_storeVersions.constEnd(); ++it) {
            QVariantMap data = m_gameService->getGameById(it.key());
            if (data.isEmpty()) continue;

            GameCheckInfo info;
            info.gameId = it.key();
            info.installPath = data["installPath"].toString();
            info.source = data["source"].toString();
            info.gameName = data["name"].toString();
            info.steamAppId = data["steamAppId"].toString();
            info.saved = it.value();
            checkList.append(info);
        }
    }

    // Do all file I/O in background thread
    (void)QtConcurrent::run([this, checkList]() {
        struct UpdateResult {
            QString gameId;
            QString gameName;
            QString summary;
        };
        QList<UpdateResult> updates;

        for (const auto& info : checkList) {
            auto current = readCurrentStoreVersion(
                info.gameId, info.installPath, info.source, info.steamAppId);

            bool changed = false;
            QString summary;

            if (!info.saved.steamBuildId.isEmpty() && !current.steamBuildId.isEmpty()) {
                if (info.saved.steamBuildId != current.steamBuildId) {
                    changed = true;
                    summary = QCoreApplication::translate("UpdateDetectionService",
                        "Steam build ID changed: %1 -> %2")
                        .arg(info.saved.steamBuildId, current.steamBuildId);
                }
            } else if (!info.saved.epicVersionString.isEmpty() && !current.epicVersionString.isEmpty()) {
                if (info.saved.epicVersionString != current.epicVersionString) {
                    changed = true;
                    summary = QCoreApplication::translate("UpdateDetectionService",
                        "Epic version changed: %1 -> %2")
                        .arg(info.saved.epicVersionString, current.epicVersionString);
                }
            } else if (!info.saved.gogBuildId.isEmpty() && !current.gogBuildId.isEmpty()) {
                if (info.saved.gogBuildId != current.gogBuildId) {
                    changed = true;
                    summary = QCoreApplication::translate("UpdateDetectionService",
                        "GOG version changed: %1 -> %2")
                        .arg(info.saved.gogBuildId, current.gogBuildId);
                }
            }

            if (!changed && info.saved.exeLastModified > 0 && current.exeLastModified > 0) {
                if (current.exeLastModified != info.saved.exeLastModified) {
                    changed = true;
                    summary = QCoreApplication::translate("UpdateDetectionService",
                        "Game executable was modified");
                }
            }

            if (changed) {
                updates.append({info.gameId, info.gameName, summary});
            }
        }

        // Emit results on main thread
        QMetaObject::invokeMethod(this, [this, updates]() {
            m_gamesWithUpdates = updates.size();
            emit gamesWithUpdatesChanged();

            for (const auto& u : updates) {
                emit gameUpdateDetected(u.gameId, u.gameName, u.summary);
            }

            m_isChecking = false;
            emit isCheckingChanged();
        }, Qt::QueuedConnection);
    });
}

// =============================================================================
// Tier 2: File Hashing & Snapshots
// =============================================================================

QString UpdateDetectionService::computeFileHash(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return {};

    QCryptographicHash hasher(QCryptographicHash::Sha256);
    constexpr qint64 chunkSize = 65536;
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(chunkSize);
        if (chunk.isEmpty()) break;
        hasher.addData(chunk);
    }
    return hasher.result().toHex().toLower();
}

QList<FileHashRecord> UpdateDetectionService::hashGameFiles(
    const QString& installPath, const EngineProfile& profile)
{
    QList<FileHashRecord> records;
    QDir baseDir(installPath);
    if (!baseDir.exists()) return records;

    QStringList matchingFiles = collectFiles(installPath, profile);

    records.reserve(matchingFiles.size());
    for (const auto& relPath : matchingFiles) {
        QString absPath = baseDir.absoluteFilePath(relPath);
        QFileInfo info(absPath);

        FileHashRecord rec;
        rec.relativePath = relPath;
        rec.fileSize = info.size();
        rec.lastModified = info.lastModified().toSecsSinceEpoch();
        rec.sha256 = computeFileHash(absPath);
        records.append(rec);
    }

    return records;
}

void UpdateDetectionService::takeSnapshot(const QString& gameId, const QString& patchVersion,
                                           const QString& installPath, const QString& engine)
{
    EngineProfile profile = profileForEngine(engine);
    QString dir = dataDir();

    (void)QtConcurrent::run([this, gameId, patchVersion, installPath, profile, dir]() {
        auto files = hashGameFiles(installPath, profile);

        GameSnapshot snapshot;
        snapshot.gameId = gameId;
        snapshot.patchVersion = patchVersion;
        snapshot.takenAt = QDateTime::currentSecsSinceEpoch();
        snapshot.files = files;

        saveSnapshot(dir, snapshot);

        QMetaObject::invokeMethod(this, [this, gameId, count = files.size()]() {
            emit snapshotTaken(gameId, count);
            qDebug() << "Snapshot taken for" << gameId << "with" << count << "files";
        }, Qt::QueuedConnection);
    });
}

bool UpdateDetectionService::hasSnapshot(const QString& gameId)
{
    return QFile::exists(dataDir() + "/snapshots/" + gameId + ".json");
}

void UpdateDetectionService::removeSnapshot(const QString& gameId)
{
    QFile::remove(dataDir() + "/snapshots/" + gameId + ".json");
}

QVariantMap UpdateDetectionService::checkCompatibility(const QString& gameId)
{
    QVariantMap result = {
        {"level", "unknown"},
        {"integrityPercent", 100},
        {"modifiedCount", 0},
        {"addedCount", 0},
        {"removedCount", 0},
        {"summary", tr("No translation snapshot available yet")}
    };

    if (!hasSnapshot(gameId)) return result;

    GameSnapshot snapshot;
    loadSnapshot(dataDir(), gameId, snapshot);
    if (snapshot.files.isEmpty()) return result;

    if (!m_gameService) return result;

    QVariantMap gameData = m_gameService->getGameById(gameId);
    if (gameData.isEmpty()) return result;

    const QString installPath = gameData["installPath"].toString();
    const QString engine = gameData["engine"].toString();
    QDir baseDir(installPath);

    if (!baseDir.exists()) {
        result["level"] = "unknown";
        result["summary"] = tr("Game install path not found");
        return result;
    }

    int unchanged = 0;
    int modified = 0;
    int removed = 0;

    for (const auto& savedFile : snapshot.files) {
        QString absPath = baseDir.absoluteFilePath(savedFile.relativePath);
        QFileInfo info(absPath);

        if (!info.exists()) {
            ++removed;
            continue;
        }

        // mtime pre-filter
        qint64 currentMtime = info.lastModified().toSecsSinceEpoch();
        if (currentMtime == savedFile.lastModified && info.size() == savedFile.fileSize) {
            ++unchanged;
            continue;
        }

        // mtime changed - compute hash
        QString currentHash = computeFileHash(absPath);
        if (currentHash == savedFile.sha256) {
            ++unchanged;
        } else {
            ++modified;
        }
    }

    // Check for new files
    EngineProfile profile = profileForEngine(engine);
    QSet<QString> snapshotPaths;
    snapshotPaths.reserve(snapshot.files.size());
    for (const auto& f : snapshot.files) {
        snapshotPaths.insert(f.relativePath);
    }

    QStringList currentFiles = collectFiles(installPath, profile);
    int added = 0;
    for (const auto& f : currentFiles) {
        if (!snapshotPaths.contains(f)) {
            ++added;
        }
    }

    int total = snapshot.files.size();
    int integrityPercent = total > 0 ? qRound(100.0 * unchanged / total) : 100;

    QString level;
    QString summary;

    if (modified == 0 && removed == 0 && added == 0) {
        level = "compatible";
        summary = tr("All %n file(s) match the translation snapshot", "", total);
    } else if (integrityPercent >= 80) {
        level = "partial";
        summary = tr("%1 of %2 files unchanged. Translation may need minor updates.")
            .arg(unchanged).arg(total);
    } else {
        level = "incompatible";
        summary = tr("Significant changes detected. Translation likely needs updating.");
    }

    result["level"] = level;
    result["integrityPercent"] = integrityPercent;
    result["modifiedCount"] = modified;
    result["addedCount"] = added;
    result["removedCount"] = removed;
    result["summary"] = summary;

    emit compatibilityChecked(gameId, result);
    return result;
}

// =============================================================================
// Background Monitoring
// =============================================================================

void UpdateDetectionService::startMonitoring()
{
    if (m_monitoringActive) return;

    m_monitoringActive = true;
    m_monitorTimer->start();
    emit monitoringActiveChanged();
    qDebug() << "Update detection monitoring started (interval:" << m_monitorTimer->interval() / 1000 << "s)";
}

void UpdateDetectionService::stopMonitoring()
{
    if (!m_monitoringActive) return;

    m_monitoringActive = false;
    m_monitorTimer->stop();
    emit monitoringActiveChanged();
    qDebug() << "Update detection monitoring stopped";
}

// =============================================================================
// Persistence
// =============================================================================

QString UpdateDetectionService::dataDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/update_detection";
}

void UpdateDetectionService::loadStoreVersions()
{
    QFile file(dataDir() + "/store_versions.json");
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    file.close();

    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;

    QMutexLocker lock(&m_storeVersionsMutex);
    const QJsonObject root = doc.object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        const QJsonObject obj = it.value().toObject();
        StoreVersionRecord rec;
        rec.gameId = it.key();
        rec.steamBuildId = obj["steamBuildId"].toString();
        rec.epicVersionString = obj["epicVersionString"].toString();
        rec.gogBuildId = obj["gogBuildId"].toString();
        rec.exeLastModified = obj["exeLastModified"].toInteger();
        rec.recordedAt = obj["recordedAt"].toInteger();
        m_storeVersions[it.key()] = rec;
    }

    qDebug() << "Loaded" << m_storeVersions.size() << "store version records";
}

void UpdateDetectionService::saveStoreVersions()
{
    QString dir = dataDir();
    QDir().mkpath(dir);

    QJsonObject root;
    {
        QMutexLocker lock(&m_storeVersionsMutex);
        for (auto it = m_storeVersions.constBegin(); it != m_storeVersions.constEnd(); ++it) {
            const auto& rec = it.value();
            QJsonObject obj;
            obj["steamBuildId"] = rec.steamBuildId;
            obj["epicVersionString"] = rec.epicVersionString;
            obj["gogBuildId"] = rec.gogBuildId;
            obj["exeLastModified"] = rec.exeLastModified;
            obj["recordedAt"] = rec.recordedAt;
            root[it.key()] = obj;
        }
    }

    QFile file(dir + "/store_versions.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    }
}

void UpdateDetectionService::loadSnapshot(const QString& dataDir,
                                            const QString& gameId, GameSnapshot& out)
{
    QFile file(dataDir + "/snapshots/" + gameId + ".json");
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    file.close();

    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;

    const QJsonObject root = doc.object();
    out.gameId = root["gameId"].toString();
    out.patchVersion = root["patchVersion"].toString();
    out.takenAt = root["takenAt"].toInteger();

    const QJsonArray filesArr = root["files"].toArray();
    out.files.reserve(filesArr.size());
    for (const auto& val : filesArr) {
        const QJsonObject fObj = val.toObject();
        FileHashRecord rec;
        rec.relativePath = fObj["path"].toString();
        rec.sha256 = fObj["sha256"].toString();
        rec.fileSize = fObj["size"].toInteger();
        rec.lastModified = fObj["mtime"].toInteger();
        out.files.append(rec);
    }
}

void UpdateDetectionService::saveSnapshot(const QString& dataDir,
                                            const GameSnapshot& snapshot)
{
    QString dir = dataDir + "/snapshots";
    QDir().mkpath(dir);

    QJsonObject root;
    root["gameId"] = snapshot.gameId;
    root["patchVersion"] = snapshot.patchVersion;
    root["takenAt"] = snapshot.takenAt;

    QJsonArray filesArr;
    for (const auto& f : snapshot.files) {
        QJsonObject fObj;
        fObj["path"] = f.relativePath;
        fObj["sha256"] = f.sha256;
        fObj["size"] = f.fileSize;
        fObj["mtime"] = f.lastModified;
        filesArr.append(fObj);
    }
    root["files"] = filesArr;

    QFile file(dir + "/" + snapshot.gameId + ".json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    }
}

} // namespace makineai
