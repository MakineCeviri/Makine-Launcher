/**
 * @file crash_recovery.cpp
 * @brief Crash recovery journal implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "makine/crash_recovery.hpp"
#include "makine/logging.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <fstream>
#include <mutex>
#include <set>
#include <string>

namespace makine::recovery {

using json = nlohmann::json;

OperationType stringToOperationType(std::string_view s) noexcept {
    if (s == "install")        return OperationType::Install;
    if (s == "uninstall")      return OperationType::Uninstall;
    if (s == "backup_create")  return OperationType::BackupCreate;
    if (s == "backup_restore") return OperationType::BackupRestore;
    return OperationType::Install;
}

namespace {

int64_t currentEpochSeconds() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

// Atomic write: write to .tmp then rename
bool atomicWriteJson(const fs::path& path, const std::string& data) {
    auto tmpPath = path;
    tmpPath += ".tmp";

    {
        std::ofstream out(tmpPath, std::ios::binary);
        if (!out) return false;
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
        if (!out) {
            std::error_code ec;
            fs::remove(tmpPath, ec);
            return false;
        }
    }

    std::error_code ec;
    // Windows: remove target first (rename fails if exists)
    if (fs::exists(path, ec)) {
        fs::remove(path, ec);
    }
    fs::rename(tmpPath, path, ec);
    return !ec;
}

#ifdef _WIN32
// Windows user fonts directory
fs::path windowsFontsDir() {
    const char* home = std::getenv("USERPROFILE");
    if (!home) return {};
    return fs::path(home) / "AppData" / "Local" / "Microsoft" / "Windows" / "Fonts";
}
#endif

} // anonymous namespace

CrashRecoveryJournal::CrashRecoveryJournal(const fs::path& dataDir)
    : dataDir_(dataDir)
{
    std::error_code ec;
    fs::create_directories(dataDir_, ec);
}

fs::path CrashRecoveryJournal::journalPath() const {
    return dataDir_ / "pending_operation.json";
}

bool CrashRecoveryJournal::beginOperation(const JournalEntry& entry) {
    std::lock_guard lock(mutex_);

    if (active_) {
        MAKINE_LOG_WARN(log::CORE, "CrashRecoveryJournal: operation already in progress");
        return false;
    }

    current_ = entry;
    current_.startedAt = currentEpochSeconds();
    current_.modifiedFiles.clear();
    pendingCount_ = 0;
    active_ = true;

    flushJournal();
    return true;
}

void CrashRecoveryJournal::recordFileModified(const std::string& relativePath) {
    std::lock_guard lock(mutex_);
    if (!active_) return;

    current_.modifiedFiles.push_back(relativePath);
    pendingCount_++;

    if (pendingCount_ >= kFlushInterval) {
        flushJournal();
        pendingCount_ = 0;
    }
}

void CrashRecoveryJournal::commitOperation() {
    std::lock_guard lock(mutex_);
    active_ = false;
    current_ = {};
    pendingCount_ = 0;
    deleteJournal();
}

void CrashRecoveryJournal::abortOperation() {
    std::lock_guard lock(mutex_);
    active_ = false;
    current_ = {};
    pendingCount_ = 0;
    deleteJournal();
}

bool CrashRecoveryJournal::isActive() const {
    std::lock_guard lock(mutex_);
    return active_;
}

bool CrashRecoveryJournal::hasPendingOperation() const {
    return fs::exists(journalPath());
}

JournalEntry CrashRecoveryJournal::readPendingOperation() const {
    JournalEntry entry;

    std::ifstream file(journalPath());
    if (!file) return entry;

    try {
        json doc = json::parse(file);

        entry.type = stringToOperationType(doc.value("type", ""));
        entry.gameId = doc.value("gameId", "");
        entry.gamePath = doc.value("gamePath", "");
        entry.backupId = doc.value("backupId", "");
        entry.backupPath = doc.value("backupPath", "");
        entry.variant = doc.value("variant", "");
        entry.startedAt = doc.value("startedAt", int64_t{0});

        if (doc.contains("modifiedFiles") && doc["modifiedFiles"].is_array()) {
            for (const auto& f : doc["modifiedFiles"]) {
                if (f.is_string()) {
                    entry.modifiedFiles.push_back(f.get<std::string>());
                }
            }
        }
    } catch (const json::exception& e) {
        MAKINE_LOG_WARN(log::CORE, "CrashRecoveryJournal: corrupted journal: {}", e.what());
        return {};
    }

    return entry;
}

RecoveryResult CrashRecoveryJournal::recover(const fs::path& installedStatePath) {
    if (!hasPendingOperation()) {
        return {true, 0, "No pending operation"};
    }

    auto entry = readPendingOperation();

    // Corrupted or empty journal
    if (entry.gameId.empty() && entry.backupPath.empty()) {
        MAKINE_LOG_WARN(log::CORE, "CrashRecoveryJournal: empty/corrupt journal, deleting");
        deleteJournal();
        return {true, 0, "Corrupted journal cleaned up"};
    }

    MAKINE_LOG_INFO(log::CORE, "CrashRecoveryJournal: recovering {} for game {}",
                      operationTypeToString(entry.type), entry.gameId);

    RecoveryResult result;
    switch (entry.type) {
        case OperationType::Install:
            result = recoverInstall(entry);
            break;
        case OperationType::Uninstall:
            result = recoverUninstall(entry, installedStatePath);
            break;
        case OperationType::BackupCreate:
            result = recoverBackupCreate(entry);
            break;
        case OperationType::BackupRestore:
            result = recoverBackupRestore(entry);
            break;
    }

    deleteJournal();

    MAKINE_LOG_INFO(log::CORE, "CrashRecoveryJournal: recovery {} ({} files processed)",
                      result.success ? "succeeded" : "failed", result.filesProcessed);
    return result;
}

// --- Recovery implementations ---

RecoveryResult CrashRecoveryJournal::recoverInstall(const JournalEntry& entry) {
    if (entry.gamePath.empty() || !fs::exists(entry.gamePath)) {
        return {false, 0, fmt::format("Game path missing: {}", entry.gamePath)};
    }

    int deleted = 0;
    std::error_code ec;

    for (const auto& relPath : entry.modifiedFiles) {
        // Font entries: "_font:filename.ttf"
        if (relPath.starts_with("_font:")) {
#ifdef _WIN32
            auto fontDir = windowsFontsDir();
            if (!fontDir.empty()) {
                auto fontPath = fontDir / relPath.substr(6);
                if (fs::remove(fontPath, ec)) {
                    deleted++;
                }
            }
#endif
            continue;
        }

        auto fullPath = fs::path(entry.gamePath) / relPath;
        if (fs::remove(fullPath, ec)) {
            deleted++;
        }
    }

    return {true, deleted,
        fmt::format("Removed {} orphaned files from install", deleted)};
}

RecoveryResult CrashRecoveryJournal::recoverUninstall(
    const JournalEntry& entry, const fs::path& installedStatePath)
{
    if (installedStatePath.empty() || !fs::exists(installedStatePath)) {
        return {true, 0, "No installed state file"};
    }

    // Read installed packages state
    std::ifstream stateFile(installedStatePath);
    if (!stateFile) return {true, 0, "No state file"};

    json root;
    try {
        root = json::parse(stateFile);
    } catch (...) {
        return {true, 0, "State file parse error"};
    }
    stateFile.close();

    if (!root.contains(entry.gameId)) {
        return {true, 0, "Already removed from state"};
    }

    auto pkgObj = root[entry.gameId];
    std::string gamePath = entry.gamePath.empty()
        ? pkgObj.value("gamePath", "") : entry.gamePath;

    // Already-deleted files from journal
    std::set<std::string> alreadyDeleted(
        entry.modifiedFiles.begin(), entry.modifiedFiles.end());

    int deleted = 0;
    std::error_code ec;

    if (pkgObj.contains("files") && pkgObj["files"].is_array()) {
        for (const auto& f : pkgObj["files"]) {
            std::string relPath = f.get<std::string>();
            if (alreadyDeleted.count(relPath)) continue;

            if (relPath.starts_with("_font:")) {
#ifdef _WIN32
                auto fontDir = windowsFontsDir();
                if (!fontDir.empty()) {
                    fs::remove(fontDir / relPath.substr(6), ec);
                    deleted++;
                }
#endif
                continue;
            }

            auto fullPath = fs::path(gamePath) / relPath;
            if (fs::remove(fullPath, ec)) {
                deleted++;
            }
        }
    }

    // Remove from installed state
    root.erase(entry.gameId);

    std::string data = root.dump(2);
    atomicWriteJson(installedStatePath, data);

    return {true, deleted,
        fmt::format("Completed uninstall recovery: {} files", deleted)};
}

RecoveryResult CrashRecoveryJournal::recoverBackupCreate(const JournalEntry& entry) {
    if (entry.backupPath.empty()) {
        return {true, 0, "No backup path"};
    }

    std::error_code ec;
    if (fs::exists(entry.backupPath, ec)) {
        auto count = fs::remove_all(entry.backupPath, ec);
        return {!ec, static_cast<int>(count),
            fmt::format("Removed orphan backup dir: {}", entry.backupPath)};
    }
    return {true, 0, "Backup dir already gone"};
}

RecoveryResult CrashRecoveryJournal::recoverBackupRestore(const JournalEntry& entry) {
    if (entry.backupPath.empty() || entry.gamePath.empty()) {
        return {false, 0, "Missing backup or game path"};
    }

    if (!fs::exists(entry.backupPath)) {
        return {false, 0, fmt::format("Backup dir missing: {}", entry.backupPath)};
    }

    std::set<std::string> alreadyRestored(
        entry.modifiedFiles.begin(), entry.modifiedFiles.end());

    int restored = 0;
    std::error_code ec;

    for (const auto& dirEntry : fs::recursive_directory_iterator(entry.backupPath, ec)) {
        if (!dirEntry.is_regular_file()) continue;

        auto relPath = fs::relative(dirEntry.path(), entry.backupPath, ec).string();
        if (ec || alreadyRestored.count(relPath)) continue;

        auto destPath = fs::path(entry.gamePath) / relPath;
        fs::create_directories(destPath.parent_path(), ec);

        if (fs::exists(destPath, ec)) {
            fs::remove(destPath, ec);
        }

        fs::copy_file(dirEntry.path(), destPath, ec);
        if (!ec) restored++;
    }

    return {true, restored,
        fmt::format("Completed backup restore: {} files", restored)};
}

// --- Internal helpers ---

void CrashRecoveryJournal::flushJournal() {
    json obj;
    obj["version"] = 1;
    obj["type"] = std::string(operationTypeToString(current_.type));
    obj["gameId"] = current_.gameId;
    obj["gamePath"] = current_.gamePath;
    obj["backupId"] = current_.backupId;
    obj["backupPath"] = current_.backupPath;
    obj["variant"] = current_.variant;
    obj["startedAt"] = current_.startedAt;
    obj["modifiedFiles"] = current_.modifiedFiles;

    auto path = journalPath();
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    atomicWriteJson(path, obj.dump());
}

void CrashRecoveryJournal::deleteJournal() {
    std::error_code ec;
    auto path = journalPath();
    fs::remove(path, ec);
    auto tmpPath = path;
    tmpPath += ".tmp";
    fs::remove(tmpPath, ec);
}

} // namespace makine::recovery
