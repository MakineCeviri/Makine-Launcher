/**
 * @file asset_parser.hpp
 * @brief Game asset file parsing and writing
 * @copyright (c) 2026 MakineAI Team
 */

#pragma once

#include "types.hpp"
#include "error.hpp"

#include <memory>
#include <unordered_map>

namespace makineai {

/**
 * @brief Extracted string entry from game assets
 */
struct StringEntry {
    std::string key;          // Unique identifier (path/id)
    std::string original;     // Original text
    std::string translated;   // Translated text (empty if not translated)
    std::string context;      // Additional context for translators
    uint64_t offset;          // File offset (for binary patching)
    uint32_t maxLength;       // Maximum allowed length (0 = unlimited)
};

/**
 * @brief Result of parsing game assets
 */
struct ParseResult {
    bool success;
    std::string message;
    std::vector<StringEntry> strings;
    std::unordered_map<std::string, std::string> metadata;
    GameEngine detectedEngine;
    std::string formatVersion;
};

/**
 * @brief Interface for asset format parsers
 */
class IAssetFormatParser {
public:
    virtual ~IAssetFormatParser() = default;

    /**
     * @brief Get parser name
     */
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    /**
     * @brief Get supported file extensions
     */
    [[nodiscard]] virtual StringList supportedExtensions() const = 0;

    /**
     * @brief Check if this parser can handle the given file
     */
    [[nodiscard]] virtual bool canParse(const fs::path& file) const = 0;

    /**
     * @brief Parse file and extract translatable strings
     */
    [[nodiscard]] virtual Result<ParseResult> parse(const fs::path& file) const = 0;

    /**
     * @brief Write translated strings back to file
     */
    [[nodiscard]] virtual VoidResult write(
        const fs::path& file,
        const std::vector<StringEntry>& strings
    ) const = 0;
};

/**
 * @brief Main asset parser that delegates to format-specific parsers
 */
class AssetParser {
public:
    AssetParser();
    ~AssetParser();

    /**
     * @brief Register a format parser
     */
    void registerParser(std::unique_ptr<IAssetFormatParser> parser);

    /**
     * @brief Get parser for a specific file
     */
    [[nodiscard]] IAssetFormatParser* getParserForFile(const fs::path& file) const;

    /**
     * @brief Parse a single file
     */
    [[nodiscard]] Result<ParseResult> parseFile(const fs::path& file) const;

    /**
     * @brief Parse all supported files in a directory
     */
    [[nodiscard]] Result<std::vector<ParseResult>> parseDirectory(
        const fs::path& directory,
        bool recursive = true,
        ProgressCallback progress = nullptr
    ) const;

    /**
     * @brief Write translations to a file
     */
    [[nodiscard]] VoidResult writeFile(
        const fs::path& file,
        const std::vector<StringEntry>& strings
    ) const;

    /**
     * @brief Detect game engine from directory contents
     */
    [[nodiscard]] GameEngine detectEngine(const fs::path& gameDir) const;

    /**
     * @brief Get all registered parsers
     */
    [[nodiscard]] const std::vector<std::unique_ptr<IAssetFormatParser>>& parsers() const {
        return parsers_;
    }

private:
    std::vector<std::unique_ptr<IAssetFormatParser>> parsers_;

    void registerBuiltinParsers();
};

// Forward declarations of built-in parsers
class UnityBundleParser;
class UnrealPakParser;
class BethesdaBa2Parser;
class GameMakerDataParser;

} // namespace makineai
