/**
 * @file types.hpp
 * @brief MakineAI core type definitions - umbrella header
 * @copyright (c) 2026 MakineAI Team
 *
 * This file provides backward compatibility by including all type headers.
 * For new code, consider including only the specific type headers you need
 * to reduce compilation dependencies.
 *
 * Modular headers:
 * - types/common.hpp          - Basic types, fs alias, progress callbacks
 * - types/game_types.hpp      - GameEngine, GameInfo, GameStore, GameId
 * - types/patch_types.hpp     - PatchStatus, PatchResult, BackupResult
 * - types/translation_types.hpp - TranslationEntry, Glossary, QA types
 * - types/tm_types.hpp        - TranslationMemoryEntry, TMMatch
 * - types/pipeline_types.hpp  - TranslationMethod, Pipeline context
 */

#pragma once

// Include all type headers for backward compatibility
#include "makineai/types/common.hpp"
#include "makineai/types/game_types.hpp"
#include "makineai/types/patch_types.hpp"
#include "makineai/types/translation_types.hpp"
#include "makineai/types/tm_types.hpp"
#include "makineai/types/pipeline_types.hpp"

// Note: This file is intentionally minimal.
// All types are now defined in their respective modular headers.
// This umbrella header exists solely for backward compatibility.
//
// Migration guide:
// - Old:  #include "makineai/types.hpp"
// - New:  #include "makineai/types/game_types.hpp"  // Only what you need
//
// Benefits of modular includes:
// - Faster compilation (less parsing)
// - Clearer dependencies (know what you use)
// - Better IDE support (smaller symbol tables)
