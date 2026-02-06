// MakineAI - Recipe Loader Implementation
// Parses YAML recipes for game-specific translation rules

#include "makineai/recipe_loader.hpp"
#include <fstream>
#include <sstream>
#include <regex>
#include <spdlog/spdlog.h>

namespace makineai {

RecipeLoader::RecipeLoader() {
    // Add default recipe directory
    m_recipeDirs.push_back("recipes");
}

RecipeLoader::~RecipeLoader() = default;

std::optional<Recipe> RecipeLoader::loadFromFile(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        m_lastError = "Recipe file not found: " + path.string();
        spdlog::error("{}", m_lastError);
        return std::nullopt;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        m_lastError = "Failed to open recipe file: " + path.string();
        spdlog::error("{}", m_lastError);
        return std::nullopt;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return loadFromString(buffer.str());
}

std::optional<Recipe> RecipeLoader::loadFromString(const std::string& yaml_content) {
    Recipe recipe;
    if (!parseYaml(yaml_content, recipe)) {
        return std::nullopt;
    }

    auto validation = validate(recipe);
    if (!validation.valid) {
        m_lastError = "Recipe validation failed: " +
                      (validation.errors.empty() ? "unknown error" : validation.errors[0]);
        return std::nullopt;
    }

    return recipe;
}

// Simple YAML parser for our recipe format
// Uses line-by-line parsing since we have a well-defined schema
bool RecipeLoader::parseYaml(const std::string& content, Recipe& recipe) {
    std::istringstream stream(content);
    std::string line;
    std::string currentSection;
    std::string currentSubSection;
    int indentLevel = 0;

    auto getIndent = [](const std::string& s) -> int {
        int count = 0;
        for (char c : s) {
            if (c == ' ') count++;
            else break;
        }
        return count / 2;  // 2 spaces per indent level
    };

    auto getValue = [](const std::string& line) -> std::string {
        auto pos = line.find(':');
        if (pos == std::string::npos) return "";
        std::string value = line.substr(pos + 1);
        // Trim whitespace
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t\r\n") + 1);
        // Remove quotes if present
        if (value.size() >= 2 &&
            ((value.front() == '"' && value.back() == '"') ||
             (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.size() - 2);
        }
        return value;
    };

    auto getKey = [](const std::string& line) -> std::string {
        auto pos = line.find(':');
        if (pos == std::string::npos) return "";
        std::string key = line.substr(0, pos);
        // Trim whitespace and list marker
        key.erase(0, key.find_first_not_of(" \t-"));
        key.erase(key.find_last_not_of(" \t") + 1);
        return key;
    };

    RecipeFile currentFile;
    bool inFileList = false;

    while (std::getline(stream, line)) {
        // Skip comments and empty lines
        std::string trimmed = line;
        trimmed.erase(0, trimmed.find_first_not_of(" \t"));
        if (trimmed.empty() || trimmed[0] == '#') continue;

        indentLevel = getIndent(line);
        std::string key = getKey(trimmed);
        std::string value = getValue(trimmed);

        // Top-level sections
        if (indentLevel == 0) {
            if (key == "metadata") currentSection = "metadata";
            else if (key == "files") { currentSection = "files"; inFileList = false; }
            else if (key == "extraction") currentSection = "extraction";
            else if (key == "validation") currentSection = "validation";
            else if (key == "application") currentSection = "application";
            else if (key == "variables") currentSection = "variables";
            continue;
        }

        // Parse based on section
        if (currentSection == "metadata" && indentLevel == 1) {
            if (key == "name") recipe.metadata.name = value;
            else if (key == "version") recipe.metadata.version = value;
            else if (key == "author") recipe.metadata.author = value;
            else if (key == "description") recipe.metadata.description = value;
            else if (key == "engine") recipe.metadata.engine = value;
            else if (key == "engine_version") recipe.metadata.engine_version = value;
            else if (key == "support_level") recipe.metadata.support_level = value;
            else if (key == "created") recipe.metadata.created_date = value;
            else if (key == "updated") recipe.metadata.updated_date = value;
        }
        else if (currentSection == "metadata" && indentLevel == 2 && currentSubSection == "tested_games") {
            if (trimmed[0] == '-') {
                recipe.metadata.tested_games.push_back(value.empty() ? trimmed.substr(2) : value);
            }
        }
        else if (currentSection == "metadata" && key == "tested_games") {
            currentSubSection = "tested_games";
        }
        else if (currentSection == "files") {
            if (trimmed[0] == '-' && indentLevel == 1) {
                // New file entry
                if (inFileList && !currentFile.path_pattern.empty()) {
                    recipe.files.push_back(currentFile);
                }
                currentFile = RecipeFile{};
                inFileList = true;
                // Check if path is on same line
                if (key == "path") currentFile.path_pattern = value;
            }
            else if (inFileList && indentLevel == 2) {
                if (key == "path") currentFile.path_pattern = value;
                else if (key == "encoding") currentFile.encoding = value;
                else if (key == "format") currentFile.format = value;
                else if (key == "translatable") currentFile.translatable = (value == "true" || value == "yes");
            }
        }
        else if (currentSection == "extraction" && indentLevel == 1) {
            if (key == "method") recipe.extraction.method = value;
            else if (key == "text_field") recipe.extraction.text_field = value;
            else if (key == "context_field") recipe.extraction.context_field = value;
            else if (key == "min_length") recipe.extraction.min_length = std::stoi(value);
            else if (key == "max_length") recipe.extraction.max_length = std::stoi(value);
            else if (key == "patterns") currentSubSection = "patterns";
        }
        else if (currentSection == "extraction" && indentLevel == 2 && currentSubSection == "patterns") {
            if (trimmed[0] == '-') {
                std::string pattern = trimmed.substr(2);
                pattern.erase(0, pattern.find_first_not_of(" \t"));
                // Remove quotes
                if (pattern.size() >= 2 && pattern.front() == '"' && pattern.back() == '"') {
                    pattern = pattern.substr(1, pattern.size() - 2);
                }
                recipe.extraction.patterns.push_back(pattern);
            }
        }
        else if (currentSection == "validation" && indentLevel == 1) {
            if (key == "max_line_length") recipe.validation.max_line_length = std::stoi(value);
            else if (key == "allow_empty") recipe.validation.allow_empty = (value == "true" || value == "yes");
            else if (key == "preserve_patterns") currentSubSection = "preserve_patterns";
            else if (key == "forbidden_chars") currentSubSection = "forbidden_chars";
        }
        else if (currentSection == "validation" && indentLevel == 2) {
            if (currentSubSection == "preserve_patterns" && trimmed[0] == '-') {
                std::string pattern = trimmed.substr(2);
                pattern.erase(0, pattern.find_first_not_of(" \t\"'"));
                pattern.erase(pattern.find_last_not_of("\"'") + 1);
                recipe.validation.preserve_patterns.push_back(pattern);
            }
            else if (currentSubSection == "forbidden_chars" && trimmed[0] == '-') {
                std::string ch = trimmed.substr(2);
                ch.erase(0, ch.find_first_not_of(" \t\"'"));
                ch.erase(ch.find_last_not_of("\"'") + 1);
                recipe.validation.forbidden_chars.push_back(ch);
            }
        }
        else if (currentSection == "application" && indentLevel == 1) {
            if (key == "method") recipe.application.method = value;
            else if (key == "preserve_formatting") recipe.application.preserve_formatting = (value == "true" || value == "yes");
            else if (key == "backup_original") recipe.application.backup_original = (value == "true" || value == "yes");
            else if (key == "backup_suffix") recipe.application.backup_suffix = value;
        }
        else if (currentSection == "variables" && indentLevel == 1) {
            recipe.variables[key] = value;
        }
    }

    // Add last file if any
    if (inFileList && !currentFile.path_pattern.empty()) {
        recipe.files.push_back(currentFile);
    }

    return !recipe.metadata.name.empty();
}

RecipeValidationResult RecipeLoader::validate(const Recipe& recipe) {
    RecipeValidationResult result;
    result.valid = true;

    // Validate metadata
    if (!validateMetadata(recipe.metadata, result.errors)) {
        result.valid = false;
    }

    // Validate files
    if (!validateFiles(recipe.files, result.errors)) {
        result.valid = false;
    }

    // Validate extraction
    if (!validateExtraction(recipe.extraction, result.errors)) {
        result.valid = false;
    }

    // Warnings for optional fields
    if (recipe.metadata.author.empty()) {
        result.warnings.push_back("No author specified");
    }
    if (recipe.metadata.tested_games.empty()) {
        result.warnings.push_back("No tested games listed");
    }

    return result;
}

bool RecipeLoader::validateMetadata(const RecipeMetadata& meta, std::vector<std::string>& errors) {
    bool valid = true;

    if (meta.name.empty()) {
        errors.push_back("Metadata: name is required");
        valid = false;
    }
    if (meta.version.empty()) {
        errors.push_back("Metadata: version is required");
        valid = false;
    }
    if (meta.engine.empty()) {
        errors.push_back("Metadata: engine is required");
        valid = false;
    }

    // Validate support level
    if (!meta.support_level.empty()) {
        std::vector<std::string> validLevels = {"gold", "silver", "bronze", "experimental"};
        bool found = false;
        for (const auto& level : validLevels) {
            if (meta.support_level == level) {
                found = true;
                break;
            }
        }
        if (!found) {
            errors.push_back("Metadata: invalid support_level (must be gold/silver/bronze/experimental)");
            valid = false;
        }
    }

    return valid;
}

bool RecipeLoader::validateFiles(const std::vector<RecipeFile>& files, std::vector<std::string>& errors) {
    if (files.empty()) {
        errors.push_back("Files: at least one file pattern is required");
        return false;
    }

    bool valid = true;
    for (size_t i = 0; i < files.size(); i++) {
        const auto& file = files[i];
        if (file.path_pattern.empty()) {
            errors.push_back("Files[" + std::to_string(i) + "]: path_pattern is required");
            valid = false;
        }
    }

    return valid;
}

bool RecipeLoader::validateExtraction(const RecipeExtraction& ext, std::vector<std::string>& errors) {
    if (ext.method.empty()) {
        errors.push_back("Extraction: method is required");
        return false;
    }

    std::vector<std::string> validMethods = {
        "script_parser", "json_path", "regex", "binary_offset", "dialogue_tags"
    };

    bool found = false;
    for (const auto& method : validMethods) {
        if (ext.method == method) {
            found = true;
            break;
        }
    }

    if (!found) {
        errors.push_back("Extraction: invalid method '" + ext.method + "'");
        return false;
    }

    return true;
}

std::optional<Recipe> RecipeLoader::findRecipeForGame(
    const std::filesystem::path& game_path,
    GameEngine engine
) {
    // First, check cache
    std::string cacheKey = game_path.string() + "_" + std::to_string(static_cast<int>(engine));
    auto it = m_recipeCache.find(cacheKey);
    if (it != m_recipeCache.end()) {
        return it->second;
    }

    // Search all recipe directories
    for (const auto& dir : m_recipeDirs) {
        if (!std::filesystem::exists(dir)) continue;

        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.path().extension() == ".yaml" || entry.path().extension() == ".yml") {
                auto recipe = loadFromFile(entry.path());
                if (recipe && stringToEngine(recipe->metadata.engine) == engine) {
                    m_recipeCache[cacheKey] = *recipe;
                    return recipe;
                }
            }
        }
    }

    return std::nullopt;
}

std::vector<Recipe> RecipeLoader::loadAllFromDirectory(const std::filesystem::path& dir) {
    std::vector<Recipe> recipes;

    if (!std::filesystem::exists(dir)) {
        spdlog::warn("Recipe directory does not exist: {}", dir.string());
        return recipes;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() == ".yaml" || entry.path().extension() == ".yml") {
            auto recipe = loadFromFile(entry.path());
            if (recipe) {
                recipes.push_back(*recipe);
            }
        }
    }

    spdlog::info("Loaded {} recipes from {}", recipes.size(), dir.string());
    return recipes;
}

std::vector<Recipe> RecipeLoader::getRecipesByEngine(GameEngine engine) {
    std::vector<Recipe> result;

    for (const auto& dir : m_recipeDirs) {
        auto recipes = loadAllFromDirectory(dir);
        for (const auto& recipe : recipes) {
            if (stringToEngine(recipe.metadata.engine) == engine) {
                result.push_back(recipe);
            }
        }
    }

    return result;
}

void RecipeLoader::addRecipeDirectory(const std::filesystem::path& dir) {
    m_recipeDirs.push_back(dir);
}

void RecipeLoader::clearCache() {
    m_recipeCache.clear();
}

GameEngine RecipeLoader::stringToEngine(const std::string& engine) {
    std::string lower = engine;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "unity" || lower == "unity3d") return GameEngine::Unity;
    if (lower == "unreal" || lower == "ue4" || lower == "ue5") return GameEngine::Unreal;
    if (lower == "renpy" || lower == "ren'py") return GameEngine::RenPy;
    if (lower == "rpgmaker" || lower == "rpg maker" || lower == "rpg_maker") return GameEngine::RPGMaker;
    if (lower == "gamemaker" || lower == "game maker" || lower == "gms" || lower == "gms2") return GameEngine::GameMaker;
    if (lower == "godot") return GameEngine::Godot;
    if (lower == "bethesda" || lower == "creation" || lower == "gamebryo") return GameEngine::Bethesda;
    if (lower == "cryengine" || lower == "cry") return GameEngine::CryEngine;
    if (lower == "source" || lower == "source2") return GameEngine::Source;
    if (lower == "frostbite") return GameEngine::Frostbite;

    return GameEngine::Unknown;
}

std::string RecipeLoader::engineToString(GameEngine engine) {
    switch (engine) {
        case GameEngine::Unity: return "unity";
        case GameEngine::Unreal: return "unreal";
        case GameEngine::RenPy: return "renpy";
        case GameEngine::RPGMaker: return "rpgmaker";
        case GameEngine::GameMaker: return "gamemaker";
        case GameEngine::Godot: return "godot";
        case GameEngine::Bethesda: return "bethesda";
        case GameEngine::CryEngine: return "cryengine";
        case GameEngine::Source: return "source";
        case GameEngine::Frostbite: return "frostbite";
        default: return "unknown";
    }
}

} // namespace makineai
