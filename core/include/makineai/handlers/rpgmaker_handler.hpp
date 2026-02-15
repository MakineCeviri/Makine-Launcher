/**
 * @file handlers/rpgmaker_handler.hpp
 * @brief RPG Maker engine handler stub
 * @copyright (c) 2026 MakineAI Team
 */

#pragma once

#include "makineai/handlers/engine_handler.hpp"

namespace makineai {

class RpgMakerHandler : public IEngineHandler {
public:
    bool canHandleGame(const fs::path& gamePath) override {
        return fs::exists(gamePath / "www" / "data") ||
               fs::exists(gamePath / "Game.rgss3a");
    }

    Result<ExtractionResult> extractStrings(
        const fs::path&, const ExtractionOptions&) override {
        return std::unexpected(Error(ErrorCode::NotImplemented, "RPGMaker handler not implemented"));
    }

    Result<HandlerPatchResult> applyTranslations(
        const fs::path&, const std::vector<TranslationEntry>&,
        const PatchOptions&) override {
        return std::unexpected(Error(ErrorCode::NotImplemented, "RPGMaker handler not implemented"));
    }

    Result<HandlerBackupResult> createBackup(
        const fs::path&, const std::string&) override {
        return std::unexpected(Error(ErrorCode::NotImplemented, "RPGMaker handler not implemented"));
    }

    Result<HandlerBackupResult> restoreBackup(
        const fs::path&, const std::string&) override {
        return std::unexpected(Error(ErrorCode::NotImplemented, "RPGMaker handler not implemented"));
    }
};

} // namespace makineai
