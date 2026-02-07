/**
 * @file font_manager.cpp
 * @brief Font analysis and Turkish character injection
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/font_manager.hpp"
#include "makineai/logging.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <regex>
#include <sstream>

namespace makineai {

namespace {

// ============================================================================
// TTF/OTF Binary Helpers
// ============================================================================

// Read big-endian uint16 from buffer
inline uint16_t readU16BE(const uint8_t* data) {
    return static_cast<uint16_t>((data[0] << 8) | data[1]);
}

// Read big-endian uint32 from buffer
inline uint32_t readU32BE(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8)  |
           static_cast<uint32_t>(data[3]);
}

// TTF table directory entry
struct TtfTableRecord {
    char tag[5]{};      // 4-byte tag + null
    uint32_t checksum;
    uint32_t offset;
    uint32_t length;
};

// Font file signatures
constexpr uint32_t TTF_MAGIC = 0x00010000;
constexpr uint32_t OTF_MAGIC = 0x4F54544F; // 'OTTO'
constexpr uint32_t TTC_MAGIC = 0x74746366; // 'ttcf'

// Find a table in TTF/OTF data
bool findTable(const std::vector<uint8_t>& data, const char* tag, uint32_t& outOffset, uint32_t& outLength) {
    if (data.size() < 12) return false;

    uint16_t numTables = readU16BE(data.data() + 4);
    if (data.size() < 12 + numTables * 16ULL) return false;

    for (uint16_t i = 0; i < numTables; ++i) {
        size_t recordOffset = 12 + i * 16;
        if (std::memcmp(data.data() + recordOffset, tag, 4) == 0) {
            outOffset = readU32BE(data.data() + recordOffset + 8);
            outLength = readU32BE(data.data() + recordOffset + 12);
            return outOffset + outLength <= data.size();
        }
    }
    return false;
}

// Parse cmap format 4 (BMP characters, most common)
void parseCmapFormat4(const uint8_t* data, size_t len, std::set<char32_t>& glyphs) {
    if (len < 14) return;

    uint16_t segCount = readU16BE(data + 6) / 2;
    size_t headerSize = 14 + segCount * 8ULL;
    if (len < headerSize) return;

    const uint8_t* endCodes   = data + 14;
    const uint8_t* startCodes = data + 14 + segCount * 2 + 2; // +2 for reservedPad
    const uint8_t* idDeltas   = data + 14 + segCount * 4 + 2;
    const uint8_t* idRangeOffsets = data + 14 + segCount * 6 + 2;

    for (uint16_t i = 0; i < segCount; ++i) {
        uint16_t endCode   = readU16BE(endCodes + i * 2);
        uint16_t startCode = readU16BE(startCodes + i * 2);
        // uint16_t idDelta  = readU16BE(idDeltas + i * 2); // unused for coverage check
        uint16_t rangeOff  = readU16BE(idRangeOffsets + i * 2);

        if (startCode == 0xFFFF) break;

        for (uint32_t c = startCode; c <= endCode; ++c) {
            uint16_t glyphIndex = 0;
            if (rangeOff == 0) {
                glyphIndex = 1; // Non-zero means glyph exists
            } else {
                size_t offset = (rangeOff / 2) + (c - startCode) + i;
                size_t byteOffset = (idRangeOffsets - data) + offset * 2;
                if (byteOffset + 1 < len) {
                    glyphIndex = readU16BE(data + byteOffset);
                }
            }
            if (glyphIndex != 0) {
                glyphs.insert(static_cast<char32_t>(c));
            }
        }
    }
}

// Parse cmap format 12 (full Unicode, 32-bit)
void parseCmapFormat12(const uint8_t* data, size_t len, std::set<char32_t>& glyphs) {
    if (len < 16) return;

    uint32_t nGroups = readU32BE(data + 12);
    if (len < 16 + nGroups * 12ULL) return;

    for (uint32_t i = 0; i < nGroups; ++i) {
        size_t groupOffset = 16 + i * 12;
        uint32_t startCode = readU32BE(data + groupOffset);
        uint32_t endCode   = readU32BE(data + groupOffset + 4);

        // Only collect chars in ranges we care about (Turkish is U+00C0-U+017F)
        uint32_t rangeStart = std::max(startCode, 0x00C0u);
        uint32_t rangeEnd   = std::min(endCode, 0x017Fu);

        for (uint32_t c = rangeStart; c <= rangeEnd; ++c) {
            glyphs.insert(static_cast<char32_t>(c));
        }
    }
}

// Font file extensions
bool isTtfOtf(const fs::path& path) {
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".ttf" || ext == ".otf" || ext == ".ttc";
}

bool isBmFont(const fs::path& path) {
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".fnt";
}

bool isFontFile(const fs::path& path) {
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".ttf" || ext == ".otf" || ext == ".ttc" ||
           ext == ".fnt" || ext == ".woff" || ext == ".woff2";
}

} // anonymous namespace

// ============================================================================
// FontAnalysis
// ============================================================================

std::string FontAnalysis::summary() const {
    std::ostringstream oss;
    oss << supportedFonts << "/" << totalFonts << " fonts have Turkish support";

    if (!commonMissing.empty()) {
        oss << ", missing: ";
        bool first = true;
        for (char32_t ch : commonMissing) {
            if (!first) oss << ", ";
            first = false;

            // Convert codepoint to readable name
            switch (ch) {
                case 0x00E7: oss << u8"ç"; break;
                case 0x00C7: oss << u8"Ç"; break;
                case 0x011F: oss << u8"ğ"; break;
                case 0x011E: oss << u8"Ğ"; break;
                case 0x0131: oss << u8"ı"; break;
                case 0x0130: oss << u8"İ"; break;
                case 0x00F6: oss << u8"ö"; break;
                case 0x00D6: oss << u8"Ö"; break;
                case 0x015F: oss << u8"ş"; break;
                case 0x015E: oss << u8"Ş"; break;
                case 0x00FC: oss << u8"ü"; break;
                case 0x00DC: oss << u8"Ü"; break;
                default: oss << "U+" << std::hex << static_cast<int>(ch); break;
            }
        }
    }

    return oss.str();
}

// ============================================================================
// FontManager
// ============================================================================

FontManager::FontManager() = default;
FontManager::~FontManager() = default;

FontFormat FontManager::detectFormat(const fs::path& path) const {
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".ttf") return FontFormat::TrueType;
    if (ext == ".otf") return FontFormat::OpenType;
    if (ext == ".fnt") return FontFormat::BitmapFont;
    if (ext == ".woff" || ext == ".woff2") return FontFormat::WebFont;

    // Check magic bytes for ambiguous files
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return FontFormat::Unknown;

    uint8_t magic[4]{};
    ifs.read(reinterpret_cast<char*>(magic), 4);
    if (!ifs) return FontFormat::Unknown;

    uint32_t sig = readU32BE(magic);
    if (sig == TTF_MAGIC || sig == TTC_MAGIC) return FontFormat::TrueType;
    if (sig == OTF_MAGIC) return FontFormat::OpenType;

    return FontFormat::Unknown;
}

std::set<char32_t> FontManager::parseTtfCmap(const fs::path& path) const {
    std::set<char32_t> glyphs;

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return glyphs;

    // Read entire font file
    ifs.seekg(0, std::ios::end);
    auto fileSize = ifs.tellg();
    if (fileSize <= 0 || fileSize > 50 * 1024 * 1024) return glyphs; // Max 50MB
    ifs.seekg(0);

    std::vector<uint8_t> data(static_cast<size_t>(fileSize));
    ifs.read(reinterpret_cast<char*>(data.data()), fileSize);
    if (!ifs) return glyphs;

    // Handle TTC (font collection) - use first font
    uint32_t fontOffset = 0;
    uint32_t sig = readU32BE(data.data());
    if (sig == TTC_MAGIC && data.size() >= 16) {
        fontOffset = readU32BE(data.data() + 12);
        if (fontOffset >= data.size()) return glyphs;
    }

    // Find cmap table
    uint32_t cmapOffset = 0, cmapLength = 0;

    // Temporarily adjust data view for table lookup
    std::vector<uint8_t> fontData(data.begin() + fontOffset, data.end());
    if (!findTable(fontData, "cmap", cmapOffset, cmapLength)) {
        return glyphs;
    }

    if (cmapOffset + cmapLength > fontData.size() || cmapLength < 4) return glyphs;

    const uint8_t* cmap = fontData.data() + cmapOffset;
    uint16_t numSubtables = readU16BE(cmap + 2);

    // Parse cmap subtables - prefer format 12 (full Unicode), fallback to format 4
    for (uint16_t i = 0; i < numSubtables; ++i) {
        size_t entryOffset = 4 + i * 8;
        if (entryOffset + 8 > cmapLength) break;

        uint16_t platformId = readU16BE(cmap + entryOffset);
        // uint16_t encodingId = readU16BE(cmap + entryOffset + 2);
        uint32_t subtableOffset = readU32BE(cmap + entryOffset + 4);

        if (subtableOffset >= cmapLength) continue;

        const uint8_t* subtable = cmap + subtableOffset;
        size_t subtableLen = cmapLength - subtableOffset;

        if (subtableLen < 2) continue;
        uint16_t format = readU16BE(subtable);

        // Platform 3 (Windows) or 0 (Unicode) are what we want
        if (platformId != 0 && platformId != 3) continue;

        if (format == 4) {
            parseCmapFormat4(subtable, subtableLen, glyphs);
        } else if (format == 12 && subtableLen >= 4) {
            // Format 12 uses a different header: format(2) + reserved(2) + length(4) + ...
            parseCmapFormat12(subtable, subtableLen, glyphs);
        }
    }

    return glyphs;
}

std::set<char32_t> FontManager::parseBmfontChars(const fs::path& path) const {
    std::set<char32_t> glyphs;

    std::ifstream ifs(path);
    if (!ifs) return glyphs;

    // BMFont text format: "char id=XX ..."
    std::string line;
    std::regex charIdRe(R"(char\s+id=(\d+))");

    while (std::getline(ifs, line)) {
        std::smatch match;
        if (std::regex_search(line, match, charIdRe)) {
            uint32_t id = std::stoul(match[1].str());
            glyphs.insert(static_cast<char32_t>(id));
        }
    }

    return glyphs;
}

std::set<char32_t> FontManager::getGlyphCoverage(const fs::path& fontPath) const {
    auto format = detectFormat(fontPath);

    if (format == FontFormat::TrueType || format == FontFormat::OpenType) {
        return parseTtfCmap(fontPath);
    }

    if (format == FontFormat::BitmapFont) {
        return parseBmfontChars(fontPath);
    }

    return {};
}

Result<FontInfo> FontManager::analyzeFontFile(const fs::path& fontPath) const {
    if (!fs::exists(fontPath)) {
        return std::unexpected(Error(ErrorCode::FileNotFound,
            "Font file not found: " + fontPath.string()));
    }

    FontInfo info;
    info.path = fontPath;
    info.format = detectFormat(fontPath);
    info.familyName = fontPath.stem().string();

    // Get glyph coverage
    auto coverage = getGlyphCoverage(fontPath);
    info.glyphCount = static_cast<uint32_t>(coverage.size());

    // Check Turkish characters
    info.hasTurkishSupport = true;
    for (char32_t ch : TURKISH_CHARS) {
        if (coverage.find(ch) == coverage.end()) {
            info.hasTurkishSupport = false;
            info.missingTurkishChars.push_back(ch);
        }
    }

    // Try to read font name from name table (TTF/OTF)
    if (info.format == FontFormat::TrueType || info.format == FontFormat::OpenType) {
        std::ifstream ifs(fontPath, std::ios::binary);
        if (ifs) {
            ifs.seekg(0, std::ios::end);
            auto sz = ifs.tellg();
            if (sz > 0 && sz < 50 * 1024 * 1024) {
                ifs.seekg(0);
                std::vector<uint8_t> data(static_cast<size_t>(sz));
                ifs.read(reinterpret_cast<char*>(data.data()), sz);

                uint32_t nameOffset = 0, nameLength = 0;
                if (findTable(data, "name", nameOffset, nameLength) && nameLength > 6) {
                    const uint8_t* nameTable = data.data() + nameOffset;
                    uint16_t count = readU16BE(nameTable + 2);
                    uint16_t stringOffset = readU16BE(nameTable + 4);

                    for (uint16_t i = 0; i < count; ++i) {
                        size_t recOff = 6 + i * 12;
                        if (recOff + 12 > nameLength) break;

                        uint16_t platformId = readU16BE(nameTable + recOff);
                        uint16_t nameId = readU16BE(nameTable + recOff + 6);
                        uint16_t strLen = readU16BE(nameTable + recOff + 8);
                        uint16_t strOff = readU16BE(nameTable + recOff + 10);

                        // Name ID 1 = Font Family, Name ID 2 = Font Subfamily
                        if (nameId == 1 && platformId == 1 && strLen > 0) {
                            size_t absOffset = nameOffset + stringOffset + strOff;
                            if (absOffset + strLen <= data.size()) {
                                info.familyName = std::string(
                                    reinterpret_cast<const char*>(data.data() + absOffset), strLen);
                            }
                        }
                        if (nameId == 2 && platformId == 1 && strLen > 0) {
                            size_t absOffset = nameOffset + stringOffset + strOff;
                            if (absOffset + strLen <= data.size()) {
                                info.styleName = std::string(
                                    reinterpret_cast<const char*>(data.data() + absOffset), strLen);
                            }
                        }
                    }
                }
            }
        }
    }

    return info;
}

Result<FontAnalysis> FontManager::analyzeGameFonts(const fs::path& gameDir) const {
    if (!fs::exists(gameDir) || !fs::is_directory(gameDir)) {
        return std::unexpected(Error(ErrorCode::DirectoryNotFound,
            "Game directory not found: " + gameDir.string()));
    }

    FontAnalysis analysis;
    std::set<char32_t> allMissing;
    bool firstFont = true;

    MAKINEAI_LOG_INFO(log::FONT, "Analyzing fonts in: {}", gameDir.string());

    // Recursively scan for font files
    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(gameDir, fs::directory_options::skip_permission_denied, ec)) {
        if (!entry.is_regular_file()) continue;
        if (!isFontFile(entry.path())) continue;

        auto result = analyzeFontFile(entry.path());
        if (!result) {
            MAKINEAI_LOG_WARN(log::FONT, "Failed to analyze font: {}", entry.path().string());
            continue;
        }

        auto& fontInfo = *result;
        analysis.fonts.push_back(fontInfo);
        analysis.totalFonts++;

        if (fontInfo.hasTurkishSupport) {
            analysis.supportedFonts++;
        }

        // Track missing chars
        if (firstFont) {
            for (char32_t ch : fontInfo.missingTurkishChars)
                allMissing.insert(ch);
            firstFont = false;
        } else {
            // Intersect: keep only chars missing in ALL fonts
            std::set<char32_t> intersection;
            for (char32_t ch : fontInfo.missingTurkishChars) {
                if (allMissing.count(ch)) intersection.insert(ch);
            }
            allMissing = intersection;
        }
    }

    analysis.commonMissing.assign(allMissing.begin(), allMissing.end());
    analysis.allFontsSupported = (analysis.totalFonts > 0 && analysis.supportedFonts == analysis.totalFonts);
    analysis.anyFontSupported = (analysis.supportedFonts > 0);

    MAKINEAI_LOG_INFO(log::FONT, "Font analysis complete: {}", analysis.summary());

    return analysis;
}

std::vector<BundledFont> FontManager::bundledFonts() const {
    return {
        {
            "Noto Sans",
            "OFL-1.1",
            "NotoSans-Regular.ttf",
            true, false,
            "General-purpose font with full Turkish support"
        },
        {
            "Inter",
            "OFL-1.1",
            "Inter-Regular.ttf",
            true, false,
            "Modern UI font optimized for screen readability"
        },
        {
            "Source Code Pro",
            "OFL-1.1",
            "SourceCodePro-Regular.ttf",
            true, false,
            "Monospace font for code and fixed-width text"
        },
        {
            "Press Start 2P",
            "OFL-1.1",
            "PressStart2P-Regular.ttf",
            true, true,
            "Pixel art font for retro-style games"
        },
    };
}

InjectionStrategy FontManager::strategyForEngine(GameEngine engine) const {
    switch (engine) {
        case GameEngine::RenPy:
            return InjectionStrategy::ConfigEdit;
        case GameEngine::RPGMaker:
        case GameEngine::RPGMaker_MV:
            return InjectionStrategy::CssOverride;
        case GameEngine::RPGMaker_VX:
            return InjectionStrategy::FileCopy;
        case GameEngine::Unity:
        case GameEngine::Unity_Mono:
        case GameEngine::Unity_IL2CPP:
            return InjectionStrategy::AssetReplace;
        case GameEngine::Godot:
            return InjectionStrategy::ResourcePack;
        case GameEngine::GameMaker:
            return InjectionStrategy::AssetReplace;
        default:
            return InjectionStrategy::None;
    }
}

Result<FontInjectionPlan> FontManager::createInjectionPlan(
    const GameInfo& game,
    const FontAnalysis& analysis
) const {
    if (analysis.allFontsSupported) {
        return std::unexpected(Error(ErrorCode::NotSupported,
            "All game fonts already support Turkish characters"));
    }

    FontInjectionPlan plan;
    plan.engine = game.engine;
    plan.strategy = strategyForEngine(game.engine);

    if (plan.strategy == InjectionStrategy::None) {
        return std::unexpected(Error(ErrorCode::NotSupported,
            "Font injection not supported for this engine"));
    }

    // Select the best bundled font
    auto fonts = bundledFonts();
    plan.recommendedFont = fonts[0]; // Default: Noto Sans

    // For pixel games, prefer pixel font
    for (const auto& font : analysis.fonts) {
        if (font.format == FontFormat::BitmapFont || font.format == FontFormat::PixelFont) {
            for (const auto& bundled : fonts) {
                if (bundled.isPixelFont) {
                    plan.recommendedFont = bundled;
                    break;
                }
            }
            break;
        }
    }

    fs::path gameDir = game.installPath;

    // Determine target path based on engine
    switch (game.engine) {
        case GameEngine::RenPy:
            plan.targetPath = gameDir / "game" / plan.recommendedFont.filename;
            plan.configPatch = "style default:\n    font \"" + plan.recommendedFont.filename + "\"\n";
            break;

        case GameEngine::RPGMaker_MV:
        case GameEngine::RPGMaker:
            plan.targetPath = gameDir / "www" / "fonts" / plan.recommendedFont.filename;
            plan.configPatch = "@font-face {\n"
                "    font-family: \"GameFont\";\n"
                "    src: url(\"fonts/" + plan.recommendedFont.filename + "\");\n"
                "}\n";
            plan.filesToModify.push_back(gameDir / "www" / "fonts" / "gamefont.css");
            break;

        case GameEngine::RPGMaker_VX:
            plan.targetPath = gameDir / "Fonts" / plan.recommendedFont.filename;
            break;

        case GameEngine::Unity:
        case GameEngine::Unity_Mono:
        case GameEngine::Unity_IL2CPP:
            // Unity font replacement requires runtime approach (XUnity handles this)
            plan.targetPath = gameDir / "BepInEx" / "Translation" / "fonts" / plan.recommendedFont.filename;
            break;

        case GameEngine::Godot:
            plan.targetPath = gameDir / plan.recommendedFont.filename;
            break;

        case GameEngine::GameMaker:
            plan.targetPath = gameDir / plan.recommendedFont.filename;
            break;

        default:
            break;
    }

    return plan;
}

VoidResult FontManager::applyFontInjection(
    const GameInfo& game,
    const FontInjectionPlan& plan,
    ProgressCallback progress
) {
    if (progress) progress(0.0f);

    // Check source font exists in bundle
    fs::path sourceFont = fontBundleDir_ / plan.recommendedFont.filename;
    if (!fs::exists(sourceFont)) {
        return std::unexpected(Error(ErrorCode::FileNotFound,
            "Bundled font not found: " + sourceFont.string()));
    }

    MAKINEAI_LOG_INFO(log::FONT, "Applying font injection: {} -> {}",
        plan.recommendedFont.name, plan.targetPath.string());

    // Create target directory
    std::error_code ec;
    fs::create_directories(plan.targetPath.parent_path(), ec);
    if (ec) {
        return std::unexpected(Error(ErrorCode::FileCreateFailed,
            "Failed to create font directory: " + ec.message()));
    }

    if (progress) progress(0.2f);

    // Copy font file
    fs::copy_file(sourceFont, plan.targetPath, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        return std::unexpected(Error(ErrorCode::FileWriteFailed,
            "Failed to copy font: " + ec.message()));
    }

    if (progress) progress(0.5f);

    // Apply engine-specific config changes
    fs::path gameDir = game.installPath;
    VoidResult configResult;

    switch (plan.strategy) {
        case InjectionStrategy::ConfigEdit:
            configResult = injectRenPyFont(gameDir, plan.recommendedFont);
            break;
        case InjectionStrategy::CssOverride:
            configResult = injectRpgMakerFont(gameDir, plan.recommendedFont);
            break;
        default:
            // FileCopy and AssetReplace: font is already copied, nothing more needed
            break;
    }

    if (configResult && !configResult.has_value()) {
        return configResult;
    }

    if (progress) progress(1.0f);

    MAKINEAI_LOG_INFO(log::FONT, "Font injection complete: {}", plan.recommendedFont.name);
    return {};
}

VoidResult FontManager::removeFontInjection(const GameInfo& game) {
    fs::path gameDir = game.installPath;

    // Check for MakineAI font marker file
    fs::path markerPath = gameDir / ".makineai_font";
    if (!fs::exists(markerPath)) {
        return std::unexpected(Error(ErrorCode::NotPatched,
            "No font injection found for this game"));
    }

    // Read marker to find injected files
    std::ifstream marker(markerPath);
    if (!marker) {
        return std::unexpected(Error(ErrorCode::FileNotFound, "Cannot read font marker"));
    }

    std::string line;
    std::error_code ec;
    while (std::getline(marker, line)) {
        if (!line.empty() && fs::exists(line)) {
            fs::remove(line, ec);
        }
    }
    marker.close();

    fs::remove(markerPath, ec);

    MAKINEAI_LOG_INFO(log::FONT, "Font injection removed from: {}", gameDir.string());
    return {};
}

// ============================================================================
// Engine-Specific Injection
// ============================================================================

VoidResult FontManager::injectRenPyFont(const fs::path& gameDir, const BundledFont& font) {
    // Ren'Py: Add font override to game/tl/turkish/style.rpy
    fs::path tlDir = gameDir / "game" / "tl" / "turkish";
    std::error_code ec;
    fs::create_directories(tlDir, ec);

    fs::path stylePath = tlDir / "style.rpy";

    std::ofstream ofs(stylePath);
    if (!ofs) {
        return std::unexpected(Error(ErrorCode::FileWriteFailed,
            "Cannot write Ren'Py style file"));
    }

    ofs << "# MakineAI Turkish font override\n"
        << "translate turkish style default:\n"
        << "    font \"" << font.filename << "\"\n"
        << "\n"
        << "translate turkish style say_dialogue:\n"
        << "    font \"" << font.filename << "\"\n"
        << "\n"
        << "translate turkish style say_label:\n"
        << "    font \"" << font.filename << "\"\n";

    // Write marker file for removal
    std::ofstream marker(gameDir / ".makineai_font");
    if (marker) {
        marker << (gameDir / "game" / font.filename).string() << "\n";
        marker << stylePath.string() << "\n";
    }

    MAKINEAI_LOG_INFO(log::FONT, "Ren'Py font style written to: {}", stylePath.string());
    return {};
}

VoidResult FontManager::injectRpgMakerFont(const fs::path& gameDir, const BundledFont& font) {
    // RPG Maker MV/MZ: Override gamefont.css
    fs::path cssPath = gameDir / "www" / "fonts" / "gamefont.css";
    if (!fs::exists(cssPath)) {
        // Try MZ layout
        cssPath = gameDir / "fonts" / "gamefont.css";
    }

    if (!fs::exists(cssPath)) {
        return std::unexpected(Error(ErrorCode::FileNotFound,
            "gamefont.css not found"));
    }

    // Backup original
    fs::path backupPath = cssPath;
    backupPath += ".makineai_backup";
    std::error_code ec;
    if (!fs::exists(backupPath)) {
        fs::copy_file(cssPath, backupPath, ec);
    }

    // Write new CSS
    std::ofstream ofs(cssPath);
    if (!ofs) {
        return std::unexpected(Error(ErrorCode::FileWriteFailed,
            "Cannot write gamefont.css"));
    }

    ofs << "/* MakineAI Turkish font override */\n"
        << "@font-face {\n"
        << "    font-family: \"GameFont\";\n"
        << "    src: url(\"" << font.filename << "\") format(\"truetype\");\n"
        << "}\n";

    // Write marker
    std::ofstream marker(gameDir / ".makineai_font");
    if (marker) {
        marker << (cssPath.parent_path() / font.filename).string() << "\n";
        marker << cssPath.string() << " (backup: " << backupPath.string() << ")\n";
    }

    MAKINEAI_LOG_INFO(log::FONT, "RPG Maker gamefont.css overridden");
    return {};
}

VoidResult FontManager::injectUnityFont(const fs::path& gameDir, const BundledFont& font) {
    // Unity: XUnity.AutoTranslator handles font replacement via config
    // Just ensure the font is in the right directory
    MAKINEAI_LOG_INFO(log::FONT, "Unity font placed in Translation/fonts directory");
    return {};
}

VoidResult FontManager::injectGodotFont(const fs::path& gameDir, const BundledFont& font) {
    // Godot: Font can be loaded via override.cfg or project.godot
    MAKINEAI_LOG_INFO(log::FONT, "Godot font placed in game directory");
    return {};
}

VoidResult FontManager::injectGameMakerFont(const fs::path& gameDir, const BundledFont& font) {
    // GameMaker: Font replacement requires data.win modification
    MAKINEAI_LOG_INFO(log::FONT, "GameMaker font placed in game directory");
    return {};
}

} // namespace makineai
