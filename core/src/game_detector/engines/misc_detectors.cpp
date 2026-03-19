/**
 * @file misc_detectors.cpp
 * @brief Miscellaneous engine detectors: Bethesda, Godot, GameMaker, Source,
 *        CryEngine, Frostbite, id Tech
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "engine_detectors.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace makine::scanners {

namespace fs = std::filesystem;

EngineDetectionResult detectBethesda(const fs::path& /*path*/, const GameSignatures& sig) {
    int confidence = 0;
    std::vector<std::string> details;

    if (sig.hasBa2Files) {
        confidence += 50;
        details.push_back(".ba2 archives found");
    }

    if (sig.hasEsmEsp) {
        confidence += 40;
        details.push_back(".esm/.esp plugins found");
    }

    if (sig.hasStringsFiles) {
        confidence += 25;
        details.push_back(".strings files found");
    }

    if (confidence < 30) {
        return EngineDetectionResult{};
    }

    std::string detailStr = joinDetails(details);

    EngineDetectionResult result;
    result.engine = GameEngine::Bethesda;
    result.confidence = std::min(confidence, 100);
    result.details = detailStr;

    return result;
}

EngineDetectionResult detectGodot(const fs::path& path, const GameSignatures& sig) {
    int confidence = 0;
    std::vector<std::string> details;

    if (sig.hasPckFiles) {
        confidence += 40;
        details.push_back(".pck files found");
    }

    if (sig.hasGodotExe) {
        confidence += 45;
        details.push_back("Godot executable found");
    }

    // Check for project.godot file (strong indicator)
    try {
        auto projectFile = path / "project.godot";
        if (fs::exists(projectFile)) {
            confidence += 30;
            details.push_back("project.godot found");
        }

        // Check for export_presets.cfg
        auto presetsFile = path / "export_presets.cfg";
        if (fs::exists(presetsFile)) {
            confidence += 15;
            details.push_back("export_presets.cfg found");
        }
    }
    catch (const std::exception&) {
        // Continue with existing confidence
    }

    if (confidence < 30) {
        return EngineDetectionResult{};
    }

    std::string detailStr = joinDetails(details);

    EngineDetectionResult result;
    result.engine = GameEngine::Godot;
    result.confidence = std::min(confidence, 100);
    result.details = detailStr;

    return result;
}

EngineDetectionResult detectGameMaker(const fs::path& path, const GameSignatures& sig) {
    int confidence = 0;
    std::vector<std::string> details;

    if (sig.hasDataWin) {
        confidence += 50;
        details.push_back("data.win found");
    }

    if (sig.hasOptionsIni) {
        // Verify options.ini content for GameMaker signatures
        try {
            auto optionsPath = path / "options.ini";
            if (fs::exists(optionsPath)) {
                std::ifstream file(optionsPath);
                if (file) {
                    std::string content((std::istreambuf_iterator<char>(file)),
                                       std::istreambuf_iterator<char>());

                    // Convert to lowercase for case-insensitive search
                    std::string lowerContent = content;
                    std::transform(lowerContent.begin(), lowerContent.end(),
                        lowerContent.begin(), ::tolower);

                    if (lowerContent.find("gamemaker") != std::string::npos ||
                        content.find("YoYoGames") != std::string::npos) {
                        confidence += 40;
                        details.push_back("GameMaker options.ini confirmed");
                    } else {
                        confidence += 15;
                        details.push_back("options.ini found (unverified)");
                    }
                }
            }
        }
        catch (const std::exception&) {
            confidence += 15;
            details.push_back("options.ini found");
        }
    }

    if (confidence < 30) {
        return EngineDetectionResult{};
    }

    std::string detailStr = joinDetails(details);

    EngineDetectionResult result;
    result.engine = GameEngine::GameMaker;
    result.confidence = std::min(confidence, 100);
    result.details = detailStr;

    return result;
}

EngineDetectionResult detectSource(const fs::path& /*path*/, const GameSignatures& sig) {
    int confidence = 0;
    std::vector<std::string> details;

    if (sig.hasVpkFiles) {
        confidence += 35;
        details.push_back(".vpk files found");
    }

    if (sig.hasBspFiles) {
        confidence += 25;
        details.push_back(".bsp map files found");
    }

    if (sig.hasSourceExe) {
        confidence += 30;
        details.push_back("Source executable found");
    }

    if (sig.hasSourceGameFolder) {
        confidence += 20;
        details.push_back("Source game folder found");
    }

    if (confidence < 30) {
        return EngineDetectionResult{};
    }

    std::string detailStr = joinDetails(details);

    EngineDetectionResult result;
    result.engine = GameEngine::Source;
    result.confidence = std::min(confidence, 100);
    result.details = detailStr;

    return result;
}

EngineDetectionResult detectCryEngine(const fs::path& /*path*/, const GameSignatures& sig) {
    int confidence = 0;
    std::vector<std::string> details;

    if (sig.hasCrySystem) {
        confidence += 55;
        details.push_back("CrySystem.dll found");
    }

    if (sig.hasCryPak) {
        confidence += 35;
        details.push_back("CryEngine .pak files found");
    }

    if (confidence < 30) {
        return EngineDetectionResult{};
    }

    std::string detailStr = joinDetails(details);

    EngineDetectionResult result;
    result.engine = GameEngine::CryEngine;
    result.confidence = std::min(confidence, 100);
    result.details = detailStr;

    return result;
}

EngineDetectionResult detectFrostbite(const fs::path& path, const GameSignatures& sig) {
    int confidence = 0;
    std::vector<std::string> details;

    // Check Frostbite signature files
    if (sig.hasFrostbiteFiles) {
        confidence += 50;
        details.push_back("Frostbite files found (.sb/.toc/cascat.cat)");
    }

    // Additional check for Frostbite-specific structures
    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.is_regular_file()) {
                auto name = entry.path().filename().string();
                std::transform(name.begin(), name.end(), name.begin(), ::tolower);

                // Frostbite games have specific file patterns
                if (name.find("frosty") != std::string::npos) {
                    confidence += 25;
                    details.push_back("Frosty mod tool detected");
                }

                // Check for superbundle files
                if (name.ends_with(".sb")) {
                    confidence += 20;
                    details.push_back("Superbundle files found");
                    break;
                }

                // Check for table of contents
                if (name.ends_with(".toc")) {
                    confidence += 15;
                    details.push_back("TOC files found");
                    break;
                }
            }
        }

        // Check Data folder for cas/cat files
        auto dataDir = path / "Data";
        if (fs::exists(dataDir)) {
            for (const auto& entry : fs::directory_iterator(dataDir)) {
                auto name = entry.path().filename().string();
                std::transform(name.begin(), name.end(), name.begin(), ::tolower);
                if (name.starts_with("cas_") || name == "cascat.cat") {
                    confidence += 30;
                    details.push_back("CAS/CAT files found");
                    break;
                }
            }
        }
    }
    catch (const std::exception&) {
        // Continue with existing confidence
    }

    if (confidence < 30) {
        return EngineDetectionResult{};
    }

    std::string detailStr = joinDetails(details);

    EngineDetectionResult result;
    result.engine = GameEngine::Frostbite;
    result.confidence = std::min(confidence, 100);
    result.details = detailStr;

    return result;
}

EngineDetectionResult detectIdTech(const fs::path& path, const GameSignatures& sig) {
    int confidence = 0;
    std::vector<std::string> details;

    // Check for pk3/pk4 files (Quake/Doom style archives)
    if (sig.hasPkFiles) {
        confidence += 45;
        details.push_back(".pk3/.pk4 files found");
    }

    // Additional id Tech specific checks
    try {
        // Look for 'base' folder (common in id Tech games)
        auto baseDir = path / "base";
        if (fs::exists(baseDir) && fs::is_directory(baseDir)) {
            confidence += 25;
            details.push_back("'base' folder found");

            // Check for pak files inside base
            for (const auto& entry : fs::directory_iterator(baseDir)) {
                auto ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".pk3" || ext == ".pk4" || ext == ".pak") {
                    confidence += 15;
                    details.push_back("PAK archives in base folder");
                    break;
                }
            }
        }

        // Check for id Tech signatures in root
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.is_regular_file()) {
                auto name = entry.path().filename().string();
                std::transform(name.begin(), name.end(), name.begin(), ::tolower);

                // id Tech specific executables
                if (name.find("quake") != std::string::npos ||
                    name.find("doom") != std::string::npos ||
                    name.find("wolfenstein") != std::string::npos ||
                    name.find("rage") != std::string::npos) {
                    confidence += 20;
                    details.push_back("id Tech game executable found");
                    break;
                }

                // Check for .qvm files (Quake VM)
                if (name.ends_with(".qvm")) {
                    confidence += 25;
                    details.push_back("QVM files found");
                    break;
                }
            }
        }
    }
    catch (const std::exception&) {
        // Continue with existing confidence
    }

    if (confidence < 30) {
        return EngineDetectionResult{};
    }

    std::string detailStr = joinDetails(details);

    EngineDetectionResult result;
    result.engine = GameEngine::IdTech;
    result.confidence = std::min(confidence, 100);
    result.details = detailStr;

    return result;
}

} // namespace makine::scanners
