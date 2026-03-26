#pragma once

/**
 * @file lazy.hpp
 * @brief Lazy loading and deferred initialization utilities for Makine
 *
 * Provides:
 * - Lazy<T>: Thread-safe lazy initialization
 * - LazyWeakRef<T>: Lazy initialization with weak ownership
 * - LazyFile: Lazy file loading with caching
 * - LazyJson: Lazy JSON parsing
 * - LazyDatabase: Lazy database connection
 *
 * Design principles:
 * - Thread-safe by default (uses std::call_once)
 * - Zero overhead when not accessed
 * - Exception-safe initialization
 * - Move-only semantics for expensive resources
 *
 * Usage:
 * @code
 * // Basic lazy value
 * Lazy<ExpensiveObject> obj([]() {
 *     return ExpensiveObject::create();
 * });
 * // Object not created yet
 * obj.get();  // Now it's created (and cached)
 * obj.get();  // Returns cached instance
 *
 * // Lazy file content
 * LazyFile config("config.json");
 * if (config.exists()) {
 *     auto content = config.content();  // Loaded on first access
 * }
 *
 * // Lazy JSON
 * LazyJson settings(configPath);
 * auto value = settings.get<std::string>("database.host", "localhost");
 * @endcode
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include "makine/types.hpp"
#include "makine/error.hpp"
#include "makine/logging.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <type_traits>
#include <variant>

#include <nlohmann/json.hpp>

namespace makine {

// =============================================================================
// LAZY<T> - Core Lazy Initialization
// =============================================================================

/**
 * @brief Thread-safe lazy initialization wrapper
 *
 * Defers construction of T until first access. Construction happens
 * exactly once, even with concurrent access from multiple threads.
 *
 * @tparam T Type to lazily initialize (must be move-constructible)
 */
template<typename T>
class Lazy {
public:
    using Factory = std::function<T()>;
    using FactoryWithError = std::function<Result<T>()>;

    /**
     * @brief Construct with factory function
     * @param factory Function that creates T (called on first access)
     */
    explicit Lazy(Factory factory)
        : factory_(std::move(factory))
        , factoryWithError_(nullptr)
        , initialized_(false)
    {}

    /**
     * @brief Construct with factory that may fail
     * @param factory Function that returns Result<T>
     */
    explicit Lazy(FactoryWithError factory)
        : factory_(nullptr)
        , factoryWithError_(std::move(factory))
        , initialized_(false)
    {}

    /**
     * @brief Construct with value (already initialized)
     */
    explicit Lazy(T value)
        : value_(std::move(value))
        , initialized_(true)
    {}

    // Move-only (copying would share state unexpectedly)
    Lazy(Lazy&& other) noexcept = default;
    Lazy& operator=(Lazy&& other) noexcept = default;
    Lazy(const Lazy&) = delete;
    Lazy& operator=(const Lazy&) = delete;

    /**
     * @brief Get the value, initializing if needed
     * @return Reference to the value
     * @throws std::runtime_error if initialization fails
     */
    [[nodiscard]] T& get() {
        ensureInitialized();
        if (error_) {
            throw std::runtime_error(error_->message());
        }
        return *value_;
    }

    [[nodiscard]] const T& get() const {
        ensureInitialized();
        if (error_) {
            throw std::runtime_error(error_->message());
        }
        return *value_;
    }

    /**
     * @brief Get the value as Result (doesn't throw)
     */
    [[nodiscard]] Result<T> tryGet() {
        ensureInitialized();
        if (error_) {
            return std::unexpected(*error_);
        }
        return *value_;
    }

    [[nodiscard]] Result<T> tryGet() const {
        ensureInitialized();
        if (error_) {
            return std::unexpected(*error_);
        }
        return *value_;
    }

    /**
     * @brief Check if value is initialized
     */
    [[nodiscard]] bool isInitialized() const noexcept {
        return initialized_.load(std::memory_order_acquire);
    }

    /**
     * @brief Check if initialization failed
     */
    [[nodiscard]] bool hasError() const noexcept {
        return error_.has_value();
    }

    /**
     * @brief Get initialization error (if any)
     */
    [[nodiscard]] std::optional<Error> error() const noexcept {
        return error_;
    }

    /**
     * @brief Pointer-like access
     */
    T& operator*() { return get(); }
    const T& operator*() const { return get(); }
    T* operator->() { return &get(); }
    const T* operator->() const { return &get(); }

    /**
     * @brief Reset to uninitialized state
     * @param newFactory Optional new factory function
     */
    void reset(Factory newFactory = nullptr) {
        std::lock_guard lock(mutex_);
        value_.reset();
        error_.reset();
        initialized_.store(false, std::memory_order_release);
        if (newFactory) {
            factory_ = std::move(newFactory);
            factoryWithError_ = nullptr;
        }
    }

    /**
     * @brief Force re-initialization on next access
     */
    void invalidate() {
        std::lock_guard lock(mutex_);
        value_.reset();
        error_.reset();
        initialized_.store(false, std::memory_order_release);
    }

private:
    void ensureInitialized() const {
        // Fast path: already initialized
        if (initialized_.load(std::memory_order_acquire)) {
            return;
        }

        // Slow path: need to initialize
        std::lock_guard lock(mutex_);

        // Double-check after acquiring lock
        if (initialized_.load(std::memory_order_relaxed)) {
            return;
        }

        try {
            if (factoryWithError_) {
                auto result = factoryWithError_();
                if (result) {
                    const_cast<Lazy*>(this)->value_ = std::move(*result);
                } else {
                    const_cast<Lazy*>(this)->error_ = result.error();
                }
            } else if (factory_) {
                const_cast<Lazy*>(this)->value_ = factory_();
            }
        } catch (const std::exception& e) {
            const_cast<Lazy*>(this)->error_ = Error(ErrorCode::Unknown,
                std::string("Lazy initialization failed: ") + e.what());
        }

        initialized_.store(true, std::memory_order_release);
    }

    Factory factory_;
    FactoryWithError factoryWithError_;
    mutable std::mutex mutex_;
    mutable std::atomic<bool> initialized_;
    std::optional<T> value_;
    std::optional<Error> error_;
};

// =============================================================================
// LAZY FILE - Lazy File Loading
// =============================================================================

/**
 * @brief Lazy file content loader with optional caching
 *
 * Defers file reading until content is actually needed.
 * Tracks file modification time for automatic reload.
 */
class LazyFile {
public:
    /**
     * @brief Construct with file path
     * @param path Path to the file
     * @param autoReload Automatically reload if file changed
     */
    explicit LazyFile(fs::path path, bool autoReload = false)
        : path_(std::move(path))
        , autoReload_(autoReload)
    {}

    /**
     * @brief Check if file exists
     */
    [[nodiscard]] bool exists() const {
        std::error_code ec;
        return fs::exists(path_, ec);
    }

    /**
     * @brief Get file path
     */
    [[nodiscard]] const fs::path& path() const noexcept {
        return path_;
    }

    /**
     * @brief Get file size (without loading content)
     */
    [[nodiscard]] Result<uintmax_t> size() const {
        std::error_code ec;
        auto sz = fs::file_size(path_, ec);
        if (ec) {
            return std::unexpected(Error(ErrorCode::FileNotFound,
                "Cannot get file size").withFile(path_));
        }
        return sz;
    }

    /**
     * @brief Get file content (loaded on first access)
     */
    [[nodiscard]] Result<std::string> content() {
        return load();
    }

    /**
     * @brief Get file content as bytes
     */
    [[nodiscard]] Result<ByteBuffer> bytes() {
        auto result = load();
        if (!result) {
            return std::unexpected(result.error());
        }
        return ByteBuffer(content_.begin(), content_.end());
    }

    /**
     * @brief Get lines (split by newline)
     */
    [[nodiscard]] Result<std::vector<std::string>> lines() {
        auto result = load();
        if (!result) {
            return std::unexpected(result.error());
        }

        std::vector<std::string> result_lines;
        std::istringstream iss(content_);
        std::string line;
        while (std::getline(iss, line)) {
            // Remove carriage return if present
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            result_lines.push_back(std::move(line));
        }
        return result_lines;
    }

    /**
     * @brief Check if content is loaded
     */
    [[nodiscard]] bool isLoaded() const noexcept {
        return loaded_.load(std::memory_order_acquire);
    }

    /**
     * @brief Force reload on next access
     */
    void invalidate() {
        std::lock_guard lock(mutex_);
        loaded_.store(false, std::memory_order_release);
        content_.clear();
    }

    /**
     * @brief Get last modification time
     */
    [[nodiscard]] std::optional<fs::file_time_type> lastModified() const {
        std::error_code ec;
        auto time = fs::last_write_time(path_, ec);
        if (ec) return std::nullopt;
        return time;
    }

    /**
     * @brief Check if file has been modified since last load
     */
    [[nodiscard]] bool isModified() const {
        auto current = lastModified();
        if (!current || !loadTime_) return true;
        return *current > *loadTime_;
    }

private:
    Result<std::string> load() {
        // Check if reload needed
        if (autoReload_ && loaded_.load(std::memory_order_acquire) && isModified()) {
            invalidate();
        }

        // Fast path
        if (loaded_.load(std::memory_order_acquire)) {
            if (error_) return std::unexpected(*error_);
            return content_;
        }

        // Slow path
        std::lock_guard lock(mutex_);

        if (loaded_.load(std::memory_order_relaxed)) {
            if (error_) return std::unexpected(*error_);
            return content_;
        }

        // Load file
        std::ifstream file(path_, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            error_ = Error(ErrorCode::FileNotFound,
                "Cannot open file").withFile(path_);
            loaded_.store(true, std::memory_order_release);
            return std::unexpected(*error_);
        }

        auto fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        content_.resize(static_cast<size_t>(fileSize));
        if (!file.read(content_.data(), fileSize)) {
            error_ = Error(ErrorCode::FileNotFound,
                "Failed to read file").withFile(path_);
            content_.clear();
            loaded_.store(true, std::memory_order_release);
            return std::unexpected(*error_);
        }

        loadTime_ = lastModified();
        error_.reset();
        loaded_.store(true, std::memory_order_release);

        MAKINE_LOG_DEBUG("LAZY", "Loaded file: {} ({} bytes)", path_.string(), content_.size());
        return content_;
    }

    fs::path path_;
    bool autoReload_;
    mutable std::mutex mutex_;
    mutable std::atomic<bool> loaded_{false};
    std::string content_;
    std::optional<Error> error_;
    std::optional<fs::file_time_type> loadTime_;
};

// =============================================================================
// LAZY JSON - Lazy JSON Document
// =============================================================================

/**
 * @brief Lazy JSON document loader
 *
 * Defers JSON parsing until values are accessed.
 * Provides type-safe accessors with default values.
 */
class LazyJson {
public:
    using Json = nlohmann::json;

    /**
     * @brief Construct with file path
     */
    explicit LazyJson(fs::path path, bool autoReload = false)
        : file_(std::move(path), autoReload)
    {}

    /**
     * @brief Construct with JSON content string
     */
    explicit LazyJson(std::string jsonContent)
        : directContent_(std::move(jsonContent))
    {}

    /**
     * @brief Check if file exists (for file-based)
     */
    [[nodiscard]] bool exists() const {
        if (directContent_) return true;
        return file_.exists();
    }

    /**
     * @brief Get value at JSON path with default
     *
     * Path uses dot notation: "database.host" accesses json["database"]["host"]
     *
     * @tparam T Type to convert to
     * @param path Dot-separated path
     * @param defaultValue Value if path doesn't exist or type mismatch
     */
    template<typename T>
    [[nodiscard]] T get(const std::string& path, const T& defaultValue = T{}) {
        auto result = parse();
        if (!result) {
            return defaultValue;
        }

        try {
            const Json* current = &(*result);

            // Navigate path
            std::istringstream iss(path);
            std::string segment;
            while (std::getline(iss, segment, '.')) {
                if (current->is_object() && current->contains(segment)) {
                    current = &(*current)[segment];
                } else if (current->is_array()) {
                    size_t index = std::stoul(segment);
                    if (index < current->size()) {
                        current = &(*current)[index];
                    } else {
                        return defaultValue;
                    }
                } else {
                    return defaultValue;
                }
            }

            return current->get<T>();
        } catch (...) {
            return defaultValue;
        }
    }

    /**
     * @brief Get value as Result (doesn't use default)
     */
    template<typename T>
    [[nodiscard]] Result<T> tryGet(const std::string& path) {
        auto result = parse();
        if (!result) {
            return std::unexpected(result.error());
        }

        try {
            const Json* current = &(*result);

            std::istringstream iss(path);
            std::string segment;
            while (std::getline(iss, segment, '.')) {
                if (current->is_object() && current->contains(segment)) {
                    current = &(*current)[segment];
                } else if (current->is_array()) {
                    size_t index = std::stoul(segment);
                    if (index < current->size()) {
                        current = &(*current)[index];
                    } else {
                        return std::unexpected(Error(ErrorCode::InvalidArgument,
                            "JSON path not found: " + path));
                    }
                } else {
                    return std::unexpected(Error(ErrorCode::InvalidArgument,
                        "JSON path not found: " + path));
                }
            }

            return current->get<T>();
        } catch (const std::exception& e) {
            return std::unexpected(Error(ErrorCode::ParseError,
                std::string("JSON conversion failed: ") + e.what()));
        }
    }

    /**
     * @brief Check if path exists
     */
    [[nodiscard]] bool has(const std::string& path) {
        auto result = parse();
        if (!result) return false;

        try {
            const Json* current = &(*result);

            std::istringstream iss(path);
            std::string segment;
            while (std::getline(iss, segment, '.')) {
                if (current->is_object() && current->contains(segment)) {
                    current = &(*current)[segment];
                } else {
                    return false;
                }
            }
            return true;
        } catch (...) {
            return false;
        }
    }

    /**
     * @brief Get the raw JSON object
     */
    [[nodiscard]] Result<Json> json() {
        return parse();
    }

    /**
     * @brief Force reload
     */
    void invalidate() {
        std::lock_guard lock(mutex_);
        parsed_.store(false, std::memory_order_release);
        json_ = Json{};
        error_.reset();
        file_.invalidate();
    }

    /**
     * @brief Check if parsed
     */
    [[nodiscard]] bool isParsed() const noexcept {
        return parsed_.load(std::memory_order_acquire);
    }

private:
    Result<Json> parse() {
        if (parsed_.load(std::memory_order_acquire)) {
            if (error_) return std::unexpected(*error_);
            return json_;
        }

        std::lock_guard lock(mutex_);

        if (parsed_.load(std::memory_order_relaxed)) {
            if (error_) return std::unexpected(*error_);
            return json_;
        }

        std::string content;

        if (directContent_) {
            content = *directContent_;
        } else {
            auto fileResult = file_.content();
            if (!fileResult) {
                error_ = fileResult.error();
                parsed_.store(true, std::memory_order_release);
                return std::unexpected(*error_);
            }
            content = *fileResult;
        }

        try {
            json_ = Json::parse(content);
            error_.reset();
        } catch (const Json::parse_error& e) {
            error_ = Error(ErrorCode::ParseError,
                std::string("JSON parse error: ") + e.what());
            parsed_.store(true, std::memory_order_release);
            return std::unexpected(*error_);
        }

        parsed_.store(true, std::memory_order_release);
        return json_;
    }

    LazyFile file_{fs::path{}};
    std::optional<std::string> directContent_;
    mutable std::mutex mutex_;
    mutable std::atomic<bool> parsed_{false};
    Json json_;
    std::optional<Error> error_;
};

// =============================================================================
// LAZY RESOURCE - Generic Resource with Cleanup
// =============================================================================

/**
 * @brief Lazy resource with custom cleanup
 *
 * For resources that need explicit cleanup (file handles, connections, etc.)
 *
 * @tparam T Resource type
 */
template<typename T>
class LazyResource {
public:
    using Factory = std::function<Result<T>()>;
    using Cleanup = std::function<void(T&)>;

    /**
     * @brief Construct with factory and cleanup
     */
    LazyResource(Factory factory, Cleanup cleanup = nullptr)
        : factory_(std::move(factory))
        , cleanup_(std::move(cleanup))
    {}

    ~LazyResource() {
        if (value_ && cleanup_) {
            try {
                cleanup_(*value_);
            } catch (...) {
                // Don't throw from destructor
            }
        }
    }

    // Move-only
    LazyResource(LazyResource&& other) noexcept = default;
    LazyResource& operator=(LazyResource&& other) noexcept {
        if (this != &other) {
            if (value_ && cleanup_) {
                cleanup_(*value_);
            }
            factory_ = std::move(other.factory_);
            cleanup_ = std::move(other.cleanup_);
            value_ = std::move(other.value_);
            error_ = std::move(other.error_);
            initialized_.store(other.initialized_.load());
            other.initialized_.store(false);
        }
        return *this;
    }
    LazyResource(const LazyResource&) = delete;
    LazyResource& operator=(const LazyResource&) = delete;

    /**
     * @brief Get resource
     */
    [[nodiscard]] Result<T> get() {
        ensureInitialized();
        if (error_) {
            return std::unexpected(*error_);
        }
        return *value_;
    }

    [[nodiscard]] Result<T> get() const {
        ensureInitialized();
        if (error_) {
            return std::unexpected(*error_);
        }
        return *value_;
    }

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool isInitialized() const noexcept {
        return initialized_.load(std::memory_order_acquire);
    }

    /**
     * @brief Release resource (calls cleanup)
     */
    void release() {
        std::lock_guard lock(mutex_);
        if (value_ && cleanup_) {
            cleanup_(*value_);
        }
        value_.reset();
        error_.reset();
        initialized_.store(false, std::memory_order_release);
    }

private:
    void ensureInitialized() const {
        if (initialized_.load(std::memory_order_acquire)) {
            return;
        }

        std::lock_guard lock(mutex_);

        if (initialized_.load(std::memory_order_relaxed)) {
            return;
        }

        auto result = factory_();
        if (result) {
            const_cast<LazyResource*>(this)->value_ = std::move(*result);
        } else {
            const_cast<LazyResource*>(this)->error_ = result.error();
        }

        initialized_.store(true, std::memory_order_release);
    }

    Factory factory_;
    Cleanup cleanup_;
    mutable std::mutex mutex_;
    mutable std::atomic<bool> initialized_{false};
    std::optional<T> value_;
    std::optional<Error> error_;
};

// =============================================================================
// LAZY SINGLETON - Thread-Safe Lazy Singleton
// =============================================================================

/**
 * @brief Helper for creating lazy singletons
 *
 * Usage:
 * @code
 * class MyService {
 * public:
 *     static MyService& instance() {
 *         return LazySingleton<MyService>::instance([]() {
 *             return MyService();
 *         });
 *     }
 * private:
 *     MyService() { // expensive init }
 * };
 * @endcode
 */
template<typename T>
class LazySingleton {
public:
    using Factory = std::function<T()>;

    /**
     * @brief Get or create singleton instance
     */
    static T& instance(Factory factory = nullptr) {
        static std::once_flag initFlag;
        static std::optional<T> instance_;

        std::call_once(initFlag, [&factory]() {
            if (factory) {
                instance_.emplace(factory());
            } else {
                instance_.emplace();
            }
        });

        return *instance_;
    }

    // Prevent instantiation
    LazySingleton() = delete;
};

// =============================================================================
// CONVENIENCE FUNCTIONS
// =============================================================================

/**
 * @brief Create a lazy value
 */
template<typename T, typename Factory>
[[nodiscard]] Lazy<T> makeLazy(Factory&& factory) {
    return Lazy<T>(std::forward<Factory>(factory));
}

/**
 * @brief Create a lazy file
 */
[[nodiscard]] inline LazyFile lazyFile(const fs::path& path, bool autoReload = false) {
    return LazyFile(path, autoReload);
}

/**
 * @brief Create a lazy JSON document
 */
[[nodiscard]] inline LazyJson lazyJson(const fs::path& path, bool autoReload = false) {
    return LazyJson(path, autoReload);
}

} // namespace makine
