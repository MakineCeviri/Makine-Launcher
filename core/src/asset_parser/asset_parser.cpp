/**
 * @file asset_parser.cpp
 * @brief Asset parser implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/asset_parser.hpp"
#include "makineai/parsers_factory.hpp"
#include "makineai/core.hpp"
#include "makineai/features.hpp"
#include "makineai/logging.hpp"
#include "makineai/metrics.hpp"

#include <algorithm>

#include "makineai/parallel.hpp"

// Optional: mio for memory-mapped file reading
#ifdef MAKINEAI_HAS_MIO
#include <mio/mmap.hpp>
#endif

// Optional: bit7z for archive extraction
#ifdef MAKINEAI_HAS_BIT7Z
#include <bit7z/bitarchivereader.hpp>
#include <bit7z/bitextractor.hpp>
#endif

// Optional: libarchive for multi-format archives
#ifdef MAKINEAI_HAS_LIBARCHIVE
#include <archive.h>
#include <archive_entry.h>
#endif

namespace makineai::parsers {

AssetParser::AssetParser() {
    registerBuiltinParsers();
}

AssetParser::~AssetParser() = default;

void AssetParser::registerParser(std::unique_ptr<IAssetFormatParser> parser) {
    if (parser) {
        MAKINEAI_LOG_DEBUG(log::PARSER, "Registering parser: {}", parser->name());
        parsers_.push_back(std::move(parser));
    }
}

void AssetParser::registerBuiltinParsers() {
    // Register built-in parsers via factory functions
    registerParser(createUnityBundleParser());
    registerParser(createUnrealPakParser());
    registerParser(createBethesdaBa2Parser());
    registerParser(createGameMakerDataParser());

    MAKINEAI_LOG_INFO(log::PARSER, "Registered {} built-in parsers", parsers_.size());
}

IAssetFormatParser* AssetParser::getParserForFile(const fs::path& file) const {
    for (const auto& parser : parsers_) {
        if (parser->canParse(file)) {
            return parser.get();
        }
    }
    return nullptr;
}

Result<ParseResult> AssetParser::parseFile(const fs::path& file) const {
    if (!fs::exists(file)) {
        MAKINEAI_LOG_ERROR(log::PARSER, "File not found: {}", file.string());
        return std::unexpected(Error(ErrorCode::FileNotFound,
            "File not found: " + file.string()));
    }

    // Record file size for histogram
    auto fileSize = fs::file_size(file);
    Metrics::instance().recordHistogram("asset_file_sizes", static_cast<int64_t>(fileSize));

    auto* parser = getParserForFile(file);
    if (!parser) {
        MAKINEAI_LOG_WARN(log::PARSER, "No parser available for: {}", file.string());
        Metrics::instance().increment("parse_failures_unknown");
        return std::unexpected(Error(ErrorCode::GameNotSupported,
            "No parser available for: " + file.string()));
    }

    MAKINEAI_LOG_DEBUG(log::PARSER, "Detected format: {} for {}", parser->name(), file.filename().string());
    MAKINEAI_LOG_INFO(log::PARSER, "Parsing {} with {}", file.filename().string(), parser->name());

    auto result = parser->parse(file);

    if (result) {
        MAKINEAI_LOG_INFO(log::PARSER, "Parsing complete: {} - {} strings extracted",
            file.filename().string(), result->strings.size());
    } else {
        MAKINEAI_LOG_ERROR(log::PARSER, "Parsing failed: {} - {}",
            file.filename().string(), result.error().message());
    }

    return result;
}

Result<std::vector<ParseResult>> AssetParser::parseDirectory(
    const fs::path& directory,
    bool recursive,
    ProgressCallback progress
) const {
    if (!fs::exists(directory) || !fs::is_directory(directory)) {
        return std::unexpected(Error(ErrorCode::DirectoryNotFound,
            "Directory not found: " + directory.string()));
    }

    std::vector<ParseResult> results;
    std::vector<fs::path> files;

    // Collect all parseable files
    auto collectFiles = [&](const fs::path& path) {
        for (const auto& parser : parsers_) {
            if (parser->canParse(path)) {
                files.push_back(path);
                break;
            }
        }
    };

    if (recursive) {
        for (const auto& entry : fs::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                collectFiles(entry.path());
            }
        }
    } else {
        for (const auto& entry : fs::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                collectFiles(entry.path());
            }
        }
    }

    // Parse collected files in parallel (Taskflow when available, std::async fallback)
    uint32_t total = static_cast<uint32_t>(files.size());
    MAKINEAI_LOG_INFO(log::PARSER, "Parsing {} files using {}", total, parallel::backendInfo());

    auto parseResults = parallel::map(files,
        [this](const fs::path& file) -> Result<ParseResult> {
            return parseFile(file);
        },
        progress ? parallel::ProgressTracker::Callback(
            [&progress, total](uint32_t current, uint32_t /*t*/, const std::string&) {
                progress(current, total, "Parsing assets...");
            }
        ) : parallel::ProgressTracker::Callback(nullptr)
    );

    for (auto& result : parseResults) {
        if (result) {
            results.push_back(std::move(*result));
        }
    }

    if (progress) {
        progress(total, total, "Parsing complete");
    }

    return results;
}

VoidResult AssetParser::writeFile(
    const fs::path& file,
    const std::vector<StringEntry>& strings
) const {
    auto* parser = getParserForFile(file);
    if (!parser) {
        MAKINEAI_LOG_ERROR(log::PARSER, "No parser available for writing: {}", file.string());
        return std::unexpected(Error(ErrorCode::GameNotSupported,
            "No parser available for: " + file.string()));
    }

    MAKINEAI_LOG_INFO(log::PARSER, "Writing {} strings to {} with {}",
        strings.size(), file.filename().string(), parser->name());
    return parser->write(file, strings);
}

GameEngine AssetParser::detectEngine(const fs::path& gameDir) const {
    if (!fs::exists(gameDir) || !fs::is_directory(gameDir)) {
        return GameEngine::Unknown;
    }

    // Check for Unity
    for (const auto& entry : fs::directory_iterator(gameDir)) {
        if (entry.is_directory()) {
            auto name = entry.path().filename().string();
            if (name.ends_with("_Data")) {
                // Check for IL2CPP vs Mono
                if (fs::exists(gameDir / "GameAssembly.dll")) {
                    return GameEngine::Unity_IL2CPP;
                }
                auto managedPath = entry.path() / "Managed";
                if (fs::exists(managedPath / "Assembly-CSharp.dll")) {
                    return GameEngine::Unity_Mono;
                }
            }
        }
    }

    // Check for Unreal Engine
    if (fs::exists(gameDir / "Engine") ||
        fs::exists(gameDir / "Content" / "Paks")) {
        return GameEngine::Unreal;
    }

    // Check for Bethesda (Starfield, Fallout)
    if (fs::exists(gameDir / "Data")) {
        for (const auto& entry : fs::directory_iterator(gameDir / "Data")) {
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".ba2" || ext == ".esm" || ext == ".esp") {
                return GameEngine::Bethesda;
            }
        }
    }

    // Check for GameMaker
    if (fs::exists(gameDir / "data.win")) {
        return GameEngine::GameMaker;
    }

    // Check for Ren'Py
    if (fs::exists(gameDir / "renpy") ||
        fs::exists(gameDir / "game" / "script.rpy")) {
        return GameEngine::RenPy;
    }

    // Check for RPG Maker MV/MZ
    if (fs::exists(gameDir / "www" / "data") ||
        fs::exists(gameDir / "data" / "System.json")) {
        return GameEngine::RPGMaker_MV;
    }

    // Check for RPG Maker VX Ace
    if (fs::exists(gameDir / "Data" / "System.rvdata2")) {
        return GameEngine::RPGMaker_VX;
    }

    return GameEngine::Unknown;
}

} // namespace makineai::parsers
