/**
 * @file parsers_factory.hpp
 * @brief Factory functions for creating asset parsers
 * @copyright (c) 2026 MakineAI Team
 */

#pragma once

#include "makineai/asset_parser.hpp"
#include <memory>

namespace makineai {

/**
 * @brief Create Unity AssetBundle parser
 * @return Parser instance for .bundle, .assets, .resource files
 */
std::unique_ptr<parsers::IAssetFormatParser> createUnityBundleParser();

/**
 * @brief Create Unreal Engine PAK parser
 * @return Parser instance for .pak files
 */
std::unique_ptr<parsers::IAssetFormatParser> createUnrealPakParser();

/**
 * @brief Create Bethesda BA2 archive parser
 * @return Parser instance for .ba2 files (Fallout 4, Starfield)
 */
std::unique_ptr<parsers::IAssetFormatParser> createBethesdaBa2Parser();

/**
 * @brief Create GameMaker data.win parser
 * @return Parser instance for data.win files
 */
std::unique_ptr<parsers::IAssetFormatParser> createGameMakerDataParser();

} // namespace makineai
