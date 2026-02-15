/**
 * @file handlers/gamemaker_handler.hpp
 * @brief GameMaker engine handler stub
 * @copyright (c) 2026 MakineAI Team
 */

#pragma once

#include "makineai/handlers/engine_handler.hpp"

namespace makineai {

class GameMakerHandler : public IEngineHandler {
public:
    bool canHandleGame(const fs::path& gamePath) override {
        return fs::exists(gamePath / "data.win");
    }

    Result<ExtractionResult> extractStrings(
        const fs::path&, const ExtractionOptions&) override {
        return std::unexpected(Error(ErrorCode::NotImplemented, "GameMaker handler not implemented"));
    }

    Result<HandlerPatchResult> applyTranslations(
        const fs::path&, const std::vector<TranslationEntry>&,
        const PatchOptions&) override {
        return std::unexpected(Error(ErrorCode::NotImplemented, "GameMaker handler not implemented"));
    }

    Result<HandlerBackupResult> createBackup(
        const fs::path&, const std::string&) override {
        return std::unexpected(Error(ErrorCode::NotImplemented, "GameMaker handler not implemented"));
    }

    Result<HandlerBackupResult> restoreBackup(
        const fs::path&, const std::string&) override {
        return std::unexpected(Error(ErrorCode::NotImplemented, "GameMaker handler not implemented"));
    }
};

} // namespace makineai
