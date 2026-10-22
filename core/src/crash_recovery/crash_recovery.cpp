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

#ifdef _WIN32
#include <windows.h>
#endif

#include <chrono>
#include <cstdlib>
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
    // The wide variable: the narrow one is in the ANSI code page, and a
    // profile folder like "Şükrü" is not UTF-8 there — fs::path threw, and
    // the whole recovery stopped half way.
    const wchar_t* home = _wgetenv(L"USERPROFILE");
    if (!home) return {};
    return fs::path(home) / "AppData" / "Local" / "Microsoft" / "Windows" / "Fonts";
}

// Deletes a directory tree bottom up through the Win32 calls, in the "\\?\"
// form. libstdc++'s fs::remove_all never returns on a tree that holds a path
// past MAX_PATH, and a backup mirrors the game's deepest paths under a longer
// root. A link inside the tree is removed, never followed.
bool removeTree(const std::wstring& dir, int& removed) {
    bool ok = true;
    WIN32_FIND_DATAW fd;
    HANDLE find = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            const std::wstring name = fd.cFileName;
            if (name == L"." || name == L"..") continue;
            const std::wstring child = dir + L"\\" + name;
            const bool isDir = fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
            if (isDir && !(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
                ok = removeTree(child, removed) && ok;
                continue;
            }
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
                SetFileAttributesW(child.c_str(), FILE_ATTRIBUTE_NORMAL);
            if (isDir ? RemoveDirectoryW(child.c_str()) : DeleteFileW(child.c_str())) ++removed;
            else ok = false;
        } while (FindNextFileW(find, &fd));
        FindClose(find);
    }
    if (RemoveDirectoryW(dir.c_str())) ++removed;
    else ok = false;
    return ok;
}

// Every file under root (a "\\?\" one), relative to it. libstdc++'s directory
// iterators cannot open a folder past MAX_PATH, and below a "\\?\" root they
// return entries that are not in it. False when a folder could not be read.
bool listFiles(const fs::path& root, const fs::path& rel, std::vector<fs::path>& out) {
    // No "root\\": the "\\?\" form takes the path exactly as written
    const std::wstring dir = rel.empty() ? root.native() : (root / rel).native();
    WIN32_FIND_DATAW fd;
    HANDLE find = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (find == INVALID_HANDLE_VALUE) return false;
    bool ok = true;
    do {
        const std::wstring name = fd.cFileName;
        if (name == L"." || name == L"..") continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;  // never followed
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            ok = listFiles(root, rel / name, out) && ok;
        else
            out.push_back(rel / name);
    } while (FindNextFileW(find, &fd));
    FindClose(find);
    return ok;
}
#endif

// The "\\?\" form of a path on Windows: file calls past MAX_PATH need it, and
// a backup holds the game's deepest paths under a longer root. Records join it
// after make_preferred(), as the form takes no '/'.
fs::path longPath(const fs::path& p) {
#ifdef _WIN32
    // Not again through absolute(): libstdc++ mangles a "\\?\" path there
    if (p.native().starts_with(LR"(\\?\)")) return p;
    std::wstring native = fs::absolute(p).lexically_normal().make_preferred().native();
    while (native.size() > 3 && native.ends_with(L'\\')) native.pop_back();
    if (native.starts_with(LR"(\\)")) return LR"(\\?\UNC\)" + native.substr(2);
    return LR"(\\?\)" + native;
#else
    return p;
#endif
}

// Every file under root, as paths relative to it. False when part of the tree
// could not be read.
bool filesUnder(const fs::path& root, std::vector<fs::path>& out) {
#ifdef _WIN32
    return listFiles(longPath(root), {}, out);
#else
    std::error_code ec;
    for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code fileEc;
        if (it->is_regular_file(fileEc)) out.push_back(it->path().lexically_relative(root));
    }
    return !ec;
#endif
}

// Removes a directory and everything under it. False when something stayed.
bool removeDirectoryTree(const fs::path& dir, int& removed) {
#ifdef _WIN32
    return removeTree(longPath(dir).native(), removed);
#else
    std::error_code ec;
    const auto count = fs::remove_all(dir, ec);
    if (!ec) removed += static_cast<int>(count);
    return !ec;
#endif
}

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
    const fs::path from = fs::path(names.substr(0, colon)).make_preferred();
    const fs::path to = fs::path(names.substr(colon + 1)).make_preferred();
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
    const fs::path gamePath = longPath(entry.gamePath);
    const fs::path backupDir = newestBackupDir(paths.backupsRoot, entry.gameId);
    const fs::path backup = backupDir.empty() ? backupDir : longPath(backupDir);

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

        const fs::path rel = fs::path(record).make_preferred();
        if (!isContainedRelative(rel)) continue;
        if (fs::exists(backup / rel, ec)) continue;  // an original — restored below
        if (fs::remove(gamePath / rel, ec)) ++processed;
        else if (ec) ++failed;
    }

    // Every original the install was about to overwrite. Copying all of them
    // back also covers files written after the journal's last flush. Listed
    // and copied in the long form: an original past MAX_PATH was skipped
    // without an error, and the recovery called the game rolled back.
    std::vector<fs::path> originals;
    if (!filesUnder(backup, originals)) ++failed;
    for (const fs::path& rel : originals) {
        std::error_code fileEc;
        if (!isContainedRelative(rel)) continue;
        const fs::path dest = gamePath / rel;
        fs::create_directories(dest.parent_path(), fileEc);
        // overwrite_existing fails on MinGW's libstdc++ when the target exists
        fs::remove(dest, fileEc);
        fs::copy_file(backup / rel, dest, fileEc);
        count(!fileEc);
    }

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
    const std::string recordedPath = entry.gamePath.empty()
        ? pkgObj.value("gamePath", "") : entry.gamePath;
    // Relative records against no folder would land in the working directory
    if (recordedPath.empty()) return {false, 0, "Uninstall recovery: no game path"};
    const fs::path gamePath = longPath(recordedPath);

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
    const fs::path backupDir = newestBackupDir(paths.backupsRoot, entry.gameId);
    const fs::path backup = backupDir.empty() ? backupDir : longPath(backupDir);

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

        const fs::path rel = fs::path(relPath).make_preferred();
        if (!isContainedRelative(rel) || replaced.count(relPath)) continue;
        if (!backup.empty() && fs::exists(backup / rel, ec)) continue;

        const auto fullPath = gamePath / rel;
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
        int count = 0;
        const bool ok = removeDirectoryTree(entry.backupPath, count);
        return {ok, count, fmt::format("{} orphan backup dir: {}",
            ok ? "Removed" : "Could not fully remove", entry.backupPath)};
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
    int failed = 0;
    const fs::path backup = longPath(entry.backupPath);
    const fs::path gamePath = longPath(entry.gamePath);

    // In the long form, as the install recovery. A copy that failed counted
    // as nothing and the restore was called complete.
    std::vector<fs::path> originals;
    if (!filesUnder(backup, originals)) ++failed;
    for (const fs::path& rel : originals) {
        // The journal records Qt's relative paths, '/' between parts
        if (!isContainedRelative(rel) || alreadyRestored.count(rel.generic_string())) continue;

        std::error_code ec;
        const fs::path destPath = gamePath / rel;
        fs::create_directories(destPath.parent_path(), ec);
        fs::remove(destPath, ec);
        fs::copy_file(backup / rel, destPath, ec);
        if (!ec) restored++;
        else ++failed;
    }

    if (failed > 0) {
        return {false, restored, fmt::format(
            "Backup restore incomplete: {} restored, {} failed", restored, failed)};
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
