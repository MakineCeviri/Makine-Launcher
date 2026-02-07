/**
 * @file font_manager.hpp
 * @brief Font analysis and Turkish character injection
 * @copyright (c) 2026 MakineAI Team
 *
 * Analyzes game fonts for Turkish character coverage and provides
 * engine-specific font injection for missing glyphs.
 */

#pragma once

#include "types.hpp"
#include "error.hpp"

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace makineai {

// Required Turkish characters beyond basic ASCII
// ç, Ç, ğ, Ğ, ı, İ, ö, Ö, ş, Ş, ü, Ü
inline constexpr char32_t TURKISH_CHARS[] = {
    0x00E7, // ç
    0x00C7, // Ç
    0x011F, // ğ
    0x011E, // Ğ
    0x0131, // ı (dotless i)
    0x0130, // İ (dotted I)
    0x00F6, // ö
    0x00D6, // Ö
    0x015F, // ş
    0x015E, // Ş
    0x00FC, // ü
    0x00DC, // Ü
};
inline constexpr size_t TURKISH_CHAR_COUNT = std::size(TURKISH_CHARS);

/**
 * @brief Font format type
 */
enum class FontFormat {
    Unknown,
    TrueType,       // .ttf
    OpenType,       // .otf
    BitmapFont,     // .fnt (BMFont format)
    SDF,            // Signed Distance Field (Unity TMP)
    MSDF,           // Multi-channel SDF
    WebFont,        // .woff/.woff2
    PixelFont,      // Raster/pixel font
};

/**
 * @brief Information about a single font file
 */
struct FontInfo {
    fs::path path;
    std::string familyName;
    std::string styleName;      // Regular, Bold, Italic, etc.
    FontFormat format{FontFormat::Unknown};
    uint32_t glyphCount{0};
    bool hasTurkishSupport{false};
    std::vector<char32_t> missingTurkishChars;
};

/**
 * @brief Result of analyzing all fonts in a game
 */
struct FontAnalysis {
    std::vector<FontInfo> fonts;
    bool allFontsSupported{false};    // All fonts have Turkish chars
    bool anyFontSupported{false};     // At least one font has Turkish chars
    size_t totalFonts{0};
    size_t supportedFonts{0};
    std::vector<char32_t> commonMissing;  // Chars missing across all fonts

    /// Summary string: "3/5 fonts have Turkish support, missing: ğ, İ, ş"
    [[nodiscard]] std::string summary() const;
};

/**
 * @brief Bundled replacement font info
 */
struct BundledFont {
    std::string name;           // "Noto Sans"
    std::string license;        // "OFL-1.1"
    std::string filename;       // "NotoSans-Regular.ttf"
    bool hasFullTurkish{true};
    bool isPixelFont{false};
    std::string description;    // "General-purpose font with full Turkish support"
};

/**
 * @brief Engine-specific font injection strategy
 */
enum class InjectionStrategy {
    None,           // Cannot inject (unknown engine)
    FileCopy,       // Copy font file to game directory
    CssOverride,    // Modify CSS (RPG Maker, web-based)
    ConfigEdit,     // Edit config file (Ren'Py style.rpy)
    AssetReplace,   // Replace font asset (Unity, GameMaker)
    ResourcePack,   // Add to resource pack (Godot .import)
};

/**
 * @brief Font injection plan for a specific game
 */
struct FontInjectionPlan {
    GameEngine engine;
    InjectionStrategy strategy;
    BundledFont recommendedFont;
    fs::path targetPath;         // Where font goes in game directory
    std::string configPatch;     // Config changes needed (if any)
    std::vector<fs::path> filesToModify;
};

/**
 * @brief Manages font analysis and injection
 */
class FontManager {
public:
    FontManager();
    ~FontManager();

    /**
     * @brief Analyze all fonts in a game directory
     *
     * Scans for TTF/OTF/FNT files and checks Turkish character coverage.
     */
    [[nodiscard]] Result<FontAnalysis> analyzeGameFonts(const fs::path& gameDir) const;

    /**
     * @brief Analyze a single font file for Turkish character support
     */
    [[nodiscard]] Result<FontInfo> analyzeFontFile(const fs::path& fontPath) const;

    /**
     * @brief Check if a TTF/OTF file contains specific Unicode codepoints
     *
     * Reads the cmap table from the font to determine glyph coverage.
     */
    [[nodiscard]] std::set<char32_t> getGlyphCoverage(const fs::path& fontPath) const;

    /**
     * @brief Get list of bundled replacement fonts
     */
    [[nodiscard]] std::vector<BundledFont> bundledFonts() const;

    /**
     * @brief Create a font injection plan for a game
     *
     * Determines the best strategy based on engine type and missing characters.
     */
    [[nodiscard]] Result<FontInjectionPlan> createInjectionPlan(
        const GameInfo& game,
        const FontAnalysis& analysis
    ) const;

    /**
     * @brief Apply font injection to a game
     *
     * Copies the replacement font and modifies config files as needed.
     */
    [[nodiscard]] VoidResult applyFontInjection(
        const GameInfo& game,
        const FontInjectionPlan& plan,
        ProgressCallback progress = nullptr
    );

    /**
     * @brief Remove injected font from a game
     */
    [[nodiscard]] VoidResult removeFontInjection(const GameInfo& game);

    /**
     * @brief Set directory containing bundled font files
     */
    void setFontBundleDirectory(const fs::path& dir) { fontBundleDir_ = dir; }

    /**
     * @brief Get font bundle directory
     */
    [[nodiscard]] fs::path fontBundleDirectory() const { return fontBundleDir_; }

private:
    fs::path fontBundleDir_;

    // TTF/OTF parsing helpers
    [[nodiscard]] FontFormat detectFormat(const fs::path& path) const;
    [[nodiscard]] std::set<char32_t> parseTtfCmap(const fs::path& path) const;
    [[nodiscard]] std::set<char32_t> parseBmfontChars(const fs::path& path) const;

    // Engine-specific injection
    [[nodiscard]] InjectionStrategy strategyForEngine(GameEngine engine) const;
    [[nodiscard]] VoidResult injectRenPyFont(const fs::path& gameDir, const BundledFont& font);
    [[nodiscard]] VoidResult injectRpgMakerFont(const fs::path& gameDir, const BundledFont& font);
    [[nodiscard]] VoidResult injectUnityFont(const fs::path& gameDir, const BundledFont& font);
    [[nodiscard]] VoidResult injectGodotFont(const fs::path& gameDir, const BundledFont& font);
    [[nodiscard]] VoidResult injectGameMakerFont(const fs::path& gameDir, const BundledFont& font);
};

} // namespace makineai
