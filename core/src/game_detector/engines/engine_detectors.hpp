/**
 * @file engine_detectors.hpp
 * @brief Free function declarations for engine detection
 * @copyright (c) 2026 MakineAI Team
 *
 * Each detector is a pure stateless function:
 *   - takes a game path + pre-scanned GameSignatures
 *   - returns an EngineDetectionResult
 *
 * These are called by GameDetector::detectEngineWithConfidence().
 */

#pragma once

#include "makineai/game_detector.hpp"

#include <memory>
#include <string>
#include <vector>

#include <filesystem>
#include <string>

namespace makineai::scanners {

namespace fs = std::filesystem;

// Unity (Mono and IL2CPP)
EngineDetectionResult detectUnity(const fs::path& path, const GameSignatures& sig);

// Unreal Engine 4/5
EngineDetectionResult detectUnreal(const fs::path& path, const GameSignatures& sig);

// Bethesda (Creation Engine: Skyrim, Fallout, etc.)
EngineDetectionResult detectBethesda(const fs::path& path, const GameSignatures& sig);

// Ren'Py visual novel engine
EngineDetectionResult detectRenpy(const fs::path& path, const GameSignatures& sig);

// RPG Maker MV / MZ
EngineDetectionResult detectRpgMakerMvMz(const fs::path& path, const GameSignatures& sig);

// RPG Maker VX Ace
EngineDetectionResult detectRpgMakerVxAce(const fs::path& path, const GameSignatures& sig);

// Godot engine
EngineDetectionResult detectGodot(const fs::path& path, const GameSignatures& sig);

// GameMaker Studio
EngineDetectionResult detectGameMaker(const fs::path& path, const GameSignatures& sig);

// Valve Source engine
EngineDetectionResult detectSource(const fs::path& path, const GameSignatures& sig);

// CryEngine / CRYENGINE
EngineDetectionResult detectCryEngine(const fs::path& path, const GameSignatures& sig);

// EA Frostbite engine
EngineDetectionResult detectFrostbite(const fs::path& path, const GameSignatures& sig);

// id Tech engine (Quake, Doom, etc.)
EngineDetectionResult detectIdTech(const fs::path& path, const GameSignatures& sig);

// Version helpers (used internally by Unity and Ren'Py detectors)
std::string readUnityVersion(const fs::path& gameDir);
std::string readRenpyVersion(const fs::path& gameDir);

} // namespace makineai::scanners
