/**
 * @file renpy_detector.cpp
 * @brief Ren'Py visual novel engine detector
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

std::string readRenpyVersion(const fs::path& gameDir) {
    try {
        auto initPy = gameDir / "renpy" / "__init__.py";
        if (fs::exists(initPy)) {
            std::ifstream file(initPy);
            if (file) {
                std::string content((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());

                // Search for version_tuple = (major, minor, patch)
                // Manual parse — avoids <regex> header overhead
                auto tuplePos = content.find("version_tuple");
                if (tuplePos != std::string::npos) {
                    auto parenPos = content.find('(', tuplePos);
                    if (parenPos != std::string::npos) {
                        // Extract three comma-separated numbers
                        auto extractNum = [&](size_t& pos) -> std::string {
                            while (pos < content.size() && (content[pos] < '0' || content[pos] > '9')) ++pos;
                            size_t numStart = pos;
                            while (pos < content.size() && content[pos] >= '0' && content[pos] <= '9') ++pos;
                            return content.substr(numStart, pos - numStart);
                        };

                        size_t pos = parenPos + 1;
                        std::string major = extractNum(pos);
                        std::string minor = extractNum(pos);
                        std::string patch = extractNum(pos);

                        if (!major.empty() && !minor.empty() && !patch.empty()) {
                            return major + "." + minor + "." + patch;
                        }
                    }
                }
            }
        }
    }
    catch (const std::exception&) {
        // Version could not be read
    }
    return "";
}

EngineDetectionResult detectRenpy(const fs::path& path, const GameSignatures& sig) {
    int confidence = 0;
    std::string version;
    std::vector<std::string> details;

    if (sig.hasRenpyFolder) {
        confidence += 40;
        details.push_back("renpy folder found");
    }

    if (sig.hasRpaFiles) {
        confidence += 35;
        details.push_back(".rpa archives found");
    }

    if (sig.hasRpycFiles) {
        confidence += 25;
        details.push_back(".rpyc files found");
    }

    if (sig.hasRenpyExe) {
        confidence += 30;
        details.push_back("renpy.exe found");
    }

    if (sig.hasGameFolder && sig.hasRenpyFolder) {
        confidence += 15;
        details.push_back("Standard Ren'Py structure");
    }

    // Try to get version
    version = readRenpyVersion(path);
    if (!version.empty()) {
        confidence += 10;
        details.push_back("Version: " + version);
    }

    if (confidence < 30) {
        return EngineDetectionResult{};
    }

    std::string detailStr = joinDetails(details);

    EngineDetectionResult result;
    result.engine = GameEngine::RenPy;
    result.confidence = std::min(confidence, 100);
    result.version = version;
    result.details = detailStr;

    return result;
}

} // namespace makine::scanners
