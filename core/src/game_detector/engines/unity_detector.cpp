/**
 * @file unity_detector.cpp
 * @brief Unity engine detector (Mono and IL2CPP)
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "engine_detectors.hpp"

#include "makine/mio_utils.hpp"
#include "makine/logging.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace makine::scanners {

namespace fs = std::filesystem;

std::string readUnityVersion(const fs::path& gameDir) {
    try {
        // Find *_Data folder
        for (const auto& entry : fs::directory_iterator(gameDir)) {
            if (entry.is_directory()) {
                auto name = entry.path().filename().string();
                std::string lowerName = name;
                std::transform(lowerName.begin(), lowerName.end(),
                               lowerName.begin(), ::tolower);

                if (lowerName.ends_with("_data")) {
                    auto ggmPath = entry.path() / "globalgamemanagers";
                    if (fs::exists(ggmPath)) {
                        // Only read first 256 bytes where version info is located
                        constexpr size_t READ_SIZE = 256;

                        auto mappedResult = mio_utils::mapFile(ggmPath, 0, READ_SIZE);
                        if (!mappedResult) {
                            MAKINE_LOG_TRACE(log::DETECTOR,
                                "Failed to map globalgamemanagers: {}",
                                mappedResult.error().message());
                            break;
                        }

                        auto view = mappedResult->view();
                        std::string content(view.stringView());

                        // Search for version pattern (e.g., "2021.3.14f1", "2022.1.0b3")
                        // Manual scan — avoids <regex> header overhead
                        for (size_t i = 0; i < content.size(); ++i) {
                            char c = content[i];
                            if (c < '0' || c > '9') continue;

                            // Try to parse digits.digits.digits[letter][digits]
                            size_t start = i;
                            // First number
                            while (i < content.size() && content[i] >= '0' && content[i] <= '9') ++i;
                            if (i >= content.size() || content[i] != '.') continue;
                            ++i; // skip '.'
                            // Second number
                            if (i >= content.size() || content[i] < '0' || content[i] > '9') continue;
                            while (i < content.size() && content[i] >= '0' && content[i] <= '9') ++i;
                            if (i >= content.size() || content[i] != '.') continue;
                            ++i; // skip '.'
                            // Third number
                            if (i >= content.size() || content[i] < '0' || content[i] > '9') continue;
                            while (i < content.size() && content[i] >= '0' && content[i] <= '9') ++i;
                            // Optional letter suffix (f1, b3, etc.)
                            if (i < content.size() && content[i] >= 'a' && content[i] <= 'z') {
                                ++i;
                                while (i < content.size() && content[i] >= '0' && content[i] <= '9') ++i;
                            }

                            std::string ver = content.substr(start, i - start);
                            // Sanity: Unity versions start with year >= 3
                            if (ver.size() >= 5) {
                                MAKINE_LOG_TRACE(log::DETECTOR,
                                    "Unity version detected: {}", ver);
                                return ver;
                            }
                        }
                    }
                    break;
                }
            }
        }
    }
    catch (const std::exception& e) {
        MAKINE_LOG_TRACE(log::DETECTOR, "Failed to read Unity version: {}", e.what());
    }
    return "";
}

EngineDetectionResult detectUnity(const fs::path& path, const GameSignatures& sig) {
    int confidence = 0;
    bool isIl2cpp = false;
    std::string version;
    std::vector<std::string> details;

    // IL2CPP detection (highest priority)
    if (sig.hasGameAssembly) {
        confidence += 50;
        isIl2cpp = true;
        details.push_back("GameAssembly.dll found");
    }

    if (sig.hasIl2cppData) {
        confidence += 30;
        isIl2cpp = true;
        details.push_back("il2cpp_data folder found");
    }

    // Mono detection
    if (sig.hasUnityEngine) {
        confidence += 40;
        details.push_back("UnityEngine.dll found");
    }

    if (sig.hasAssemblyCSharp) {
        confidence += 30;
        details.push_back("Assembly-CSharp.dll found");
    }

    if (sig.hasManagedFolder && !isIl2cpp) {
        confidence += 20;
        details.push_back("Managed folder found");
    }

    if (sig.hasMonoFolder) {
        confidence += 15;
        details.push_back("Mono folder found");
    }

    // General Unity indicators
    if (sig.hasGlobalGameManagers) {
        confidence += 25;
        details.push_back("globalgamemanagers found");
        version = readUnityVersion(path);
    }

    if (sig.hasResourcesAssets) {
        confidence += 15;
        details.push_back("resources.assets found");
    }

    if (sig.hasDataFolder) {
        confidence += 10;
        details.push_back("*_Data folder found");
    }

    if (sig.hasUnity3d) {
        confidence += 20;
        details.push_back(".unity3d files found");
    }

    if (confidence < 30) {
        return EngineDetectionResult{};
    }

    // Build details string
    std::string detailStr = joinDetails(details);

    EngineDetectionResult result;
    result.engine = isIl2cpp ? GameEngine::Unity_IL2CPP : GameEngine::Unity_Mono;
    result.confidence = std::min(confidence, 100);
    result.version = version;
    result.details = detailStr;
    result.metadata["isIl2cpp"] = isIl2cpp ? "true" : "false";
    result.metadata["hasManagedDlls"] = sig.hasAssemblyCSharp ? "true" : "false";

    return result;
}

} // namespace makine::scanners
