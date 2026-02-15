/**
 * @file handlers/unreal_handler.hpp
 * @brief Unreal Engine handler stub
 * @copyright (c) 2026 MakineAI Team
 */

#pragma once

#include "makineai/handlers/engine_handler.hpp"

namespace makineai {

class UnrealHandler : public IEngineHandler {
public:
    bool canHandleGame(const fs::path& gamePath) override {
        std::error_code ec;
        for (auto& entry : fs::directory_iterator(gamePath, fs::directory_options::skip_permission_denied, ec)) {
            if (entry.path().extension() == ".pak") return true;
        }
        return false;
    }

    Result<ExtractionResult> extractStrings(
        const fs::path&, const ExtractionOptions&) override {
        return std::unexpected(Error(ErrorCode::NotImplemented, "Unreal handler not implemented"));
    }

    Result<HandlerPatchResult> applyTranslations(
        const fs::path&, const std::vector<TranslationEntry>&,
        const PatchOptions&) override {
        return std::unexpected(Error(ErrorCode::NotImplemented, "Unreal handler not implemented"));
    }

    Result<HandlerBackupResult> createBackup(
        const fs::path&, const std::string&) override {
        return std::unexpected(Error(ErrorCode::NotImplemented, "Unreal handler not implemented"));
    }

    Result<HandlerBackupResult> restoreBackup(
        const fs::path&, const std::string&) override {
        return std::unexpected(Error(ErrorCode::NotImplemented, "Unreal handler not implemented"));
    }
};

} // namespace makineai
