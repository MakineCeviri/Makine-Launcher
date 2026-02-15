/**
 * @file handlers/engine_handler.hpp
 * @brief Engine handler interface and related types
 * @copyright (c) 2026 MakineAI Team
 *
 * Deferred feature — stub interface for compilation.
 * Handler-based pipeline was closed; these provide the interface
 * that CoreBridge references.
 */

#pragma once

#include "makineai/types.hpp"
#include "makineai/error.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace makineai {

// ============================================================================
// Handler-specific types
// ============================================================================

struct ExtractionOptions {
    int minLength = 1;
    int maxLength = 10000;
};

struct ExtractionResult {
    std::vector<TranslationEntry> entries;
};

struct PatchOptions {
    bool createBackup = true;
};

struct HandlerPatchResult {
    int appliedCount = 0;
};

struct HandlerBackupResult {
    bool success = false;
    std::string errorMessage;
    std::string backupId;
};

// ============================================================================
// IEngineHandler interface
// ============================================================================

class IEngineHandler {
public:
    virtual ~IEngineHandler() = default;

    virtual bool canHandleGame(const fs::path& gamePath) = 0;

    virtual Result<ExtractionResult> extractStrings(
        const fs::path& gamePath,
        const ExtractionOptions& options = {}
    ) = 0;

    virtual Result<HandlerPatchResult> applyTranslations(
        const fs::path& gamePath,
        const std::vector<TranslationEntry>& translations,
        const PatchOptions& options = {}
    ) = 0;

    virtual Result<HandlerBackupResult> createBackup(
        const fs::path& gamePath,
        const std::string& backupId
    ) = 0;

    virtual Result<HandlerBackupResult> restoreBackup(
        const fs::path& gamePath,
        const std::string& backupId
    ) = 0;
};

} // namespace makineai
