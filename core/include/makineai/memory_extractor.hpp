#pragma once

/// @file memory_extractor.hpp
/// @brief Process memory translation extractor — engine-agnostic core
///
/// Reads translation strings from running game processes by scanning
/// committed memory regions for Turkish character fingerprints.
/// Works for ANY game regardless of engine, encryption, or protection.
///
/// Architecture:
///   ProcessAttacher → MemoryScanner → StringExtractor → TranslationDB
///
/// The core scanning is language-fingerprint based (Turkish chars: İıŞşÇçĞğÖöÜü).
/// Engine-specific modules can optionally extract structured data (hash tables, etc.)
/// but the base extraction works universally.

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace makineai {

// ────────────────────────────────────────────────────────────────
// Data types
// ────────────────────────────────────────────────────────────────

/// Information about an attached process
struct ProcessInfo {
    uint32_t pid{0};
    std::string name;
    std::string exe_path;
    void* handle{nullptr};      // HANDLE (opaque on non-Windows)
};

/// A readable memory region
struct MemoryRegion {
    uint64_t base{0};
    size_t size{0};
    uint32_t protection{0};
};

/// A single extracted translation entry
struct TranslationEntry {
    uint32_t hash{0};           // Engine-specific text hash (0 if unknown)
    uint32_t meta{0};           // Engine-specific metadata
    std::string text;           // Decoded, clean text
    std::string raw_text;       // Original text (before encoding fix)
    std::string category;       // "dialogue", "ui", "item", "narrative", "general"
    std::string encoding;       // "utf-8", "utf-16-le"
    uint64_t address{0};        // Memory address where found
    uint32_t length{0};         // Byte length in memory
};

/// Encoding fix mapping (obfuscated char → real char)
using EncodingFixMap = std::unordered_map<char32_t, char32_t>;

/// Extraction configuration
struct ExtractionConfig {
    size_t min_string_length{4};        // Minimum string length to consider
    size_t max_string_length{4096};     // Maximum string length
    float min_printable_ratio{0.8f};    // Minimum printable character ratio
    float min_letter_ratio{0.1f};       // Minimum alphabetic character ratio
    bool scan_utf16{true};              // Also scan for UTF-16LE strings
    bool auto_detect_encoding{true};    // Auto-detect encoding obfuscation
    size_t max_region_size{500 * 1024 * 1024}; // Skip regions larger than this
};

/// Extraction statistics
struct ExtractionStats {
    size_t total_regions{0};
    size_t total_bytes{0};
    double scan_duration_s{0};
    size_t raw_entries{0};
    size_t unique_entries{0};
    size_t encoding_fixes{0};
    size_t dialogue_count{0};
    size_t ui_count{0};
    size_t general_count{0};
};

/// Complete extraction result
struct ExtractionResult {
    std::string game_name;
    std::string engine;
    ExtractionStats stats;
    EncodingFixMap encoding_fixes;
    std::vector<TranslationEntry> entries;

    /// Get hash→text mapping (for entries with known hashes)
    std::unordered_map<uint32_t, std::string> hashMap() const;
};

// ────────────────────────────────────────────────────────────────
// Engine module interface
// ────────────────────────────────────────────────────────────────

/// Base interface for engine-specific extraction modules
class IEngineModule {
public:
    virtual ~IEngineModule() = default;

    /// Module identifier (e.g., "rage", "ue4", "unity")
    virtual std::string_view name() const = 0;

    /// Check if this engine's signatures are present in a memory sample
    virtual bool detect(std::span<const uint8_t> data) const = 0;

    /// Extract structured entries from a memory region
    /// Engine modules extract hash→text pairs with format awareness.
    /// Returns entries found in this region.
    virtual std::vector<TranslationEntry> extractFromRegion(
        std::span<const uint8_t> data,
        uint64_t base_addr) const = 0;
};

// ────────────────────────────────────────────────────────────────
// Turkish language fingerprinting
// ────────────────────────────────────────────────────────────────

/// Turkish-specific character detection utilities
namespace turkish {

/// UTF-8 byte sequences for Turkish special characters
/// These are unique fingerprints that reliably identify Turkish text
/// in binary data with minimal false positives.
struct Fingerprints {
    static constexpr uint8_t UPPER_I_DOTTED[] = {0xC4, 0xB0};  // İ
    static constexpr uint8_t LOWER_I_DOTLESS[] = {0xC4, 0xB1};  // ı
    static constexpr uint8_t UPPER_S_CEDILLA[] = {0xC5, 0x9E};  // Ş
    static constexpr uint8_t LOWER_S_CEDILLA[] = {0xC5, 0x9F};  // ş
    static constexpr uint8_t UPPER_C_CEDILLA[] = {0xC3, 0x87};  // Ç
    static constexpr uint8_t LOWER_C_CEDILLA[] = {0xC3, 0xA7};  // ç
    static constexpr uint8_t UPPER_G_BREVE[] = {0xC4, 0x9E};    // Ğ
    static constexpr uint8_t LOWER_G_BREVE[] = {0xC4, 0x9F};    // ğ
    static constexpr uint8_t UPPER_O_DIAERESIS[] = {0xC3, 0x96}; // Ö
    static constexpr uint8_t LOWER_O_DIAERESIS[] = {0xC3, 0xB6}; // ö
    static constexpr uint8_t UPPER_U_DIAERESIS[] = {0xC3, 0x9C}; // Ü
    static constexpr uint8_t LOWER_U_DIAERESIS[] = {0xC3, 0xBC}; // ü
};

/// Check if a UTF-8 string contains Turkish special characters
bool containsTurkishChars(std::string_view text);

/// Check if a byte sequence contains Turkish UTF-8 fingerprints
bool containsTurkishBytes(std::span<const uint8_t> data);

/// Score how "Turkish" a text is (0.0 = not Turkish, 1.0 = very Turkish)
float turkishScore(std::string_view text);

} // namespace turkish

// ────────────────────────────────────────────────────────────────
// Encoding obfuscation detection
// ────────────────────────────────────────────────────────────────

/// Detect character substitution patterns in extracted strings
/// Returns a mapping of obfuscated → real characters
EncodingFixMap detectEncodingObfuscation(
    const std::vector<std::string>& sample_texts);

/// Apply encoding fixes to a string
std::string applyEncodingFix(std::string_view text, const EncodingFixMap& fixes);

// ────────────────────────────────────────────────────────────────
// Main extractor
// ────────────────────────────────────────────────────────────────

/// Memory Translation Extractor — core extraction engine
///
/// Usage:
///   MemoryExtractor ext;
///   ext.registerModule(std::make_unique<RAGEModule>());
///   auto result = ext.extract("RDR2.exe");
///
class MemoryExtractor {
public:
    MemoryExtractor();
    ~MemoryExtractor();

    // Non-copyable, movable
    MemoryExtractor(const MemoryExtractor&) = delete;
    MemoryExtractor& operator=(const MemoryExtractor&) = delete;
    MemoryExtractor(MemoryExtractor&&) noexcept;
    MemoryExtractor& operator=(MemoryExtractor&&) noexcept;

    /// Register an engine-specific extraction module
    void registerModule(std::unique_ptr<IEngineModule> module);

    /// Set extraction configuration
    void setConfig(const ExtractionConfig& config);

    /// Set progress callback (called during scan)
    /// Signature: void(size_t current_region, size_t total_regions, size_t entries_found)
    using ProgressCallback = std::function<void(size_t, size_t, size_t)>;
    void setProgressCallback(ProgressCallback cb);

    /// Find a game process by executable name
    static std::optional<ProcessInfo> findProcess(std::string_view exe_name);

    /// Find a game process by PID
    static std::optional<ProcessInfo> openProcess(uint32_t pid);

    /// Extract translations from a running process
    /// @param process_name  Executable name (e.g., "RDR2.exe")
    /// @param engine_hint   Engine name hint ("auto" for auto-detect)
    /// @return Extraction result with all found translations
    ExtractionResult extract(std::string_view process_name,
                            std::string_view engine_hint = "auto");

    /// Extract translations from an already-opened process
    ExtractionResult extract(const ProcessInfo& process,
                            std::string_view engine_hint = "auto");

    /// Save extraction result to JSON file
    static bool saveToJson(const ExtractionResult& result,
                          const std::string& path);

    /// Load extraction result from JSON file
    static std::optional<ExtractionResult> loadFromJson(const std::string& path);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// ────────────────────────────────────────────────────────────────
// Built-in engine modules
// ────────────────────────────────────────────────────────────────

/// RAGE Engine module (GTA IV/V, RDR2)
/// Detects: GXT2/2TXG magic, ~z~ dialogue markers
/// Extracts: Jenkins hash → text mappings
class RAGEEngineModule : public IEngineModule {
public:
    std::string_view name() const override { return "rage"; }
    bool detect(std::span<const uint8_t> data) const override;
    std::vector<TranslationEntry> extractFromRegion(
        std::span<const uint8_t> data,
        uint64_t base_addr) const override;
};

/// Unreal Engine 4/5 module
/// Detects: FText serialization patterns, LocRes headers
/// Extracts: FNV hash → text mappings
class UnrealEngineModule : public IEngineModule {
public:
    std::string_view name() const override { return "ue4"; }
    bool detect(std::span<const uint8_t> data) const override;
    std::vector<TranslationEntry> extractFromRegion(
        std::span<const uint8_t> data,
        uint64_t base_addr) const override;
};

/// Unity engine module
/// Detects: I2 Localization, TextMeshPro patterns
/// Extracts: Key → text mappings
class UnityEngineModule : public IEngineModule {
public:
    std::string_view name() const override { return "unity"; }
    bool detect(std::span<const uint8_t> data) const override;
    std::vector<TranslationEntry> extractFromRegion(
        std::span<const uint8_t> data,
        uint64_t base_addr) const override;
};

/// Generic module — pure Turkish fingerprint scanning
/// Works for ANY engine but doesn't extract hash mappings.
/// Used as fallback when no engine is detected.
class GenericEngineModule : public IEngineModule {
public:
    std::string_view name() const override { return "generic"; }
    bool detect(std::span<const uint8_t> data) const override;
    std::vector<TranslationEntry> extractFromRegion(
        std::span<const uint8_t> data,
        uint64_t base_addr) const override;
};

} // namespace makineai
