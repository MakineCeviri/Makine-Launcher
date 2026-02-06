/**
 * @file glossary_service.cpp
 * @brief Glossary service implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/glossary_service.hpp"
#include "makineai/database.hpp"
#include "makineai/core.hpp"
#include "makineai/logging.hpp"
#include "makineai/metrics.hpp"

#include <algorithm>
#include <cctype>
#include <regex>

namespace makineai {

// =============================================================================
// SINGLETON & CACHE
// =============================================================================

GlossaryService& GlossaryService::instance() {
    static GlossaryService instance;
    return instance;
}

void GlossaryService::clearCache() {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    cachedTerms_.clear();
    lastCacheUpdate_ = std::chrono::steady_clock::time_point{};
}

bool GlossaryService::isCacheValid() const {
    if (cachedTerms_.empty()) return false;

    auto now = std::chrono::steady_clock::now();
    return (now - lastCacheUpdate_) < CACHE_DURATION;
}

// =============================================================================
// BASIC OPERATIONS
// =============================================================================

Result<int64_t> GlossaryService::addTerm(const GlossaryTerm& term) {
    clearCache();

    auto result = Database::instance().addGlossaryTerm(term);
    if (!result) {
        return std::unexpected(result.error());
    }

    MAKINEAI_LOG_DEBUG(log::GLOSSARY, "Glossary term added: {} -> {} (ID: {})",
        term.termSource, term.termTarget, *result);

    return *result;
}

Result<void> GlossaryService::updateTerm(const GlossaryTerm& term) {
    if (!term.id.has_value()) {
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "Term ID is required for update"));
    }

    clearCache();
    return Database::instance().updateGlossaryTerm(term);
}

Result<void> GlossaryService::deleteTerm(int64_t termId) {
    clearCache();
    return Database::instance().deleteGlossaryTerm(termId);
}

Result<void> GlossaryService::addAlternative(
    int64_t termId,
    const std::string& alternative,
    const std::optional<std::string>& context
) {
    clearCache();
    return Database::instance().addGlossaryAlternative(termId, alternative, context);
}

Result<void> GlossaryService::addForbidden(
    int64_t termId,
    const std::string& forbidden,
    const std::optional<std::string>& reason
) {
    clearCache();
    return Database::instance().addForbiddenTranslation(termId, forbidden, reason);
}

// =============================================================================
// QUERYING
// =============================================================================

Result<std::vector<GlossaryTerm>> GlossaryService::getAllTerms(bool forceRefresh) {
    std::lock_guard<std::mutex> lock(cacheMutex_);

    if (!forceRefresh && isCacheValid()) {
        return cachedTerms_;
    }

    auto result = Database::instance().getAllGlossaryTerms();
    if (!result) {
        return std::unexpected(result.error());
    }

    cachedTerms_ = std::move(*result);
    lastCacheUpdate_ = std::chrono::steady_clock::now();

    return cachedTerms_;
}

Result<std::vector<GlossaryTerm>> GlossaryService::getTermsByDomain(TermDomain domain) {
    auto allTerms = getAllTerms();
    if (!allTerms) {
        return std::unexpected(allTerms.error());
    }

    std::vector<GlossaryTerm> filtered;
    for (const auto& term : *allTerms) {
        if (!term.domain.has_value() ||
            *term.domain == domain ||
            *term.domain == TermDomain::General) {
            filtered.push_back(term);
        }
    }

    return filtered;
}

Result<std::vector<GlossaryTerm>> GlossaryService::getTermsForGame(const std::string& gameId) {
    auto allTerms = getAllTerms();
    if (!allTerms) {
        return std::unexpected(allTerms.error());
    }

    std::vector<GlossaryTerm> filtered;
    for (const auto& term : *allTerms) {
        if (!term.gameSpecific.has_value() || *term.gameSpecific == gameId) {
            filtered.push_back(term);
        }
    }

    return filtered;
}

Result<std::vector<GlossaryTerm>> GlossaryService::searchTerms(const std::string& query) {
    metrics().increment("glossary_lookups");
    MAKINEAI_LOG_DEBUG(log::GLOSSARY, "Searching glossary for: {}", query);

    auto allTerms = getAllTerms();
    if (!allTerms) {
        return std::unexpected(allTerms.error());
    }

    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(),
        [](unsigned char c) { return std::tolower(c); });

    std::vector<GlossaryTerm> results;
    for (const auto& term : *allTerms) {
        std::string lowerSource = term.termSource;
        std::string lowerTarget = term.termTarget;

        std::transform(lowerSource.begin(), lowerSource.end(), lowerSource.begin(),
            [](unsigned char c) { return std::tolower(c); });
        std::transform(lowerTarget.begin(), lowerTarget.end(), lowerTarget.begin(),
            [](unsigned char c) { return std::tolower(c); });

        if (lowerSource.find(lowerQuery) != std::string::npos ||
            lowerTarget.find(lowerQuery) != std::string::npos) {
            results.push_back(term);
            continue;
        }

        // Check alternatives
        for (const auto& alt : term.alternatives) {
            std::string lowerAlt = alt;
            std::transform(lowerAlt.begin(), lowerAlt.end(), lowerAlt.begin(),
                [](unsigned char c) { return std::tolower(c); });

            if (lowerAlt.find(lowerQuery) != std::string::npos) {
                results.push_back(term);
                break;
            }
        }
    }

    if (results.empty()) {
        metrics().increment("glossary_misses");
        MAKINEAI_LOG_WARN(log::GLOSSARY, "No glossary terms found for: {}", query);
    } else {
        metrics().increment("glossary_hits");
        MAKINEAI_LOG_DEBUG(log::GLOSSARY, "Found {} glossary terms for: {}", results.size(), query);
    }

    return results;
}

// =============================================================================
// TEXT MATCHING
// =============================================================================

std::vector<MatchPosition> GlossaryService::findMatchPositions(
    const std::string& text,
    const GlossaryTerm& term
) const {
    std::vector<MatchPosition> positions;

    std::string searchText = term.caseSensitive ? text : [&]() {
        std::string lower = text;
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return lower;
    }();

    std::string searchTerm = term.caseSensitive ? term.termSource : [&]() {
        std::string lower = term.termSource;
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return lower;
    }();

    if (term.exactMatch) {
        // Whole word match using regex
        try {
            std::regex pattern("\\b" + searchTerm + "\\b");
            auto begin = std::sregex_iterator(searchText.begin(), searchText.end(), pattern);
            auto end = std::sregex_iterator();

            for (auto it = begin; it != end; ++it) {
                positions.push_back(MatchPosition{
                    static_cast<size_t>(it->position()),
                    static_cast<size_t>(it->position() + it->length())
                });
            }
        }
        catch (const std::regex_error&) {
            // Fall back to simple search if regex fails
        }
    }
    else {
        // Substring search
        size_t pos = 0;
        while ((pos = searchText.find(searchTerm, pos)) != std::string::npos) {
            positions.push_back(MatchPosition{pos, pos + searchTerm.length()});
            pos += 1;  // Move past to find overlapping matches
        }
    }

    return positions;
}

Result<std::vector<GlossaryMatch>> GlossaryService::findTermsInText(
    const std::string& text,
    const std::optional<TermDomain>& domain,
    const std::optional<std::string>& gameId
) {
    metrics().increment("glossary_lookups");
    MAKINEAI_LOG_DEBUG(log::GLOSSARY, "Finding terms in text (length: {})", text.length());

    auto termsResult = getAllTerms();
    if (!termsResult) {
        return std::unexpected(termsResult.error());
    }

    auto terms = *termsResult;

    // Apply filters
    if (domain.has_value()) {
        terms.erase(
            std::remove_if(terms.begin(), terms.end(),
                [&domain](const GlossaryTerm& t) {
                    return t.domain.has_value() &&
                           *t.domain != *domain &&
                           *t.domain != TermDomain::General;
                }),
            terms.end()
        );
    }

    if (gameId.has_value()) {
        terms.erase(
            std::remove_if(terms.begin(), terms.end(),
                [&gameId](const GlossaryTerm& t) {
                    return t.gameSpecific.has_value() && *t.gameSpecific != *gameId;
                }),
            terms.end()
        );
    }

    std::vector<GlossaryMatch> matches;

    for (const auto& term : terms) {
        auto positions = findMatchPositions(text, term);
        if (!positions.empty()) {
            matches.push_back(GlossaryMatch{term, std::move(positions)});
        }
    }

    // Sort by priority (descending)
    std::sort(matches.begin(), matches.end(),
        [](const GlossaryMatch& a, const GlossaryMatch& b) {
            return a.term.priority > b.term.priority;
        });

    if (matches.empty()) {
        metrics().increment("glossary_misses");
        MAKINEAI_LOG_DEBUG(log::GLOSSARY, "No glossary matches found in text");
    } else {
        metrics().increment("glossary_hits");
        MAKINEAI_LOG_DEBUG(log::GLOSSARY, "Found {} glossary matches in text", matches.size());
    }

    return matches;
}

Result<std::vector<ForbiddenViolation>> GlossaryService::checkForbiddenTerms(
    const std::string& sourceText,
    const std::string& targetText,
    const std::optional<std::string>& gameId
) {
    auto termsResult = getAllTerms();
    if (!termsResult) {
        return std::unexpected(termsResult.error());
    }

    std::vector<ForbiddenViolation> violations;
    std::string lowerTarget = targetText;
    std::transform(lowerTarget.begin(), lowerTarget.end(), lowerTarget.begin(),
        [](unsigned char c) { return std::tolower(c); });

    for (const auto& term : *termsResult) {
        // Check if term matches source text
        auto positions = findMatchPositions(sourceText, term);
        if (positions.empty()) continue;

        // Check for forbidden translations
        for (const auto& forbidden : term.forbidden) {
            std::string lowerForbidden = forbidden.forbiddenTranslation;
            std::transform(lowerForbidden.begin(), lowerForbidden.end(),
                lowerForbidden.begin(), [](unsigned char c) { return std::tolower(c); });

            if (lowerTarget.find(lowerForbidden) != std::string::npos) {
                violations.push_back(ForbiddenViolation{
                    term,
                    forbidden,
                    forbidden.forbiddenTranslation
                });
            }
        }
    }

    return violations;
}

Result<std::string> GlossaryService::applyGlossary(
    const std::string& text,
    const std::optional<TermDomain>& domain,
    const std::optional<std::string>& gameId
) {
    auto matchesResult = findTermsInText(text, domain, gameId);
    if (!matchesResult) {
        return std::unexpected(matchesResult.error());
    }

    if (matchesResult->empty()) {
        return text;
    }

    // Collect all replacements
    struct ReplacementInfo {
        size_t start;
        size_t end;
        std::string replacement;
    };

    std::vector<ReplacementInfo> replacements;

    for (const auto& match : *matchesResult) {
        if (match.term.doNotTranslate) continue;

        for (const auto& pos : match.positions) {
            replacements.push_back(ReplacementInfo{
                pos.start,
                pos.end,
                match.term.termTarget
            });
        }
    }

    // Sort from end to start (to avoid offset issues)
    std::sort(replacements.begin(), replacements.end(),
        [](const ReplacementInfo& a, const ReplacementInfo& b) {
            return a.start > b.start;
        });

    // Remove overlapping replacements (keep earlier ones)
    std::vector<ReplacementInfo> filteredReplacements;
    for (const auto& r : replacements) {
        bool overlaps = false;
        for (const auto& existing : filteredReplacements) {
            if (r.end > existing.start && r.start < existing.end) {
                overlaps = true;
                break;
            }
        }
        if (!overlaps) {
            filteredReplacements.push_back(r);
        }
    }

    // Apply replacements
    std::string result = text;
    for (const auto& r : filteredReplacements) {
        result = result.substr(0, r.start) + r.replacement + result.substr(r.end);
    }

    return result;
}

// =============================================================================
// DEFAULT TERMS
// =============================================================================

Result<int> GlossaryService::loadDefaultTerms() {
    MAKINEAI_LOG_INFO(log::GLOSSARY, "Loading default glossary terms...");

    auto defaultTerms = DefaultGlossary::getAllTerms();
    int loaded = 0;

    for (const auto& term : defaultTerms) {
        auto result = addTerm(term);
        if (result) {
            ++loaded;
        } else {
            // Probably already exists (UNIQUE constraint)
            MAKINEAI_LOG_DEBUG(log::GLOSSARY, "Term skipped (probably exists): {}", term.termSource);
        }
    }

    MAKINEAI_LOG_INFO(log::GLOSSARY, "{} default glossary terms loaded", loaded);
    return loaded;
}

Result<void> GlossaryService::ensureDefaultTermsLoaded() {
    auto stats = getStats();
    if (!stats) {
        return std::unexpected(stats.error());
    }

    if (stats->totalTerms == 0) {
        auto loadResult = loadDefaultTerms();
        if (!loadResult) {
            return std::unexpected(loadResult.error());
        }
    }

    return {};
}

// =============================================================================
// STATISTICS
// =============================================================================

Result<GlossaryStats> GlossaryService::getStats() {
    auto allTerms = getAllTerms();
    if (!allTerms) {
        return std::unexpected(allTerms.error());
    }

    GlossaryStats stats;
    stats.totalTerms = allTerms->size();

    for (const auto& term : *allTerms) {
        if (term.doNotTranslate) {
            stats.doNotTranslate++;
        }
        stats.totalAlternatives += term.alternatives.size();
        stats.totalForbidden += term.forbidden.size();

        if (term.domain.has_value()) {
            std::string domainStr;
            switch (*term.domain) {
                case TermDomain::General: domainStr = "general"; break;
                case TermDomain::RPG: domainStr = "rpg"; break;
                case TermDomain::FPS: domainStr = "fps"; break;
                case TermDomain::VisualNovel: domainStr = "visual_novel"; break;
                case TermDomain::Strategy: domainStr = "strategy"; break;
                case TermDomain::Simulation: domainStr = "simulation"; break;
                case TermDomain::Adventure: domainStr = "adventure"; break;
                case TermDomain::Puzzle: domainStr = "puzzle"; break;
                case TermDomain::Action: domainStr = "action"; break;
                case TermDomain::Horror: domainStr = "horror"; break;
            }
            stats.domainDistribution[domainStr]++;
        }
    }

    return stats;
}

// =============================================================================
// DEFAULT GLOSSARY
// =============================================================================

std::vector<GlossaryTerm> DefaultGlossary::getAllTerms() {
    std::vector<GlossaryTerm> all;

    auto ui = getUITerms();
    auto rpg = getRPGTerms();
    auto fps = getFPSTerms();
    auto action = getActionTerms();

    all.insert(all.end(), ui.begin(), ui.end());
    all.insert(all.end(), rpg.begin(), rpg.end());
    all.insert(all.end(), fps.begin(), fps.end());
    all.insert(all.end(), action.begin(), action.end());

    return all;
}

std::vector<GlossaryTerm> DefaultGlossary::getUITerms() {
    std::vector<GlossaryTerm> terms;
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    auto addTerm = [&](const std::string& source, const std::string& target,
                       int priority = 50, bool exactMatch = true) {
        GlossaryTerm term;
        term.termSource = source;
        term.termTarget = target;
        term.termType = TermType::UI;
        term.domain = TermDomain::General;
        term.priority = priority;
        term.exactMatch = exactMatch;
        term.createdAt = now;
        terms.push_back(term);
    };

    // Common UI terms
    addTerm("New Game", "Yeni Oyun", 80);
    addTerm("Continue", "Devam Et", 80);
    addTerm("Load Game", "Oyun Yükle", 80);
    addTerm("Save Game", "Oyun Kaydet", 80);
    addTerm("Options", "Ayarlar", 80);
    addTerm("Settings", "Ayarlar", 80);
    addTerm("Exit", "Çıkış", 80);
    addTerm("Quit", "Çık", 80);
    addTerm("Back", "Geri", 75);
    addTerm("Cancel", "İptal", 75);
    addTerm("Confirm", "Onayla", 75);
    addTerm("Accept", "Kabul Et", 75);
    addTerm("Yes", "Evet", 70);
    addTerm("No", "Hayır", 70);
    addTerm("OK", "Tamam", 70);
    addTerm("Apply", "Uygula", 70);
    addTerm("Reset", "Sıfırla", 70);
    addTerm("Default", "Varsayılan", 70);

    // Graphics settings
    addTerm("Graphics", "Grafik", 60);
    addTerm("Resolution", "Çözünürlük", 60);
    addTerm("Fullscreen", "Tam Ekran", 60);
    addTerm("Windowed", "Pencereli", 60);
    addTerm("V-Sync", "Dikey Senkronizasyon", 55);
    addTerm("Brightness", "Parlaklık", 55);
    addTerm("Contrast", "Kontrast", 55);

    // Audio settings
    addTerm("Audio", "Ses", 60);
    addTerm("Sound", "Ses", 60);
    addTerm("Music", "Müzik", 60);
    addTerm("Volume", "Ses Seviyesi", 60);
    addTerm("Master Volume", "Ana Ses", 55);
    addTerm("SFX", "Efektler", 55);
    addTerm("Voice", "Seslendirme", 55);
    addTerm("Subtitles", "Altyazı", 60);

    // Controls
    addTerm("Controls", "Kontroller", 60);
    addTerm("Keybindings", "Tuş Atamaları", 55);
    addTerm("Mouse Sensitivity", "Fare Hassasiyeti", 55);
    addTerm("Invert Y-Axis", "Y Eksenini Ters Çevir", 50);

    return terms;
}

std::vector<GlossaryTerm> DefaultGlossary::getRPGTerms() {
    std::vector<GlossaryTerm> terms;
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    auto addTerm = [&](const std::string& source, const std::string& target,
                       int priority = 50) {
        GlossaryTerm term;
        term.termSource = source;
        term.termTarget = target;
        term.termType = TermType::Stat;
        term.domain = TermDomain::RPG;
        term.priority = priority;
        term.exactMatch = true;
        term.createdAt = now;
        terms.push_back(term);
    };

    // Stats
    addTerm("Health", "Can", 80);
    addTerm("HP", "Can", 80);
    addTerm("Mana", "Büyü Gücü", 80);
    addTerm("MP", "Büyü Gücü", 80);
    addTerm("Stamina", "Dayanıklılık", 75);
    addTerm("Experience", "Deneyim", 75);
    addTerm("XP", "Deneyim", 75);
    addTerm("Level", "Seviye", 80);
    addTerm("Strength", "Güç", 70);
    addTerm("Dexterity", "Çeviklik", 70);
    addTerm("Intelligence", "Zeka", 70);
    addTerm("Wisdom", "Akıl", 70);
    addTerm("Constitution", "Dayanıklılık", 70);
    addTerm("Charisma", "Karizma", 70);
    addTerm("Luck", "Şans", 70);

    // Combat
    addTerm("Attack", "Saldırı", 75);
    addTerm("Defense", "Savunma", 75);
    addTerm("Damage", "Hasar", 75);
    addTerm("Armor", "Zırh", 75);
    addTerm("Critical Hit", "Kritik Vuruş", 70);
    addTerm("Dodge", "Kaçınma", 70);
    addTerm("Block", "Engelleme", 70);

    // Items
    addTerm("Inventory", "Envanter", 75);
    addTerm("Equipment", "Ekipman", 75);
    addTerm("Weapon", "Silah", 70);
    addTerm("Shield", "Kalkan", 70);
    addTerm("Helmet", "Kask", 65);
    addTerm("Chest Armor", "Göğüs Zırhı", 65);
    addTerm("Potion", "İksir", 70);
    addTerm("Scroll", "Tomar", 65);

    // Classes
    addTerm("Warrior", "Savaşçı", 70);
    addTerm("Mage", "Büyücü", 70);
    addTerm("Rogue", "Haydut", 70);
    addTerm("Priest", "Rahip", 70);
    addTerm("Paladin", "Şövalye", 70);
    addTerm("Ranger", "Korucu", 70);

    // Quest
    addTerm("Quest", "Görev", 80);
    addTerm("Main Quest", "Ana Görev", 75);
    addTerm("Side Quest", "Yan Görev", 75);
    addTerm("Objective", "Hedef", 70);
    addTerm("Reward", "Ödül", 70);

    return terms;
}

std::vector<GlossaryTerm> DefaultGlossary::getFPSTerms() {
    std::vector<GlossaryTerm> terms;
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    auto addTerm = [&](const std::string& source, const std::string& target,
                       int priority = 50) {
        GlossaryTerm term;
        term.termSource = source;
        term.termTarget = target;
        term.termType = TermType::Action;
        term.domain = TermDomain::FPS;
        term.priority = priority;
        term.exactMatch = true;
        term.createdAt = now;
        terms.push_back(term);
    };

    // Weapons
    addTerm("Rifle", "Tüfek", 70);
    addTerm("Pistol", "Tabanca", 70);
    addTerm("Shotgun", "Pompalı", 70);
    addTerm("Sniper", "Keskin Nişancı", 70);
    addTerm("Grenade", "El Bombası", 70);
    addTerm("Ammo", "Cephane", 75);
    addTerm("Reload", "Şarjör Değiştir", 75);
    addTerm("Magazine", "Şarjör", 65);

    // Actions
    addTerm("Crouch", "Çömel", 65);
    addTerm("Prone", "Yat", 65);
    addTerm("Sprint", "Koş", 70);
    addTerm("Jump", "Zıpla", 70);
    addTerm("Aim", "Nişan Al", 70);
    addTerm("Fire", "Ateş Et", 70);
    addTerm("Melee", "Yakın Dövüş", 65);

    // Multiplayer
    addTerm("Team Deathmatch", "Takım Ölüm Maçı", 60);
    addTerm("Capture the Flag", "Bayrak Kapma", 60);
    addTerm("Respawn", "Yeniden Doğ", 65);
    addTerm("Spawn Point", "Doğma Noktası", 60);
    addTerm("Kill", "Öldürme", 65);
    addTerm("Death", "Ölüm", 65);
    addTerm("K/D Ratio", "Ö/Ö Oranı", 55);

    return terms;
}

std::vector<GlossaryTerm> DefaultGlossary::getActionTerms() {
    std::vector<GlossaryTerm> terms;
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    auto addTerm = [&](const std::string& source, const std::string& target,
                       int priority = 50) {
        GlossaryTerm term;
        term.termSource = source;
        term.termTarget = target;
        term.termType = TermType::Action;
        term.domain = TermDomain::Action;
        term.priority = priority;
        term.exactMatch = true;
        term.createdAt = now;
        terms.push_back(term);
    };

    // Common actions
    addTerm("Use", "Kullan", 70);
    addTerm("Take", "Al", 70);
    addTerm("Drop", "Bırak", 70);
    addTerm("Examine", "İncele", 65);
    addTerm("Interact", "Etkileşim", 65);
    addTerm("Open", "Aç", 70);
    addTerm("Close", "Kapat", 70);
    addTerm("Lock", "Kilitle", 65);
    addTerm("Unlock", "Kilidi Aç", 65);
    addTerm("Push", "İt", 60);
    addTerm("Pull", "Çek", 60);

    // Movement
    addTerm("Walk", "Yürü", 65);
    addTerm("Run", "Koş", 70);
    addTerm("Climb", "Tırman", 65);
    addTerm("Swim", "Yüz", 65);
    addTerm("Slide", "Kay", 60);
    addTerm("Roll", "Yuvarlan", 60);

    // Combat (generic)
    addTerm("Fight", "Savaş", 70);
    addTerm("Defend", "Savun", 70);
    addTerm("Parry", "Savuştur", 65);
    addTerm("Counter", "Karşı Saldır", 65);
    addTerm("Combo", "Kombo", 60);
    addTerm("Finisher", "Bitirici", 60);

    return terms;
}

} // namespace makineai
