// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

/**
 * @file crash_recovery.cpp
 * @brief Crash recovery journal implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "makine/crash_recovery.hpp"
#include "makine/logging.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <exception>
#include <fstream>
#include <mutex>
#include <set>
#include <string>
#include <vector>

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

// A journal record that names a file inside the game folder, not one that
// climbs out of it.
bool isContainedRelative(const fs::path& rel) {
    if (rel.empty() || rel.has_root_name() || rel.has_root_directory()) return false;
    for (const auto& part : rel) {
        if (part == "..") return false;
    }
    return true;
}

// Markers the installer journals beside plain paths: "_font:", "_desktop:",
// "_rename:", "_vpatch:", "_steamlang:". A Windows file name cannot hold ':',
// so a record starting with '_' and carrying one is never a real path.
bool isMarker(const std::string& record) {
    return record.starts_with('_') && record.find(':') != std::string::npos;
}

// Undoes "_rename:<from>:<to>" (names is "<from>:<to>"): puts <to> back under
// its original name. False with ec clear when there is nothing to undo.
bool reverseRename(const fs::path& gamePath, const std::string& names, std::error_code& ec) {
    ec.clear();
    const auto colon = names.find(':');
    if (colon == std::string::npos) return false;
    const fs::path from(names.substr(0, colon));
    const fs::path to(names.substr(colon + 1));
    if (!isContainedRelative(from) || !isContainedRelative(to)) return false;
    if (!fs::exists(gamePath / to, ec)) return false;
    fs::remove(gamePath / from, ec);
    fs::rename(gamePath / to, gamePath / from, ec);
    return !ec;
}

// The newest backup taken for the game — the one taken right before the
// install that was interrupted. Empty when the game has none.
fs::path newestBackupDir(const fs::path& backupsRoot, const std::string& gameId) {
    if (backupsRoot.empty() || gameId.empty()) return {};
    std::error_code ec;
    fs::path newest;
    fs::file_time_type newestTime{};
    for (fs::directory_iterator it(backupsRoot / gameId, ec), end; !ec && it != end;
         it.increment(ec)) {
        std::error_code entryEc;
        if (!it->is_directory(entryEc)) continue;
        const auto when = fs::last_write_time(it->path(), entryEc);
        if (entryEc) continue;
        if (newest.empty() || when > newestTime) {
            newest = it->path();
            newestTime = when;
        }
    }
    return newest;
}

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

fs::path CrashRecoveryJournal::failedJournalPath() const {
    return dataDir_ / "pending_operation.failed.json";
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
    active_ = true;

    flushJournal();
    records_.open(journalPath(), std::ios::binary | std::ios::app);
    return true;
}

void CrashRecoveryJournal::recordFileModified(const std::string& relativePath) {
    std::lock_guard lock(mutex_);
    if (!active_) return;

    current_.modifiedFiles.push_back(relativePath);

    // One line per record, flushed at once. Rewriting the whole journal every
    // 20 records left up to 19 written files out of it after a crash, and
    // recovery could not delete what it did not know about.
    if (records_.is_open()) {
        records_ << json(relativePath).dump(-1, ' ', false, json::error_handler_t::replace)
                 << '\n';
        records_.flush();
    }
}

void CrashRecoveryJournal::commitOperation() {
    std::lock_guard lock(mutex_);
    active_ = false;
    current_ = {};
    records_.close();
    deleteJournal();
}

void CrashRecoveryJournal::abortOperation() {
    std::lock_guard lock(mutex_);
    active_ = false;
    current_ = {};
    records_.close();
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

    std::ifstream file(journalPath(), std::ios::binary);
    if (!file) return entry;

    // Line one is the entry as begin wrote it; every line after it is one
    // file record, appended as it was made.
    std::string header;
    std::getline(file, header);
    try {
        json doc = json::parse(header);

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

    for (std::string line; std::getline(file, line);) {
        if (line.empty()) continue;
        try {
            const json record = json::parse(line);
            if (record.is_string()) entry.modifiedFiles.push_back(record.get<std::string>());
        } catch (const json::exception&) {
            break;  // the record the crash cut short
        }
    }

    return entry;
}

RecoveryResult CrashRecoveryJournal::recover(const RecoveryPaths& paths) {
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

    // A throw here used to skip deleteJournal(): the next launch ran into the
    // same journal and threw again, on every start.
    RecoveryResult result;
    try {
        switch (entry.type) {
            case OperationType::Install:
                result = recoverInstall(entry, paths);
                break;
            case OperationType::Uninstall:
                result = recoverUninstall(entry, paths);
                break;
            case OperationType::BackupCreate:
                result = recoverBackupCreate(entry);
                break;
            case OperationType::BackupRestore:
                result = recoverBackupRestore(entry);
                break;
        }
    } catch (const std::exception& e) {
        result = {false, 0, fmt::format("Recovery threw: {}", e.what())};
    }

    if (result.success) {
        deleteJournal();
    } else {
        // Kept aside rather than deleted: it is the only record of what was
        // left half done, and retrying it on every launch would not help.
        std::error_code ec;
        fs::rename(journalPath(), failedJournalPath(), ec);
        if (ec) deleteJournal();
    }

    MAKINE_LOG_INFO(log::CORE, "CrashRecoveryJournal: recovery {} ({} files processed)",
                      result.success ? "succeeded" : "failed", result.filesProcessed);
    return result;
}

// --- Recovery implementations ---

RecoveryResult CrashRecoveryJournal::recoverInstall(const JournalEntry& entry,
                                                    const RecoveryPaths& paths) {
    std::error_code ec;
    if (entry.gamePath.empty() || !fs::exists(entry.gamePath, ec)) {
        return {false, 0, fmt::format("Game path missing: {}", entry.gamePath)};
    }
    const fs::path gamePath(entry.gamePath);
    const fs::path backup = newestBackupDir(paths.backupsRoot, entry.gameId);

    // The journal lists every file the install wrote, originals it replaced
    // included; deleting them all left the game without files it shipped
    // with. Only the backup tells the two apart, so without one nothing is
    // touched.
    if (backup.empty()) {
        if (entry.modifiedFiles.empty()) return {true, 0, "Nothing to undo"};
        return {false, 0, fmt::format(
            "No backup for {}; {} written files left in place",
            entry.gameId, entry.modifiedFiles.size())};
    }

    int processed = 0;
    int failed = 0;
    const auto count = [&](bool ok) { ok ? ++processed : ++failed; };

    // Newest first: a later step may have written where an earlier one
    // renamed a file away from.
    for (auto it = entry.modifiedFiles.rbegin(); it != entry.modifiedFiles.rend(); ++it) {
        const std::string& record = *it;

        if (record.starts_with("_font:")) {
#ifdef _WIN32
            const auto fontDir = windowsFontsDir();
            if (!fontDir.empty() && fs::remove(fontDir / record.substr(6), ec)) ++processed;
#endif
            continue;
        }
        if (record.starts_with("_desktop:")) {
            if (!paths.desktopDir.empty() && fs::remove(paths.desktopDir / record.substr(9), ec))
                ++processed;
            continue;
        }
        if (record.starts_with("_rename:")) {
            if (reverseRename(gamePath, record.substr(8), ec)) ++processed;
            else if (ec) ++failed;
            continue;
        }
        // "_vpatch:" targets are originals patched in place: the backup holds
        // them and puts them back below. Other markers leave nothing in the
        // game folder to undo.
        if (isMarker(record)) continue;

        const fs::path rel(record);
        if (!isContainedRelative(rel)) continue;
        if (fs::exists(backup / rel, ec)) continue;  // an original — restored below
        if (fs::remove(gamePath / rel, ec)) ++processed;
        else if (ec) ++failed;
    }

    // Every original the install was about to overwrite. Copying all of them
    // back also covers files written after the journal's last flush.
    for (fs::recursive_directory_iterator it(backup, ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code fileEc;
        if (!it->is_regular_file(fileEc)) continue;
        const fs::path rel = fs::relative(it->path(), backup, fileEc);
        if (fileEc || !isContainedRelative(rel)) continue;
        const fs::path dest = gamePath / rel;
        fs::create_directories(dest.parent_path(), fileEc);
        // overwrite_existing fails on MinGW's libstdc++ when the target exists
        fs::remove(dest, fileEc);
        fs::copy_file(it->path(), dest, fileEc);
        count(!fileEc);
    }
    if (ec) ++failed;

    if (failed > 0) {
        return {false, processed, fmt::format(
            "Install rollback incomplete: {} undone, {} failed (backup {})",
            processed, failed, backup.filename().string())};
    }
    return {true, processed, fmt::format(
        "Rolled back interrupted install: {} files (backup {})",
        processed, backup.filename().string())};
}

RecoveryResult CrashRecoveryJournal::recoverUninstall(
    const JournalEntry& entry, const RecoveryPaths& paths)
{
    const fs::path& installedStatePath = paths.installedState;
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

    // The uninstall journal begins after the backup restore: the originals
    // are back in the game folder, under names the record also lists.
    // Deleting every recorded file took them out again, so the same rules as
    // LocalPackageManager::uninstallPackage apply: a replaced file, or one
    // the game's backup holds, stays.
    std::set<std::string> replaced;
    if (pkgObj.contains("replacedFiles") && pkgObj["replacedFiles"].is_array()) {
        for (const auto& f : pkgObj["replacedFiles"])
            if (f.is_string()) replaced.insert(f.get<std::string>());
    }
    const fs::path backup = newestBackupDir(paths.backupsRoot, entry.gameId);

    int deleted = 0;
    int failed = 0;
    std::error_code ec;

    std::vector<std::string> records;
    if (pkgObj.contains("files") && pkgObj["files"].is_array()) {
        for (const auto& f : pkgObj["files"])
            if (f.is_string()) records.push_back(f.get<std::string>());
    }

    // Newest first, as the install's own undo: a later step may have written
    // where an earlier one renamed a file away from.
    for (auto it = records.rbegin(); it != records.rend(); ++it) {
        const std::string& relPath = *it;
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
        if (relPath.starts_with("_desktop:")) {
            if (!paths.desktopDir.empty() && fs::remove(paths.desktopDir / relPath.substr(9), ec))
                ++deleted;
            continue;
        }
        if (relPath.starts_with("_rename:")) {
            if (reverseRename(gamePath, relPath.substr(8), ec)) ++deleted;
            else if (ec) ++failed;
            continue;
        }
        // "_vpatch:" targets are originals the restore put back; the other
        // markers name nothing in the game folder.
        if (isMarker(relPath)) continue;

        const fs::path rel(relPath);
        if (!isContainedRelative(rel) || replaced.count(relPath)) continue;
        if (!backup.empty() && fs::exists(backup / rel, ec)) continue;

        const auto fullPath = fs::path(gamePath) / rel;
        if (fs::remove(fullPath, ec)) {
            deleted++;
        } else if (ec) {
            // A read-only attribute refuses the delete on Windows
            std::error_code permEc;
            fs::permissions(fullPath, fs::perms::owner_write, fs::perm_options::add, permEc);
            if (fs::remove(fullPath, ec)) ++deleted;
            else ++failed;
        }
    }

    // A file that would not go stays on record, so "Kaldır" can still finish
    // the job; dropping the record left it behind with nothing to retry from.
    if (failed > 0) {
        return {false, deleted, fmt::format(
            "Uninstall recovery incomplete: {} removed, {} failed; record kept",
            deleted, failed)};
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
    // Ends its line: the file records are appended after it.
    atomicWriteJson(path, obj.dump() + '\n');
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
