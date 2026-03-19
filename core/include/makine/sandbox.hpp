/**
 * @file sandbox.hpp
 * @brief Sandboxing preparation for file and network access control
 * @copyright (c) 2026 MakineCeviri Team
 *
 * This header provides infrastructure for future sandboxing capabilities.
 * Current implementation focuses on policy definition and access control
 * interfaces that can be integrated with OS-level sandboxing.
 *
 * Features:
 * - File system access policies
 * - Network access policies
 * - Process restrictions
 * - Resource limits
 *
 * @note Full sandboxing requires OS-level support (Windows Job Objects,
 * Linux seccomp, etc.). This header provides the policy layer.
 */

#pragma once

#include "fwd.hpp"
#include "error.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace makine {

// ============================================================================
// POLICY TYPES
// ============================================================================

/**
 * @brief File access permission levels
 */
enum class FilePermission : int {
    None = 0,
    Read = 1,
    Write = 2,
    Execute = 4,
    Delete = 8,
    ReadWrite = Read | Write,
    Full = Read | Write | Execute | Delete
};

/**
 * @brief Combine file permissions
 */
inline FilePermission operator|(FilePermission a, FilePermission b) {
    return static_cast<FilePermission>(static_cast<int>(a) | static_cast<int>(b));
}

/**
 * @brief Check if permission is set
 */
inline bool hasPermission(FilePermission permissions, FilePermission check) {
    return (static_cast<int>(permissions) & static_cast<int>(check)) != 0;
}

/**
 * @brief Network access level
 */
enum class NetworkAccess : int {
    None = 0,
    LocalOnly = 1,
    AllowList = 2,
    DenyList = 3,
    Full = 4
};

/**
 * @brief Process restriction level
 */
enum class ProcessRestriction : int {
    None = 0,
    NoChildProcesses = 1,
    NoElevation = 2,
    NoDynamicCode = 4,
    NoWin32kCalls = 8,
    Strict = NoChildProcesses | NoElevation | NoDynamicCode
};

inline ProcessRestriction operator|(ProcessRestriction a, ProcessRestriction b) {
    return static_cast<ProcessRestriction>(static_cast<int>(a) | static_cast<int>(b));
}

// ============================================================================
// FILE ACCESS RULE
// ============================================================================

/**
 * @brief Rule for file system access control
 */
struct FileAccessRule {
    /// Path pattern (glob or regex)
    std::string pathPattern;

    /// Whether pattern is regex (false = glob)
    bool isRegex = false;

    /// Allowed permissions
    FilePermission permissions = FilePermission::None;

    /// Priority (higher = checked first)
    int priority = 0;

    /// Description for logging
    std::string description;

    /**
     * @brief Check if path matches this rule
     */
    [[nodiscard]] bool matches(const fs::path& path) const;

    /**
     * @brief Create read-only rule for path
     */
    static FileAccessRule readOnly(const std::string& pattern, const std::string& desc = "") {
        return {pattern, false, FilePermission::Read, 0, desc};
    }

    /**
     * @brief Create read-write rule for path
     */
    static FileAccessRule readWrite(const std::string& pattern, const std::string& desc = "") {
        return {pattern, false, FilePermission::ReadWrite, 0, desc};
    }

    /**
     * @brief Create deny rule for path
     */
    static FileAccessRule deny(const std::string& pattern, const std::string& desc = "") {
        return {pattern, false, FilePermission::None, 100, desc};  // High priority
    }
};

// ============================================================================
// NETWORK ACCESS RULE
// ============================================================================

/**
 * @brief Rule for network access control
 */
struct NetworkAccessRule {
    /// Host pattern (domain or IP)
    std::string hostPattern;

    /// Port range (0 = any)
    uint16_t portMin = 0;
    uint16_t portMax = 65535;

    /// Whether to allow or deny
    bool allow = true;

    /// Protocol (tcp, udp, or empty for both)
    std::string protocol;

    /// Description
    std::string description;

    /**
     * @brief Check if host:port matches this rule
     */
    [[nodiscard]] bool matches(const std::string& host, uint16_t port) const;

    /**
     * @brief Create allow rule for HTTPS to specific host
     */
    static NetworkAccessRule allowHttps(const std::string& host, const std::string& desc = "") {
        return {host, 443, 443, true, "tcp", desc};
    }

    /**
     * @brief Create allow rule for localhost only
     */
    static NetworkAccessRule localhostOnly(const std::string& desc = "") {
        return {"localhost", 0, 65535, true, "", desc};
    }
};

// ============================================================================
// RESOURCE LIMITS
// ============================================================================

/**
 * @brief Resource limits for sandboxed context
 */
struct ResourceLimits {
    /// Maximum memory usage (bytes, 0 = unlimited)
    size_t maxMemoryBytes = 0;

    /// Maximum CPU time (0 = unlimited)
    std::chrono::seconds maxCpuTime{0};

    /// Maximum wall clock time
    std::chrono::seconds maxWallTime{0};

    /// Maximum number of open files
    size_t maxOpenFiles = 1024;

    /// Maximum file size for writes (bytes)
    size_t maxFileSizeBytes = 100 * 1024 * 1024;  // 100 MB

    /// Maximum total disk writes (bytes)
    size_t maxDiskWriteBytes = 1024 * 1024 * 1024;  // 1 GB

    /// Maximum network bandwidth (bytes/sec, 0 = unlimited)
    size_t maxNetworkBandwidth = 0;

    /**
     * @brief Create strict limits for untrusted code
     */
    static ResourceLimits strict() {
        ResourceLimits limits;
        limits.maxMemoryBytes = 512 * 1024 * 1024;  // 512 MB
        limits.maxCpuTime = std::chrono::seconds(60);
        limits.maxWallTime = std::chrono::seconds(300);
        limits.maxOpenFiles = 100;
        limits.maxFileSizeBytes = 10 * 1024 * 1024;  // 10 MB
        limits.maxDiskWriteBytes = 100 * 1024 * 1024;  // 100 MB
        return limits;
    }

    /**
     * @brief Create permissive limits for trusted code
     */
    static ResourceLimits permissive() {
        return ResourceLimits{};  // Defaults (mostly unlimited)
    }
};

// ============================================================================
// SANDBOX POLICY
// ============================================================================

/**
 * @brief Complete sandbox policy configuration
 */
struct SandboxPolicy {
    /// Policy name
    std::string name = "default";

    /// File access rules (checked in priority order)
    std::vector<FileAccessRule> fileRules;

    /// Network access rules
    std::vector<NetworkAccessRule> networkRules;

    /// Network access level
    NetworkAccess networkAccess = NetworkAccess::AllowList;

    /// Process restrictions
    ProcessRestriction processRestrictions = ProcessRestriction::None;

    /// Resource limits
    ResourceLimits limits;

    /// Whether policy is enforced (false = audit only)
    bool enforced = false;

    /// Callback for policy violations (for audit mode)
    std::function<void(const std::string& violation)> onViolation;

    /**
     * @brief Create policy for patching operations
     *
     * Allows read-write to game directory, read-only to system,
     * network access to Makine API only.
     */
    static SandboxPolicy forPatching(const fs::path& gameDir);

    /**
     * @brief Create policy for package extraction
     *
     * Allows read from package, write to temp and game dir,
     * no network access.
     */
    static SandboxPolicy forExtraction(const fs::path& packagePath, const fs::path& targetDir);

    /**
     * @brief Create audit-only policy (logs but doesn't block)
     */
    static SandboxPolicy auditOnly(const std::string& name);
};

// ============================================================================
// SANDBOX CONTEXT
// ============================================================================

/**
 * @brief Sandbox execution context
 *
 * Provides access control and resource tracking for sandboxed operations.
 * Can be used in audit mode (log violations) or enforcement mode (block violations).
 */
class SandboxContext {
public:
    /**
     * @brief Create sandbox context with policy
     */
    explicit SandboxContext(SandboxPolicy policy);

    /**
     * @brief Destructor (releases any OS resources)
     */
    ~SandboxContext();

    // Non-copyable, movable
    SandboxContext(const SandboxContext&) = delete;
    SandboxContext& operator=(const SandboxContext&) = delete;
    SandboxContext(SandboxContext&&) noexcept;
    SandboxContext& operator=(SandboxContext&&) noexcept;

    /**
     * @brief Get policy name
     */
    [[nodiscard]] const std::string& name() const noexcept { return policy_.name; }

    /**
     * @brief Check if enforcement is enabled
     */
    [[nodiscard]] bool isEnforced() const noexcept { return policy_.enforced; }

    // =========== FILE ACCESS ===========

    /**
     * @brief Check if file access is allowed
     * @param path File path
     * @param permission Required permission
     * @return true if allowed, false if denied
     */
    [[nodiscard]] bool checkFileAccess(const fs::path& path, FilePermission permission);

    /**
     * @brief Check and log file access
     * @return Result with error if denied and enforced
     */
    [[nodiscard]] Result<void> requireFileAccess(const fs::path& path, FilePermission permission);

    /**
     * @brief Add allowed path at runtime
     */
    void allowPath(const fs::path& path, FilePermission permission);

    /**
     * @brief Remove path from allowed list
     */
    void denyPath(const fs::path& path);

    // =========== NETWORK ACCESS ===========

    /**
     * @brief Check if network access is allowed
     * @param host Host name or IP
     * @param port Port number
     * @return true if allowed
     */
    [[nodiscard]] bool checkNetworkAccess(const std::string& host, uint16_t port);

    /**
     * @brief Check and log network access
     */
    [[nodiscard]] Result<void> requireNetworkAccess(const std::string& host, uint16_t port);

    /**
     * @brief Add allowed host at runtime
     */
    void allowHost(const std::string& host, uint16_t port = 0);

    /**
     * @brief Block host
     */
    void denyHost(const std::string& host);

    // =========== RESOURCE TRACKING ===========

    /**
     * @brief Record memory allocation
     * @param bytes Number of bytes allocated
     * @return true if within limits
     */
    [[nodiscard]] bool recordMemoryAlloc(size_t bytes);

    /**
     * @brief Record memory deallocation
     */
    void recordMemoryFree(size_t bytes);

    /**
     * @brief Record file write
     * @return true if within limits
     */
    [[nodiscard]] bool recordFileWrite(size_t bytes);

    /**
     * @brief Check if CPU time limit exceeded
     */
    [[nodiscard]] bool isCpuTimeExceeded() const;

    /**
     * @brief Check if wall time limit exceeded
     */
    [[nodiscard]] bool isWallTimeExceeded() const;

    // =========== STATISTICS ===========

    /**
     * @brief Sandbox statistics
     */
    struct Stats {
        size_t fileAccessChecks = 0;
        size_t fileAccessDenied = 0;
        size_t networkAccessChecks = 0;
        size_t networkAccessDenied = 0;
        size_t currentMemoryBytes = 0;
        size_t peakMemoryBytes = 0;
        size_t totalDiskWriteBytes = 0;
        std::vector<std::string> violations;
    };

    /**
     * @brief Get current statistics
     */
    [[nodiscard]] Stats getStats() const;

    /**
     * @brief Reset statistics
     */
    void resetStats();

private:
    SandboxPolicy policy_;
    mutable std::mutex mutex_;

    // Runtime state
    std::unordered_set<std::string> runtimeAllowedPaths_;
    std::unordered_set<std::string> runtimeDeniedPaths_;
    std::unordered_set<std::string> runtimeAllowedHosts_;
    std::unordered_set<std::string> runtimeDeniedHosts_;

    // Resource tracking
    std::atomic<size_t> currentMemory_{0};
    std::atomic<size_t> peakMemory_{0};
    std::atomic<size_t> totalDiskWrites_{0};
    std::chrono::steady_clock::time_point startTime_;

    // Statistics
    std::atomic<size_t> fileAccessChecks_{0};
    std::atomic<size_t> fileAccessDenied_{0};
    std::atomic<size_t> networkAccessChecks_{0};
    std::atomic<size_t> networkAccessDenied_{0};
    std::vector<std::string> violations_;

    void recordViolation(const std::string& message);
};

// ============================================================================
// SCOPED SANDBOX
// ============================================================================

/**
 * @brief RAII guard for sandbox context
 *
 * Automatically enters sandbox on construction and exits on destruction.
 * Thread-local sandbox context stack.
 */
class ScopedSandbox {
public:
    /**
     * @brief Enter sandbox with policy
     */
    explicit ScopedSandbox(SandboxPolicy policy);

    /**
     * @brief Enter sandbox with existing context
     */
    explicit ScopedSandbox(std::shared_ptr<SandboxContext> context);

    /**
     * @brief Exit sandbox
     */
    ~ScopedSandbox();

    // Non-copyable
    ScopedSandbox(const ScopedSandbox&) = delete;
    ScopedSandbox& operator=(const ScopedSandbox&) = delete;

    /**
     * @brief Get current context
     */
    [[nodiscard]] SandboxContext& context() { return *context_; }

    /**
     * @brief Get current sandbox context (thread-local)
     * @return Current context or nullptr if not in sandbox
     */
    [[nodiscard]] static SandboxContext* current();

    /**
     * @brief Check if currently in a sandbox
     */
    [[nodiscard]] static bool isActive();

private:
    std::shared_ptr<SandboxContext> context_;
    SandboxContext* previous_;
};

// ============================================================================
// CONVENIENCE FUNCTIONS
// ============================================================================

/**
 * @brief Check file access in current sandbox (if any)
 *
 * If not in sandbox, always returns true.
 */
[[nodiscard]] inline bool sandboxCheckFile(const fs::path& path, FilePermission perm) {
    auto* ctx = ScopedSandbox::current();
    return ctx ? ctx->checkFileAccess(path, perm) : true;
}

/**
 * @brief Check network access in current sandbox (if any)
 */
[[nodiscard]] inline bool sandboxCheckNetwork(const std::string& host, uint16_t port) {
    auto* ctx = ScopedSandbox::current();
    return ctx ? ctx->checkNetworkAccess(host, port) : true;
}

/**
 * @brief Run function in sandbox
 */
template<typename Func>
auto runInSandbox(SandboxPolicy policy, Func&& func) -> decltype(func()) {
    ScopedSandbox sandbox(std::move(policy));
    return func();
}

// ============================================================================
// SANDBOX MACROS
// ============================================================================

/**
 * @brief Check file access and return error if denied
 */
#define SANDBOX_CHECK_FILE(path, perm) \
    do { \
        if (auto* _ctx = makine::ScopedSandbox::current()) { \
            auto _result = _ctx->requireFileAccess(path, perm); \
            if (!_result) return _result.error(); \
        } \
    } while (false)

/**
 * @brief Check network access and return error if denied
 */
#define SANDBOX_CHECK_NETWORK(host, port) \
    do { \
        if (auto* _ctx = makine::ScopedSandbox::current()) { \
            auto _result = _ctx->requireNetworkAccess(host, port); \
            if (!_result) return _result.error(); \
        } \
    } while (false)

}  // namespace makine
