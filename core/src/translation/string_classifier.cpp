/**
 * @file string_classifier.cpp
 * @brief Implementation of string classification for translation filtering
 */

#include "makineai/string_classifier.hpp"
#include <algorithm>
#include <cctype>
#include <iterator>
#include <regex>
#include <unordered_set>
#include <spdlog/spdlog.h>

namespace makineai {

// Common English words for detection
static const std::unordered_set<std::string> COMMON_ENGLISH_WORDS = {
    "the", "be", "to", "of", "and", "a", "in", "that", "have", "i",
    "it", "for", "not", "on", "with", "he", "as", "you", "do", "at",
    "this", "but", "his", "by", "from", "they", "we", "say", "her", "she",
    "or", "an", "will", "my", "one", "all", "would", "there", "their", "what",
    "so", "up", "out", "if", "about", "who", "get", "which", "go", "me",
    "when", "make", "can", "like", "time", "no", "just", "him", "know", "take",
    "people", "into", "year", "your", "good", "some", "could", "them", "see", "other",
    "than", "then", "now", "look", "only", "come", "its", "over", "think", "also",
    "back", "after", "use", "two", "how", "our", "work", "first", "well", "way",
    "even", "new", "want", "because", "any", "these", "give", "day", "most", "us",
    // Game-specific
    "level", "player", "game", "score", "health", "damage", "attack", "defense",
    "item", "quest", "mission", "objective", "inventory", "weapon", "armor", "skill",
    "press", "click", "select", "continue", "start", "exit", "menu", "options",
    "save", "load", "new", "settings", "pause", "resume", "quit", "yes", "no",
    "ok", "cancel", "confirm", "back", "next", "previous", "help", "tutorial"
};

// Technical/code keywords to exclude
static const std::unordered_set<std::string> CODE_KEYWORDS = {
    "null", "nullptr", "void", "int", "float", "double", "bool", "string",
    "class", "struct", "enum", "public", "private", "protected", "static",
    "const", "virtual", "override", "return", "if", "else", "for", "while",
    "switch", "case", "break", "continue", "try", "catch", "throw", "new",
    "delete", "this", "base", "true", "false", "namespace", "using", "include",
    "define", "ifdef", "ifndef", "endif", "pragma", "template", "typename",
    "function", "var", "let", "async", "await", "import", "export", "default",
    "module", "require", "extends", "implements", "interface", "abstract"
};

// ============================================================================
// ClassificationRules Implementation
// ============================================================================

bool ClassificationRules::looksLikeCode(const std::string& text) {
    // Check for code patterns
    static const std::regex codePatterns[] = {
        std::regex(R"(\w+\s*[=!<>]+\s*\w+)"),           // assignments/comparisons
        std::regex(R"(\w+\s*\(\s*\))"),                  // function calls
        std::regex(R"(\w+\.\w+\.\w+)"),                  // chained properties
        std::regex(R"(\{\s*\w+\s*:\s*\w+)"),            // object literals
        std::regex(R"(^\s*[/\*#])"),                     // comments
        std::regex(R"(\w+\s*->\s*\w+)"),                // arrow operator
        std::regex(R"(::\w+)"),                          // scope resolution
        std::regex(R"(\[\s*\d+\s*\])"),                  // array indexing
        std::regex(R"(0x[0-9a-fA-F]+)"),                // hex numbers
        std::regex(R"(\w+<\w+>)"),                       // templates/generics
    };

    for (const auto& pattern : codePatterns) {
        if (std::regex_search(text, pattern)) {
            return true;
        }
    }

    // Check for high concentration of code keywords
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    int keywordCount = 0;
    for (const auto& keyword : CODE_KEYWORDS) {
        if (lower.find(keyword) != std::string::npos) {
            keywordCount++;
        }
    }

    return keywordCount >= 3;
}

bool ClassificationRules::looksLikeFilePath(const std::string& text) {
    // Exclude .NET constructor names
    if (text == ".ctor" || text == ".cctor") return false;

    // Must have at least some path-like structure
    bool hasSlash = text.find('/') != std::string::npos || text.find('\\') != std::string::npos;
    bool hasExtension = false;

    static const std::regex extensionPattern(R"(\.(png|jpg|jpeg|gif|bmp|wav|mp3|ogg|flac|prefab|asset|mat|shader|cs|js|json|xml|txt|dll|exe|so|bundle|unity|meta|anim|controller|fbx|obj|blend)$)", std::regex::icase);
    hasExtension = std::regex_search(text, extensionPattern);

    // If no slash and no recognized extension, not a path
    if (!hasSlash && !hasExtension) return false;

    static const std::regex pathPatterns[] = {
        std::regex(R"([A-Za-z]:[/\\])"),                // Windows absolute path
        std::regex(R"(^[/\\])"),                         // Unix absolute path
        std::regex(R"([/\\]\w+[/\\]\w+)"),              // Directory structure
        std::regex(R"(\.\./)"),                          // Relative path
        std::regex(R"(Assets[/\\])"),                   // Unity assets
        std::regex(R"(Resources[/\\])"),                // Resources folder
    };

    for (const auto& pattern : pathPatterns) {
        if (std::regex_search(text, pattern)) {
            return true;
        }
    }

    return hasExtension; // Has a recognized file extension
}

bool ClassificationRules::looksLikeURL(const std::string& text) {
    static const std::regex urlPattern(
        R"((https?://|www\.|ftp://|mailto:)[\w\-._~:/?#\[\]@!$&'()*+,;=%]+)",
        std::regex::icase
    );
    return std::regex_search(text, urlPattern);
}

bool ClassificationRules::looksLikeIdentifier(const std::string& text) {
    // Must be single "word" (no spaces)
    if (text.find(' ') != std::string::npos) {
        return false;
    }

    // .NET/C# compiler-generated names
    if (text.find("<>") != std::string::npos) return true;
    if (text.find("`") != std::string::npos) return true;
    if (text.find("__") != std::string::npos) return true;
    if (text.length() >= 2 && text[0] == '<' && text.back() == '>') return true;

    // .NET special names
    if (text == ".ctor" || text == ".cctor") return true;
    if (text.find("AnonymousType") != std::string::npos) return true;

    // .NET framework namespaces and types
    static const std::vector<std::string> dotnetPrefixes = {
        "System.", "Mono.", "Microsoft.", "Unity.", "UnityEngine.",
        "Newtonsoft.", "mscorlib", "netstandard"
    };
    for (const auto& prefix : dotnetPrefixes) {
        if (text.find(prefix) == 0 || text == prefix.substr(0, prefix.length() - 1)) {
            return true;
        }
    }

    // .NET exception/error resource identifiers
    static const std::regex dotnetResourcePatterns[] = {
        std::regex(R"(^(Argument|NotSupported|Invalid|Format|Object|IO|Security|Access|Null)_\w+$)"),
        std::regex(R"(_Exception$)"),
        std::regex(R"(^Arg_\w+$)"),
        std::regex(R"(^Exc_\w+$)"),
        std::regex(R"(^(Cannot|Could|Failed|Unable|Missing|Invalid)\w+$)"),
        // Event handlers
        std::regex(R"(^(add_|remove_|get_|set_)\w+$)"),
        // Resource identifiers with underscores
        std::regex(R"(\w+(Access|Disposed|Exception|Error|Closed)_\w+)"),
        std::regex(R"(^(Unauthorized|Object|File|Stream|Socket|Registry|Crypto)\w*_\w+$)"),
    };
    for (const auto& pattern : dotnetResourcePatterns) {
        if (std::regex_match(text, pattern)) {
            return true;
        }
    }

    // C#/C++ field naming conventions
    if (text.length() >= 2) {
        // s_ prefix (static fields), g_ (global), k_ (constants), E_ (error codes)
        if ((text[0] == 's' || text[0] == 'g' || text[0] == 'k' || text[0] == 'E') && text[1] == '_') {
            return true;
        }
        // Hungarian notation prefixes
        if (text.length() >= 3 && text[0] == 'p' && std::isupper(text[1])) {
            return true;  // pMyVariable
        }
    }

    // Strings with underscore and no spaces - check if it's an identifier
    if (text.find('_') != std::string::npos && text.find(' ') == std::string::npos) {
        std::string lower = text;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        // Count underscores - many underscores = likely identifier
        int underscoreCount = std::count(text.begin(), text.end(), '_');
        if (underscoreCount >= 3) return true;

        // Technical patterns with underscores
        static const std::regex techPatterns[] = {
            std::regex(R"(^[A-Z][a-z]+_[A-Z][a-z]+_[A-Z])"),  // Pascal_Case_Identifier
            std::regex(R"(_[A-Z]{2,}_)"),                      // Contains CAPS between underscores
            std::regex(R"(^(On|Get|Set|Is|Has|Do|Create|Destroy|Init|Update)_)"),
            std::regex(R"(_(Handler|Manager|Controller|Factory|Provider|Service)$)"),
            std::regex(R"(_(Impl|Internal|Native|Cached|Default)$)"),
        };
        for (const auto& pattern : techPatterns) {
            if (std::regex_search(text, pattern)) {
                return true;
            }
        }

        // Split by underscore and check for common words
        bool hasCommonWord = false;
        int partCount = 0;
        size_t pos = 0;
        while (pos < lower.length()) {
            size_t next = lower.find('_', pos);
            if (next == std::string::npos) next = lower.length();
            std::string part = lower.substr(pos, next - pos);
            if (part.length() >= 3) {
                partCount++;
                if (COMMON_ENGLISH_WORDS.count(part) > 0) {
                    hasCommonWord = true;
                }
            }
            pos = next + 1;
        }

        // If 2+ parts and no common words, likely identifier
        if (partCount >= 2 && !hasCommonWord) {
            return true;
        }
    }

    // Additional .NET identifier patterns - single words with mixed case that look like type names
    // Check for PascalCase with capitals in the middle (likely .NET types)
    if (text.find('_') == std::string::npos && text.find(' ') == std::string::npos) {
        int upperCount = 0;
        int lowerCount = 0;
        for (char c : text) {
            if (std::isupper(c)) upperCount++;
            else if (std::islower(c)) lowerCount++;
        }
        // Multiple capitals scattered through the word = likely identifier
        if (upperCount >= 2 && lowerCount >= 2 && upperCount < lowerCount) {
            // Check if it's a common word before rejecting
            std::string lower = text;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (COMMON_ENGLISH_WORDS.count(lower) == 0) {
                // PascalCase or camelCase identifier
                return true;
            }
        }
    }

    // Method signatures
    if (text.find("::") != std::string::npos) return true;
    if (text.find(".get_") != std::string::npos) return true;
    if (text.find(".set_") != std::string::npos) return true;
    if (text.find("op_") == 0) return true;  // operator overloads

    // Namespace-style names
    if (std::count(text.begin(), text.end(), '.') >= 2) return true;

    // Pure identifier patterns
    static const std::regex identifierPatterns[] = {
        std::regex(R"(^[a-z][a-zA-Z0-9]*$)"),          // camelCase
        std::regex(R"(^[A-Z][a-zA-Z0-9]*$)"),          // PascalCase
        std::regex(R"(^[a-z_][a-z0-9_]*$)"),           // snake_case
        std::regex(R"(^[A-Z_][A-Z0-9_]*$)"),           // SCREAMING_SNAKE
        std::regex(R"(^[a-z]+-[a-z]+(-[a-z]+)*$)"),    // kebab-case
        std::regex(R"(^m_\w+$)"),                       // member variable
        std::regex(R"(^_\w+$)"),                        // private variable
        std::regex(R"(^[A-Z]{2,}$)"),                   // Acronym
        std::regex(R"(^\w+_\d+$)"),                     // name_123
        std::regex(R"(^[a-z]+\d+$)"),                   // name123
    };

    for (const auto& pattern : identifierPatterns) {
        if (std::regex_match(text, pattern)) {
            return true;
        }
    }
    return false;
}

bool ClassificationRules::looksLikeDebug(const std::string& text) {
    static const std::regex debugPatterns[] = {
        std::regex(R"(\[DEBUG\]|\[INFO\]|\[WARN\]|\[ERROR\])", std::regex::icase),
        std::regex(R"(^(Debug|Log|Print|Console)\s*[:\.])", std::regex::icase),
        std::regex(R"(Exception|StackTrace|NullReference)", std::regex::icase),
        std::regex(R"(at\s+\w+\.\w+\()", std::regex::icase),  // stack trace
        std::regex(R"(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2})"), // timestamps
        std::regex(R"(^\s*#\d+)"),                       // line numbers
    };

    for (const auto& pattern : debugPatterns) {
        if (std::regex_search(text, pattern)) {
            return true;
        }
    }
    return false;
}

bool ClassificationRules::looksLikeMarkup(const std::string& text) {
    static const std::regex markupPatterns[] = {
        std::regex(R"(<[a-zA-Z/][^>]*>)"),              // HTML/XML tags
        std::regex(R"(\{"\w+":\s*)"),                   // JSON
        std::regex(R"(^\s*\[[\w\s]+\]\s*$)"),          // INI sections
        std::regex(R"(^\s*\w+\s*=\s*".+"\s*$)"),       // INI values
        std::regex(R"(<!--.*-->)"),                     // HTML comments
        std::regex(R"(<!\[CDATA\[)"),                   // CDATA
    };

    for (const auto& pattern : markupPatterns) {
        if (std::regex_search(text, pattern)) {
            return true;
        }
    }

    // Count angle brackets - too many suggests markup
    int brackets = std::count(text.begin(), text.end(), '<') +
                   std::count(text.begin(), text.end(), '>');
    return brackets > 4;
}

bool ClassificationRules::looksLikeNumeric(const std::string& text) {
    // Remove spaces and check if mostly numeric
    std::string stripped;
    std::copy_if(text.begin(), text.end(), std::back_inserter(stripped),
                 [](char c) { return !std::isspace(c); });

    if (stripped.empty()) return false;

    int digits = std::count_if(stripped.begin(), stripped.end(), ::isdigit);
    int total = static_cast<int>(stripped.length());

    // More than 70% digits
    if (digits > total * 0.7) {
        return true;
    }

    // Version numbers
    static const std::regex versionPattern(R"(^v?\d+(\.\d+)+$)", std::regex::icase);
    if (std::regex_match(stripped, versionPattern)) {
        return true;
    }

    return false;
}

bool ClassificationRules::looksLikeGarbage(const std::string& text) {
    if (text.empty()) return true;

    int total = static_cast<int>(text.length());

    // Count different character types
    int printable = 0;
    int control = 0;
    int highByte = 0;
    int alphanumeric = 0;
    int letters = 0;
    int punctuation = 0;

    for (unsigned char c : text) {
        if (c < 32 && c != '\n' && c != '\r' && c != '\t') {
            control++;
        } else if (c > 127) {
            highByte++;
        } else if (std::isprint(c)) {
            printable++;
            if (std::isalnum(c)) {
                alphanumeric++;
                if (std::isalpha(c)) letters++;
            } else if (!std::isspace(c)) {
                punctuation++;
            }
        }
    }

    // Too many control characters
    if (control > total * 0.1) return true;

    // Too many high bytes (unless it's UTF-8)
    if (highByte > total * 0.5 && !containsEnglishWords(text)) return true;

    // Too few alphanumeric
    if (alphanumeric < total * 0.3 && total > 10) return true;

    // Short strings with too much punctuation (like "P)p", "!@#")
    if (total <= 6 && punctuation >= letters) return true;
    if (total <= 4 && punctuation > 0 && letters < 3) return true;

    // Too few letters for short strings
    if (total <= 5 && letters < 2) return true;

    // Repeated characters
    if (text.length() > 5) {
        bool allSame = std::all_of(text.begin() + 1, text.end(),
                                    [&](char c) { return c == text[0]; });
        if (allSame) return true;
    }

    // Alternating patterns like "CcCcCc" or repeated pairs
    if (total >= 6) {
        bool isPairs = true;
        for (size_t i = 0; i + 1 < text.length(); i += 2) {
            if (i + 2 < text.length() && text[i] != text[i + 2]) {
                isPairs = false;
                break;
            }
        }
        if (isPairs && !containsEnglishWords(text)) return true;
    }

    // Check for character class strings like "LmLoLtLuMcMeMnNl"
    // (alternating upper-lower pairs with no recognizable words)
    if (total >= 8) {
        int upperLowerPairs = 0;
        for (size_t i = 0; i + 1 < text.length(); ++i) {
            if (std::isupper(text[i]) && std::islower(text[i+1])) {
                upperLowerPairs++;
            }
        }
        // If more than 60% of the string is upper-lower pairs, likely garbage
        if (upperLowerPairs >= total / 3 && !containsEnglishWords(text)) {
            return true;
        }
    }

    return false;
}

bool ClassificationRules::looksLikeDialogue(const std::string& text) {
    // Dialogue indicators
    if (text.empty() || text.length() < 3) return false;

    // Has proper sentence structure
    bool startsUpper = std::isupper(text[0]);
    bool endsPunctuation = text.back() == '.' || text.back() == '!' ||
                          text.back() == '?' || text.back() == '"' ||
                          text.back() == '\'' || text.back() == ')';

    // Contains common dialogue patterns
    static const std::regex dialoguePatterns[] = {
        std::regex(R"(^["'].*["']$)"),                  // Quoted text
        std::regex(R"(\.\.\.)"),                        // Ellipsis
        std::regex(R"(\?$)"),                           // Questions
        std::regex(R"(!$)"),                            // Exclamations
        std::regex(R"(^(I|You|We|They|He|She|It)\s)", std::regex::icase),
    };

    for (const auto& pattern : dialoguePatterns) {
        if (std::regex_search(text, pattern)) {
            return true;
        }
    }

    // Multiple words with proper capitalization
    int words = countWords(text);
    return words >= 3 && startsUpper && (endsPunctuation || words >= 5);
}

bool ClassificationRules::looksLikeUIText(const std::string& text) {
    // Short, title-case text is likely UI
    if (text.empty()) return false;
    if (text.length() < 2) return false;

    int words = countWords(text);
    if (words < 1 || words > 6) return false;

    // Must have reasonable alphanumeric ratio (no cryptic symbols)
    int alphaCount = 0;
    int specialCount = 0;
    for (char c : text) {
        if (std::isalpha(c)) alphaCount++;
        else if (!std::isspace(c) && !std::isalnum(c)) specialCount++;
    }

    // Reject if too few letters or too many special chars
    if (alphaCount < static_cast<int>(text.length()) * 0.6) return false;
    if (specialCount > 2) return false;

    // Reject compiler-generated names
    if (text.find("__") != std::string::npos) return false;
    if (text.find("<>") != std::string::npos) return false;
    if (text.find("`") != std::string::npos) return false;

    // Reject mixed case runs like "CcCfCnCo" (character class lists)
    if (text.length() > 4) {
        int caseChanges = 0;
        for (size_t i = 1; i < text.length(); ++i) {
            if (std::islower(text[i-1]) && std::isupper(text[i])) caseChanges++;
            if (std::isupper(text[i-1]) && std::islower(text[i])) caseChanges++;
        }
        // Too many case changes per character = likely code
        if (caseChanges > static_cast<int>(text.length()) / 3) return false;
    }

    // Common UI keywords (exact or partial match)
    static const std::unordered_set<std::string> uiKeywords = {
        "start", "new", "load", "save", "options", "settings", "quit", "exit",
        "continue", "resume", "pause", "menu", "help", "tutorial", "credits",
        "back", "next", "ok", "cancel", "yes", "no", "confirm", "apply",
        "play", "stop", "retry", "skip", "close", "open", "select", "choose",
        "game", "level", "score", "player", "health", "mana", "gold", "item"
    };

    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    for (const auto& keyword : uiKeywords) {
        if (lower == keyword || lower.find(keyword) != std::string::npos) {
            return true;
        }
    }

    // For single words, must match a keyword or be a common English word
    if (words == 1) {
        // Reject short single words without keyword match
        if (text.length() < 4) return false;
        // Must be a known English word
        return containsEnglishWords(text);
    }

    // Multi-word: title case with spaces, mostly letters
    bool hasSpace = text.find(' ') != std::string::npos;
    bool startsUpper = std::isupper(text[0]);

    return hasSpace && startsUpper && containsEnglishWords(text);
}

bool ClassificationRules::looksLikeItemName(const std::string& text) {
    int words = countWords(text);
    if (words < 1 || words > 6) return false;

    // Item patterns
    static const std::unordered_set<std::string> itemKeywords = {
        "sword", "shield", "armor", "helmet", "boots", "gloves", "ring",
        "potion", "elixir", "scroll", "staff", "wand", "bow", "arrow",
        "key", "gem", "coin", "gold", "silver", "bronze", "iron", "steel",
        "magic", "fire", "ice", "lightning", "holy", "dark", "cursed",
        "rare", "epic", "legendary", "common", "uncommon", "mythic"
    };

    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    for (const auto& keyword : itemKeywords) {
        if (lower.find(keyword) != std::string::npos) {
            return true;
        }
    }

    // Title case, 2-4 words, no punctuation
    bool titleCase = std::isupper(text[0]);
    return titleCase && words >= 2 && words <= 4 && !hasPunctuation(text);
}

bool ClassificationRules::containsEnglishWords(const std::string& text) {
    // Tokenize and check against common words
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    std::istringstream iss(lower);
    std::string word;
    int englishWords = 0;
    int totalWords = 0;

    while (iss >> word) {
        // Remove punctuation
        word.erase(std::remove_if(word.begin(), word.end(),
                   [](char c) { return !std::isalpha(c); }), word.end());

        if (word.length() >= 2) {
            totalWords++;
            if (COMMON_ENGLISH_WORDS.count(word) > 0) {
                englishWords++;
            }
        }
    }

    // At least 20% recognized English words (or at least 1 if short)
    return totalWords > 0 && (englishWords > 0 || totalWords >= 5);
}

int ClassificationRules::countWords(const std::string& text) {
    std::istringstream iss(text);
    return static_cast<int>(std::distance(
        std::istream_iterator<std::string>(iss),
        std::istream_iterator<std::string>()
    ));
}

bool ClassificationRules::hasProperCapitalization(const std::string& text) {
    if (text.empty()) return false;
    return std::isupper(text[0]);
}

bool ClassificationRules::hasPunctuation(const std::string& text) {
    return std::any_of(text.begin(), text.end(),
                       [](char c) { return c == '.' || c == '!' || c == '?' ||
                                           c == ',' || c == ';' || c == ':'; });
}

// ============================================================================
// StringClassifier Implementation
// ============================================================================

class StringClassifier::Impl {
public:
    ClassifierConfig config;

    ClassificationResult classifyInternal(const std::string& text) const {
        ClassificationResult result;
        result.category = StringCategory::Unknown;
        result.confidence = 0.0f;
        result.isTranslatable = false;

        // Length checks
        if (text.length() < config.minLength) {
            result.category = StringCategory::Garbage;
            result.reason = "Too short";
            return result;
        }

        if (text.length() > config.maxLength) {
            result.category = StringCategory::Garbage;
            result.reason = "Too long";
            return result;
        }

        // Garbage check (first, as it's fastest)
        if (ClassificationRules::looksLikeGarbage(text)) {
            result.category = StringCategory::Garbage;
            result.confidence = 0.9f;
            result.reason = "Contains garbage/binary data";
            return result;
        }

        // Technical patterns (non-translatable)
        if (ClassificationRules::looksLikeFilePath(text)) {
            result.category = StringCategory::FilePath;
            result.confidence = 0.95f;
            result.reason = "Looks like file path";
            return result;
        }

        if (ClassificationRules::looksLikeURL(text)) {
            result.category = StringCategory::URL;
            result.confidence = 0.95f;
            result.reason = "Looks like URL";
            return result;
        }

        if (ClassificationRules::looksLikeMarkup(text)) {
            result.category = StringCategory::Markup;
            result.confidence = 0.85f;
            result.reason = "Contains markup/structured data";
            return result;
        }

        if (ClassificationRules::looksLikeDebug(text)) {
            result.category = StringCategory::Debug;
            result.confidence = 0.85f;
            result.reason = "Looks like debug/log message";
            return result;
        }

        if (ClassificationRules::looksLikeCode(text)) {
            result.category = StringCategory::Code;
            result.confidence = 0.8f;
            result.reason = "Contains code patterns";
            return result;
        }

        if (ClassificationRules::looksLikeNumeric(text)) {
            result.category = StringCategory::Numeric;
            result.confidence = 0.9f;
            result.reason = "Mostly numeric data";
            return result;
        }

        // Identifier check (single word, camelCase/snake_case)
        if (ClassificationRules::looksLikeIdentifier(text)) {
            result.category = StringCategory::Identifier;
            result.confidence = 0.75f;
            result.reason = "Looks like identifier/variable name";
            return result;
        }

        // Now check for translatable content
        int wordCount = ClassificationRules::countWords(text);

        // UI text (short, title case)
        if (ClassificationRules::looksLikeUIText(text)) {
            result.category = StringCategory::UIText;
            result.confidence = 0.8f;
            result.isTranslatable = true;
            result.reason = "Short UI-style text";
            result.suggestedContext = "UI";
            return result;
        }

        // Item names
        if (ClassificationRules::looksLikeItemName(text)) {
            result.category = StringCategory::ItemName;
            result.confidence = 0.75f;
            result.isTranslatable = true;
            result.reason = "Looks like item/ability name";
            result.suggestedContext = "Item";
            return result;
        }

        // Dialogue (longer text with sentence structure)
        if (ClassificationRules::looksLikeDialogue(text)) {
            result.category = StringCategory::Dialogue;
            result.confidence = 0.85f;
            result.isTranslatable = true;
            result.reason = "Sentence structure, proper punctuation";
            result.suggestedContext = "Dialogue";
            return result;
        }

        // Contains English words but didn't match other patterns
        if (ClassificationRules::containsEnglishWords(text) && wordCount >= 2) {
            // Check for technical patterns even if it has English words
            bool hasDot = text.find('.') != std::string::npos && text.find(' ') == std::string::npos;
            bool hasUnderscore = text.find('_') != std::string::npos;
            bool hasCamelCase = false;
            for (size_t i = 1; i < text.length(); ++i) {
                if (std::islower(text[i-1]) && std::isupper(text[i])) {
                    hasCamelCase = true;
                    break;
                }
            }

            // If it looks technical, don't mark as translatable
            if ((hasDot && !ClassificationRules::hasPunctuation(text)) || (hasUnderscore && wordCount == 1)) {
                result.category = StringCategory::Technical;
                result.confidence = 0.5f;
                result.reason = "Contains technical patterns despite English words";
                return result;
            }

            // Likely description or general text
            if (wordCount >= 5) {
                result.category = StringCategory::Description;
                result.isTranslatable = true;
            } else if (wordCount >= 2) {
                result.category = StringCategory::UIText;
                result.isTranslatable = true;
            }
            result.confidence = 0.6f;
            result.reason = "Contains recognizable words";
            return result;
        }

        // If strict mode and nothing matched, mark as technical
        if (config.strictMode) {
            result.category = StringCategory::Technical;
            result.confidence = 0.5f;
            result.reason = "No translatable patterns found (strict mode)";
            return result;
        }

        // Default: unknown - NOT translatable unless very clearly text
        result.category = StringCategory::Unknown;
        result.confidence = 0.3f;
        // Only mark as translatable if it has multiple words AND contains English
        result.isTranslatable = wordCount >= 3 && ClassificationRules::containsEnglishWords(text);
        result.reason = "Could not determine category";
        return result;
    }
};

StringClassifier::StringClassifier() : impl_(std::make_unique<Impl>()) {}

StringClassifier::StringClassifier(const ClassifierConfig& config)
    : impl_(std::make_unique<Impl>())
{
    impl_->config = config;
}

StringClassifier::~StringClassifier() = default;

void StringClassifier::setConfig(const ClassifierConfig& config) {
    impl_->config = config;
}

ClassifierConfig StringClassifier::getConfig() const {
    return impl_->config;
}

ClassificationResult StringClassifier::classify(const std::string& text) const {
    return impl_->classifyInternal(text);
}

bool StringClassifier::isTranslatable(const std::string& text) const {
    auto result = classify(text);
    return result.isTranslatable && result.confidence >= impl_->config.minConfidence;
}

std::vector<ClassificationResult> StringClassifier::classifyBatch(
    const std::vector<std::string>& texts) const
{
    std::vector<ClassificationResult> results;
    results.reserve(texts.size());

    for (const auto& text : texts) {
        results.push_back(classify(text));
    }

    return results;
}

std::vector<TranslationEntry> StringClassifier::filterTranslatable(
    const std::vector<TranslationEntry>& entries) const
{
    std::vector<TranslationEntry> filtered;

    for (const auto& entry : entries) {
        if (isTranslatable(entry.sourceText)) {
            filtered.push_back(entry);
        }
    }

    return filtered;
}

ClassificationStats StringClassifier::getStats(
    const std::vector<TranslationEntry>& entries) const
{
    ClassificationStats stats;
    stats.total = static_cast<int>(entries.size());

    for (const auto& entry : entries) {
        auto result = classify(entry.sourceText);

        if (result.isTranslatable) {
            stats.translatable++;
        }

        switch (result.category) {
            case StringCategory::Dialogue:
                stats.dialogue++;
                break;
            case StringCategory::UIText:
                stats.uiText++;
                break;
            case StringCategory::ItemName:
                stats.itemNames++;
                break;
            case StringCategory::Description:
                stats.descriptions++;
                break;
            case StringCategory::Code:
                stats.code++;
                break;
            case StringCategory::FilePath:
                stats.filePaths++;
                break;
            case StringCategory::Identifier:
                stats.identifiers++;
                break;
            case StringCategory::Debug:
                stats.debug++;
                break;
            case StringCategory::Garbage:
                stats.garbage++;
                break;
            default:
                stats.unknown++;
                break;
        }
    }

    return stats;
}

bool StringClassifier::isCategoryTranslatable(StringCategory category) {
    switch (category) {
        case StringCategory::Dialogue:
        case StringCategory::UIText:
        case StringCategory::ItemName:
        case StringCategory::Description:
        case StringCategory::Tutorial:
        case StringCategory::Notification:
        case StringCategory::Error:
            return true;
        default:
            return false;
    }
}

std::string StringClassifier::categoryToString(StringCategory category) {
    switch (category) {
        case StringCategory::Dialogue:      return "Dialogue";
        case StringCategory::UIText:        return "UI Text";
        case StringCategory::ItemName:      return "Item Name";
        case StringCategory::Description:   return "Description";
        case StringCategory::Tutorial:      return "Tutorial";
        case StringCategory::Notification:  return "Notification";
        case StringCategory::Error:         return "Error";
        case StringCategory::Code:          return "Code";
        case StringCategory::FilePath:      return "File Path";
        case StringCategory::URL:           return "URL";
        case StringCategory::Identifier:    return "Identifier";
        case StringCategory::Debug:         return "Debug";
        case StringCategory::Markup:        return "Markup";
        case StringCategory::Numeric:       return "Numeric";
        case StringCategory::Technical:     return "Technical";
        case StringCategory::Garbage:       return "Garbage";
        case StringCategory::Unknown:       return "Unknown";
    }
    return "Unknown";
}

} // namespace makineai
