/**
 * @file scanner_base.hpp
 * @brief Standardized scanner interface for MakineAI
 *
 * Provides:
 * - IScannerV2: Enhanced scanner interface with standard error handling
 * - ScannerBase: Base implementation with common functionality
 * - ScannerRegistry: Dynamic scanner registration
 * - ScannerCache: Shared caching for all scanners
 *
 * Design principles:
 * - Consistent error handling across all scanners
 * - Shared caching strategy
 * - Common path resolution utilities
 * - Standardized progress reporting
 *
 * Usage:
 * @code
 * class MySteamScanner : public ScannerBase {
 * public:
 *     MySteamScanner() : ScannerBase("Steam", GameStore::Steam) {}
 *
 * protected:
 *     std::vector<GameInfo> doScan(ScanContext& ctx) override {
 *         // Implementation
 *     }
 * };
 *
 * // Register scanner
 * ScannerRegistry::instance().registerScanner<MySteamScanner>();
 *
 * // Use all registered scanners
 * auto games = ScannerRegistry::instance().scanAll();
 * @endcode
 *
 * Copyright (c) 2026 MakineAI Team
 */

#pragma once

#include "makineai/types.hpp"
#include "makineai/error.hpp"
#include "makineai/logging.hpp"
#include "makineai/cache.hpp"

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace makineai::scanners {

// Forward declarations
class ScannerBase;

// =============================================================================
// SCAN CONTEXT
// =============================================================================

/**
 * @brief Context passed to scanners during scan operation
 */
struct ScanContext {
    // Progress reporting
    std::function<void(const std::string&, float)> onProgress;

    // Cancellation
    std::atomic<bool>* cancelToken = nullptr;

    // Options
    bool includeNonGame = false;        ///< Include non-game software
    bool deepScan = false;              ///< Perform deeper analysis (slower)
    bool useCache = true;               ///< Use cached results if available
    std::chrono::seconds cacheMaxAge{300};  ///< Max cache age (5 min default)

    // Filters
    std::vector<std::string> includeStores;  ///< Only scan these stores (empty = all)
    std::vector<fs::path> additionalPaths;   ///< Additional paths to scan

    /**
     * @brief Report progress
     */
    void reportProgress(const std::string& message, float percent) const {
        if (onProgress) {
            onProgress(message, percent);
        }
    }

    /**
     * @brief Check if cancelled
     */
    [[nodiscard]] bool isCancelled() const noexcept {
        return cancelToken && cancelToken->load();
    }

    /**
     * @brief Check if store should be scanned
     */
    [[nodiscard]] bool shouldScanStore(const std::string& storeName) const {
        if (includeStores.empty()) return true;
        return std::find(includeStores.begin(), includeStores.end(), storeName)
               != includeStores.end();
    }
};

// =============================================================================
// SCAN RESULT
// =============================================================================

/**
 * @brief Result from a single scanner
 */
struct ScanResult {
    std::string scannerName;
    GameStore store = GameStore::Unknown;
    std::vector<GameInfo> games;
    std::vector<Error> errors;
    std::vector<std::string> warnings;

    std::chrono::milliseconds duration{0};
    bool fromCache = false;
    bool cancelled = false;

    /**
     * @brief Check if scan was successful
     */
    [[nodiscard]] bool success() const noexcept {
        return errors.empty() && !cancelled;
    }

    /**
     * @brief Get game count
     */
    [[nodiscard]] size_t gameCount() const noexcept {
        return games.size();
    }

    /**
     * @brief Merge results from another scan
     */
    void merge(const ScanResult& other) {
        games.insert(games.end(), other.games.begin(), other.games.end());
        errors.insert(errors.end(), other.errors.begin(), other.errors.end());
        warnings.insert(warnings.end(), other.warnings.begin(), other.warnings.end());
        duration += other.duration;
    }
};

// =============================================================================
// SCANNER INTERFACE V2
// =============================================================================

/**
 * @brief Enhanced scanner interface with standardized error handling
 */
class IScannerV2 {
public:
    virtual ~IScannerV2() = default;

    /**
     * @brief Get scanner name (e.g., "Steam", "Epic", "GOG")
     */
    [[nodiscard]] virtual std::string name() const = 0;

    /**
     * @brief Get associated game store
     */
    [[nodiscard]] virtual GameStore store() const = 0;

    /**
     * @brief Check if scanner is available on this system
     */
    [[nodiscard]] virtual bool isAvailable() const = 0;

    /**
     * @brief Perform scan operation
     */
    [[nodiscard]] virtual ScanResult scan(ScanContext& ctx) = 0;

    /**
     * @brief Get scanner priority (higher = scanned first)
     */
    [[nodiscard]] virtual int priority() const { return 0; }

    /**
     * @brief Get estimated scan time (for progress calculation)
     */
    [[nodiscard]] virtual std::chrono::milliseconds estimatedScanTime() const {
        return std::chrono::milliseconds{1000};
    }
};

// =============================================================================
// SCANNER BASE IMPLEMENTATION
// =============================================================================

/**
 * @brief Base implementation providing common scanner functionality
 */
class ScannerBase : public IScannerV2 {
public:
    /**
     * @brief Construct with name and store
     */
    ScannerBase(std::string name, GameStore store)
        : name_(std::move(name))
        , store_(store)
    {}

    [[nodiscard]] std::string name() const override { return name_; }
    [[nodiscard]] GameStore store() const override { return store_; }

    /**
     * @brief Default availability check (returns true)
     */
    [[nodiscard]] bool isAvailable() const override { return true; }

    /**
     * @brief Scan with error handling and caching
     */
    [[nodiscard]] ScanResult scan(ScanContext& ctx) override {
        ScanResult result;
        result.scannerName = name_;
        result.store = store_;

        auto startTime = std::chrono::steady_clock::now();

        // Check cache
        if (ctx.useCache) {
            auto cached = getFromCache(ctx.cacheMaxAge);
            if (cached) {
                MAKINEAI_LOG_DEBUG("SCANNER", "{}: Using cached results ({} games)",
                    name_, cached->size());
                result.games = std::move(*cached);
                result.fromCache = true;
                result.duration = std::chrono::milliseconds{0};
                return result;
            }
        }

        // Check availability
        if (!isAvailable()) {
            MAKINEAI_LOG_INFO("SCANNER", "{}: Not available on this system", name_);
            return result;
        }

        // Perform scan
        try {
            ctx.reportProgress(name_ + ": Scanning...", 0.0f);

            result.games = doScan(ctx);

            // Update cache
            updateCache(result.games);

            MAKINEAI_LOG_INFO("SCANNER", "{}: Found {} games", name_, result.games.size());

        } catch (const std::exception& e) {
            MAKINEAI_LOG_ERROR("SCANNER", "{}: Scan failed: {}", name_, e.what());
            result.errors.push_back(Error(ErrorCode::ScanFailed,
                std::string("Scan failed: ") + e.what()));
        }

        // Check cancellation
        if (ctx.isCancelled()) {
            result.cancelled = true;
            result.warnings.push_back("Scan was cancelled");
        }

        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime);

        return result;
    }

protected:
    /**
     * @brief Implement actual scan logic in derived class
     */
    virtual std::vector<GameInfo> doScan(ScanContext& ctx) = 0;

    // =========================================================================
    // UTILITY METHODS FOR DERIVED CLASSES
    // =========================================================================

    /**
     * @brief Resolve environment variable in path
     */
    [[nodiscard]] static fs::path resolveEnvPath(const std::string& pathWithEnv) {
        std::string result = pathWithEnv;

        // Find and replace %VAR% patterns
        size_t start = 0;
        while ((start = result.find('%', start)) != std::string::npos) {
            size_t end = result.find('%', start + 1);
            if (end == std::string::npos) break;

            std::string varName = result.substr(start + 1, end - start - 1);
            const char* varValue = std::getenv(varName.c_str());

            if (varValue) {
                result.replace(start, end - start + 1, varValue);
            } else {
                start = end + 1;
            }
        }

        return fs::path(result);
    }

    /**
     * @brief Check if path is valid game directory
     */
    [[nodiscard]] static bool isValidGameDirectory(const fs::path& path) {
        if (path.empty()) return false;

        std::error_code ec;
        if (!fs::exists(path, ec) || !fs::is_directory(path, ec)) {
            return false;
        }

        // Check it's not empty
        return !fs::is_empty(path, ec);
    }

    /**
     * @brief Find executable in directory
     */
    [[nodiscard]] static std::optional<fs::path> findExecutable(const fs::path& dir) {
        if (!fs::exists(dir)) return std::nullopt;

        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (!entry.is_regular_file()) continue;

            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            if (ext == ".exe") {
                return entry.path();
            }
        }

        return std::nullopt;
    }

    /**
     * @brief Read file content safely
     */
    [[nodiscard]] static Result<std::string> readFileContent(const fs::path& path,
                                                              size_t maxSize = 10 * 1024 * 1024) {
        if (!fs::exists(path)) {
            return std::unexpected(Error(ErrorCode::FileNotFound,
                "File not found").withFile(path));
        }

        std::error_code ec;
        auto size = fs::file_size(path, ec);
        if (ec || size > maxSize) {
            return std::unexpected(Error(ErrorCode::FileTooLarge,
                "File too large or unreadable").withFile(path));
        }

        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return std::unexpected(Error(ErrorCode::FileAccessDenied,
                "Cannot open file").withFile(path));
        }

        std::string content(static_cast<size_t>(size), '\0');
        if (!file.read(content.data(), size)) {
            return std::unexpected(Error(ErrorCode::FileReadFailed,
                "Failed to read file").withFile(path));
        }

        return content;
    }

    /**
     * @brief Create GameInfo with common fields filled
     */
    [[nodiscard]] GameInfo createGameInfo(const std::string& id,
                                          const std::string& name,
                                          const fs::path& installPath) const {
        GameInfo info;
        info.id = GameId(store_, id);
        info.name = name;
        info.installPath = installPath;
        info.store = store_;
        info.detectedAt = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        return info;
    }

    /**
     * @brief Add warning to context (for logging)
     */
    void addWarning(ScanContext& ctx, const std::string& message) const {
        MAKINEAI_LOG_WARN("SCANNER", "{}: {}", name_, message);
    }

private:
    std::optional<std::vector<GameInfo>> getFromCache(std::chrono::seconds maxAge) const {
        std::lock_guard lock(cacheMutex_);

        if (!cachedGames_.has_value()) return std::nullopt;

        auto age = std::chrono::steady_clock::now() - cacheTime_;
        if (age > maxAge) {
            return std::nullopt;
        }

        return cachedGames_;
    }

    void updateCache(const std::vector<GameInfo>& games) {
        std::lock_guard lock(cacheMutex_);
        cachedGames_ = games;
        cacheTime_ = std::chrono::steady_clock::now();
    }

    std::string name_;
    GameStore store_;

    mutable std::mutex cacheMutex_;
    std::optional<std::vector<GameInfo>> cachedGames_;
    std::chrono::steady_clock::time_point cacheTime_;
};

// =============================================================================
// SCANNER REGISTRY
// =============================================================================

/**
 * @brief Registry for dynamic scanner management
 */
class ScannerRegistry {
public:
    /**
     * @brief Get singleton instance
     */
    static ScannerRegistry& instance() {
        static ScannerRegistry registry;
        return registry;
    }

    /**
     * @brief Register a scanner
     */
    template<typename T, typename... Args>
    void registerScanner(Args&&... args) {
        static_assert(std::is_base_of_v<IScannerV2, T>,
            "T must derive from IScannerV2");

        auto scanner = std::make_unique<T>(std::forward<Args>(args)...);
        std::lock_guard lock(mutex_);
        scanners_.push_back(std::move(scanner));

        // Sort by priority
        std::sort(scanners_.begin(), scanners_.end(),
            [](const auto& a, const auto& b) {
                return a->priority() > b->priority();
            });
    }

    /**
     * @brief Register an existing scanner instance
     */
    void registerScanner(std::unique_ptr<IScannerV2> scanner) {
        std::lock_guard lock(mutex_);
        scanners_.push_back(std::move(scanner));

        std::sort(scanners_.begin(), scanners_.end(),
            [](const auto& a, const auto& b) {
                return a->priority() > b->priority();
            });
    }

    /**
     * @brief Get all registered scanners
     */
    [[nodiscard]] std::vector<IScannerV2*> getScanners() const {
        std::lock_guard lock(mutex_);
        std::vector<IScannerV2*> result;
        result.reserve(scanners_.size());
        for (const auto& s : scanners_) {
            result.push_back(s.get());
        }
        return result;
    }

    /**
     * @brief Get scanner by name
     */
    [[nodiscard]] IScannerV2* getScanner(const std::string& name) const {
        std::lock_guard lock(mutex_);
        for (const auto& s : scanners_) {
            if (s->name() == name) {
                return s.get();
            }
        }
        return nullptr;
    }

    /**
     * @brief Get scanner by store
     */
    [[nodiscard]] IScannerV2* getScannerByStore(GameStore store) const {
        std::lock_guard lock(mutex_);
        for (const auto& s : scanners_) {
            if (s->store() == store) {
                return s.get();
            }
        }
        return nullptr;
    }

    /**
     * @brief Scan using all registered scanners
     */
    [[nodiscard]] ScanResult scanAll(ScanContext ctx = {}) {
        ScanResult combined;
        combined.scannerName = "All";

        auto startTime = std::chrono::steady_clock::now();

        std::vector<IScannerV2*> scanners;
        {
            std::lock_guard lock(mutex_);
            for (const auto& s : scanners_) {
                if (ctx.shouldScanStore(s->name())) {
                    scanners.push_back(s.get());
                }
            }
        }

        // Calculate total estimated time for progress
        std::chrono::milliseconds totalEstimated{0};
        for (const auto* scanner : scanners) {
            totalEstimated += scanner->estimatedScanTime();
        }

        std::chrono::milliseconds elapsed{0};

        for (auto* scanner : scanners) {
            if (ctx.isCancelled()) {
                combined.cancelled = true;
                break;
            }

            // Create sub-context with adjusted progress
            ScanContext subCtx = ctx;
            subCtx.onProgress = [&](const std::string& msg, float /*percent*/) {
                if (ctx.onProgress) {
                    float totalPercent = totalEstimated.count() > 0
                        ? 100.0f * elapsed.count() / totalEstimated.count()
                        : 0.0f;
                    ctx.onProgress(msg, totalPercent);
                }
            };

            auto result = scanner->scan(subCtx);
            combined.merge(result);

            elapsed += scanner->estimatedScanTime();
        }

        combined.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime);

        // Remove duplicates (same game detected by multiple scanners)
        removeDuplicates(combined.games);

        return combined;
    }

    /**
     * @brief Get available scanner names
     */
    [[nodiscard]] std::vector<std::string> availableScanners() const {
        std::lock_guard lock(mutex_);
        std::vector<std::string> result;
        for (const auto& s : scanners_) {
            if (s->isAvailable()) {
                result.push_back(s->name());
            }
        }
        return result;
    }

    /**
     * @brief Clear all registered scanners
     */
    void clear() {
        std::lock_guard lock(mutex_);
        scanners_.clear();
    }

private:
    ScannerRegistry() = default;

    static void removeDuplicates(std::vector<GameInfo>& games) {
        // Sort by install path
        std::sort(games.begin(), games.end(),
            [](const GameInfo& a, const GameInfo& b) {
                return a.installPath < b.installPath;
            });

        // Remove duplicates
        games.erase(std::unique(games.begin(), games.end(),
            [](const GameInfo& a, const GameInfo& b) {
                return a.installPath == b.installPath;
            }), games.end());
    }

    mutable std::mutex mutex_;
    std::vector<std::unique_ptr<IScannerV2>> scanners_;
};

// =============================================================================
// CONVENIENCE FUNCTIONS
// =============================================================================

/**
 * @brief Access scanner registry
 */
inline ScannerRegistry& scannerRegistry() {
    return ScannerRegistry::instance();
}

/**
 * @brief Quick scan all available stores
 */
inline ScanResult scanAllStores(ScanContext ctx = {}) {
    return ScannerRegistry::instance().scanAll(std::move(ctx));
}

} // namespace makineai::scanners
