/**
 * @file makineai_metatypes.h
 * @brief Shared metatypes and enums for MakineAI QML module
 * @copyright (c) 2026 MakineAI Team
 *
 * This file contains Q_NAMESPACE and all Q_ENUM_NS declarations
 * to avoid duplicate symbol errors when multiple headers use the
 * same namespace.
 */

#pragma once

#include <QObject>
#include <QQmlEngine>

namespace makineai {
Q_NAMESPACE
QML_ELEMENT

// ========== Game List Enums ==========

/**
 * @brief Game store filter enum for QML
 */
enum class GameStoreFilter {
    All = 0,
    Steam,
    Epic,
    GOG,
    Manual
};
Q_ENUM_NS(GameStoreFilter)

/**
 * @brief Game sort order enum for QML
 */
enum class GameSortOrder {
    NameAsc = 0,
    NameDesc,
    EngineAsc,
    EngineDesc,
    RecentFirst
};
Q_ENUM_NS(GameSortOrder)

// ========== Translation Model Enums ==========

/**
 * @brief Translation entry status filter
 */
enum class EntryStatusFilter {
    All = 0,
    Untranslated,
    Translated,
    Fuzzy,
    Verified,
    HasIssues
};
Q_ENUM_NS(EntryStatusFilter)

/**
 * @brief Translation entry category filter
 */
enum class EntryCategoryFilter {
    All = 0,
    Dialog,
    UI,
    Item,
    Skill,
    System,
    Narration,
    Other
};
Q_ENUM_NS(EntryCategoryFilter)

// ========== Translation Service Enums ==========

/**
 * @brief Translation workflow phases
 */
enum class TranslationPhase {
    Idle = 0,
    Detecting,
    Extracting,
    Matching,
    Reviewing,
    Applying,
    Completed,
    Error,
    Paused
};
Q_ENUM_NS(TranslationPhase)

} // namespace makineai
