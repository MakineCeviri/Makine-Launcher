/**
 * @file handlers/renpy_handler.hpp
 * @brief Ren'Py engine handler stub
 * @copyright (c) 2026 MakineAI Team
 */

#pragma once

#include "makineai/handlers/engine_handler.hpp"

namespace makineai {

class RenpyHandler : public IEngineHandler {
public:
    bool canHandleGame(const fs::path& gamePath) override {
        return fs::exists(gamePath / "renpy") ||
               fs::exists(gamePath / "game" / "script.rpy");
    }

    Result<ExtractionResult> extractStrings(
        const fs::path&, const ExtractionOptions&) override {
        return std::unexpected(Error(ErrorCode::NotImplemented, "RenPy handler not implemented"));
    }

    Result<HandlerPatchResult> applyTranslations(
        const fs::path&, const std::vector<TranslationEntry>&,
        const PatchOptions&) override {
        return std::unexpected(Error(ErrorCode::NotImplemented, "RenPy handler not implemented"));
    }

    Result<HandlerBackupResult> createBackup(
        const fs::path&, const std::string&) override {
        return std::unexpected(Error(ErrorCode::NotImplemented, "RenPy handler not implemented"));
    }

    Result<HandlerBackupResult> restoreBackup(
        const fs::path&, const std::string&) override {
        return std::unexpected(Error(ErrorCode::NotImplemented, "RenPy handler not implemented"));
    }
};

} // namespace makineai
