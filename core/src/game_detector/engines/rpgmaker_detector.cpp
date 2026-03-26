/**
 * @file rpgmaker_detector.cpp
 * @brief RPG Maker MV/MZ and VX Ace engine detectors
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

EngineDetectionResult detectRpgMakerMvMz(const fs::path& path, const GameSignatures& sig) {
    int confidence = 0;
    bool isMz = false;
    std::vector<std::string> details;

    if (sig.hasWwwFolder) {
        confidence += 30;
        details.push_back("www folder found");
    }

    if (sig.hasRpgCore) {
        confidence += 40;
        details.push_back("rpg_core.js found");
    }

    if (sig.hasJsFolder) {
        confidence += 15;
        details.push_back("js folder found");
    }

    if (sig.hasImgFolder) {
        confidence += 10;
        details.push_back("img folder found");
    }

    if (sig.hasNwJs) {
        confidence += 15;
        details.push_back("NW.js executable found");
    }

    // Try to differentiate MV from MZ
    if (sig.hasPackageJson || sig.hasWwwFolder) {
        try {
            // MZ has an 'effects' folder in www
            auto effectsDir = path / "www" / "effects";
            if (fs::exists(effectsDir) && fs::is_directory(effectsDir)) {
                isMz = true;
                confidence += 10;
                details.push_back("RPG Maker MZ detected (effects folder)");
            }

            // Check package.json for MZ indicators
            auto packageJson = path / "package.json";
            if (fs::exists(packageJson)) {
                std::ifstream file(packageJson);
                if (file) {
                    std::string content((std::istreambuf_iterator<char>(file)),
                                       std::istreambuf_iterator<char>());
                    if (content.find("MZ") != std::string::npos) {
                        isMz = true;
                        confidence += 10;
                        details.push_back("RPG Maker MZ in package.json");
                    }
                }
            }
        }
        catch (const std::exception&) {
            // Continue without MZ detection
        }
    }

    if (confidence < 30) {
        return EngineDetectionResult{};
    }

    std::string detailStr = joinDetails(details);

    EngineDetectionResult result;
    result.engine = GameEngine::RPGMaker_MV;  // Both MV and MZ use same enum for now
    result.confidence = std::min(confidence, 100);
    result.details = detailStr;
    result.metadata["isMz"] = isMz ? "true" : "false";

    return result;
}

EngineDetectionResult detectRpgMakerVxAce(const fs::path& /*path*/, const GameSignatures& sig) {
    int confidence = 0;
    std::vector<std::string> details;

    if (sig.hasRgss) {
        confidence += 45;
        details.push_back("RGSS DLL found");
    }

    if (sig.hasRvdata2) {
        confidence += 35;
        details.push_back(".rvdata2 files found");
    }

    if (sig.hasRgss3a) {
        confidence += 30;
        details.push_back(".rgss3a archive found");
    }

    if (sig.hasDataFolderRpg) {
        confidence += 15;
        details.push_back("Data folder found");
    }

    if (sig.hasGraphicsFolder) {
        confidence += 10;
        details.push_back("Graphics folder found");
    }

    if (sig.hasAudioFolder) {
        confidence += 5;
        details.push_back("Audio folder found");
    }

    if (confidence < 30) {
        return EngineDetectionResult{};
    }

    std::string detailStr = joinDetails(details);

    EngineDetectionResult result;
    result.engine = GameEngine::RPGMaker_VX;
    result.confidence = std::min(confidence, 100);
    result.details = detailStr;

    return result;
}

} // namespace makine::scanners
