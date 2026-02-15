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
 */

#pragma once

#include "makineai/types/common.hpp"
#include "makineai/types/game_types.hpp"
#include "makineai/types/patch_types.hpp"
#include "makineai/types/translation_types.hpp"
#include "makineai/types/tm_types.hpp"
#include "makineai/types/pipeline_types.hpp"
