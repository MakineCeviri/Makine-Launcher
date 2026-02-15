/**
 * @file glossary_service.hpp
 * @brief Glossary service - stub
 * @copyright (c) 2026 MakineAI Team
 *
 * Deferred feature — minimal stub for compilation.
 */

#pragma once

#include "makineai/types/translation_types.hpp"

namespace makineai {

/**
 * @brief Glossary service (stub)
 *
 * Will provide terminology management and enforcement.
 * Currently a placeholder using singleton pattern.
 */
class GlossaryService {
public:
    static GlossaryService& instance() {
        static GlossaryService s;
        return s;
    }

    GlossaryService(const GlossaryService&) = delete;
    GlossaryService& operator=(const GlossaryService&) = delete;

private:
    GlossaryService() = default;
};

} // namespace makineai
