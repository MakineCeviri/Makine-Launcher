/**
 * @file types.hpp
 * @brief Makine core type definitions - umbrella header
 * @copyright (c) 2026 MakineCeviri Team
 *
 * This file provides backward compatibility by including all type headers.
 * For new code, consider including only the specific type headers you need
 * to reduce compilation dependencies.
 *
 * Modular headers:
 * - types/common.hpp          - Basic types, fs alias, progress callbacks
 * - types/game_types.hpp      - GameEngine, GameInfo, GameStore, GameId
 * - types/patch_types.hpp     - PatchStatus, PatchResult, BackupResult
 */

#pragma once

#include "makine/types/common.hpp"
#include "makine/types/game_types.hpp"
#include "makine/types/patch_types.hpp"
