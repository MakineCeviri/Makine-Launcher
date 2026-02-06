/**
 * @file mock_handler.hpp
 * @brief Mock implementations of engine handler interfaces for testing
 *
 * Provides Google Mock implementations of IEngineHandler for unit testing
 * without requiring actual game files or modifications.
 *
 * Copyright (c) 2026 MakineAI Team
 */

#pragma once

#include <gmock/gmock.h>
#include "makineai/handlers/engine_handler.hpp"

namespace makineai::testing {

/**
 * @brief Mock implementation of IEngineHandler
 *
 * Usage:
 * @code
 * MockHandler handler;
 * EXPECT_CALL(handler, extractStrings(testing::_))
 *     .WillOnce(Return(Result<std::vector<StringEntry>>{...}));
 * @endcode
 */
class MockHandler : public IEngineHandler {
public:
    MOCK_METHOD(std::string_view, name, (), (const, noexcept, override));
    MOCK_METHOD(GameEngine, engine, (), (const, noexcept, override));
    MOCK_METHOD(std::vector<GameEngine>, supportedEngines, (), (const, override));
    MOCK_METHOD(bool, canHandle, (GameEngine engine), (const, override));

    MOCK_METHOD(Result<std::vector<StringEntry>>, extractStrings,
                (const fs::path& gamePath), (const, override));

    MOCK_METHOD(Result<HandlerPatchResult>, applyTranslations,
                (const fs::path& gamePath, const std::vector<TranslationEntry>& translations),
                (const, override));

    MOCK_METHOD(Result<HandlerBackupResult>, createBackup,
                (const fs::path& gamePath, const fs::path& backupPath),
                (const, override));

    MOCK_METHOD(Result<HandlerRestoreResult>, restoreBackup,
                (const fs::path& gamePath, const fs::path& backupPath),
                (const, override));

    MOCK_METHOD(Result<ValidationResult>, validatePatch,
                (const fs::path& gamePath),
                (const, override));
};

/**
 * @brief Fake handler that returns predefined results
 *
 * Useful for deterministic testing without mocking every call.
 */
class FakeHandler : public IEngineHandler {
public:
    explicit FakeHandler(
        std::string_view name,
        GameEngine engine
    )
        : name_(name)
        , engine_(engine)
    {}

    [[nodiscard]] std::string_view name() const noexcept override {
        return name_;
    }

    [[nodiscard]] GameEngine engine() const noexcept override {
        return engine_;
    }

    [[nodiscard]] std::vector<GameEngine> supportedEngines() const override {
        return {engine_};
    }

    [[nodiscard]] bool canHandle(GameEngine engine) const override {
        return engine == engine_;
    }

    [[nodiscard]] Result<std::vector<StringEntry>> extractStrings(
        const fs::path& /*gamePath*/
    ) const override {
        return strings_;
    }

    [[nodiscard]] Result<HandlerPatchResult> applyTranslations(
        const fs::path& /*gamePath*/,
        const std::vector<TranslationEntry>& translations
    ) const override {
        HandlerPatchResult result;
        result.success = true;
        result.patchedFiles = 1;
        result.patchedStrings = static_cast<uint32_t>(translations.size());
        return result;
    }

    [[nodiscard]] Result<HandlerBackupResult> createBackup(
        const fs::path& /*gamePath*/,
        const fs::path& /*backupPath*/
    ) const override {
        HandlerBackupResult result;
        result.success = true;
        result.backedUpFiles = 5;
        return result;
    }

    [[nodiscard]] Result<HandlerRestoreResult> restoreBackup(
        const fs::path& /*gamePath*/,
        const fs::path& /*backupPath*/
    ) const override {
        HandlerRestoreResult result;
        result.success = true;
        result.restoredFiles = 5;
        return result;
    }

    [[nodiscard]] Result<ValidationResult> validatePatch(
        const fs::path& /*gamePath*/
    ) const override {
        ValidationResult result;
        result.isValid = true;
        return result;
    }

    // Test helpers
    void setStrings(std::vector<StringEntry> strings) {
        strings_ = std::move(strings);
    }

    void addString(const std::string& key, const std::string& original,
                   const std::string& context = "") {
        StringEntry entry;
        entry.key = key;
        entry.original = original;
        entry.context = context;
        strings_.push_back(std::move(entry));
    }

    void clearStrings() {
        strings_.clear();
    }

private:
    std::string name_;
    GameEngine engine_;
    std::vector<StringEntry> strings_;
};

/**
 * @brief Create a fake StringEntry for testing
 */
inline StringEntry createFakeStringEntry(
    const std::string& key,
    const std::string& original,
    const std::string& translated = "",
    const std::string& context = ""
) {
    StringEntry entry;
    entry.key = key;
    entry.original = original;
    entry.translated = translated;
    entry.context = context;
    return entry;
}

/**
 * @brief Create a fake TranslationEntry for testing
 */
inline TranslationEntry createFakeTranslationEntry(
    const std::string& key,
    const std::string& original,
    const std::string& translated
) {
    TranslationEntry entry;
    entry.key = key;
    entry.original = original;
    entry.translated = translated;
    return entry;
}

/**
 * @brief Create sample strings for testing
 */
inline std::vector<StringEntry> createSampleStrings() {
    return {
        createFakeStringEntry("greeting", "Hello, world!"),
        createFakeStringEntry("farewell", "Goodbye!"),
        createFakeStringEntry("menu_start", "Start Game"),
        createFakeStringEntry("menu_load", "Load Game"),
        createFakeStringEntry("menu_options", "Options"),
        createFakeStringEntry("menu_exit", "Exit")
    };
}

/**
 * @brief Create sample translations for testing
 */
inline std::vector<TranslationEntry> createSampleTranslations() {
    return {
        createFakeTranslationEntry("greeting", "Hello, world!", "Merhaba, dünya!"),
        createFakeTranslationEntry("farewell", "Goodbye!", "Hoşça kal!"),
        createFakeTranslationEntry("menu_start", "Start Game", "Oyunu Başlat"),
        createFakeTranslationEntry("menu_load", "Load Game", "Oyun Yükle"),
        createFakeTranslationEntry("menu_options", "Options", "Seçenekler"),
        createFakeTranslationEntry("menu_exit", "Exit", "Çıkış")
    };
}

} // namespace makineai::testing
