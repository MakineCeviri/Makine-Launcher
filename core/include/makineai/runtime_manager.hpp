/**
 * @file runtime_manager.hpp
 * @brief Runtime translation system manager (stub)
 * @copyright (c) 2026 MakineAI Team
 *
 * BepInEx-specific logic has been removed. This header is retained as a
 * minimal stub so that Core and other modules that reference RuntimeManager
 * continue to compile. Future runtime backends can be added here.
 */

#pragma once

#include "types.hpp"
#include "error.hpp"

#include <memory>

namespace makineai {

/**
 * @brief Stub runtime manager
 *
 * Previously managed BepInEx + XUnity.AutoTranslator installation.
 * Now an empty shell awaiting a new runtime strategy.
 */
class RuntimeManager {
public:
    RuntimeManager() = default;
    ~RuntimeManager() = default;

    /**
     * @brief Check if a game needs a runtime translation layer
     * @return Always false until a new runtime backend is implemented
     */
    [[nodiscard]] bool needsRuntime(const GameInfo& game) const;
};

} // namespace makineai
