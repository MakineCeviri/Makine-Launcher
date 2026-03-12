/**
 * @file runtime_manager.cpp
 * @brief Runtime translation system manager (stub)
 * @copyright (c) 2026 MakineAI Team
 *
 * BepInEx-specific logic has been removed. Only the minimal stub remains.
 */

#include "makineai/runtime_manager.hpp"

namespace makineai {

bool RuntimeManager::needsRuntime([[maybe_unused]] const GameInfo& game) const {
    // No runtime backend configured — nothing needs runtime translation
    return false;
}

} // namespace makineai
