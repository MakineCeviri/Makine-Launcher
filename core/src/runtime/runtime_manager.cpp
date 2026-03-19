/**
 * @file runtime_manager.cpp
 * @brief Runtime translation system manager (stub)
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "makine/runtime_manager.hpp"

namespace makine {

bool RuntimeManager::needsRuntime([[maybe_unused]] const GameInfo& game) const {
    // No runtime backend configured — nothing needs runtime translation
    return false;
}

} // namespace makine
