/**
 * @file vdfparser.h
 * @brief Valve Data Format (VDF) parser - header-only
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Copied from core/include/makine/vdf_parser.hpp for UI_ONLY builds.
 * Recursive descent parser for Valve's key-value text format.
 * Used by Steam's libraryfolders.vdf and appmanifest_*.acf files.
 */

#pragma once

#include <map>
#include <optional>
#include <string>

namespace makine::vdf {

/// Tree node for parsed VDF data
struct Node {
    std::string value;
    std::map<std::string, Node> children;

    [[nodiscard]] bool isObject() const { return !children.empty(); }

    [[nodiscard]] const Node* find(const std::string& key) const {
        auto it = children.find(key);
        return it != children.end() ? &it->second : nullptr;
    }

    [[nodiscard]] std::string getString(const std::string& key,
                                        const std::string& defaultVal = "") const {
        const auto* node = find(key);
        return (node && !node->value.empty()) ? node->value : defaultVal;
    }
};

namespace detail {

inline void skipWhitespace(const std::string& s, size_t& pos) {
    while (pos < s.size()) {
        char c = s[pos];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            ++pos;
        } else if (pos + 1 < s.size() && c == '/' && s[pos + 1] == '/') {
            while (pos < s.size() && s[pos] != '\n') ++pos;
        } else {
            break;
        }
    }
}

inline std::optional<std::string> parseQuotedString(const std::string& s, size_t& pos) {
    skipWhitespace(s, pos);
    if (pos >= s.size() || s[pos] != '"') return std::nullopt;
    ++pos;

    std::string result;
    while (pos < s.size() && s[pos] != '"') {
        if (s[pos] == '\\' && pos + 1 < s.size()) {
            char next = s[pos + 1];
            switch (next) {
                case '\\': result += '\\'; break;
                case '"':  result += '"';  break;
                case 'n':  result += '\n'; break;
                case 't':  result += '\t'; break;
                default:   result += next; break;
            }
            pos += 2;
        } else {
            result += s[pos];
            ++pos;
        }
    }
    if (pos < s.size()) ++pos; // closing quote
    return result;
}

static constexpr int kMaxRecursionDepth = 32;
static constexpr size_t kMaxVdfFileSize = 10 * 1024 * 1024; // 10 MB

inline void parseKeyValues(const std::string& s, size_t& pos, Node& node, int depth = 0) {
    if (depth > kMaxRecursionDepth) return;
    while (pos < s.size()) {
        skipWhitespace(s, pos);
        if (pos >= s.size() || s[pos] == '}') break;

        auto key = parseQuotedString(s, pos);
        if (!key) break;

        skipWhitespace(s, pos);
        if (pos >= s.size()) break;

        if (s[pos] == '{') {
            ++pos;
            Node child;
            parseKeyValues(s, pos, child, depth + 1);
            skipWhitespace(s, pos);
            if (pos < s.size() && s[pos] == '}') ++pos;
            node.children[std::move(*key)] = std::move(child);
        } else if (s[pos] == '"') {
            auto val = parseQuotedString(s, pos);
            if (val) {
                Node leaf;
                leaf.value = std::move(*val);
                node.children[std::move(*key)] = std::move(leaf);
            }
        }
    }
}

} // namespace detail

/// Parse VDF text content into a tree structure
inline std::optional<Node> parse(const std::string& content) {
    size_t pos = 0;
    Node root;
    detail::parseKeyValues(content, pos, root);
    return root;
}

} // namespace makine::vdf
