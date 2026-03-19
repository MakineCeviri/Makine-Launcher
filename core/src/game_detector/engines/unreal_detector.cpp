/**
 * @file unreal_detector.cpp
 * @brief Unreal Engine 4/5 detector
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "engine_detectors.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace makine::scanners {

EngineDetectionResult detectUnreal(const fs::path& /*path*/, const GameSignatures& sig) {
    int confidence = 0;
    std::vector<std::string> details;

    if (sig.hasPakFiles) {
        confidence += 30;
        details.push_back(".pak files found");
    }

    if (sig.hasEngineFolder) {
        confidence += 25;
        details.push_back("Engine folder found");
    }

    if (sig.hasContentFolder) {
        confidence += 20;
        details.push_back("Content folder found");
    }

    if (sig.hasUasset) {
        confidence += 25;
        details.push_back(".uasset files found");
    }

    if (sig.hasUmap) {
        confidence += 15;
        details.push_back(".umap files found");
    }

    if (sig.hasUeExecutable) {
        confidence += 30;
        details.push_back("UE executable found");
    }

    if (confidence < 30) {
        return EngineDetectionResult{};
    }

    std::string detailStr = joinDetails(details);

    EngineDetectionResult result;
    result.engine = GameEngine::Unreal;
    result.confidence = std::min(confidence, 100);
    result.details = detailStr;

    return result;
}

} // namespace makine::scanners
