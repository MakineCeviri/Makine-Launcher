/**
 * @file translation_api.hpp
 * @brief Machine Translation API integrations
 *
 * Supports multiple translation backends:
 * - DeepL (highest quality)
 * - Google Translate
 * - LibreTranslate (self-hosted, free)
 * - OpenAI GPT (context-aware)
 * - Local models (future)
 */

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <expected>
#include <functional>
#include <memory>
#include <chrono>
#include "error.hpp"
#include "types.hpp"

namespace makineai {

// Forward declarations
class TranslationAPI;

/**
 * @brief Translation provider types
 */
enum class TranslationProvider {
    DeepL,          // Best quality, paid
    Google,         // Good quality, paid
    LibreTranslate, // Self-hosted, free
    OpenAI,         // GPT-based, context-aware
    Azure,          // Microsoft Translator
    Yandex,         // Good for Slavic languages
    Local,          // Future: local LLM
    Mock            // For testing
};

/**
 * @brief Translation request
 */
struct TranslationRequest {
    std::string text;                           // Text to translate
    std::string sourceLanguage = "en";          // Source language code
    std::string targetLanguage = "tr";          // Target language code
    std::optional<std::string> context;         // Surrounding context
    std::optional<std::string> domain;          // Game, UI, dialogue, etc.
    std::optional<std::string> glossaryId;      // Custom glossary
    bool preserveFormatting = true;             // Keep {tags}, \n, etc.
    bool formalTone = false;                    // Formal vs informal
};

/**
 * @brief Batch translation request
 */
struct BatchTranslationRequest {
    std::vector<std::string> texts;
    std::string sourceLanguage = "en";
    std::string targetLanguage = "tr";
    std::optional<std::string> glossaryId;
    bool preserveFormatting = true;
};

/**
 * @brief Translation result
 */
struct TranslationResult {
    std::string translatedText;
    float confidence = 1.0f;                    // 0.0 - 1.0
    std::optional<std::string> detectedLanguage;
    std::optional<std::vector<std::string>> alternatives;
    int tokensUsed = 0;                         // For API cost tracking
    std::chrono::milliseconds latency{0};
};

/**
 * @brief Batch translation result
 */
struct BatchTranslationResult {
    std::vector<TranslationResult> results;
    int totalTokensUsed = 0;
    std::chrono::milliseconds totalLatency{0};
    std::vector<std::string> errors;            // Per-item errors
};

/**
 * @brief API configuration
 */
struct TranslationAPIConfig {
    TranslationProvider provider = TranslationProvider::LibreTranslate;
    std::string apiKey;                         // API key (if required)
    std::string endpoint;                       // Custom endpoint URL
    int maxRetries = 3;
    std::chrono::milliseconds timeout{30000};
    int maxBatchSize = 50;                      // Max texts per batch
    int rateLimit = 0;                          // Requests per second (0 = unlimited)
    bool cacheResults = true;                   // Cache translations locally
};

/**
 * @brief Usage statistics
 */
struct APIUsageStats {
    int totalRequests = 0;
    int successfulRequests = 0;
    int failedRequests = 0;
    int totalCharacters = 0;
    int totalTokens = 0;
    std::chrono::milliseconds totalLatency{0};
    double estimatedCost = 0.0;                 // In USD
};

/**
 * @brief Progress callback for batch operations
 */
using TranslationProgressCallback = std::function<void(int completed, int total)>;

/**
 * @brief Translation API interface
 */
class ITranslationAPI {
public:
    virtual ~ITranslationAPI() = default;

    // Single translation
    virtual std::expected<TranslationResult, Error> translate(
        const TranslationRequest& request) = 0;

    // Batch translation (more efficient)
    virtual std::expected<BatchTranslationResult, Error> translateBatch(
        const BatchTranslationRequest& request,
        TranslationProgressCallback progress = nullptr) = 0;

    // Language detection
    virtual std::expected<std::string, Error> detectLanguage(
        const std::string& text) = 0;

    // Supported languages
    virtual std::vector<std::string> getSupportedLanguages() = 0;

    // Check if language pair is supported
    virtual bool isLanguagePairSupported(
        const std::string& source,
        const std::string& target) = 0;

    // Usage stats
    virtual APIUsageStats getUsageStats() const = 0;
    virtual void resetUsageStats() = 0;

    // Provider info
    virtual TranslationProvider getProvider() const = 0;
    virtual std::string getProviderName() const = 0;
};

/**
 * @brief Main Translation API manager
 */
class TranslationAPI {
public:
    TranslationAPI();
    ~TranslationAPI();

    // Configuration
    void configure(const TranslationAPIConfig& config);
    TranslationAPIConfig getConfig() const;

    // Set API keys for different providers
    void setDeepLKey(const std::string& key);
    void setGoogleKey(const std::string& key);
    void setOpenAIKey(const std::string& key);
    void setLibreTranslateEndpoint(const std::string& url);

    // Get provider instance
    std::shared_ptr<ITranslationAPI> getProvider(TranslationProvider provider);

    // Convenience methods using configured provider
    std::expected<TranslationResult, Error> translate(
        const std::string& text,
        const std::string& targetLang = "tr",
        const std::string& sourceLang = "en");

    std::expected<BatchTranslationResult, Error> translateBatch(
        const std::vector<std::string>& texts,
        const std::string& targetLang = "tr",
        const std::string& sourceLang = "en",
        TranslationProgressCallback progress = nullptr);

    // Smart translation with fallback
    std::expected<TranslationResult, Error> translateWithFallback(
        const TranslationRequest& request,
        const std::vector<TranslationProvider>& fallbackOrder = {
            TranslationProvider::DeepL,
            TranslationProvider::Google,
            TranslationProvider::LibreTranslate
        });

    // Game-aware translation
    std::expected<TranslationResult, Error> translateGameText(
        const std::string& text,
        const std::string& gameTitle,
        const std::string& context = "",
        const std::string& targetLang = "tr");

    // Cache management
    void enableCache(bool enabled);
    void clearCache();
    size_t getCacheSize() const;

    // Combined stats from all providers
    APIUsageStats getTotalUsageStats() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief LibreTranslate implementation (free, self-hosted)
 */
class LibreTranslateAPI : public ITranslationAPI {
public:
    explicit LibreTranslateAPI(const std::string& endpoint = "https://libretranslate.com");
    ~LibreTranslateAPI() override;

    std::expected<TranslationResult, Error> translate(
        const TranslationRequest& request) override;

    std::expected<BatchTranslationResult, Error> translateBatch(
        const BatchTranslationRequest& request,
        TranslationProgressCallback progress = nullptr) override;

    std::expected<std::string, Error> detectLanguage(
        const std::string& text) override;

    std::vector<std::string> getSupportedLanguages() override;

    bool isLanguagePairSupported(
        const std::string& source,
        const std::string& target) override;

    APIUsageStats getUsageStats() const override;
    void resetUsageStats() override;

    TranslationProvider getProvider() const override {
        return TranslationProvider::LibreTranslate;
    }
    std::string getProviderName() const override {
        return "LibreTranslate";
    }

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief DeepL API implementation (best quality)
 */
class DeepLAPI : public ITranslationAPI {
public:
    explicit DeepLAPI(const std::string& apiKey, bool useFreeAPI = false);
    ~DeepLAPI() override;

    std::expected<TranslationResult, Error> translate(
        const TranslationRequest& request) override;

    std::expected<BatchTranslationResult, Error> translateBatch(
        const BatchTranslationRequest& request,
        TranslationProgressCallback progress = nullptr) override;

    std::expected<std::string, Error> detectLanguage(
        const std::string& text) override;

    std::vector<std::string> getSupportedLanguages() override;

    bool isLanguagePairSupported(
        const std::string& source,
        const std::string& target) override;

    APIUsageStats getUsageStats() const override;
    void resetUsageStats() override;

    TranslationProvider getProvider() const override {
        return TranslationProvider::DeepL;
    }
    std::string getProviderName() const override {
        return "DeepL";
    }

    // DeepL-specific
    void setFormality(const std::string& formality); // "default", "more", "less"
    void setGlossaryId(const std::string& glossaryId);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief OpenAI GPT-based translation (context-aware)
 */
class OpenAITranslateAPI : public ITranslationAPI {
public:
    explicit OpenAITranslateAPI(const std::string& apiKey,
                                 const std::string& model = "gpt-4o-mini");
    ~OpenAITranslateAPI() override;

    std::expected<TranslationResult, Error> translate(
        const TranslationRequest& request) override;

    std::expected<BatchTranslationResult, Error> translateBatch(
        const BatchTranslationRequest& request,
        TranslationProgressCallback progress = nullptr) override;

    std::expected<std::string, Error> detectLanguage(
        const std::string& text) override;

    std::vector<std::string> getSupportedLanguages() override;

    bool isLanguagePairSupported(
        const std::string& source,
        const std::string& target) override;

    APIUsageStats getUsageStats() const override;
    void resetUsageStats() override;

    TranslationProvider getProvider() const override {
        return TranslationProvider::OpenAI;
    }
    std::string getProviderName() const override {
        return "OpenAI";
    }

    // OpenAI-specific
    void setModel(const std::string& model);
    void setSystemPrompt(const std::string& prompt);
    void setTemperature(float temp);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace makineai
