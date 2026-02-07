/**
 * @file sandbox.cpp
 * @brief Sandbox policy enforcement implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/sandbox.hpp"
#include "makineai/logging.hpp"

#include <algorithm>
#include <regex>

namespace makineai {

// ============================================================================
// GLOB MATCHING
// ============================================================================

namespace {

// Simple glob pattern matching (supports * and ?)
bool globMatch(std::string_view pattern, std::string_view text) {
    size_t pi = 0, ti = 0;
    size_t starP = std::string_view::npos, starT = 0;

    while (ti < text.size()) {
        if (pi < pattern.size() && (pattern[pi] == text[ti] || pattern[pi] == '?')) {
            ++pi;
            ++ti;
        } else if (pi < pattern.size() && pattern[pi] == '*') {
            starP = pi++;
            starT = ti;
        } else if (starP != std::string_view::npos) {
            pi = starP + 1;
            ti = ++starT;
        } else {
            return false;
        }
    }
    while (pi < pattern.size() && pattern[pi] == '*') ++pi;
    return pi == pattern.size();
}

// Normalize path for comparison (lowercase, forward slashes)
std::string normalizePath(const fs::path& path) {
    auto str = path.generic_string();
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return str;
}

// Thread-local sandbox context stack
thread_local SandboxContext* t_currentContext = nullptr;

} // anonymous namespace

// ============================================================================
// FILE ACCESS RULE
// ============================================================================

bool FileAccessRule::matches(const fs::path& path) const {
    auto normalized = normalizePath(path);

    if (isRegex) {
        try {
            std::regex re(pathPattern, std::regex::icase);
            return std::regex_match(normalized, re);
        } catch (...) {
            return false;
        }
    }

    // Glob match (case-insensitive via normalized path)
    auto normalizedPattern = pathPattern;
    std::transform(normalizedPattern.begin(), normalizedPattern.end(),
                   normalizedPattern.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return globMatch(normalizedPattern, normalized);
}

// ============================================================================
// NETWORK ACCESS RULE
// ============================================================================

bool NetworkAccessRule::matches(const std::string& host, uint16_t port) const {
    // Check port range
    if (port < portMin || port > portMax) {
        return false;
    }

    // Check host pattern
    auto lowerHost = host;
    std::transform(lowerHost.begin(), lowerHost.end(), lowerHost.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    auto lowerPattern = hostPattern;
    std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    return globMatch(lowerPattern, lowerHost);
}

// ============================================================================
// SANDBOX POLICY FACTORIES
// ============================================================================

SandboxPolicy SandboxPolicy::forPatching(const fs::path& gameDir) {
    SandboxPolicy policy;
    policy.name = "patching";
    policy.enforced = true;
    policy.limits = ResourceLimits::strict();

    // Allow read-write to game directory
    policy.fileRules.push_back(
        FileAccessRule::readWrite(gameDir.generic_string() + "/*", "Game directory"));

    // Allow read-write to appdata (backups, cache)
    policy.fileRules.push_back(
        FileAccessRule::readWrite("*/MakineAI/*", "App data"));

    // Allow temp directory
    policy.fileRules.push_back(
        FileAccessRule::readWrite("*/temp/*", "Temp directory"));
    policy.fileRules.push_back(
        FileAccessRule::readWrite("*/tmp/*", "Temp directory"));

    // Block everything else (low priority, checked last)
    auto denyAll = FileAccessRule::deny("*", "Default deny");
    denyAll.priority = -100;
    policy.fileRules.push_back(denyAll);

    // Network: MakineAI API only
    policy.networkAccess = NetworkAccess::AllowList;
    policy.networkRules.push_back(
        NetworkAccessRule::allowHttps("api.makineai.com", "MakineAI API"));
    policy.networkRules.push_back(
        NetworkAccessRule::allowHttps("cdn.makineai.com", "MakineAI CDN"));

    policy.processRestrictions = ProcessRestriction::NoElevation;

    return policy;
}

SandboxPolicy SandboxPolicy::forExtraction(
    const fs::path& packagePath,
    const fs::path& targetDir
) {
    SandboxPolicy policy;
    policy.name = "extraction";
    policy.enforced = true;
    policy.limits = ResourceLimits::strict();
    policy.limits.maxDiskWriteBytes = 2ULL * 1024 * 1024 * 1024;  // 2 GB for large games

    // Read from package
    policy.fileRules.push_back(
        FileAccessRule::readOnly(packagePath.generic_string(), "Source package"));

    // Write to target
    policy.fileRules.push_back(
        FileAccessRule::readWrite(targetDir.generic_string() + "/*", "Target directory"));

    // No network
    policy.networkAccess = NetworkAccess::None;

    policy.processRestrictions = ProcessRestriction::Strict;

    return policy;
}

SandboxPolicy SandboxPolicy::auditOnly(const std::string& name) {
    SandboxPolicy policy;
    policy.name = name;
    policy.enforced = false;  // Audit only — log but don't block
    policy.limits = ResourceLimits::permissive();
    policy.networkAccess = NetworkAccess::Full;
    return policy;
}

// ============================================================================
// SANDBOX CONTEXT
// ============================================================================

SandboxContext::SandboxContext(SandboxPolicy policy)
    : policy_(std::move(policy))
    , startTime_(std::chrono::steady_clock::now())
{
    // Sort file rules by priority (highest first)
    std::sort(policy_.fileRules.begin(), policy_.fileRules.end(),
              [](const FileAccessRule& a, const FileAccessRule& b) {
                  return a.priority > b.priority;
              });

    MAKINEAI_LOG_INFO(log::SECURITY, "Sandbox '{}' created (enforced={})",
        policy_.name, policy_.enforced);
}

SandboxContext::~SandboxContext() {
    auto stats = getStats();
    MAKINEAI_LOG_INFO(log::SECURITY,
        "Sandbox '{}' destroyed: {} file checks ({} denied), "
        "{} network checks ({} denied), {} violations",
        policy_.name,
        stats.fileAccessChecks, stats.fileAccessDenied,
        stats.networkAccessChecks, stats.networkAccessDenied,
        stats.violations.size());
}

SandboxContext::SandboxContext(SandboxContext&&) noexcept = default;
SandboxContext& SandboxContext::operator=(SandboxContext&&) noexcept = default;

// --- File Access ---

bool SandboxContext::checkFileAccess(const fs::path& path, FilePermission permission) {
    fileAccessChecks_.fetch_add(1, std::memory_order_relaxed);

    // Check runtime denied paths first
    {
        std::lock_guard lock(mutex_);
        auto normalized = normalizePath(path);
        if (runtimeDeniedPaths_.count(normalized)) {
            fileAccessDenied_.fetch_add(1, std::memory_order_relaxed);
            recordViolation("File denied (runtime): " + path.string());
            return false;
        }
        if (runtimeAllowedPaths_.count(normalized)) {
            return true;
        }
    }

    // Check rules in priority order
    for (const auto& rule : policy_.fileRules) {
        if (rule.matches(path)) {
            if (hasPermission(rule.permissions, permission)) {
                return true;
            }
            fileAccessDenied_.fetch_add(1, std::memory_order_relaxed);
            recordViolation("File access denied by rule '" + rule.description +
                          "': " + path.string());
            return false;
        }
    }

    // No rule matched — allow (default open for rules that don't cover the path)
    return true;
}

Result<void> SandboxContext::requireFileAccess(
    const fs::path& path, FilePermission permission
) {
    if (checkFileAccess(path, permission)) {
        return {};
    }

    if (!policy_.enforced) {
        return {};  // Audit mode — log but allow
    }

    return std::unexpected(Error(ErrorCode::SecurityViolation,
        "Sandbox '" + policy_.name + "' denied file access: " + path.string()));
}

void SandboxContext::allowPath(const fs::path& path, FilePermission /*permission*/) {
    std::lock_guard lock(mutex_);
    runtimeAllowedPaths_.insert(normalizePath(path));
    runtimeDeniedPaths_.erase(normalizePath(path));
}

void SandboxContext::denyPath(const fs::path& path) {
    std::lock_guard lock(mutex_);
    runtimeDeniedPaths_.insert(normalizePath(path));
    runtimeAllowedPaths_.erase(normalizePath(path));
}

// --- Network Access ---

bool SandboxContext::checkNetworkAccess(const std::string& host, uint16_t port) {
    networkAccessChecks_.fetch_add(1, std::memory_order_relaxed);

    if (policy_.networkAccess == NetworkAccess::None) {
        networkAccessDenied_.fetch_add(1, std::memory_order_relaxed);
        recordViolation("Network denied (no network policy): " + host);
        return false;
    }

    if (policy_.networkAccess == NetworkAccess::Full) {
        return true;
    }

    if (policy_.networkAccess == NetworkAccess::LocalOnly) {
        auto lowerHost = host;
        std::transform(lowerHost.begin(), lowerHost.end(), lowerHost.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (lowerHost == "localhost" || lowerHost == "127.0.0.1" || lowerHost == "::1") {
            return true;
        }
        networkAccessDenied_.fetch_add(1, std::memory_order_relaxed);
        recordViolation("Network denied (local only): " + host);
        return false;
    }

    // Check runtime overrides
    {
        std::lock_guard lock(mutex_);
        if (runtimeDeniedHosts_.count(host)) {
            networkAccessDenied_.fetch_add(1, std::memory_order_relaxed);
            recordViolation("Network denied (runtime): " + host);
            return false;
        }
        if (runtimeAllowedHosts_.count(host)) {
            return true;
        }
    }

    // AllowList mode: must match a rule
    if (policy_.networkAccess == NetworkAccess::AllowList) {
        for (const auto& rule : policy_.networkRules) {
            if (rule.allow && rule.matches(host, port)) {
                return true;
            }
        }
        networkAccessDenied_.fetch_add(1, std::memory_order_relaxed);
        recordViolation("Network denied (not in allowlist): " + host +
                       ":" + std::to_string(port));
        return false;
    }

    // DenyList mode: blocked if matches a deny rule
    if (policy_.networkAccess == NetworkAccess::DenyList) {
        for (const auto& rule : policy_.networkRules) {
            if (!rule.allow && rule.matches(host, port)) {
                networkAccessDenied_.fetch_add(1, std::memory_order_relaxed);
                recordViolation("Network denied (in denylist): " + host);
                return false;
            }
        }
        return true;
    }

    return true;
}

Result<void> SandboxContext::requireNetworkAccess(
    const std::string& host, uint16_t port
) {
    if (checkNetworkAccess(host, port)) {
        return {};
    }

    if (!policy_.enforced) {
        return {};
    }

    return std::unexpected(Error(ErrorCode::SecurityViolation,
        "Sandbox '" + policy_.name + "' denied network access: " +
        host + ":" + std::to_string(port)));
}

void SandboxContext::allowHost(const std::string& host, uint16_t /*port*/) {
    std::lock_guard lock(mutex_);
    runtimeAllowedHosts_.insert(host);
    runtimeDeniedHosts_.erase(host);
}

void SandboxContext::denyHost(const std::string& host) {
    std::lock_guard lock(mutex_);
    runtimeDeniedHosts_.insert(host);
    runtimeAllowedHosts_.erase(host);
}

// --- Resource Tracking ---

bool SandboxContext::recordMemoryAlloc(size_t bytes) {
    auto current = currentMemory_.fetch_add(bytes, std::memory_order_relaxed) + bytes;

    // Update peak
    auto peak = peakMemory_.load(std::memory_order_relaxed);
    while (current > peak &&
           !peakMemory_.compare_exchange_weak(peak, current, std::memory_order_relaxed)) {}

    if (policy_.limits.maxMemoryBytes > 0 && current > policy_.limits.maxMemoryBytes) {
        recordViolation("Memory limit exceeded: " + std::to_string(current) +
                       " > " + std::to_string(policy_.limits.maxMemoryBytes));
        return false;
    }
    return true;
}

void SandboxContext::recordMemoryFree(size_t bytes) {
    auto current = currentMemory_.load(std::memory_order_relaxed);
    if (bytes > current) {
        currentMemory_.store(0, std::memory_order_relaxed);
    } else {
        currentMemory_.fetch_sub(bytes, std::memory_order_relaxed);
    }
}

bool SandboxContext::recordFileWrite(size_t bytes) {
    auto total = totalDiskWrites_.fetch_add(bytes, std::memory_order_relaxed) + bytes;
    if (policy_.limits.maxDiskWriteBytes > 0 && total > policy_.limits.maxDiskWriteBytes) {
        recordViolation("Disk write limit exceeded: " + std::to_string(total));
        return false;
    }
    return true;
}

bool SandboxContext::isCpuTimeExceeded() const {
    if (policy_.limits.maxCpuTime.count() == 0) return false;
    auto elapsed = std::chrono::steady_clock::now() - startTime_;
    return elapsed > policy_.limits.maxCpuTime;
}

bool SandboxContext::isWallTimeExceeded() const {
    if (policy_.limits.maxWallTime.count() == 0) return false;
    auto elapsed = std::chrono::steady_clock::now() - startTime_;
    return elapsed > policy_.limits.maxWallTime;
}

// --- Statistics ---

SandboxContext::Stats SandboxContext::getStats() const {
    Stats stats;
    stats.fileAccessChecks = fileAccessChecks_.load(std::memory_order_relaxed);
    stats.fileAccessDenied = fileAccessDenied_.load(std::memory_order_relaxed);
    stats.networkAccessChecks = networkAccessChecks_.load(std::memory_order_relaxed);
    stats.networkAccessDenied = networkAccessDenied_.load(std::memory_order_relaxed);
    stats.currentMemoryBytes = currentMemory_.load(std::memory_order_relaxed);
    stats.peakMemoryBytes = peakMemory_.load(std::memory_order_relaxed);
    stats.totalDiskWriteBytes = totalDiskWrites_.load(std::memory_order_relaxed);

    std::lock_guard lock(mutex_);
    stats.violations = violations_;
    return stats;
}

void SandboxContext::resetStats() {
    fileAccessChecks_.store(0, std::memory_order_relaxed);
    fileAccessDenied_.store(0, std::memory_order_relaxed);
    networkAccessChecks_.store(0, std::memory_order_relaxed);
    networkAccessDenied_.store(0, std::memory_order_relaxed);
    currentMemory_.store(0, std::memory_order_relaxed);
    peakMemory_.store(0, std::memory_order_relaxed);
    totalDiskWrites_.store(0, std::memory_order_relaxed);

    std::lock_guard lock(mutex_);
    violations_.clear();
}

void SandboxContext::recordViolation(const std::string& message) {
    MAKINEAI_LOG_WARN(log::SECURITY, "Sandbox '{}' violation: {}",
        policy_.name, message);

    if (policy_.onViolation) {
        policy_.onViolation(message);
    }

    std::lock_guard lock(mutex_);
    if (violations_.size() < 1000) {  // Cap violation log
        violations_.push_back(message);
    }
}

// ============================================================================
// SCOPED SANDBOX
// ============================================================================

ScopedSandbox::ScopedSandbox(SandboxPolicy policy)
    : context_(std::make_shared<SandboxContext>(std::move(policy)))
    , previous_(t_currentContext)
{
    t_currentContext = context_.get();
}

ScopedSandbox::ScopedSandbox(std::shared_ptr<SandboxContext> context)
    : context_(std::move(context))
    , previous_(t_currentContext)
{
    t_currentContext = context_.get();
}

ScopedSandbox::~ScopedSandbox() {
    t_currentContext = previous_;
}

SandboxContext* ScopedSandbox::current() {
    return t_currentContext;
}

bool ScopedSandbox::isActive() {
    return t_currentContext != nullptr;
}

} // namespace makineai
