/**
 * @file string_classifier.hpp
 * @brief Classify and filter extracted strings for translation
 *
 * Separates translatable text (dialogue, UI) from:
 * - Code/technical strings
 * - File paths
 * - Variable names
 * - Debug messages
 * - Non-text data
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <optional>
#include <memory>
#include "types.hpp"

namespace makineai {

/**
 * @brief String classification categories
 */
enum class StringCategory {
    // Translatable
    Dialogue,           // Character speech, narration
    UIText,             // Buttons, labels, menus
    ItemName,           // Items, weapons, abilities
    Description,        // Item/ability descriptions
    Tutorial,           // Help text, tutorials
    Notification,       // Alerts, notifications
    Error,              // User-facing errors

    // Non-translatable
    Code,               // Programming code
    FilePath,           // File/directory paths
    URL,                // Web URLs
    Identifier,         // Variable names, IDs
    Debug,              // Debug/log messages
    Markup,             // HTML/XML/JSON
    Numeric,            // Numbers, dates, versions
    Technical,          // Technical strings
    Garbage,            // Corrupted/binary data
    Unknown             // Unclassified
};

/**
 * @brief Classification result for a string
 */
struct ClassificationResult {
    StringCategory category = StringCategory::Unknown;
    float confidence = 0.0f;        // 0.0 - 1.0
    bool isTranslatable = false;
    std::string reason;             // Why this classification
    std::optional<std::string> suggestedContext;
};

/**
 * @brief Batch classification statistics
 */
struct ClassificationStats {
    int total = 0;
    int translatable = 0;
    int dialogue = 0;
    int uiText = 0;
    int itemNames = 0;
    int descriptions = 0;
    int code = 0;
    int filePaths = 0;
    int identifiers = 0;
    int debug = 0;
    int garbage = 0;
    int unknown = 0;

    float translatablePercent() const {
        return total > 0 ? (translatable * 100.0f / total) : 0.0f;
    }
};

/**
 * @brief Configuration for string classifier
 */
struct ClassifierConfig {
    int minWordCount = 1;           // Minimum words for dialogue
    int minLength = 2;              // Minimum character length
    int maxLength = 10000;          // Maximum length
    float minConfidence = 0.5f;     // Minimum confidence to accept
    bool strictMode = false;        // More aggressive filtering
    std::string sourceLanguage = "en";
};

/**
 * @brief String classifier for filtering translatable text
 */
class StringClassifier {
public:
    StringClassifier();
    explicit StringClassifier(const ClassifierConfig& config);
    ~StringClassifier();

    // Configuration
    void setConfig(const ClassifierConfig& config);
    ClassifierConfig getConfig() const;

    // Single string classification
    ClassificationResult classify(const std::string& text) const;

    // Quick check if string is likely translatable
    bool isTranslatable(const std::string& text) const;

    // Batch classification
    std::vector<ClassificationResult> classifyBatch(
        const std::vector<std::string>& texts) const;

    // Filter entries keeping only translatable ones
    std::vector<TranslationEntry> filterTranslatable(
        const std::vector<TranslationEntry>& entries) const;

    // Get statistics for a batch
    ClassificationStats getStats(
        const std::vector<TranslationEntry>& entries) const;

    // Category helpers
    static bool isCategoryTranslatable(StringCategory category);
    static std::string categoryToString(StringCategory category);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief Rule-based patterns for classification
 */
class ClassificationRules {
public:
    // Code patterns
    static bool looksLikeCode(const std::string& text);
    static bool looksLikeFilePath(const std::string& text);
    static bool looksLikeURL(const std::string& text);
    static bool looksLikeIdentifier(const std::string& text);
    static bool looksLikeDebug(const std::string& text);
    static bool looksLikeMarkup(const std::string& text);
    static bool looksLikeNumeric(const std::string& text);
    static bool looksLikeGarbage(const std::string& text);

    // Translatable patterns
    static bool looksLikeDialogue(const std::string& text);
    static bool looksLikeUIText(const std::string& text);
    static bool looksLikeItemName(const std::string& text);

    // Language detection (simple heuristic)
    static bool containsEnglishWords(const std::string& text);
    static int countWords(const std::string& text);
    static bool hasProperCapitalization(const std::string& text);
    static bool hasPunctuation(const std::string& text);
};

} // namespace makineai
