/**
 * @file file_watcher.hpp
 * @brief File system watcher with optional EFSW backend
 * @copyright (c) 2026 MakineAI Team
 *
 * Provides cross-platform file system monitoring.
 * Uses EFSW when available for robust watching.
 *
 * Compile-time detection:
 * - MAKINEAI_HAS_EFSW - EFSW library available
 *
 * Fallback: Polling-based watcher using std::filesystem.
 */

#pragma once

#include "features.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef MAKINEAI_HAS_EFSW
#include <efsw/efsw.hpp>
#endif

namespace makineai::watch {

namespace fs = std::filesystem;

// =============================================================================
// Types
// =============================================================================

/**
 * @brief File change action types
 */
enum class Action {
    Add,        ///< File was created
    Delete,     ///< File was deleted
    Modified,   ///< File was modified
    Moved,      ///< File was renamed/moved
    Unknown
};

/**
 * @brief Convert action to string
 */
inline const char* actionToString(Action action) noexcept {
    switch (action) {
        case Action::Add: return "Add";
        case Action::Delete: return "Delete";
        case Action::Modified: return "Modified";
        case Action::Moved: return "Moved";
        default: return "Unknown";
    }
}

/**
 * @brief File change event
 */
struct FileEvent {
    Action action;
    fs::path directory;
    fs::path filename;
    fs::path oldFilename;  ///< For Moved action
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Callback type for file events
 */
using EventCallback = std::function<void(const FileEvent&)>;

// =============================================================================
// File Watcher Interface
// =============================================================================

/**
 * @brief Abstract file watcher interface
 */
class IFileWatcher {
public:
    virtual ~IFileWatcher() = default;

    /**
     * @brief Add a directory to watch
     * @param path Directory path
     * @param recursive Watch subdirectories
     * @return Watch ID or 0 on failure
     */
    virtual int addWatch(const fs::path& path, bool recursive = true) = 0;

    /**
     * @brief Remove a watch by ID
     */
    virtual void removeWatch(int watchId) = 0;

    /**
     * @brief Start watching (non-blocking)
     */
    virtual void start() = 0;

    /**
     * @brief Stop watching
     */
    virtual void stop() = 0;

    /**
     * @brief Check if watcher is running
     */
    virtual bool isRunning() const = 0;

    /**
     * @brief Set callback for file events
     */
    virtual void setCallback(EventCallback callback) = 0;
};

// =============================================================================
// EFSW Implementation
// =============================================================================

#ifdef MAKINEAI_HAS_EFSW

/**
 * @brief EFSW-based file watcher implementation
 */
class EfswWatcher : public IFileWatcher, private efsw::FileWatchListener {
public:
    EfswWatcher() : watcher_(std::make_unique<efsw::FileWatcher>()) {}

    ~EfswWatcher() override {
        stop();
    }

    int addWatch(const fs::path& path, bool recursive = true) override {
        return static_cast<int>(
            watcher_->addWatch(path.string(), this, recursive)
        );
    }

    void removeWatch(int watchId) override {
        watcher_->removeWatch(static_cast<efsw::WatchID>(watchId));
    }

    void start() override {
        watcher_->watch();
        running_ = true;
    }

    void stop() override {
        // EFSW doesn't have explicit stop - watches are removed
        running_ = false;
    }

    bool isRunning() const override {
        return running_;
    }

    void setCallback(EventCallback callback) override {
        std::lock_guard lock(mutex_);
        callback_ = std::move(callback);
    }

private:
    void handleFileAction(
        efsw::WatchID /*watchid*/,
        const std::string& dir,
        const std::string& filename,
        efsw::Action action,
        std::string oldFilename
    ) override {
        FileEvent event;
        event.directory = dir;
        event.filename = filename;
        event.oldFilename = oldFilename;
        event.timestamp = std::chrono::system_clock::now();

        switch (action) {
            case efsw::Actions::Add:
                event.action = Action::Add;
                break;
            case efsw::Actions::Delete:
                event.action = Action::Delete;
                break;
            case efsw::Actions::Modified:
                event.action = Action::Modified;
                break;
            case efsw::Actions::Moved:
                event.action = Action::Moved;
                break;
            default:
                event.action = Action::Unknown;
        }

        std::lock_guard lock(mutex_);
        if (callback_) {
            callback_(event);
        }
    }

    std::unique_ptr<efsw::FileWatcher> watcher_;
    EventCallback callback_;
    std::mutex mutex_;
    std::atomic<bool> running_{false};
};

#endif // MAKINEAI_HAS_EFSW

// =============================================================================
// Polling Implementation (Fallback)
// =============================================================================

/**
 * @brief Polling-based file watcher implementation
 *
 * Fallback when EFSW is not available.
 * Less efficient but portable.
 */
class PollingWatcher : public IFileWatcher {
public:
    explicit PollingWatcher(std::chrono::milliseconds interval = std::chrono::milliseconds(1000))
        : pollInterval_(interval) {}

    ~PollingWatcher() override {
        stop();
    }

    int addWatch(const fs::path& path, bool recursive = true) override {
        std::lock_guard lock(mutex_);

        WatchEntry entry;
        entry.path = path;
        entry.recursive = recursive;

        // Initial scan
        scanDirectory(entry);

        int id = nextId_++;
        watches_[id] = std::move(entry);
        return id;
    }

    void removeWatch(int watchId) override {
        std::lock_guard lock(mutex_);
        watches_.erase(watchId);
    }

    void start() override {
        if (running_.exchange(true)) {
            return;  // Already running
        }

        pollThread_ = std::thread([this]() {
            while (running_) {
                poll();
                std::this_thread::sleep_for(pollInterval_);
            }
        });
    }

    void stop() override {
        running_ = false;
        if (pollThread_.joinable()) {
            pollThread_.join();
        }
    }

    bool isRunning() const override {
        return running_;
    }

    void setCallback(EventCallback callback) override {
        std::lock_guard lock(mutex_);
        callback_ = std::move(callback);
    }

private:
    struct FileInfo {
        fs::file_time_type lastWrite;
        uintmax_t size;
    };

    struct WatchEntry {
        fs::path path;
        bool recursive = true;
        std::unordered_map<std::string, FileInfo> files;
    };

    void scanDirectory(WatchEntry& entry) {
        std::error_code ec;
        auto iter = entry.recursive
            ? fs::recursive_directory_iterator(entry.path, ec)
            : fs::directory_iterator(entry.path, ec);

        if (ec) return;

        for (const auto& dirEntry : iter) {
            if (dirEntry.is_regular_file()) {
                std::string pathStr = dirEntry.path().string();
                FileInfo info;
                info.lastWrite = dirEntry.last_write_time();
                info.size = dirEntry.file_size();
                entry.files[pathStr] = info;
            }
        }
    }

    void poll() {
        std::lock_guard lock(mutex_);

        for (auto& [watchId, entry] : watches_) {
            std::unordered_map<std::string, FileInfo> currentFiles;

            std::error_code ec;
            auto begin = entry.recursive
                ? fs::recursive_directory_iterator(entry.path, ec)
                : fs::directory_iterator(entry.path, ec);

            if (ec) continue;

            for (const auto& dirEntry : begin) {
                if (!dirEntry.is_regular_file()) continue;

                std::string pathStr = dirEntry.path().string();
                FileInfo info;
                info.lastWrite = dirEntry.last_write_time();
                info.size = dirEntry.file_size();
                currentFiles[pathStr] = info;

                auto it = entry.files.find(pathStr);
                if (it == entry.files.end()) {
                    // New file
                    emitEvent(Action::Add, entry.path, dirEntry.path().filename());
                } else if (it->second.lastWrite != info.lastWrite ||
                           it->second.size != info.size) {
                    // Modified
                    emitEvent(Action::Modified, entry.path, dirEntry.path().filename());
                }
            }

            // Check for deleted files
            for (const auto& [pathStr, info] : entry.files) {
                if (currentFiles.find(pathStr) == currentFiles.end()) {
                    fs::path p(pathStr);
                    emitEvent(Action::Delete, entry.path, p.filename());
                }
            }

            entry.files = std::move(currentFiles);
        }
    }

    void emitEvent(Action action, const fs::path& dir, const fs::path& filename) {
        if (callback_) {
            FileEvent event;
            event.action = action;
            event.directory = dir;
            event.filename = filename;
            event.timestamp = std::chrono::system_clock::now();
            callback_(event);
        }
    }

    std::chrono::milliseconds pollInterval_;
    std::unordered_map<int, WatchEntry> watches_;
    int nextId_ = 1;
    EventCallback callback_;
    std::mutex mutex_;
    std::atomic<bool> running_{false};
    std::thread pollThread_;
};

// =============================================================================
// Factory
// =============================================================================

/**
 * @brief Create the best available file watcher
 */
inline std::unique_ptr<IFileWatcher> createWatcher() {
#ifdef MAKINEAI_HAS_EFSW
    return std::make_unique<EfswWatcher>();
#else
    return std::make_unique<PollingWatcher>();
#endif
}

/**
 * @brief Check if EFSW is available
 */
inline bool hasEfsw() noexcept {
#ifdef MAKINEAI_HAS_EFSW
    return true;
#else
    return false;
#endif
}

// =============================================================================
// Simple Watch Helper
// =============================================================================

/**
 * @brief Simple one-shot watch for a single directory
 *
 * Convenience function for simple use cases.
 */
inline std::unique_ptr<IFileWatcher> watch(
    const fs::path& path,
    EventCallback callback,
    bool recursive = true
) {
    auto watcher = createWatcher();
    watcher->setCallback(std::move(callback));
    watcher->addWatch(path, recursive);
    watcher->start();
    return watcher;
}

} // namespace makineai::watch
