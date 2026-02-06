/**
 * @file rpgmaker_handler.cpp
 * @brief RPG Maker game handler implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/handlers/rpgmaker_handler.hpp"
#include "makineai/json_utils.hpp"
#include "makineai/logging.hpp"
#include "makineai/metrics.hpp"
#include "makineai/audit.hpp"

#include <fstream>
#include <sstream>
#include <regex>
#include <nlohmann/json.hpp>

namespace makineai {

// Note: Using nlohmann::json directly to avoid conflict with makineai::json namespace

// ========== Game Detection ==========

bool RpgMakerHandler::canHandleGame(const fs::path& gameDir) {
    // RPG Maker MV/MZ markers
    const std::vector<std::string> mvMzMarkers = {
        "www/data", "data", "js/plugins.js", "js/rpg_core.js"
    };

    for (const auto& marker : mvMzMarkers) {
        auto markerPath = gameDir / marker;
        if (fs::exists(markerPath)) {
            // Check for System.json
            auto dataDir = findDataDir(gameDir);
            if (dataDir) {
                auto systemFile = *dataDir / "System.json";
                if (fs::exists(systemFile)) {
                    detectedVersion_ = RpgMakerVersion::MVMZ;
                    return true;
                }
            }
        }
    }

    // VX Ace markers
    if (fs::exists(gameDir / "Data/System.rvdata2") ||
        fs::exists(gameDir / "RGSS301.dll") ||
        fs::exists(gameDir / "RGSS300.dll")) {
        detectedVersion_ = RpgMakerVersion::VXAce;
        return true;
    }

    // VX markers
    if (fs::exists(gameDir / "Data/System.rvdata") ||
        fs::exists(gameDir / "RGSS202E.dll")) {
        detectedVersion_ = RpgMakerVersion::VX;
        return true;
    }

    // XP markers
    if (fs::exists(gameDir / "Data/System.rxdata") ||
        fs::exists(gameDir / "RGSS104E.dll")) {
        detectedVersion_ = RpgMakerVersion::XP;
        return true;
    }

    return false;
}

std::optional<fs::path> RpgMakerHandler::findDataDir(const fs::path& gameDir) {
    std::vector<fs::path> candidates = {
        gameDir / "www/data",
        gameDir / "data",
        gameDir / "Data"
    };

    for (const auto& dir : candidates) {
        if (fs::exists(dir) && fs::is_directory(dir)) {
            return dir;
        }
    }
    return std::nullopt;
}

GameFileType RpgMakerHandler::getFileType(const std::string& filename) {
    std::string lower = filename;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.find("map") == 0) return GameFileType::Data;
    if (lower == "system.json") return GameFileType::Config;
    if (lower == "plugins.json") return GameFileType::Config;
    if (lower == "commonevents.json") return GameFileType::Script;
    if (lower == "mapinfos.json") return GameFileType::Data;

    // Data files
    const std::vector<std::string> dataFiles = {
        "actors.json", "classes.json", "skills.json", "items.json",
        "weapons.json", "armors.json", "enemies.json", "troops.json",
        "states.json", "animations.json", "tilesets.json"
    };

    for (const auto& df : dataFiles) {
        if (lower == df) return GameFileType::Data;
    }

    return GameFileType::Data;
}

// ========== File Discovery ==========

Result<std::vector<GameFile>> RpgMakerHandler::findGameFiles(const fs::path& gameDir) {
    std::vector<GameFile> files;

    // MV/MZ JSON files
    if (detectedVersion_ == RpgMakerVersion::MVMZ) {
        auto dataDir = findDataDir(gameDir);
        if (dataDir) {
            for (const auto& entry : fs::directory_iterator(*dataDir)) {
                if (!entry.is_regular_file()) continue;

                auto ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                if (ext == ".json") {
                    auto relativePath = fs::relative(entry.path(), gameDir).string();
                    auto filename = entry.path().filename().string();

                    files.push_back(GameFile{
                        .path = entry.path().string(),
                        .relativePath = relativePath,
                        .type = getFileType(filename),
                        .size = fs::file_size(entry.path()),
                        .encoding = "utf-8"
                    });
                }
            }
        }
    }
    // VX Ace binary files
    else if (detectedVersion_ == RpgMakerVersion::VXAce) {
        auto dataDir = gameDir / "Data";
        if (fs::exists(dataDir)) {
            for (const auto& entry : fs::directory_iterator(dataDir)) {
                if (!entry.is_regular_file()) continue;

                auto ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                if (ext == ".rvdata2") {
                    auto relativePath = fs::relative(entry.path(), gameDir).string();

                    files.push_back(GameFile{
                        .path = entry.path().string(),
                        .relativePath = relativePath,
                        .type = GameFileType::Data,
                        .size = fs::file_size(entry.path())
                    });
                }
            }
        }
    }

    MAKINEAI_LOG_DEBUG(log::HANDLER, "RPG Maker: Found {} game files", files.size());
    return files;
}

// ========== String Extraction ==========

Result<ExtractionResult> RpgMakerHandler::extractStrings(
    const fs::path& gameDir,
    const ExtractionOptions& options
) {
    MAKINEAI_LOG_INFO(log::HANDLER, "RPG Maker: Starting string extraction from {}", gameDir.string());
    auto timer = metrics().timer("rpgmaker_extract_strings");

    std::vector<TranslationEntry> entries;
    std::vector<ExtractionError> errors;
    std::vector<GameFile> processedFiles;
    int totalStrings = 0;
    int skippedStrings = 0;
    int filesProcessed = 0;

    // VX Ace Ruby Marshal not supported in C++ version yet
    if (detectedVersion_ == RpgMakerVersion::VXAce ||
        detectedVersion_ == RpgMakerVersion::VX ||
        detectedVersion_ == RpgMakerVersion::XP) {
        MAKINEAI_LOG_WARN(log::HANDLER, "RPG Maker: VX/VX Ace/XP Ruby Marshal format not yet supported");
        errors.push_back(ExtractionError{
            .file = gameDir.string(),
            .message = "RPG Maker VX/VX Ace/XP Ruby Marshal format not yet supported. Use MV/MZ JSON format.",
            .severity = ExtractionSeverity::Fatal
        });

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - std::chrono::steady_clock::now() + timer.elapsed());

        return ExtractionResult{
            .entries = entries,
            .errors = errors,
            .processedFiles = processedFiles,
            .totalStrings = 0,
            .skippedStrings = 0,
            .duration = duration
        };
    }

    // MV/MZ JSON extraction
    auto gameFilesResult = findGameFiles(gameDir);
    if (!gameFilesResult) {
        MAKINEAI_LOG_ERROR(log::HANDLER, "RPG Maker: Failed to find game files: {}", gameFilesResult.error().message());
        return std::unexpected(gameFilesResult.error());
    }

    MAKINEAI_LOG_DEBUG(log::HANDLER, "RPG Maker: Found {} potential data files", gameFilesResult->size());

    for (const auto& gameFile : *gameFilesResult) {
        // Skip non-JSON files
        auto ext = fs::path(gameFile.path).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".json") continue;

        // File filter
        if (!options.includeFiles.empty()) {
            bool found = false;
            for (const auto& f : options.includeFiles) {
                if (gameFile.relativePath.find(f) != std::string::npos) {
                    found = true;
                    break;
                }
            }
            if (!found) continue;
        }

        if (!options.excludeFiles.empty()) {
            bool excluded = false;
            for (const auto& f : options.excludeFiles) {
                if (gameFile.relativePath.find(f) != std::string::npos) {
                    excluded = true;
                    break;
                }
            }
            if (excluded) continue;
        }

        MAKINEAI_LOG_DEBUG(log::HANDLER, "RPG Maker: Processing file: {}", gameFile.relativePath);

        try {
            auto batch = extractFromJsonFile(fs::path(gameFile.path), gameFile, options);

            totalStrings += batch.total;
            skippedStrings += batch.skipped;
            entries.insert(entries.end(), batch.entries.begin(), batch.entries.end());

            processedFiles.push_back(GameFile{
                .path = gameFile.path,
                .relativePath = gameFile.relativePath,
                .type = gameFile.type,
                .size = gameFile.size,
                .stringCount = static_cast<int>(batch.entries.size())
            });
            filesProcessed++;

            MAKINEAI_LOG_DEBUG(log::HANDLER, "RPG Maker: Extracted {} strings from {}",
                batch.entries.size(), gameFile.relativePath);
        } catch (const std::exception& e) {
            MAKINEAI_LOG_ERROR(log::HANDLER, "RPG Maker: JSON parse error in {}: {}",
                gameFile.relativePath, e.what());
            errors.push_back(ExtractionError{
                .file = gameFile.relativePath,
                .message = std::string("JSON parse error: ") + e.what(),
                .severity = ExtractionSeverity::Error
            });
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - std::chrono::steady_clock::now() + timer.elapsed());

    // Record metrics
    metrics().increment("rpgmaker_files_processed", filesProcessed);
    metrics().increment("rpgmaker_strings_extracted", entries.size());

    MAKINEAI_LOG_INFO(log::HANDLER, "RPG Maker: Extraction complete - {} strings from {} files in {}ms",
                       entries.size(), filesProcessed, duration.count());

    return ExtractionResult{
        .entries = entries,
        .errors = errors,
        .processedFiles = processedFiles,
        .totalStrings = totalStrings,
        .skippedStrings = skippedStrings,
        .duration = duration
    };
}

RpgMakerHandler::ExtractionBatch RpgMakerHandler::extractFromJsonFile(
    const fs::path& file,
    const GameFile& gameFile,
    const ExtractionOptions& options
) {
    MAKINEAI_TIMED_SCOPE(log::HANDLER, "RPGMaker extractFromJsonFile");

    ExtractionBatch batch;

    // Use simdjson for fast parsing, then convert to nlohmann for processing
    // This gives us fast parsing while keeping existing extraction logic
    auto docResult = makineai::json::parseFile(file);
    if (!docResult) {
        MAKINEAI_LOG_WARN(log::HANDLER, "Failed to parse RPG Maker JSON {}: {}",
                         file.string(), docResult.error().message());
        return batch;
    }

    // Convert to nlohmann::json for compatibility with existing extraction code
    nlohmann::json data = docResult->toNlohmann();
    std::string fileName = file.filename().string();

    MAKINEAI_LOG_TRACE(log::HANDLER, "Extracting strings from {} (using {})",
                       fileName, makineai::json::backendInfo());

    extractFromJson(data, gameFile.relativePath, fileName, options, batch);

    return batch;
}

void RpgMakerHandler::extractFromJson(
    const nlohmann::json& data,
    const std::string& filePath,
    const std::string& fileName,
    const ExtractionOptions& options,
    ExtractionBatch& batch
) {
    std::string basePath = fileName;
    if (basePath.size() > 5 && basePath.substr(basePath.size() - 5) == ".json") {
        basePath = basePath.substr(0, basePath.size() - 5);
    }

    // Handle arrays (Actors.json, Items.json, etc.)
    if (data.is_array()) {
        for (size_t i = 0; i < data.size(); i++) {
            const auto& item = data[i];
            if (item.is_null()) continue;

            std::string itemPath = basePath + "[" + std::to_string(i) + "]";

            // Extract standard fields
            if (item.contains("id")) {
                int id = item["id"].get<int>();

                // Name
                if (item.contains("name") && item["name"].is_string()) {
                    std::string name = item["name"].get<std::string>();
                    if (!name.empty()) {
                        batch.total++;
                        if (isValidString(name, options)) {
                            batch.entries.push_back(TranslationEntry{
                                .filePath = filePath,
                                .entryKey = basePath + "_name_" + std::to_string(id),
                                .sourceText = name,
                                .context = guessContext("name", fileName),
                                .category = guessCategory("name", fileName)
                            });
                        } else {
                            batch.skipped++;
                        }
                    }
                }

                // Description
                if (item.contains("description") && item["description"].is_string()) {
                    std::string desc = item["description"].get<std::string>();
                    if (!desc.empty()) {
                        batch.total++;
                        if (isValidString(desc, options)) {
                            batch.entries.push_back(TranslationEntry{
                                .filePath = filePath,
                                .entryKey = basePath + "_description_" + std::to_string(id),
                                .sourceText = desc,
                                .context = "Description",
                                .category = EntryCategory::UI
                            });
                        } else {
                            batch.skipped++;
                        }
                    }
                }

                // Profile (Actors)
                if (item.contains("profile") && item["profile"].is_string()) {
                    std::string profile = item["profile"].get<std::string>();
                    if (!profile.empty()) {
                        batch.total++;
                        if (isValidString(profile, options)) {
                            batch.entries.push_back(TranslationEntry{
                                .filePath = filePath,
                                .entryKey = basePath + "_profile_" + std::to_string(id),
                                .sourceText = profile,
                                .context = "Character profile",
                                .category = EntryCategory::UI
                            });
                        } else {
                            batch.skipped++;
                        }
                    }
                }

                // Nickname (Actors)
                if (item.contains("nickname") && item["nickname"].is_string()) {
                    std::string nickname = item["nickname"].get<std::string>();
                    if (!nickname.empty()) {
                        batch.total++;
                        if (isValidString(nickname, options)) {
                            batch.entries.push_back(TranslationEntry{
                                .filePath = filePath,
                                .entryKey = basePath + "_nickname_" + std::to_string(id),
                                .sourceText = nickname,
                                .context = "Nickname",
                                .category = EntryCategory::UI
                            });
                        } else {
                            batch.skipped++;
                        }
                    }
                }

                // Message 1-4 (Skills)
                for (int m = 1; m <= 4; m++) {
                    std::string msgKey = "message" + std::to_string(m);
                    if (item.contains(msgKey) && item[msgKey].is_string()) {
                        std::string msg = item[msgKey].get<std::string>();
                        if (!msg.empty()) {
                            batch.total++;
                            if (isValidString(msg, options)) {
                                batch.entries.push_back(TranslationEntry{
                                    .filePath = filePath,
                                    .entryKey = basePath + "_" + msgKey + "_" + std::to_string(id),
                                    .sourceText = msg,
                                    .context = "Skill message",
                                    .category = EntryCategory::Skill
                                });
                            } else {
                                batch.skipped++;
                            }
                        }
                    }
                }

                // Note (metadata, sometimes contains display text)
                if (item.contains("note") && item["note"].is_string()) {
                    std::string note = item["note"].get<std::string>();
                    if (!note.empty() && containsDisplayText(note)) {
                        batch.total++;
                        if (isValidString(note, options)) {
                            batch.entries.push_back(TranslationEntry{
                                .filePath = filePath,
                                .entryKey = basePath + "_note_" + std::to_string(id),
                                .sourceText = note,
                                .context = "Note/Metadata",
                                .category = EntryCategory::Other
                            });
                        } else {
                            batch.skipped++;
                        }
                    }
                }
            }

            // Event list in item
            if (item.contains("list") && item["list"].is_array()) {
                if (isEventList(item["list"])) {
                    extractFromEventList(item["list"], filePath, itemPath, options, batch);
                }
            }

            // Pages (for events in map files)
            if (item.contains("pages") && item["pages"].is_array()) {
                const auto& pages = item["pages"];
                for (size_t p = 0; p < pages.size(); p++) {
                    const auto& page = pages[p];
                    if (page.contains("list") && page["list"].is_array()) {
                        if (isEventList(page["list"])) {
                            std::string pagePath = itemPath + "_page_" + std::to_string(p);
                            extractFromEventList(page["list"], filePath, pagePath, options, batch);
                        }
                    }
                }
            }
        }
    }
    // Handle objects (System.json, Map files)
    else if (data.is_object()) {
        // System.json special fields
        if (data.contains("gameTitle") && data["gameTitle"].is_string()) {
            std::string title = data["gameTitle"].get<std::string>();
            if (!title.empty()) {
                batch.total++;
                if (isValidString(title, options)) {
                    batch.entries.push_back(TranslationEntry{
                        .filePath = filePath,
                        .entryKey = basePath + "_gameTitle",
                        .sourceText = title,
                        .context = "Game title",
                        .category = EntryCategory::UI
                    });
                } else {
                    batch.skipped++;
                }
            }
        }

        if (data.contains("currencyUnit") && data["currencyUnit"].is_string()) {
            std::string currency = data["currencyUnit"].get<std::string>();
            if (!currency.empty()) {
                batch.total++;
                if (isValidString(currency, options)) {
                    batch.entries.push_back(TranslationEntry{
                        .filePath = filePath,
                        .entryKey = basePath + "_currencyUnit",
                        .sourceText = currency,
                        .context = "Currency unit",
                        .category = EntryCategory::Item
                    });
                } else {
                    batch.skipped++;
                }
            }
        }

        // Terms (System.json)
        if (data.contains("terms") && data["terms"].is_object()) {
            extractTerms(data["terms"], filePath, basePath, options, batch);
        }

        // Map display name
        if (data.contains("displayName") && data["displayName"].is_string()) {
            std::string displayName = data["displayName"].get<std::string>();
            if (!displayName.empty()) {
                batch.total++;
                if (isValidString(displayName, options)) {
                    batch.entries.push_back(TranslationEntry{
                        .filePath = filePath,
                        .entryKey = basePath + "_displayName",
                        .sourceText = displayName,
                        .context = "Map name",
                        .category = EntryCategory::Narration
                    });
                } else {
                    batch.skipped++;
                }
            }
        }

        // Events in map
        if (data.contains("events") && data["events"].is_array()) {
            const auto& events = data["events"];
            for (size_t i = 0; i < events.size(); i++) {
                const auto& event = events[i];
                if (event.is_null()) continue;

                std::string eventPath = basePath + "_event_" + std::to_string(i);

                // Event name
                if (event.contains("name") && event["name"].is_string()) {
                    std::string eventName = event["name"].get<std::string>();
                    if (!eventName.empty() && containsDisplayText(eventName)) {
                        batch.total++;
                        if (isValidString(eventName, options)) {
                            batch.entries.push_back(TranslationEntry{
                                .filePath = filePath,
                                .entryKey = eventPath + "_name",
                                .sourceText = eventName,
                                .context = "Event name",
                                .category = EntryCategory::Dialog
                            });
                        } else {
                            batch.skipped++;
                        }
                    }
                }

                // Pages
                if (event.contains("pages") && event["pages"].is_array()) {
                    const auto& pages = event["pages"];
                    for (size_t p = 0; p < pages.size(); p++) {
                        const auto& page = pages[p];
                        if (page.contains("list") && page["list"].is_array()) {
                            if (isEventList(page["list"])) {
                                std::string pagePath = eventPath + "_page_" + std::to_string(p);
                                extractFromEventList(page["list"], filePath, pagePath, options, batch);
                            }
                        }
                    }
                }
            }
        }
    }
}

void RpgMakerHandler::extractFromEventList(
    const nlohmann::json& events,
    const std::string& filePath,
    const std::string& basePath,
    const ExtractionOptions& options,
    ExtractionBatch& batch
) {
    std::string textBuffer;
    std::string currentContext = "Dialog";
    int eventIndex = 0;

    for (size_t i = 0; i < events.size(); i++) {
        const auto& event = events[i];
        if (!event.is_object()) continue;

        int code = event.value("code", 0);
        const auto& parameters = event.value("parameters", nlohmann::json::array());

        // Show Text body (401)
        if (code == EventCode::ShowTextBody && !parameters.empty() && parameters[0].is_string()) {
            std::string text = parameters[0].get<std::string>();
            if (!text.empty()) {
                if (!textBuffer.empty()) textBuffer += "\n";
                textBuffer += text;
            }
        }
        // Show Text header (101)
        else if (code == EventCode::ShowTextHeader) {
            // Save previous text
            if (!textBuffer.empty()) {
                batch.total++;
                if (isValidString(textBuffer, options)) {
                    batch.entries.push_back(TranslationEntry{
                        .filePath = filePath,
                        .entryKey = basePath + "_event_" + std::to_string(eventIndex) + "_text",
                        .sourceText = textBuffer,
                        .context = currentContext,
                        .category = EntryCategory::Dialog
                    });
                } else {
                    batch.skipped++;
                }
                textBuffer.clear();
                eventIndex++;
            }

            // Speaker info
            if (parameters.size() >= 5 && parameters[0].is_string()) {
                std::string faceName = parameters[0].get<std::string>();
                currentContext = faceName.empty() ? "Dialog" : "Speaker: " + faceName;
            }
        }
        // Show Choices (102)
        else if (code == EventCode::ShowChoices && !parameters.empty()) {
            // Save previous text
            if (!textBuffer.empty()) {
                batch.total++;
                if (isValidString(textBuffer, options)) {
                    batch.entries.push_back(TranslationEntry{
                        .filePath = filePath,
                        .entryKey = basePath + "_event_" + std::to_string(eventIndex) + "_text",
                        .sourceText = textBuffer,
                        .context = currentContext,
                        .category = EntryCategory::Dialog
                    });
                } else {
                    batch.skipped++;
                }
                textBuffer.clear();
                eventIndex++;
            }

            // Extract choices
            if (parameters[0].is_array()) {
                const auto& choices = parameters[0];
                for (size_t j = 0; j < choices.size(); j++) {
                    if (choices[j].is_string()) {
                        std::string choice = choices[j].get<std::string>();
                        if (!choice.empty()) {
                            batch.total++;
                            if (isValidString(choice, options)) {
                                batch.entries.push_back(TranslationEntry{
                                    .filePath = filePath,
                                    .entryKey = basePath + "_event_" + std::to_string(eventIndex) + "_choice_" + std::to_string(j),
                                    .sourceText = choice,
                                    .context = "Choice",
                                    .category = EntryCategory::Dialog
                                });
                            } else {
                                batch.skipped++;
                            }
                        }
                    }
                }
                eventIndex++;
            }
        }
        // Scroll Text body (405)
        else if (code == EventCode::ScrollTextBody && !parameters.empty() && parameters[0].is_string()) {
            std::string text = parameters[0].get<std::string>();
            if (!text.empty()) {
                if (!textBuffer.empty()) textBuffer += "\n";
                textBuffer += text;
            }
        }
        // Scroll Text header (105)
        else if (code == EventCode::ScrollTextHeader) {
            if (!textBuffer.empty()) {
                batch.total++;
                if (isValidString(textBuffer, options)) {
                    batch.entries.push_back(TranslationEntry{
                        .filePath = filePath,
                        .entryKey = basePath + "_event_" + std::to_string(eventIndex) + "_scroll",
                        .sourceText = textBuffer,
                        .context = "Scrolling text",
                        .category = EntryCategory::Narration
                    });
                } else {
                    batch.skipped++;
                }
                textBuffer.clear();
                eventIndex++;
            }
        }
        // Change Name/Nickname/Profile (320, 324, 325)
        else if ((code == EventCode::ChangeName ||
                  code == EventCode::ChangeNickname ||
                  code == EventCode::ChangeProfile) &&
                 parameters.size() >= 2 && parameters[1].is_string()) {
            std::string value = parameters[1].get<std::string>();
            if (!value.empty()) {
                batch.total++;
                if (isValidString(value, options)) {
                    std::string type = (code == EventCode::ChangeName) ? "name" :
                                       (code == EventCode::ChangeNickname) ? "nickname" : "profile";
                    std::string ctx = (code == EventCode::ChangeName) ? "Name change" :
                                      (code == EventCode::ChangeNickname) ? "Nickname" : "Profile";

                    batch.entries.push_back(TranslationEntry{
                        .filePath = filePath,
                        .entryKey = basePath + "_event_" + std::to_string(eventIndex) + "_" + type,
                        .sourceText = value,
                        .context = ctx,
                        .category = EntryCategory::UI
                    });
                } else {
                    batch.skipped++;
                }
                eventIndex++;
            }
        }
    }

    // Save final buffer
    if (!textBuffer.empty()) {
        batch.total++;
        if (isValidString(textBuffer, options)) {
            batch.entries.push_back(TranslationEntry{
                .filePath = filePath,
                .entryKey = basePath + "_event_" + std::to_string(eventIndex) + "_text",
                .sourceText = textBuffer,
                .context = currentContext,
                .category = EntryCategory::Dialog
            });
        } else {
            batch.skipped++;
        }
    }
}

void RpgMakerHandler::extractTerms(
    const nlohmann::json& terms,
    const std::string& filePath,
    const std::string& basePath,
    const ExtractionOptions& options,
    ExtractionBatch& batch
) {
    const std::vector<std::string> termGroups = {"basic", "commands", "params", "messages"};

    for (const auto& groupKey : termGroups) {
        if (!terms.contains(groupKey)) continue;

        const auto& group = terms[groupKey];

        if (group.is_array()) {
            for (size_t i = 0; i < group.size(); i++) {
                if (group[i].is_string()) {
                    std::string term = group[i].get<std::string>();
                    if (!term.empty()) {
                        batch.total++;
                        if (isValidString(term, options)) {
                            batch.entries.push_back(TranslationEntry{
                                .filePath = filePath,
                                .entryKey = basePath + "_terms_" + groupKey + "_" + std::to_string(i),
                                .sourceText = term,
                                .context = "System term (" + groupKey + ")",
                                .category = EntryCategory::UI
                            });
                        } else {
                            batch.skipped++;
                        }
                    }
                }
            }
        } else if (group.is_object()) {
            for (auto& [key, value] : group.items()) {
                if (value.is_string()) {
                    std::string term = value.get<std::string>();
                    if (!term.empty()) {
                        batch.total++;
                        if (isValidString(term, options)) {
                            batch.entries.push_back(TranslationEntry{
                                .filePath = filePath,
                                .entryKey = basePath + "_terms_" + groupKey + "_" + key,
                                .sourceText = term,
                                .context = "System term (" + groupKey + ")",
                                .category = EntryCategory::UI
                            });
                        } else {
                            batch.skipped++;
                        }
                    }
                }
            }
        }
    }
}

// ========== Patching ==========

Result<HandlerPatchResult> RpgMakerHandler::applyTranslations(
    const fs::path& gameDir,
    const std::vector<TranslationEntry>& translations,
    const PatchOptions& options
) {
    MAKINEAI_LOG_INFO(log::HANDLER, "RPG Maker: Starting translation application to {}", gameDir.string());
    auto timer = metrics().timer("rpgmaker_apply_translations");

    std::vector<PatchedFile> patchedFiles;
    std::vector<PatchError> errors;
    std::string backupId;
    int appliedCount = 0;
    int skippedCount = 0;
    int filesPatched = 0;

    // Create backup
    if (options.createBackup) {
        backupId = options.backupId.empty() ?
            std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) :
            options.backupId;

        MAKINEAI_LOG_DEBUG(log::HANDLER, "RPG Maker: Creating backup with ID: {}", backupId);
        auto backupResult = createBackup(gameDir, backupId);
        if (!backupResult || !backupResult->success) {
            MAKINEAI_LOG_ERROR(log::HANDLER, "RPG Maker: Backup creation failed");
            return std::unexpected(Error{ErrorCode::BackupFailed, "Failed to create backup"});
        }
    }

    // Group translations by file
    std::map<std::string, std::vector<TranslationEntry>> translationsByFile;
    for (const auto& entry : translations) {
        if (!entry.targetText || entry.targetText->empty()) continue;
        translationsByFile[entry.filePath].push_back(entry);
    }

    MAKINEAI_LOG_DEBUG(log::HANDLER, "RPG Maker: {} translations grouped into {} files",
        translations.size(), translationsByFile.size());

    // Apply translations
    for (const auto& [filePath, fileTranslations] : translationsByFile) {
        MAKINEAI_LOG_DEBUG(log::HANDLER, "RPG Maker: Patching file: {} ({} translations)",
            filePath, fileTranslations.size());

        fs::path fullPath = gameDir / filePath;

        if (!fs::exists(fullPath)) {
            MAKINEAI_LOG_WARN(log::HANDLER, "RPG Maker: File not found: {}", filePath);
            errors.push_back(PatchError{
                .file = filePath,
                .message = "File not found"
            });
            continue;
        }

        try {
            auto batch = applyToJsonFile(fullPath, fileTranslations, options);

            appliedCount += batch.applied;
            skippedCount += batch.skipped;

            patchedFiles.push_back(PatchedFile{
                .path = filePath,
                .changedStrings = batch.applied,
                .totalStrings = static_cast<int>(fileTranslations.size()),
                .backed = !backupId.empty()
            });
            filesPatched++;

            // Audit log for file modification
            AuditLogger::logFileAccess(fullPath, "patch",
                true, "Applied " + std::to_string(batch.applied) + " translations");

            MAKINEAI_LOG_DEBUG(log::HANDLER, "RPG Maker: Patched {} - {} applied, {} skipped",
                filePath, batch.applied, batch.skipped);
        } catch (const std::exception& e) {
            MAKINEAI_LOG_ERROR(log::HANDLER, "RPG Maker: Patch error in {}: {}", filePath, e.what());
            errors.push_back(PatchError{
                .file = filePath,
                .message = std::string("Patch error: ") + e.what()
            });
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - std::chrono::steady_clock::now() + timer.elapsed());

    // Record metrics
    metrics().increment("rpgmaker_files_patched", filesPatched);
    metrics().increment("rpgmaker_translations_applied", appliedCount);

    MAKINEAI_LOG_INFO(log::HANDLER, "RPG Maker: Translation complete - {} applied, {} skipped in {} files ({}ms)",
        appliedCount, skippedCount, filesPatched, duration.count());

    return HandlerPatchResult{
        .success = errors.empty(),
        .patchedFiles = patchedFiles,
        .errors = errors,
        .backupId = backupId,
        .appliedCount = appliedCount,
        .skippedCount = skippedCount,
        .duration = duration
    };
}

RpgMakerHandler::PatchBatch RpgMakerHandler::applyToJsonFile(
    const fs::path& file,
    const std::vector<TranslationEntry>& translations,
    const PatchOptions& options
) {
    PatchBatch batch;

    std::ifstream ifs(file, std::ios::binary);
    if (!ifs) return batch;

    nlohmann::json data = nlohmann::json::parse(ifs);
    ifs.close();

    for (const auto& translation : translations) {
        if (options.dryRun) {
            batch.applied++;
            continue;
        }

        if (applyTranslationToJson(data, translation)) {
            batch.applied++;
        } else {
            batch.skipped++;
        }
    }

    // Write back atomically
    if (batch.applied > 0 && !options.dryRun) {
        fs::path tempPath = file.string() + ".makineai_tmp";

        {
            std::ofstream ofs(tempPath, std::ios::binary | std::ios::trunc);
            if (!ofs) {
                batch.errors.push_back("Cannot create temp file for writing");
                return batch;
            }

            ofs << std::setw(2) << data;
            ofs.flush();

            if (!ofs.good()) {
                ofs.close();
                std::error_code ec;
                fs::remove(tempPath, ec);
                batch.errors.push_back("Write failed - possible disk full");
                return batch;
            }
        } // File closed

        // Atomic rename
        std::error_code ec;
        fs::rename(tempPath, file, ec);
        if (ec) {
            fs::remove(tempPath, ec);
            batch.errors.push_back("Rename failed: " + ec.message());
            return batch;
        }
    }

    return batch;
}

bool RpgMakerHandler::applyTranslationToJson(
    nlohmann::json& data,
    const TranslationEntry& translation
) {
    if (!translation.entryKey || translation.entryKey->empty()) return false;

    // Event translation
    if (translation.entryKey->find("_event_") != std::string::npos) {
        return applyEventTranslation(data, translation);
    }

    // Field translation
    return applyFieldTranslation(data, translation);
}

bool RpgMakerHandler::applyEventTranslation(
    nlohmann::json& data,
    const TranslationEntry& translation
) {
    // Parse event key: basePath_event_INDEX_TYPE(_SUBINDEX)?
    std::string key = *translation.entryKey;
    size_t eventSep = key.find("_event_");
    if (eventSep == std::string::npos) return false;

    std::string basePath = key.substr(0, eventSep);
    std::string eventPart = key.substr(eventSep + 7); // "_event_" length

    // Parse event index and type
    std::vector<std::string> parts;
    std::istringstream iss(eventPart);
    std::string part;
    while (std::getline(iss, part, '_')) {
        parts.push_back(part);
    }

    if (parts.empty()) return false;

    int eventIndex = 0;
    try {
        eventIndex = std::stoi(parts[0]);
    } catch (...) {
        return false;
    }

    std::string eventType = parts.size() > 1 ? parts[1] : "text";
    std::optional<int> subIndex;
    if (parts.size() > 2) {
        try {
            subIndex = std::stoi(parts[2]);
        } catch (...) {}
    }

    // Find event list
    nlohmann::json* events = findEventListAtPath(data, basePath);
    if (!events) {
        events = findEventList(data);
    }
    if (!events) return false;

    return applyToEventList(*events, eventIndex, eventType, subIndex,
                           translation.sourceText, *translation.targetText);
}

bool RpgMakerHandler::applyFieldTranslation(
    nlohmann::json& data,
    const TranslationEntry& translation
) {
    // Parse key: FileName_FIELD_ID or FileName_FIELD
    std::vector<std::string> parts;
    std::string key = *translation.entryKey;
    std::istringstream iss(key);
    std::string part;
    while (std::getline(iss, part, '_')) {
        parts.push_back(part);
    }

    if (data.is_array()) {
        for (auto& item : data) {
            if (!item.is_object()) continue;

            std::string fieldName = parts.size() >= 2 ? parts[parts.size() - 2] : "name";
            int id = -1;
            if (!parts.empty()) {
                try {
                    id = std::stoi(parts.back());
                } catch (...) {}
            }

            if (id >= 0 && item.contains("id") && item["id"].get<int>() == id &&
                item.contains(fieldName) && item[fieldName].is_string() &&
                item[fieldName].get<std::string>() == translation.sourceText) {
                item[fieldName] = *translation.targetText;
                return true;
            }

            if (item.contains(fieldName) && item[fieldName].is_string() &&
                item[fieldName].get<std::string>() == translation.sourceText) {
                item[fieldName] = *translation.targetText;
                return true;
            }
        }
    }

    return false;
}

bool RpgMakerHandler::applyToEventList(
    nlohmann::json& events,
    int targetEventIndex,
    const std::string& eventType,
    std::optional<int> subIndex,
    const std::string& sourceText,
    const std::string& targetText
) {
    int currentEventIndex = 0;
    std::vector<size_t> textLineIndices;

    for (size_t i = 0; i < events.size(); i++) {
        auto& event = events[i];
        if (!event.is_object()) continue;

        int code = event.value("code", 0);
        auto& parameters = event["parameters"];

        if (code == EventCode::ShowTextBody) {
            textLineIndices.push_back(i);
        }
        else if (code == EventCode::ShowTextHeader) {
            if (!textLineIndices.empty() && eventType == "text" && currentEventIndex == targetEventIndex) {
                if (applyTextTranslation(events, textLineIndices, sourceText, targetText)) {
                    return true;
                }
            }
            textLineIndices.clear();
            currentEventIndex++;
        }
        else if (code == EventCode::ShowChoices) {
            if (!textLineIndices.empty() && eventType == "text" && currentEventIndex == targetEventIndex) {
                if (applyTextTranslation(events, textLineIndices, sourceText, targetText)) {
                    return true;
                }
            }
            textLineIndices.clear();

            if (eventType == "choice" && currentEventIndex == targetEventIndex && subIndex.has_value()) {
                if (parameters.is_array() && !parameters.empty() && parameters[0].is_array()) {
                    auto& choices = parameters[0];
                    if (*subIndex < static_cast<int>(choices.size()) &&
                        choices[*subIndex].is_string() &&
                        choices[*subIndex].get<std::string>() == sourceText) {
                        choices[*subIndex] = targetText;
                        return true;
                    }
                }
            }
            currentEventIndex++;
        }
        else if (code == EventCode::ScrollTextBody) {
            textLineIndices.push_back(i);
        }
        else if (code == EventCode::ScrollTextHeader) {
            if (!textLineIndices.empty() && eventType == "scroll" && currentEventIndex == targetEventIndex) {
                if (applyTextTranslation(events, textLineIndices, sourceText, targetText)) {
                    return true;
                }
            }
            textLineIndices.clear();
            currentEventIndex++;
        }
        else if (code == EventCode::ChangeName || code == EventCode::ChangeNickname || code == EventCode::ChangeProfile) {
            std::string type = (code == EventCode::ChangeName) ? "name" :
                               (code == EventCode::ChangeNickname) ? "nickname" : "profile";
            if (eventType == type && currentEventIndex == targetEventIndex &&
                parameters.is_array() && parameters.size() >= 2 &&
                parameters[1].is_string() && parameters[1].get<std::string>() == sourceText) {
                parameters[1] = targetText;
                return true;
            }
            currentEventIndex++;
        }
    }

    // Check final buffer
    if (!textLineIndices.empty() && eventType == "text" && currentEventIndex == targetEventIndex) {
        if (applyTextTranslation(events, textLineIndices, sourceText, targetText)) {
            return true;
        }
    }

    return false;
}

bool RpgMakerHandler::applyTextTranslation(
    nlohmann::json& events,
    const std::vector<size_t>& textLineIndices,
    const std::string& sourceText,
    const std::string& targetText
) {
    // Build current text
    std::string currentText;
    for (size_t idx : textLineIndices) {
        auto& params = events[idx]["parameters"];
        if (params.is_array() && !params.empty() && params[0].is_string()) {
            if (!currentText.empty()) currentText += "\n";
            currentText += params[0].get<std::string>();
        }
    }

    if (currentText != sourceText) return false;

    // Split target text
    std::vector<std::string> targetLines;
    std::istringstream iss(targetText);
    std::string line;
    while (std::getline(iss, line)) {
        targetLines.push_back(line);
    }

    // Apply translation
    if (targetLines.size() == textLineIndices.size()) {
        for (size_t i = 0; i < textLineIndices.size(); i++) {
            events[textLineIndices[i]]["parameters"][0] = targetLines[i];
        }
    } else if (targetLines.size() < textLineIndices.size()) {
        for (size_t i = 0; i < textLineIndices.size(); i++) {
            if (i < targetLines.size()) {
                events[textLineIndices[i]]["parameters"][0] = targetLines[i];
            } else {
                events[textLineIndices[i]]["parameters"][0] = "";
            }
        }
    } else {
        // More lines than slots - fit into existing
        for (size_t i = 0; i < textLineIndices.size(); i++) {
            if (i < textLineIndices.size() - 1) {
                events[textLineIndices[i]]["parameters"][0] = targetLines[i];
            } else {
                // Combine remaining lines
                std::string remaining;
                for (size_t j = i; j < targetLines.size(); j++) {
                    if (!remaining.empty()) remaining += "\n";
                    remaining += targetLines[j];
                }
                events[textLineIndices[i]]["parameters"][0] = remaining;
            }
        }
    }

    return true;
}

bool RpgMakerHandler::isEventList(const nlohmann::json& list) {
    if (!list.is_array() || list.empty()) return false;

    const auto& first = list[0];
    return first.is_object() && first.contains("code") && first.contains("parameters");
}

nlohmann::json* RpgMakerHandler::findEventListAtPath(nlohmann::json& data, const std::string& path) {
    // Simple path - check for event list directly
    if (path.find('.') == std::string::npos && path.find('[') == std::string::npos) {
        return findEventList(data);
    }
    return nullptr;
}

nlohmann::json* RpgMakerHandler::findEventList(nlohmann::json& data) {
    if (data.is_array()) {
        for (auto& item : data) {
            if (item.is_object() && item.contains("list") && item["list"].is_array()) {
                if (isEventList(item["list"])) {
                    return &item["list"];
                }
            }
        }
        if (isEventList(data)) return &data;
    } else if (data.is_object()) {
        if (data.contains("events") && data["events"].is_array()) {
            for (auto& event : data["events"]) {
                if (event.is_null()) continue;
                if (event.is_object() && event.contains("pages") && event["pages"].is_array()) {
                    for (auto& page : event["pages"]) {
                        if (page.is_object() && page.contains("list") && page["list"].is_array()) {
                            if (isEventList(page["list"])) {
                                return &page["list"];
                            }
                        }
                    }
                }
            }
        }
        if (data.contains("list") && data["list"].is_array() && isEventList(data["list"])) {
            return &data["list"];
        }
    }
    return nullptr;
}

// ========== Backup/Restore ==========

Result<HandlerBackupResult> RpgMakerHandler::createBackup(
    const fs::path& gameDir,
    const std::string& backupId,
    const std::vector<std::string>& specificFiles
) {
    // Security check
    if (backupId.find("..") != std::string::npos ||
        backupId.find('/') != std::string::npos ||
        backupId.find('\\') != std::string::npos) {
        return HandlerBackupResult{
            .success = false,
            .backupId = backupId,
            .errorMessage = "Invalid backup ID (security)"
        };
    }

    fs::path backupDir = gameDir / "_makineai_backups" / backupId;
    std::vector<std::string> backedUpFiles;
    uint64_t totalSize = 0;

    try {
        fs::create_directories(backupDir);

        auto dataDir = findDataDir(gameDir);
        if (!dataDir) {
            return HandlerBackupResult{
                .success = false,
                .backupId = backupId,
                .backupPath = backupDir.string(),
                .errorMessage = "Data directory not found"
            };
        }

        for (const auto& entry : fs::directory_iterator(*dataDir)) {
            if (!entry.is_regular_file()) continue;

            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext != ".json") continue;

            // Check specific files filter
            if (!specificFiles.empty()) {
                bool found = false;
                std::string filename = entry.path().filename().string();
                for (const auto& f : specificFiles) {
                    if (filename == f || entry.path().string().find(f) != std::string::npos) {
                        found = true;
                        break;
                    }
                }
                if (!found) continue;
            }

            auto relativePath = fs::relative(entry.path(), gameDir);
            auto backupPath = backupDir / relativePath;

            fs::create_directories(backupPath.parent_path());
            fs::copy_file(entry.path(), backupPath, fs::copy_options::overwrite_existing);

            totalSize += fs::file_size(entry.path());
            backedUpFiles.push_back(relativePath.string());
        }

        return HandlerBackupResult{
            .success = true,
            .backupId = backupId,
            .backupPath = backupDir.string(),
            .backedUpFiles = backedUpFiles,
            .totalSize = totalSize
        };
    } catch (const std::exception& e) {
        return HandlerBackupResult{
            .success = false,
            .backupId = backupId,
            .backupPath = backupDir.string(),
            .errorMessage = e.what()
        };
    }
}

Result<HandlerRestoreResult> RpgMakerHandler::restoreBackup(
    const fs::path& gameDir,
    const std::string& backupId
) {
    // Security check
    if (backupId.find("..") != std::string::npos ||
        backupId.find('/') != std::string::npos ||
        backupId.find('\\') != std::string::npos) {
        return HandlerRestoreResult{
            .success = false,
            .errorMessage = "Invalid backup ID (security)"
        };
    }

    fs::path backupDir = gameDir / "_makineai_backups" / backupId;
    std::vector<std::string> restoredFiles;

    if (!fs::exists(backupDir)) {
        return HandlerRestoreResult{
            .success = false,
            .errorMessage = "Backup not found: " + backupId
        };
    }

    try {
        for (const auto& entry : fs::recursive_directory_iterator(backupDir)) {
            if (!entry.is_regular_file()) continue;

            auto relativePath = fs::relative(entry.path(), backupDir);
            auto targetPath = gameDir / relativePath;

            fs::create_directories(targetPath.parent_path());
            fs::copy_file(entry.path(), targetPath, fs::copy_options::overwrite_existing);

            restoredFiles.push_back(relativePath.string());
        }

        // Clean up backup
        fs::remove_all(backupDir);

        return HandlerRestoreResult{
            .success = true,
            .restoredFiles = restoredFiles
        };
    } catch (const std::exception& e) {
        return HandlerRestoreResult{
            .success = false,
            .errorMessage = e.what()
        };
    }
}

// ========== Validation ==========

Result<ValidationResult> RpgMakerHandler::validatePatch(
    const fs::path& gameDir,
    const std::vector<TranslationEntry>& translations
) {
    std::vector<ValidationIssue> issues;
    int checked = 0;
    int passed = 0;
    int failed = 0;

    auto gameFilesResult = findGameFiles(gameDir);
    if (!gameFilesResult) {
        return std::unexpected(gameFilesResult.error());
    }

    for (const auto& translation : translations) {
        if (!translation.targetText || translation.targetText->empty()) continue;
        checked++;

        // Check file exists
        bool fileExists = false;
        for (const auto& f : *gameFilesResult) {
            if (f.relativePath == translation.filePath) {
                fileExists = true;
                break;
            }
        }

        if (!fileExists) {
            failed++;
            issues.push_back(ValidationIssue{
                .file = translation.filePath,
                .entryKey = translation.entryKey.value_or(""),
                .message = "Source file not found",
                .severity = ValidationSeverity::Error
            });
            continue;
        }

        // Check for invalid control characters
        static std::regex controlChars(R"([\x00-\x08\x0B\x0C\x0E-\x1F])");
        if (std::regex_search(*translation.targetText, controlChars)) {
            failed++;
            issues.push_back(ValidationIssue{
                .file = translation.filePath,
                .entryKey = translation.entryKey.value_or(""),
                .message = "Contains invalid control characters",
                .severity = ValidationSeverity::Error
            });
            continue;
        }

        // Length warning
        if (translation.targetText->length() > translation.sourceText.length() * 3) {
            issues.push_back(ValidationIssue{
                .file = translation.filePath,
                .entryKey = translation.entryKey.value_or(""),
                .message = "Translation is 3x longer than original",
                .severity = ValidationSeverity::Warning
            });
        }

        passed++;
    }

    return ValidationResult{
        .isValid = (failed == 0),
        .issues = issues,
        .checkedCount = checked,
        .passedCount = passed,
        .failedCount = failed
    };
}

// ========== Helper Functions ==========

bool RpgMakerHandler::isValidString(const std::string& text, const ExtractionOptions& options) {
    // Empty or whitespace only
    std::string trimmed = text;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
    if (trimmed.empty()) return false;

    // Minimum length
    if (text.length() < static_cast<size_t>(options.minLength)) return false;

    // Maximum length
    if (options.maxLength > 0 && text.length() > static_cast<size_t>(options.maxLength)) return false;

    // Only numbers/symbols
    static std::regex numbersOnly(R"(^[\d\s\-\+\*\/\=\.\,\!\?\:\;\(\)\[\]\{\}]+$)");
    if (std::regex_match(text, numbersOnly)) return false;

    // Only control/escape characters
    static std::regex escapeOnly(R"(^[\\\/\n\r\t\s]+$)");
    if (std::regex_match(text, escapeOnly)) return false;

    // RPG Maker control codes
    static std::regex controlCode(R"(^\\[A-Za-z]+(\[\d+\])?$)");
    if (std::regex_match(text, controlCode)) return false;

    return true;
}

bool RpgMakerHandler::containsDisplayText(const std::string& text) {
    // Plugin tags usually don't need translation
    if (text.front() == '<' && text.back() == '>') {
        static std::regex pluginTag(R"(^<[A-Za-z]+:.*>$)");
        if (std::regex_match(text, pluginTag)) return false;
    }

    // Contains actual text characters
    static std::regex hasText(R"([A-Za-z\xC0-\xFF])");
    return std::regex_search(text, hasText);
}

std::string RpgMakerHandler::guessContext(const std::string& path, const std::string& fileName) {
    std::string lower = fileName;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.find("actors") == 0) return "Character";
    if (lower.find("classes") == 0) return "Class";
    if (lower.find("skills") == 0) return "Skill";
    if (lower.find("items") == 0) return "Item";
    if (lower.find("weapons") == 0) return "Weapon";
    if (lower.find("armors") == 0) return "Armor";
    if (lower.find("enemies") == 0) return "Enemy";
    if (lower.find("states") == 0) return "Status effect";
    if (lower.find("map") == 0) return "Map";
    if (lower == "system.json") return "System";
    if (lower.find("commonevents") == 0) return "Common event";

    std::string lowerPath = path;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);

    if (lowerPath.find("name") != std::string::npos) return "Name";
    if (lowerPath.find("description") != std::string::npos) return "Description";
    if (lowerPath.find("message") != std::string::npos) return "Message";

    return "General";
}

EntryCategory RpgMakerHandler::guessCategory(const std::string& path, const std::string& fileName) {
    std::string lower = fileName;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.find("actors") == 0 || lower.find("enemies") == 0) return EntryCategory::UI;
    if (lower.find("skills") == 0) return EntryCategory::Skill;
    if (lower.find("items") == 0 || lower.find("weapons") == 0 || lower.find("armors") == 0) return EntryCategory::Item;
    if (lower.find("map") == 0 || lower.find("commonevents") == 0) return EntryCategory::Dialog;
    if (lower == "system.json") return EntryCategory::System;

    std::string lowerPath = path;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);

    if (lowerPath.find("dialog") != std::string::npos ||
        lowerPath.find("text") != std::string::npos ||
        lowerPath.find("message") != std::string::npos) {
        return EntryCategory::Dialog;
    }

    return EntryCategory::Other;
}

std::string RpgMakerHandler::getFieldContext(const std::string& field) {
    if (field == "displayName") return "Map name";
    if (field == "gameTitle") return "Game title";
    if (field == "currencyUnit") return "Currency";
    if (field == "title1Name" || field == "title2Name") return "Title screen";
    return "System";
}

EntryCategory RpgMakerHandler::getFieldCategory(const std::string& field) {
    if (field == "displayName") return EntryCategory::Narration;
    if (field == "gameTitle" || field == "title1Name" || field == "title2Name") return EntryCategory::UI;
    if (field == "currencyUnit") return EntryCategory::Item;
    return EntryCategory::UI;
}

} // namespace makineai
