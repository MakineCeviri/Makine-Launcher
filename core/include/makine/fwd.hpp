/**
 * @file fwd.hpp
 * @brief Forward declarations for Makine types
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Lightweight forward declarations for compile-time optimization.
 * Include this instead of full headers when you only need type references.
 */

#pragma once

#include <cstddef>      // size_t
#include <cstdint>      // int64_t, uint8_t, etc.
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace makine {

// ============================================================================
// NAMESPACE ALIASES
// ============================================================================

namespace fs = std::filesystem;

// ============================================================================
// BASIC TYPE ALIASES
// ============================================================================

using ByteBuffer = std::vector<uint8_t>;
using ByteSpan = std::span<const uint8_t>;
using StringList = std::vector<std::string>;
using ProgressCallback = std::function<void(float progress, const std::string& message)>;
using CancellationToken = std::function<bool()>;

// ============================================================================
// FORWARD DECLARATIONS - ENUMS
// ============================================================================

enum class GameEngine : int;
enum class GameStore : int;
enum class PatchStatus : int;
enum class BackupStatus : int;
enum class EntryStatus : int;
enum class ErrorCode : int;
enum class LogLevel : int;
enum class AuditSeverity : int;
enum class AuditCategory : int;

// ============================================================================
// FORWARD DECLARATIONS - STRUCTS & CLASSES
// ============================================================================

// --- Core Types ---
struct Version;
struct GameId;
struct GameInfo;
struct TranslationPackage;

// --- Patch Types ---
struct PatchResult;
struct BackupResult;
struct RestoreResult;
struct BackupRecord;

// --- Error Handling ---
class Error;
template<typename T> class Result;
using VoidResult = Result<void>;

// --- Configuration ---
struct ScanningConfig;
struct PatchingConfig;
struct TranslationConfig;
struct SecurityConfig;
struct NetworkConfig;
struct LoggingConfig;
struct DatabaseConfig;
struct CoreConfig;
class ConfigManager;

// --- Async Types ---
struct AsyncProgress;
template<typename T> class AsyncOperation;
class AsyncQueue;

// --- Cache Types ---
template<typename Key, typename Value> class LRUCache;
template<typename Key, typename Value> class TTLCache;
class GameInfoCache;
class TranslationCache;
class CacheManager;

// --- Metrics & Health ---
struct MetricValue;
class Metrics;
struct ComponentHealth;
struct HealthStatus;
class HealthChecker;

// --- Debug & Audit ---
struct DebugConfig;
struct CrashReport;
class DebugDumper;
struct AuditEvent;
struct AuditConfig;
class AuditLogger;

// --- Validation ---
struct ValidationError;
class ValidationBuilder;

// ============================================================================
// FORWARD DECLARATIONS - SHARED CLASSES
// ============================================================================

// --- Core Modules ---
class Database;

// --- Security ---
template<typename T> class SecureBuffer;

// --- Parser Formats (shared between Makine and Makine) ---
namespace formats {
class UnityBundleParser;
class UnrealPakParser;
class BethesdaBA2Parser;
class GameMakerDataParser;
}  // namespace formats

}  // namespace makine
