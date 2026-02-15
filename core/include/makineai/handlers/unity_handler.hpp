/**
 * @file handlers/unity_handler.hpp
 * @brief Unity engine handler stub
 * @copyright (c) 2026 MakineAI Team
 */

#pragma once

#include "makineai/handlers/engine_handler.hpp"

namespace makineai {

class UnityHandler : public IEngineHandler {
public:
    bool canHandleGame(const fs::path& gamePath) override {
        return fs::exists(gamePath / "UnityPlayer.dll") ||
               fs::exists(gamePath / "GameAssembly.dll");
    }

    Result<ExtractionResult> extractStrings(
        const fs::path&, const ExtractionOptions&) override {
        return std::unexpected(Error(ErrorCode::NotImplemented, "Unity handler not implemented"));
    }

    Result<HandlerPatchResult> applyTranslations(
        const fs::path&, const std::vector<TranslationEntry>&,
        const PatchOptions&) override {
        return std::unexpected(Error(ErrorCode::NotImplemented, "Unity handler not implemented"));
    }

    Result<HandlerBackupResult> createBackup(
        const fs::path&, const std::string&) override {
        return std::unexpected(Error(ErrorCode::NotImplemented, "Unity handler not implemented"));
    }

    Result<HandlerBackupResult> restoreBackup(
        const fs::path&, const std::string&) override {
        return std::unexpected(Error(ErrorCode::NotImplemented, "Unity handler not implemented"));
    }
};

} // namespace makineai
