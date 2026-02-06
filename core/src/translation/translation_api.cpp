/**
 * @file translation_api.cpp
 * @brief Machine Translation API implementations
 */

#include "makineai/translation_api.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>

namespace makineai {

using json = nlohmann::json;

// ============================================================================
// Utility functions
// ============================================================================

namespace {

// CURL write callback
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

// HTTP POST request helper
std::expected<std::string, Error> httpPost(
    const std::string& url,
    const std::string& body,
    const std::vector<std::pair<std::string, std::string>>& headers = {},
    std::chrono::milliseconds timeout = std::chrono::milliseconds{30000})
{
    CURL* curl = curl_easy_init();
    if (!curl) {
        return std::unexpected(Error{ErrorCode::NetworkError, "Failed to initialize CURL"});
    }

    std::string response;
    struct curl_slist* headerList = nullptr;

    // Set headers
    headerList = curl_slist_append(headerList, "Content-Type: application/json");
    for (const auto& [key, value] : headers) {
        std::string header = key + ": " + value;
        headerList = curl_slist_append(headerList, header.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout.count());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

    CURLcode res = curl_easy_perform(curl);

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return std::unexpected(Error{
            ErrorCode::NetworkError,
            std::string("CURL error: ") + curl_easy_strerror(res)
        });
    }

    if (httpCode >= 400) {
        return std::unexpected(Error{
            ErrorCode::NetworkError,
            "HTTP error " + std::to_string(httpCode) + ": " + response
        });
    }

    return response;
}

// Simple translation cache
class TranslationCache {
public:
    struct CacheKey {
        std::string text;
        std::string sourceLang;
        std::string targetLang;

        bool operator==(const CacheKey& other) const {
            return text == other.text &&
                   sourceLang == other.sourceLang &&
                   targetLang == other.targetLang;
        }
    };

    struct CacheKeyHash {
        size_t operator()(const CacheKey& key) const {
            size_t h1 = std::hash<std::string>{}(key.text);
            size_t h2 = std::hash<std::string>{}(key.sourceLang);
            size_t h3 = std::hash<std::string>{}(key.targetLang);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

    std::optional<std::string> get(const CacheKey& key) {
        std::shared_lock lock(mutex_);
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    void put(const CacheKey& key, const std::string& value) {
        std::unique_lock lock(mutex_);
        // Simple LRU: if cache is full, clear half
        if (cache_.size() >= maxSize_) {
            auto it = cache_.begin();
            std::advance(it, cache_.size() / 2);
            cache_.erase(cache_.begin(), it);
        }
        cache_[key] = value;
    }

    void clear() {
        std::unique_lock lock(mutex_);
        cache_.clear();
    }

    size_t size() const {
        std::shared_lock lock(mutex_);
        return cache_.size();
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<CacheKey, std::string, CacheKeyHash> cache_;
    size_t maxSize_ = 10000;
};

} // anonymous namespace

// ============================================================================
// TranslationAPI Implementation
// ============================================================================

class TranslationAPI::Impl {
public:
    TranslationAPIConfig config;
    std::unordered_map<TranslationProvider, std::shared_ptr<ITranslationAPI>> providers;
    TranslationCache cache;
    bool cacheEnabled = true;
    mutable std::mutex mutex;

    std::shared_ptr<ITranslationAPI> getOrCreateProvider(TranslationProvider provider) {
        std::lock_guard lock(mutex);

        auto it = providers.find(provider);
        if (it != providers.end()) {
            return it->second;
        }

        std::shared_ptr<ITranslationAPI> api;
        switch (provider) {
            case TranslationProvider::LibreTranslate:
                api = std::make_shared<LibreTranslateAPI>(
                    config.endpoint.empty() ? "https://libretranslate.com" : config.endpoint
                );
                break;
            case TranslationProvider::DeepL:
                if (!config.apiKey.empty()) {
                    api = std::make_shared<DeepLAPI>(config.apiKey);
                }
                break;
            case TranslationProvider::OpenAI:
                if (!config.apiKey.empty()) {
                    api = std::make_shared<OpenAITranslateAPI>(config.apiKey);
                }
                break;
            default:
                break;
        }

        if (api) {
            providers[provider] = api;
        }
        return api;
    }
};

TranslationAPI::TranslationAPI() : impl_(std::make_unique<Impl>()) {}
TranslationAPI::~TranslationAPI() = default;

void TranslationAPI::configure(const TranslationAPIConfig& config) {
    impl_->config = config;
}

TranslationAPIConfig TranslationAPI::getConfig() const {
    return impl_->config;
}

void TranslationAPI::setDeepLKey(const std::string& key) {
    impl_->config.apiKey = key;
    impl_->providers.erase(TranslationProvider::DeepL);
}

void TranslationAPI::setGoogleKey(const std::string& key) {
    // Google implementation would use this
    impl_->providers.erase(TranslationProvider::Google);
}

void TranslationAPI::setOpenAIKey(const std::string& key) {
    impl_->config.apiKey = key;
    impl_->providers.erase(TranslationProvider::OpenAI);
}

void TranslationAPI::setLibreTranslateEndpoint(const std::string& url) {
    impl_->config.endpoint = url;
    impl_->providers.erase(TranslationProvider::LibreTranslate);
}

std::shared_ptr<ITranslationAPI> TranslationAPI::getProvider(TranslationProvider provider) {
    return impl_->getOrCreateProvider(provider);
}

std::expected<TranslationResult, Error> TranslationAPI::translate(
    const std::string& text,
    const std::string& targetLang,
    const std::string& sourceLang)
{
    // Check cache first
    if (impl_->cacheEnabled) {
        TranslationCache::CacheKey key{text, sourceLang, targetLang};
        if (auto cached = impl_->cache.get(key)) {
            TranslationResult result;
            result.translatedText = *cached;
            result.confidence = 1.0f;
            return result;
        }
    }

    auto provider = impl_->getOrCreateProvider(impl_->config.provider);
    if (!provider) {
        return std::unexpected(Error{
            ErrorCode::InvalidConfiguration,
            "Translation provider not configured"
        });
    }

    TranslationRequest request;
    request.text = text;
    request.sourceLanguage = sourceLang;
    request.targetLanguage = targetLang;

    auto result = provider->translate(request);

    // Cache successful result
    if (result && impl_->cacheEnabled) {
        TranslationCache::CacheKey key{text, sourceLang, targetLang};
        impl_->cache.put(key, result->translatedText);
    }

    return result;
}

std::expected<BatchTranslationResult, Error> TranslationAPI::translateBatch(
    const std::vector<std::string>& texts,
    const std::string& targetLang,
    const std::string& sourceLang,
    TranslationProgressCallback progress)
{
    auto provider = impl_->getOrCreateProvider(impl_->config.provider);
    if (!provider) {
        return std::unexpected(Error{
            ErrorCode::InvalidConfiguration,
            "Translation provider not configured"
        });
    }

    BatchTranslationRequest request;
    request.texts = texts;
    request.sourceLanguage = sourceLang;
    request.targetLanguage = targetLang;

    return provider->translateBatch(request, progress);
}

std::expected<TranslationResult, Error> TranslationAPI::translateWithFallback(
    const TranslationRequest& request,
    const std::vector<TranslationProvider>& fallbackOrder)
{
    for (auto provider : fallbackOrder) {
        auto api = impl_->getOrCreateProvider(provider);
        if (!api) continue;

        auto result = api->translate(request);
        if (result) {
            spdlog::debug("Translation succeeded with {}", api->getProviderName());
            return result;
        }
        spdlog::warn("Translation failed with {}, trying next provider",
                     api->getProviderName());
    }

    return std::unexpected(Error{
        ErrorCode::TranslationFailed,
        "All translation providers failed"
    });
}

std::expected<TranslationResult, Error> TranslationAPI::translateGameText(
    const std::string& text,
    const std::string& gameTitle,
    const std::string& context,
    const std::string& targetLang)
{
    TranslationRequest request;
    request.text = text;
    request.targetLanguage = targetLang;
    request.context = context.empty() ? std::nullopt : std::optional(context);
    request.domain = "game";
    request.preserveFormatting = true;

    // For game text, prefer OpenAI (context-aware) if available
    auto openai = impl_->getOrCreateProvider(TranslationProvider::OpenAI);
    if (openai) {
        // Add game context to the request
        request.context = "Game: " + gameTitle + (context.empty() ? "" : "\nContext: " + context);
        return openai->translate(request);
    }

    // Fallback to configured provider
    return translateWithFallback(request);
}

void TranslationAPI::enableCache(bool enabled) {
    impl_->cacheEnabled = enabled;
}

void TranslationAPI::clearCache() {
    impl_->cache.clear();
}

size_t TranslationAPI::getCacheSize() const {
    return impl_->cache.size();
}

APIUsageStats TranslationAPI::getTotalUsageStats() const {
    APIUsageStats total;
    std::lock_guard lock(impl_->mutex);
    for (const auto& [_, provider] : impl_->providers) {
        auto stats = provider->getUsageStats();
        total.totalRequests += stats.totalRequests;
        total.successfulRequests += stats.successfulRequests;
        total.failedRequests += stats.failedRequests;
        total.totalCharacters += stats.totalCharacters;
        total.totalTokens += stats.totalTokens;
        total.totalLatency += stats.totalLatency;
        total.estimatedCost += stats.estimatedCost;
    }
    return total;
}

// ============================================================================
// LibreTranslateAPI Implementation
// ============================================================================

class LibreTranslateAPI::Impl {
public:
    std::string endpoint;
    APIUsageStats stats;
    std::mutex statsMutex;
    std::vector<std::string> supportedLanguages;
    bool languagesLoaded = false;

    void recordRequest(bool success, int chars, std::chrono::milliseconds latency) {
        std::lock_guard lock(statsMutex);
        stats.totalRequests++;
        if (success) {
            stats.successfulRequests++;
        } else {
            stats.failedRequests++;
        }
        stats.totalCharacters += chars;
        stats.totalLatency += latency;
        // LibreTranslate is free, so no cost
    }
};

LibreTranslateAPI::LibreTranslateAPI(const std::string& endpoint)
    : impl_(std::make_unique<Impl>())
{
    impl_->endpoint = endpoint;
    if (impl_->endpoint.back() == '/') {
        impl_->endpoint.pop_back();
    }
}

LibreTranslateAPI::~LibreTranslateAPI() = default;

std::expected<TranslationResult, Error> LibreTranslateAPI::translate(
    const TranslationRequest& request)
{
    auto start = std::chrono::steady_clock::now();

    json payload = {
        {"q", request.text},
        {"source", request.sourceLanguage},
        {"target", request.targetLanguage},
        {"format", "text"}
    };

    auto response = httpPost(impl_->endpoint + "/translate", payload.dump());

    auto end = std::chrono::steady_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    if (!response) {
        impl_->recordRequest(false, 0, latency);
        return std::unexpected(response.error());
    }

    try {
        auto j = json::parse(*response);

        TranslationResult result;
        result.translatedText = j.value("translatedText", "");
        result.confidence = 1.0f;
        result.latency = latency;

        if (j.contains("detectedLanguage")) {
            auto detected = j["detectedLanguage"];
            if (detected.contains("language")) {
                result.detectedLanguage = detected["language"].get<std::string>();
                result.confidence = detected.value("confidence", 1.0f);
            }
        }

        impl_->recordRequest(true, static_cast<int>(request.text.size()), latency);
        return result;

    } catch (const json::exception& e) {
        impl_->recordRequest(false, 0, latency);
        return std::unexpected(Error{
            ErrorCode::ParseError,
            std::string("JSON parse error: ") + e.what()
        });
    }
}

std::expected<BatchTranslationResult, Error> LibreTranslateAPI::translateBatch(
    const BatchTranslationRequest& request,
    TranslationProgressCallback progress)
{
    BatchTranslationResult batchResult;
    auto totalStart = std::chrono::steady_clock::now();

    for (size_t i = 0; i < request.texts.size(); ++i) {
        TranslationRequest singleRequest;
        singleRequest.text = request.texts[i];
        singleRequest.sourceLanguage = request.sourceLanguage;
        singleRequest.targetLanguage = request.targetLanguage;
        singleRequest.preserveFormatting = request.preserveFormatting;

        auto result = translate(singleRequest);

        if (result) {
            batchResult.results.push_back(*result);
        } else {
            // Record error but continue
            TranslationResult errorResult;
            errorResult.translatedText = request.texts[i]; // Keep original
            errorResult.confidence = 0.0f;
            batchResult.results.push_back(errorResult);
            batchResult.errors.push_back(result.error().message());
        }

        if (progress) {
            progress(static_cast<int>(i + 1), static_cast<int>(request.texts.size()));
        }
    }

    auto totalEnd = std::chrono::steady_clock::now();
    batchResult.totalLatency = std::chrono::duration_cast<std::chrono::milliseconds>(
        totalEnd - totalStart);

    return batchResult;
}

std::expected<std::string, Error> LibreTranslateAPI::detectLanguage(const std::string& text) {
    json payload = {{"q", text}};

    auto response = httpPost(impl_->endpoint + "/detect", payload.dump());
    if (!response) {
        return std::unexpected(response.error());
    }

    try {
        auto j = json::parse(*response);
        if (j.is_array() && !j.empty()) {
            return j[0]["language"].get<std::string>();
        }
        return std::unexpected(Error{ErrorCode::ParseError, "No language detected"});
    } catch (const json::exception& e) {
        return std::unexpected(Error{ErrorCode::ParseError, e.what()});
    }
}

std::vector<std::string> LibreTranslateAPI::getSupportedLanguages() {
    if (impl_->languagesLoaded) {
        return impl_->supportedLanguages;
    }

    // Try to fetch from API
    CURL* curl = curl_easy_init();
    if (!curl) {
        return {"en", "tr", "de", "fr", "es", "it", "pt", "ru", "zh", "ja", "ko"};
    }

    std::string response;
    std::string url = impl_->endpoint + "/languages";

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res == CURLE_OK) {
        try {
            auto j = json::parse(response);
            for (const auto& lang : j) {
                impl_->supportedLanguages.push_back(lang["code"].get<std::string>());
            }
            impl_->languagesLoaded = true;
        } catch (...) {
            // Use fallback
        }
    }

    if (impl_->supportedLanguages.empty()) {
        impl_->supportedLanguages = {"en", "tr", "de", "fr", "es", "it", "pt", "ru", "zh", "ja", "ko"};
    }

    return impl_->supportedLanguages;
}

bool LibreTranslateAPI::isLanguagePairSupported(
    const std::string& source,
    const std::string& target)
{
    auto languages = getSupportedLanguages();
    bool sourceFound = std::find(languages.begin(), languages.end(), source) != languages.end();
    bool targetFound = std::find(languages.begin(), languages.end(), target) != languages.end();
    return sourceFound && targetFound;
}

APIUsageStats LibreTranslateAPI::getUsageStats() const {
    std::lock_guard lock(impl_->statsMutex);
    return impl_->stats;
}

void LibreTranslateAPI::resetUsageStats() {
    std::lock_guard lock(impl_->statsMutex);
    impl_->stats = APIUsageStats{};
}

// ============================================================================
// DeepLAPI Implementation
// ============================================================================

class DeepLAPI::Impl {
public:
    std::string apiKey;
    std::string endpoint;
    std::string formality = "default";
    std::string glossaryId;
    APIUsageStats stats;
    std::mutex statsMutex;

    void recordRequest(bool success, int chars, int tokens, std::chrono::milliseconds latency) {
        std::lock_guard lock(statsMutex);
        stats.totalRequests++;
        if (success) {
            stats.successfulRequests++;
        } else {
            stats.failedRequests++;
        }
        stats.totalCharacters += chars;
        stats.totalTokens += tokens;
        stats.totalLatency += latency;
        // DeepL pricing: ~$20 per 1M characters
        stats.estimatedCost += (chars / 1000000.0) * 20.0;
    }
};

DeepLAPI::DeepLAPI(const std::string& apiKey, bool useFreeAPI)
    : impl_(std::make_unique<Impl>())
{
    impl_->apiKey = apiKey;
    impl_->endpoint = useFreeAPI
        ? "https://api-free.deepl.com/v2"
        : "https://api.deepl.com/v2";
}

DeepLAPI::~DeepLAPI() = default;

std::expected<TranslationResult, Error> DeepLAPI::translate(
    const TranslationRequest& request)
{
    auto start = std::chrono::steady_clock::now();

    json payload = {
        {"text", {request.text}},
        {"source_lang", request.sourceLanguage},
        {"target_lang", request.targetLanguage}
    };

    if (request.preserveFormatting) {
        payload["preserve_formatting"] = true;
    }

    if (impl_->formality != "default") {
        payload["formality"] = impl_->formality;
    }

    if (!impl_->glossaryId.empty()) {
        payload["glossary_id"] = impl_->glossaryId;
    }

    std::vector<std::pair<std::string, std::string>> headers = {
        {"Authorization", "DeepL-Auth-Key " + impl_->apiKey}
    };

    auto response = httpPost(impl_->endpoint + "/translate", payload.dump(), headers);

    auto end = std::chrono::steady_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    if (!response) {
        impl_->recordRequest(false, 0, 0, latency);
        return std::unexpected(response.error());
    }

    try {
        auto j = json::parse(*response);

        TranslationResult result;
        if (j.contains("translations") && !j["translations"].empty()) {
            result.translatedText = j["translations"][0]["text"].get<std::string>();
            if (j["translations"][0].contains("detected_source_language")) {
                result.detectedLanguage = j["translations"][0]["detected_source_language"].get<std::string>();
            }
        }
        result.confidence = 0.95f; // DeepL is generally high quality
        result.latency = latency;

        impl_->recordRequest(true, static_cast<int>(request.text.size()), 0, latency);
        return result;

    } catch (const json::exception& e) {
        impl_->recordRequest(false, 0, 0, latency);
        return std::unexpected(Error{ErrorCode::ParseError, e.what()});
    }
}

std::expected<BatchTranslationResult, Error> DeepLAPI::translateBatch(
    const BatchTranslationRequest& request,
    TranslationProgressCallback progress)
{
    auto start = std::chrono::steady_clock::now();

    // DeepL supports batch natively
    json payload = {
        {"text", request.texts},
        {"source_lang", request.sourceLanguage},
        {"target_lang", request.targetLanguage}
    };

    if (request.preserveFormatting) {
        payload["preserve_formatting"] = true;
    }

    std::vector<std::pair<std::string, std::string>> headers = {
        {"Authorization", "DeepL-Auth-Key " + impl_->apiKey}
    };

    auto response = httpPost(impl_->endpoint + "/translate", payload.dump(), headers);

    auto end = std::chrono::steady_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    if (!response) {
        return std::unexpected(response.error());
    }

    try {
        auto j = json::parse(*response);

        BatchTranslationResult batchResult;
        batchResult.totalLatency = latency;

        if (j.contains("translations")) {
            for (const auto& t : j["translations"]) {
                TranslationResult result;
                result.translatedText = t["text"].get<std::string>();
                result.confidence = 0.95f;
                if (t.contains("detected_source_language")) {
                    result.detectedLanguage = t["detected_source_language"].get<std::string>();
                }
                batchResult.results.push_back(result);
            }
        }

        if (progress) {
            progress(static_cast<int>(batchResult.results.size()),
                     static_cast<int>(request.texts.size()));
        }

        return batchResult;

    } catch (const json::exception& e) {
        return std::unexpected(Error{ErrorCode::ParseError, e.what()});
    }
}

std::expected<std::string, Error> DeepLAPI::detectLanguage(const std::string& text) {
    TranslationRequest request;
    request.text = text;
    request.sourceLanguage = ""; // Auto-detect
    request.targetLanguage = "EN";

    auto result = translate(request);
    if (result && result->detectedLanguage) {
        return *result->detectedLanguage;
    }
    return std::unexpected(Error{ErrorCode::TranslationFailed, "Language detection failed"});
}

std::vector<std::string> DeepLAPI::getSupportedLanguages() {
    return {
        "BG", "CS", "DA", "DE", "EL", "EN", "ES", "ET", "FI", "FR",
        "HU", "ID", "IT", "JA", "KO", "LT", "LV", "NB", "NL", "PL",
        "PT", "RO", "RU", "SK", "SL", "SV", "TR", "UK", "ZH"
    };
}

bool DeepLAPI::isLanguagePairSupported(
    const std::string& source,
    const std::string& target)
{
    auto languages = getSupportedLanguages();
    std::string srcUpper = source, tgtUpper = target;
    std::transform(srcUpper.begin(), srcUpper.end(), srcUpper.begin(), ::toupper);
    std::transform(tgtUpper.begin(), tgtUpper.end(), tgtUpper.begin(), ::toupper);

    bool sourceFound = std::find(languages.begin(), languages.end(), srcUpper) != languages.end();
    bool targetFound = std::find(languages.begin(), languages.end(), tgtUpper) != languages.end();
    return sourceFound && targetFound;
}

APIUsageStats DeepLAPI::getUsageStats() const {
    std::lock_guard lock(impl_->statsMutex);
    return impl_->stats;
}

void DeepLAPI::resetUsageStats() {
    std::lock_guard lock(impl_->statsMutex);
    impl_->stats = APIUsageStats{};
}

void DeepLAPI::setFormality(const std::string& formality) {
    impl_->formality = formality;
}

void DeepLAPI::setGlossaryId(const std::string& glossaryId) {
    impl_->glossaryId = glossaryId;
}

// ============================================================================
// OpenAITranslateAPI Implementation
// ============================================================================

class OpenAITranslateAPI::Impl {
public:
    std::string apiKey;
    std::string model = "gpt-4o-mini";
    std::string systemPrompt;
    float temperature = 0.3f;
    APIUsageStats stats;
    std::mutex statsMutex;

    std::string getDefaultSystemPrompt() {
        return R"(You are a professional game translator specializing in Turkish localization.

Rules:
1. Preserve all formatting tags like {name}, [color], \n, etc.
2. Keep variable placeholders unchanged: %s, %d, {0}, {1}
3. Maintain the tone and style appropriate for games
4. Use natural Turkish that sounds native, not machine-translated
5. For UI elements, keep translations concise
6. For dialogue, maintain character voice and personality
7. Return ONLY the translated text, no explanations)";
    }

    void recordRequest(bool success, int tokens, std::chrono::milliseconds latency) {
        std::lock_guard lock(statsMutex);
        stats.totalRequests++;
        if (success) {
            stats.successfulRequests++;
        } else {
            stats.failedRequests++;
        }
        stats.totalTokens += tokens;
        stats.totalLatency += latency;
        // GPT-4o-mini pricing: ~$0.15 per 1M input tokens, ~$0.60 per 1M output tokens
        stats.estimatedCost += (tokens / 1000000.0) * 0.375; // Average
    }
};

OpenAITranslateAPI::OpenAITranslateAPI(const std::string& apiKey, const std::string& model)
    : impl_(std::make_unique<Impl>())
{
    impl_->apiKey = apiKey;
    impl_->model = model;
    impl_->systemPrompt = impl_->getDefaultSystemPrompt();
}

OpenAITranslateAPI::~OpenAITranslateAPI() = default;

std::expected<TranslationResult, Error> OpenAITranslateAPI::translate(
    const TranslationRequest& request)
{
    auto start = std::chrono::steady_clock::now();

    std::string userPrompt = "Translate the following text from " +
        request.sourceLanguage + " to " + request.targetLanguage + ":\n\n" +
        request.text;

    if (request.context) {
        userPrompt = "Context: " + *request.context + "\n\n" + userPrompt;
    }

    json payload = {
        {"model", impl_->model},
        {"messages", {
            {{"role", "system"}, {"content", impl_->systemPrompt}},
            {{"role", "user"}, {"content", userPrompt}}
        }},
        {"temperature", impl_->temperature},
        {"max_tokens", 4096}
    };

    std::vector<std::pair<std::string, std::string>> headers = {
        {"Authorization", "Bearer " + impl_->apiKey}
    };

    auto response = httpPost(
        "https://api.openai.com/v1/chat/completions",
        payload.dump(),
        headers,
        std::chrono::milliseconds{60000} // OpenAI can be slow
    );

    auto end = std::chrono::steady_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    if (!response) {
        impl_->recordRequest(false, 0, latency);
        return std::unexpected(response.error());
    }

    try {
        auto j = json::parse(*response);

        TranslationResult result;
        if (j.contains("choices") && !j["choices"].empty()) {
            result.translatedText = j["choices"][0]["message"]["content"].get<std::string>();
            // Trim whitespace
            auto start = result.translatedText.find_first_not_of(" \t\n\r");
            auto end = result.translatedText.find_last_not_of(" \t\n\r");
            if (start != std::string::npos) {
                result.translatedText = result.translatedText.substr(start, end - start + 1);
            }
        }

        result.confidence = 0.9f; // GPT translations are generally good
        result.latency = latency;

        int tokensUsed = 0;
        if (j.contains("usage")) {
            tokensUsed = j["usage"].value("total_tokens", 0);
            result.tokensUsed = tokensUsed;
        }

        impl_->recordRequest(true, tokensUsed, latency);
        return result;

    } catch (const json::exception& e) {
        impl_->recordRequest(false, 0, latency);
        return std::unexpected(Error{ErrorCode::ParseError, e.what()});
    }
}

std::expected<BatchTranslationResult, Error> OpenAITranslateAPI::translateBatch(
    const BatchTranslationRequest& request,
    TranslationProgressCallback progress)
{
    // OpenAI doesn't have native batch, so we do sequential
    // (Could be parallelized with async in the future)
    BatchTranslationResult batchResult;
    auto totalStart = std::chrono::steady_clock::now();

    for (size_t i = 0; i < request.texts.size(); ++i) {
        TranslationRequest singleRequest;
        singleRequest.text = request.texts[i];
        singleRequest.sourceLanguage = request.sourceLanguage;
        singleRequest.targetLanguage = request.targetLanguage;
        singleRequest.preserveFormatting = request.preserveFormatting;

        auto result = translate(singleRequest);

        if (result) {
            batchResult.results.push_back(*result);
            batchResult.totalTokensUsed += result->tokensUsed;
        } else {
            TranslationResult errorResult;
            errorResult.translatedText = request.texts[i];
            errorResult.confidence = 0.0f;
            batchResult.results.push_back(errorResult);
            batchResult.errors.push_back(result.error().message());
        }

        if (progress) {
            progress(static_cast<int>(i + 1), static_cast<int>(request.texts.size()));
        }
    }

    auto totalEnd = std::chrono::steady_clock::now();
    batchResult.totalLatency = std::chrono::duration_cast<std::chrono::milliseconds>(
        totalEnd - totalStart);

    return batchResult;
}

std::expected<std::string, Error> OpenAITranslateAPI::detectLanguage(const std::string& text) {
    json payload = {
        {"model", impl_->model},
        {"messages", {
            {{"role", "system"}, {"content", "Detect the language of the given text. Reply with only the ISO 639-1 language code (e.g., 'en', 'tr', 'de')."}},
            {{"role", "user"}, {"content", text}}
        }},
        {"temperature", 0.0f},
        {"max_tokens", 10}
    };

    std::vector<std::pair<std::string, std::string>> headers = {
        {"Authorization", "Bearer " + impl_->apiKey}
    };

    auto response = httpPost(
        "https://api.openai.com/v1/chat/completions",
        payload.dump(),
        headers
    );

    if (!response) {
        return std::unexpected(response.error());
    }

    try {
        auto j = json::parse(*response);
        if (j.contains("choices") && !j["choices"].empty()) {
            std::string lang = j["choices"][0]["message"]["content"].get<std::string>();
            // Clean up response
            lang.erase(std::remove_if(lang.begin(), lang.end(), ::isspace), lang.end());
            std::transform(lang.begin(), lang.end(), lang.begin(), ::tolower);
            return lang.substr(0, 2); // Just the code
        }
        return std::unexpected(Error{ErrorCode::ParseError, "No language detected"});
    } catch (const json::exception& e) {
        return std::unexpected(Error{ErrorCode::ParseError, e.what()});
    }
}

std::vector<std::string> OpenAITranslateAPI::getSupportedLanguages() {
    // GPT supports virtually all languages
    return {
        "en", "tr", "de", "fr", "es", "it", "pt", "ru", "zh", "ja", "ko",
        "ar", "hi", "bn", "pa", "vi", "th", "id", "ms", "tl", "pl", "uk",
        "nl", "sv", "da", "no", "fi", "cs", "sk", "hu", "ro", "bg", "el",
        "he", "fa", "ur", "sw", "ta", "te", "ml", "kn", "mr", "gu"
    };
}

bool OpenAITranslateAPI::isLanguagePairSupported(
    const std::string& /*source*/,
    const std::string& /*target*/)
{
    // GPT can translate between any language pairs
    return true;
}

APIUsageStats OpenAITranslateAPI::getUsageStats() const {
    std::lock_guard lock(impl_->statsMutex);
    return impl_->stats;
}

void OpenAITranslateAPI::resetUsageStats() {
    std::lock_guard lock(impl_->statsMutex);
    impl_->stats = APIUsageStats{};
}

void OpenAITranslateAPI::setModel(const std::string& model) {
    impl_->model = model;
}

void OpenAITranslateAPI::setSystemPrompt(const std::string& prompt) {
    impl_->systemPrompt = prompt;
}

void OpenAITranslateAPI::setTemperature(float temp) {
    impl_->temperature = std::clamp(temp, 0.0f, 2.0f);
}

} // namespace makineai
