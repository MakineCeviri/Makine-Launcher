/**
 * @file engine_detectors.hpp
 * @brief Free function declarations for engine detection
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Each detector is a pure stateless function:
 *   - takes a game path + pre-scanned GameSignatures
 *   - returns an EngineDetectionResult
 *
 * These are called by GameDetector::detectEngineWithConfidence().
 */

#pragma once

#include "makine/game_detector.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace makine::scanners {

namespace fs = std::filesystem;

/// Join a vector of detail strings with ", " separator
inline std::string joinDetails(const std::vector<std::string>& details) {
    std::string result;
    for (const auto& d : details) {
        if (!result.empty()) result += ", ";
        result += d;
    }
    return result;
}

// Unity (Mono and IL2CPP)
[[nodiscard]] EngineDetectionResult detectUnity(const fs::path& path, const GameSignatures& sig);

// Unreal Engine 4/5
[[nodiscard]] EngineDetectionResult detectUnreal(const fs::path& path, const GameSignatures& sig);

// Bethesda (Creation Engine: Skyrim, Fallout, etc.)
[[nodiscard]] EngineDetectionResult detectBethesda(const fs::path& path, const GameSignatures& sig);

// Ren'Py visual novel engine
[[nodiscard]] EngineDetectionResult detectRenpy(const fs::path& path, const GameSignatures& sig);

// RPG Maker MV / MZ
[[nodiscard]] EngineDetectionResult detectRpgMakerMvMz(const fs::path& path, const GameSignatures& sig);

// RPG Maker VX Ace
[[nodiscard]] EngineDetectionResult detectRpgMakerVxAce(const fs::path& path, const GameSignatures& sig);

// Godot engine
[[nodiscard]] EngineDetectionResult detectGodot(const fs::path& path, const GameSignatures& sig);

// GameMaker Studio
[[nodiscard]] EngineDetectionResult detectGameMaker(const fs::path& path, const GameSignatures& sig);

// Valve Source engine
[[nodiscard]] EngineDetectionResult detectSource(const fs::path& path, const GameSignatures& sig);

// CryEngine / CRYENGINE
[[nodiscard]] EngineDetectionResult detectCryEngine(const fs::path& path, const GameSignatures& sig);

// EA Frostbite engine
[[nodiscard]] EngineDetectionResult detectFrostbite(const fs::path& path, const GameSignatures& sig);

// id Tech engine (Quake, Doom, etc.)
[[nodiscard]] EngineDetectionResult detectIdTech(const fs::path& path, const GameSignatures& sig);

// Version helpers (used internally by Unity and Ren'Py detectors)
std::string readUnityVersion(const fs::path& gameDir);
std::string readRenpyVersion(const fs::path& gameDir);

} // namespace makine::scanners
