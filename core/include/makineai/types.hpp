/**
 * @file types.hpp
 * @brief MakineAI core type definitions
 * @copyright (c) 2026 MakineAI Team
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <atomic>
#include <variant>
#include <vector>

namespace makineai {

// Namespace aliases
namespace fs = std::filesystem;

// Basic type aliases
using ByteBuffer = std::vector<uint8_t>;
using ByteSpan = std::span<const uint8_t>;
using StringList = std::vector<std::string>;

/**
 * @brief Game engine types supported by MakineAI
 */
enum class GameEngine {
    Unknown,
    Unity_Mono,      // Unity with Mono scripting backend
    Unity_IL2CPP,    // Unity with IL2CPP scripting backend
    Unreal,          // Unreal Engine 4/5
    Bethesda,        // Creation Engine (Starfield, TES, Fallout)
    GameMaker,       // GameMaker Studio 1/2
    RenPy,           // Ren'Py visual novel engine
    RPGMaker_MV,     // RPG Maker MV/MZ (JavaScript)
    RPGMaker_VX,     // RPG Maker VX Ace (Ruby)
    Godot,           // Godot Engine
    Source,          // Valve Source Engine
    CryEngine,       // CryEngine
    Frostbite,       // Frostbite Engine (EA)
    IdTech,          // id Tech Engine
    Custom           // Custom/unknown engine with recipe
};

/**
 * @brief String representation of game engines
 */
constexpr std::string_view engineToString(GameEngine engine) noexcept {
    switch (engine) {
        case GameEngine::Unity_Mono:    return "Unity (Mono)";
        case GameEngine::Unity_IL2CPP:  return "Unity (IL2CPP)";
        case GameEngine::Unreal:        return "Unreal Engine";
        case GameEngine::Bethesda:      return "Bethesda Creation Engine";
        case GameEngine::GameMaker:     return "GameMaker";
        case GameEngine::RenPy:         return "Ren'Py";
        case GameEngine::RPGMaker_MV:   return "RPG Maker MV/MZ";
        case GameEngine::RPGMaker_VX:   return "RPG Maker VX Ace";
        case GameEngine::Godot:         return "Godot";
        case GameEngine::Source:        return "Source Engine";
        case GameEngine::CryEngine:     return "CryEngine";
        case GameEngine::Frostbite:     return "Frostbite";
        case GameEngine::IdTech:        return "id Tech";
        case GameEngine::Custom:        return "Custom";
        default:                        return "Unknown";
    }
}

/**
 * @brief Translation patch status
 */
enum class PatchStatus {
    NotInstalled,     // No translation installed
    Installed,        // Translation installed and active
    Outdated,         // Game updated, translation may not work
    Incompatible,     // Translation incompatible with game version
    Corrupted         // Translation files corrupted
};

/**
 * @brief Game store/launcher type
 */
enum class GameStore {
    Unknown,
    Steam,
    EpicGames,
    GOG,
    Xbox,           // Microsoft Store / Xbox Game Pass
    Manual          // Manually added by user
};

/**
 * @brief Unique identifier for a game
 */
struct GameId {
    std::string storeId;      // e.g., Steam AppID "1716740"
    GameStore store;
    std::string exeHash;      // SHA256 of main executable

    bool operator==(const GameId&) const = default;
    auto operator<=>(const GameId&) const = default;
};

/**
 * @brief Information about a detected game
 */
struct GameInfo {
    GameId id;
    std::string name;             // Display name
    fs::path installPath;         // Game installation directory
    fs::path executablePath;      // Main executable path
    GameEngine engine;
    std::string version;          // Game version string
    uint64_t sizeBytes;           // Total size
    bool is64Bit;                 // x64 or x86

    // Optional metadata
    std::optional<std::string> iconPath;
    std::optional<std::string> publisher;
    std::optional<std::string> developer;
};

/**
 * @brief Translation package metadata
 */
struct TranslationPackage {
    std::string packageId;        // Unique package ID (e.g., "SF10310Hv19")
    std::string gameId;           // Target game ID
    std::string gameName;         // Game display name
    std::string version;          // Package version
    StringList supportedGameVersions;  // Compatible game versions
    StringList supportedGameHashes;    // Compatible exe hashes

    std::string downloadUrl;      // Package download URL
    std::string signature;        // RSA signature
    uint64_t sizeBytes;           // Download size
    std::string sha256;           // Package checksum

    GameEngine targetEngine;
    bool requiresRuntime;         // Needs BepInEx/XUnity?
};

/**
 * @brief Result of a patch operation
 */
struct PatchResult {
    bool success;
    std::string message;
    uint32_t filesPatched;
    uint32_t filesFailed;
    StringList errors;
    fs::path backupPath;          // Where backup was stored
};

/**
 * @brief Result of a backup operation
 */
struct BackupResult {
    bool success;
    std::string message;
    fs::path backupPath;
    uint64_t sizeBytes;
    uint32_t fileCount;
};

/**
 * @brief Result of a restore operation
 */
struct RestoreResult {
    bool success;
    std::string message;
    uint32_t filesRestored;
    uint32_t filesFailed;
};

/**
 * @brief Version information structure
 */
struct Version {
    uint32_t major = 0;
    uint32_t minor = 0;
    uint32_t patch = 0;
    uint32_t build = 0;

    std::string toString() const {
        return std::to_string(major) + "." +
               std::to_string(minor) + "." +
               std::to_string(patch) + "." +
               std::to_string(build);
    }

    static std::optional<Version> parse(std::string_view str);

    auto operator<=>(const Version&) const = default;
};

/**
 * @brief Progress callback for long-running operations
 */
using ProgressCallback = std::function<void(
    uint32_t current,     // Current step
    uint32_t total,       // Total steps
    std::string_view message  // Status message
)>;

/**
 * @brief Cancellation token for async operations
 */
class CancellationToken {
public:
    void cancel() noexcept { cancelled_ = true; }
    [[nodiscard]] bool isCancelled() const noexcept { return cancelled_; }
    void reset() noexcept { cancelled_ = false; }

private:
    std::atomic<bool> cancelled_{false};
};

// ============== TRANSLATION TYPES ==============

/**
 * @brief Translation entry status
 */
enum class EntryStatus {
    Untranslated,   // Not yet translated
    Translated,     // Has translation
    Fuzzy,          // Fuzzy match used (needs review)
    Verified,       // Human verified
    Rejected        // Rejected translation
};

/**
 * @brief Translation entry category
 */
enum class EntryCategory {
    UI,             // Interface (menu, button)
    Dialog,         // Dialog
    Narration,      // Narration
    Item,           // Item name/description
    Skill,          // Skill name/description
    Quest,          // Quest
    System,         // System message
    Tutorial,       // Tutorial
    Credits,        // Credits
    Lore,           // World lore, books
    Other           // Other
};

/**
 * @brief Placeholder type in text
 */
enum class PlaceholderType {
    Printf,         // %s, %d, %f
    Named,          // {name}, ${var}
    Indexed,        // {0}, {1}, $1
    Ruby,           // #{var}
    RenPy,          // [var]
    Unity,          // <color=#fff>
    BBCode,         // [b], [i]
    Html,           // <br>, &nbsp;
    Escape,         // \n, \t
    Unknown         // Unknown type
};

/**
 * @brief Placeholder info in text
 */
struct PlaceholderInfo {
    PlaceholderType type;
    std::string original;
    size_t startIndex;
    size_t endIndex;
    std::optional<std::string> variableName;
};

/**
 * @brief QA issue severity
 */
enum class QASeverity {
    Info = 1,       // Information only
    Warning = 2,    // Warning
    Major = 3,      // Major issue
    Critical = 4    // Critical issue
};

/**
 * @brief QA issue structure
 */
struct QAIssue {
    std::string code;
    std::string message;
    QASeverity severity;
    int penaltyPoints;
};

/**
 * @brief Translation entry (single translatable string)
 */
struct TranslationEntry {
    std::optional<int64_t> id;
    std::string projectId;
    std::string filePath;
    std::optional<std::string> entryKey;
    std::string sourceText;
    std::optional<std::string> targetText;
    std::optional<std::string> context;
    std::optional<EntryCategory> category;
    EntryStatus status = EntryStatus::Untranslated;
    int qaScore = 100;
    std::vector<QAIssue> qaIssues;
    std::vector<PlaceholderInfo> placeholders;
    std::optional<int> lineNumber;
    int64_t createdAt;
    int64_t updatedAt;
    std::optional<std::string> translatedBy;

    // IL2CPP metadata
    int64_t offset = 0;         // Binary offset
    int64_t length = 0;         // Original length

    [[nodiscard]] bool isTranslated() const noexcept {
        return targetText.has_value() && !targetText->empty() &&
               status != EntryStatus::Untranslated;
    }

    [[nodiscard]] bool passedQA() const noexcept {
        return qaScore >= 70;
    }
};

// ============== GLOSSARY TYPES ==============

/**
 * @brief Glossary term type
 */
enum class TermType {
    Noun,
    Verb,
    Adjective,
    UI,
    Item,
    Skill,
    Stat,
    Action,
    Currency,
    Place,
    Character,
    Other
};

/**
 * @brief Glossary domain (game genre)
 */
enum class TermDomain {
    General,
    RPG,
    FPS,
    VisualNovel,
    Strategy,
    Simulation,
    Adventure,
    Puzzle,
    Action,
    Horror
};

/**
 * @brief Forbidden translation entry
 */
struct ForbiddenTranslation {
    std::optional<int64_t> id;
    int64_t glossaryId;
    std::string forbiddenTranslation;
    std::optional<std::string> reason;
};

/**
 * @brief Glossary term
 */
struct GlossaryTerm {
    std::optional<int64_t> id;
    std::string termSource;
    std::string termTarget;
    std::optional<TermType> termType;
    std::optional<TermDomain> domain;
    bool caseSensitive = false;
    bool exactMatch = false;
    int priority = 50;
    std::optional<std::string> notes;
    std::vector<std::string> examples;
    bool doNotTranslate = false;
    int64_t createdAt;
    std::optional<std::string> gameSpecific;
    std::vector<std::string> alternatives;
    std::vector<ForbiddenTranslation> forbidden;

    [[nodiscard]] bool matches(std::string_view text) const;
};

// ============== TRANSLATION MEMORY TYPES ==============

/**
 * @brief Match type for translation memory
 */
enum class MatchType {
    Exact,          // 100% match
    NearExact,      // 95-99% match
    Fuzzy,          // 75-94% match
    Poor            // <75% match
};

/**
 * @brief Translation memory entry
 */
struct TranslationMemoryEntry {
    std::optional<int64_t> id;
    std::string sourceText;
    std::string targetText;
    std::string sourceHash;
    std::string sourceLang = "en";
    std::string targetLang = "tr";
    std::optional<std::string> context;
    std::optional<std::string> gameId;
    std::optional<std::string> engineType;
    std::optional<std::string> category;
    int qualityScore = 100;
    int usageCount = 1;
    int64_t createdAt;
    int64_t updatedAt;
    std::optional<std::string> createdBy;
    bool verified = false;
};

/**
 * @brief Translation memory match result
 */
struct TMMatch {
    TranslationMemoryEntry entry;
    double similarity;      // 0.0 - 100.0
    MatchType matchType;
};

// ============== PROJECT TYPES ==============

/**
 * @brief Translation project status
 */
enum class ProjectStatus {
    Active,
    Completed,
    Archived,
    Paused
};

/**
 * @brief Translation project
 */
struct TranslationProject {
    std::string id;
    std::optional<std::string> gameId;
    std::string name;
    std::string sourceLang = "en";
    std::string targetLang = "tr";
    int64_t createdAt;
    int64_t updatedAt;
    ProjectStatus status = ProjectStatus::Active;
    double progress = 0.0;
    std::optional<std::string> settings;  // JSON string
};

// ============== BACKUP TYPES ==============

/**
 * @brief Backup status
 */
enum class BackupStatus {
    Active,
    Deleted,
    Corrupted
};

/**
 * @brief Backup record
 */
struct BackupRecord {
    std::string id;
    std::string gameId;
    int64_t createdAt;
    std::string manifest;   // JSON string
    std::optional<uint64_t> sizeBytes;
    BackupStatus status = BackupStatus::Active;
};

} // namespace makineai
