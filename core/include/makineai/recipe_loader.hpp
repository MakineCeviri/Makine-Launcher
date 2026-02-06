// MakineAI - Recipe Loader
// Loads and validates game-specific recipes from YAML files

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "types.hpp"

namespace makineai {

// Recipe structure matching YAML schema
struct RecipeFile {
    std::string path_pattern;    // Glob pattern for files
    std::string encoding;        // File encoding (utf-8, utf-16-le, etc.)
    std::string format;          // text, json, binary, rpy
    bool translatable = true;    // Whether this file should be translated
    std::vector<std::string> exclude_patterns;  // Patterns to exclude
};

struct RecipeExtraction {
    std::string method;          // script_parser, json_path, regex, binary_offset
    std::vector<std::string> patterns;
    std::string text_field;      // For JSON: field containing text
    std::string context_field;   // For JSON: field for context
    std::optional<int> min_length;
    std::optional<int> max_length;
};

struct RecipeValidation {
    std::optional<int> max_line_length;
    std::vector<std::string> preserve_patterns;  // Patterns to keep as-is
    std::vector<std::string> forbidden_chars;
    bool allow_empty = false;
};

struct RecipeApplication {
    std::string method;          // replace_in_place, rebuild_file, binary_patch
    bool preserve_formatting = true;
    bool backup_original = true;
    std::string backup_suffix;
};

struct RecipeMetadata {
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    std::string engine;
    std::string engine_version;
    std::string support_level;   // gold, silver, bronze, experimental
    std::vector<std::string> tested_games;
    std::string created_date;
    std::string updated_date;
};

struct Recipe {
    RecipeMetadata metadata;
    std::vector<RecipeFile> files;
    RecipeExtraction extraction;
    RecipeValidation validation;
    RecipeApplication application;
    std::unordered_map<std::string, std::string> variables;  // Custom variables
};

// Recipe validation result
struct RecipeValidationResult {
    bool valid = false;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

class RecipeLoader {
public:
    RecipeLoader();
    ~RecipeLoader();

    // Load recipe from YAML file
    std::optional<Recipe> loadFromFile(const std::filesystem::path& path);

    // Load recipe from YAML string
    std::optional<Recipe> loadFromString(const std::string& yaml_content);

    // Validate recipe structure
    RecipeValidationResult validate(const Recipe& recipe);

    // Find matching recipe for a game
    std::optional<Recipe> findRecipeForGame(
        const std::filesystem::path& game_path,
        GameEngine engine
    );

    // Load all recipes from directory
    std::vector<Recipe> loadAllFromDirectory(const std::filesystem::path& dir);

    // Get recipe by engine type
    std::vector<Recipe> getRecipesByEngine(GameEngine engine);

    // Register recipe directory
    void addRecipeDirectory(const std::filesystem::path& dir);

    // Clear loaded recipes
    void clearCache();

    // Get last error
    const std::string& getLastError() const { return m_lastError; }

private:
    std::vector<std::filesystem::path> m_recipeDirs;
    std::unordered_map<std::string, Recipe> m_recipeCache;
    std::string m_lastError;

    // Parse YAML to Recipe struct
    bool parseYaml(const std::string& content, Recipe& recipe);

    // Validate individual sections
    bool validateMetadata(const RecipeMetadata& meta, std::vector<std::string>& errors);
    bool validateFiles(const std::vector<RecipeFile>& files, std::vector<std::string>& errors);
    bool validateExtraction(const RecipeExtraction& ext, std::vector<std::string>& errors);

    // Convert engine string to enum
    static GameEngine stringToEngine(const std::string& engine);
    static std::string engineToString(GameEngine engine);
};

} // namespace makineai
