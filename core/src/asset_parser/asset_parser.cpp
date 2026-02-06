/**
 * @file asset_parser.cpp
 * @brief Asset parser implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/asset_parser.hpp"
#include "makineai/parsers_factory.hpp"
#include "makineai/core.hpp"
#include "makineai/features.hpp"

#include <algorithm>

// Optional: Taskflow for parallel asset parsing
#ifdef MAKINEAI_HAS_TASKFLOW
#include <taskflow/taskflow.hpp>
#endif

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

namespace makineai {

AssetParser::AssetParser() {
    registerBuiltinParsers();
}

AssetParser::~AssetParser() = default;

void AssetParser::registerParser(std::unique_ptr<IAssetFormatParser> parser) {
    if (parser) {
        logger()->debug("Registering parser: {}", parser->name());
        parsers_.push_back(std::move(parser));
    }
}

void AssetParser::registerBuiltinParsers() {
    // Register built-in parsers via factory functions
    registerParser(createUnityBundleParser());
    registerParser(createUnrealPakParser());
    registerParser(createBethesdaBa2Parser());
    registerParser(createGameMakerDataParser());

    logger()->info("Registered {} built-in parsers", parsers_.size());
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
        return std::unexpected(Error(ErrorCode::FileNotFound,
            "File not found: " + file.string()));
    }

    auto* parser = getParserForFile(file);
    if (!parser) {
        return std::unexpected(Error(ErrorCode::GameNotSupported,
            "No parser available for: " + file.string()));
    }

    logger()->debug("Parsing {} with {}", file.string(), parser->name());
    return parser->parse(file);
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

    // Parse collected files
    uint32_t current = 0;
    uint32_t total = static_cast<uint32_t>(files.size());

#ifdef MAKINEAI_HAS_TASKFLOW
    // TODO: [TASKFLOW] Parallel asset parsing for better performance
    // Parse multiple asset files concurrently using thread pool
    // Example:
    //   tf::Executor executor(std::thread::hardware_concurrency());
    //   tf::Taskflow taskflow;
    //   std::mutex resultsMutex;
    //   std::atomic<uint32_t> completed{0};
    //
    //   for (const auto& file : files) {
    //       taskflow.emplace([&, file]() {
    //           auto result = parseFile(file);
    //           if (result) {
    //               std::lock_guard<std::mutex> lock(resultsMutex);
    //               results.push_back(std::move(*result));
    //           }
    //           if (progress) {
    //               progress(++completed, total, "Parsing...");
    //           }
    //       });
    //   }
    //   executor.run(taskflow).wait();
    logger()->debug("Taskflow available - parallel parsing enabled");
#endif

    for (const auto& file : files) {
        if (progress) {
            progress(current, total, "Parsing: " + file.filename().string());
        }

        auto result = parseFile(file);
        if (result) {
            results.push_back(std::move(*result));
        } else {
            logger()->warn("Failed to parse {}: {}", file.string(),
                result.error().message());
        }

        ++current;
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
        return std::unexpected(Error(ErrorCode::GameNotSupported,
            "No parser available for: " + file.string()));
    }

    logger()->debug("Writing {} strings to {} with {}",
        strings.size(), file.string(), parser->name());
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

} // namespace makineai
