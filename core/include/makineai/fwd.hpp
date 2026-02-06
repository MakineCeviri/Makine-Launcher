/**
 * @file fwd.hpp
 * @brief Forward declarations for MakineAI types
 * @copyright (c) 2026 MakineAI Team
 *
 * This header provides forward declarations for all MakineAI types.
 * Include this instead of full headers when you only need type references
 * (pointers, references, or use in function signatures).
 *
 * This significantly reduces compile times by avoiding unnecessary
 * header inclusions.
 *
 * @code
 * // Instead of:
 * #include <makineai/types.hpp>      // Heavy include
 * #include <makineai/database.hpp>   // Heavy include
 *
 * // Use:
 * #include <makineai/fwd.hpp>        // Lightweight forward declarations
 *
 * // Then in .cpp:
 * #include <makineai/types.hpp>      // Full definitions only where needed
 * @endcode
 */

#pragma once

#include <cstddef>  // size_t
#include <cstdint>  // int64_t, uint8_t, etc.
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace makineai {

// ============================================================================
// NAMESPACE ALIASES
// ============================================================================

namespace fs = std::filesystem;

// ============================================================================
// BASIC TYPE ALIASES (safe to define in forward declaration header)
// ============================================================================

/// @brief Byte buffer type
using ByteBuffer = std::vector<uint8_t>;

/// @brief Byte span for non-owning views
using ByteSpan = std::span<const uint8_t>;

/// @brief String list
using StringList = std::vector<std::string>;

/// @brief Progress callback (0.0 to 1.0)
using ProgressCallback = std::function<void(float progress, const std::string& message)>;

/// @brief Cancellation token
using CancellationToken = std::function<bool()>;

// ============================================================================
// FORWARD DECLARATIONS - ENUMS (with underlying type for forward declaration)
// ============================================================================

/// @brief Game engine types
enum class GameEngine : int;

/// @brief Game store types
enum class GameStore : int;

/// @brief Patch status
enum class PatchStatus : int;

/// @brief Backup status
enum class BackupStatus : int;

/// @brief Translation entry status
enum class EntryStatus : int;

/// @brief Translation entry category
enum class EntryCategory : int;

/// @brief Placeholder type
enum class PlaceholderType : int;

/// @brief QA severity levels
enum class QASeverity : int;

/// @brief Translation match type
enum class MatchType : int;

/// @brief Term type for glossary
enum class TermType : int;

/// @brief Term domain for glossary
enum class TermDomain : int;

/// @brief Project status
enum class ProjectStatus : int;

/// @brief Translation method
enum class TranslationMethod : int;

/// @brief Method capability flags
enum class MethodCapability : int;

/// @brief Pipeline execution phase
enum class PipelinePhase : int;

/// @brief Decision confidence level
enum class DecisionConfidence : int;

/// @brief Error codes
enum class ErrorCode : int;

/// @brief Log level
enum class LogLevel : int;

/// @brief Audit severity
enum class AuditSeverity : int;

/// @brief Audit category
enum class AuditCategory : int;

/// @brief Async operation status
enum class AsyncStatus : int;

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

// --- Translation Types ---
struct TranslationEntry;
struct GlossaryTerm;
struct TranslationProject;
struct QAIssue;

// --- Translation Memory Types ---
struct TranslationMemoryEntry;
struct TMMatch;
struct TMStats;

// --- Pipeline Types ---
struct MethodDecision;
struct GameAnalysis;
struct PipelineContext;
struct PipelineOptions;
struct PipelineResult;

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

// --- String & Memory ---
class StringPool;
template<typename T> class ObjectPool;
class BufferPool;
class GlobalPools;
struct MemoryReport;
class MemoryTracker;
class MemoryGuard;

// --- Lazy Loading ---
template<typename T> class Lazy;
class LazyFile;
class LazyJson;
template<typename T> class LazyResource;

// --- Batch Processing ---
struct BatchProgress;
template<typename TInput, typename TOutput> struct BatchItemResult;
template<typename TInput, typename TOutput> struct BatchResult;
struct BatchOptions;
template<typename TInput, typename TOutput> class BatchProcessor;

// --- Validation ---
struct ValidationError;
class ValidationBuilder;

// ============================================================================
// FORWARD DECLARATIONS - MAJOR CLASSES
// ============================================================================

// --- Core Modules ---
class Core;
class Database;

// --- Game Detection ---
class GameDetector;
class IGameScanner;
class SteamScanner;
class EpicScanner;
class GOGScanner;

// --- Scanner v2 (new interface) ---
struct ScanContext;
struct ScanResult;
class IScannerV2;
class ScannerBase;
class ScannerRegistry;

// --- Asset Parsing ---
class AssetParser;
class IAssetFormatParser;
struct StringEntry;
struct ParseResult;

// --- Patching ---
class PatchEngine;
class IFileBackupStorage;
class FileBackupStorage;

// --- Package Management ---
class PackageManager;

// --- Runtime Management ---
class RuntimeManager;

// --- Security ---
class SecurityManager;
template<typename T> class SecureBuffer;

// --- Version Tracking ---
class VersionTracker;

// --- Translation Services ---
class TranslationMemory;
class GlossaryService;
class QAService;
class TranslationAPI;
class StringClassifier;

// --- Translation Pipeline ---
class TranslationPipeline;
class MethodRegistry;
class DecisionEngine;
class GameAnalyzer;

// --- Recipe System ---
class RecipeLoader;
struct Recipe;
struct RecipeStep;

// --- Engine Handlers ---
namespace handlers {
class IEngineHandler;
class EngineHandlerBase;
class UnityHandler;
class UnrealHandler;
class RenPyHandler;
class RPGMakerHandler;
class GameMakerHandler;
}  // namespace handlers

// --- Parser Formats ---
namespace formats {
class UnityBundleParser;
class UnrealPakParser;
class BethesdaBA2Parser;
class GameMakerDataParser;
}  // namespace formats

// ============================================================================
// FORWARD DECLARATIONS - NAMESPACES
// ============================================================================

/// @brief Scanner implementations
namespace scanners {
using ::makineai::GameDetector;
using ::makineai::IGameScanner;
using ::makineai::SteamScanner;
using ::makineai::EpicScanner;
using ::makineai::GOGScanner;
}  // namespace scanners

/// @brief Parser implementations
namespace parsers {
using ::makineai::AssetParser;
using ::makineai::IAssetFormatParser;
using ::makineai::StringEntry;
using ::makineai::ParseResult;
}  // namespace parsers

// ============================================================================
// UTILITY MACROS
// ============================================================================

/// @brief Declare a smart pointer type pair
#define MAKINEAI_DECLARE_PTR(Class) \
    class Class; \
    using Class##Ptr = std::unique_ptr<Class>; \
    using Class##SharedPtr = std::shared_ptr<Class>

// Common smart pointer declarations
MAKINEAI_DECLARE_PTR(Database);
MAKINEAI_DECLARE_PTR(GameDetector);
MAKINEAI_DECLARE_PTR(AssetParser);
MAKINEAI_DECLARE_PTR(PatchEngine);
MAKINEAI_DECLARE_PTR(PackageManager);
MAKINEAI_DECLARE_PTR(RuntimeManager);
MAKINEAI_DECLARE_PTR(SecurityManager);
MAKINEAI_DECLARE_PTR(VersionTracker);
MAKINEAI_DECLARE_PTR(TranslationMemory);
MAKINEAI_DECLARE_PTR(TranslationPipeline);

#undef MAKINEAI_DECLARE_PTR

}  // namespace makineai
