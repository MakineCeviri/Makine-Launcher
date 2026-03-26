#pragma once

/**
 * @file json_utils.hpp
 * @brief JSON parsing utilities with simdjson integration
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Provides a unified API for JSON parsing.
 * Uses simdjson when available for high performance (4-10x faster),
 * falls back to nlohmann-json otherwise.
 *
 * Design Philosophy:
 * - simdjson for READ-ONLY parsing (fastest)
 * - nlohmann-json for modification/serialization
 * - Abstraction layer hides implementation details
 *
 * Usage:
 *   // Parse and access
 *   auto result = json::parseFile(path);
 *   if (result) {
 *       auto name = result->getString("name");
 *       auto count = result->getInt("count");
 *   }
 *
 *   // For write operations, use nlohmann directly:
 *   nlohmann::json obj;
 *   obj["key"] = "value";
 */

#include "makine/error.hpp"
#include "makine/features.hpp"
#include "makine/logging.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#ifdef MAKINE_HAS_SIMDJSON
#include <simdjson.h>
#endif

namespace makine::json {

namespace fs = std::filesystem;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

class JsonValue;
class JsonObject;
class JsonArray;

// =============================================================================
// JSON VALUE - READ-ONLY ACCESSOR
// =============================================================================

/**
 * @brief Read-only JSON value accessor
 *
 * Provides a type-safe interface for accessing JSON values.
 * Works with both simdjson and nlohmann-json backends.
 */
class JsonValue {
public:
    enum class Type {
        Null,
        Bool,
        Int64,
        Uint64,
        Double,
        String,
        Array,
        Object
    };

    JsonValue() = default;

    /// Construct from nlohmann::json
    explicit JsonValue(const nlohmann::json& j) : nlohmann_(j) {}

#ifdef MAKINE_HAS_SIMDJSON
    /// Construct from simdjson element
    explicit JsonValue(simdjson::dom::element elem) : simdjson_(elem), useSimdjson_(true) {}
#endif

    /// Get type of value
    [[nodiscard]] Type type() const;

    /// Check if value is null
    [[nodiscard]] bool isNull() const;

    /// Get as boolean (returns nullopt if not bool)
    [[nodiscard]] std::optional<bool> asBool() const;

    /// Get as int64 (returns nullopt if not integer)
    [[nodiscard]] std::optional<int64_t> asInt64() const;

    /// Get as uint64 (returns nullopt if not unsigned integer)
    [[nodiscard]] std::optional<uint64_t> asUint64() const;

    /// Get as double (returns nullopt if not number)
    [[nodiscard]] std::optional<double> asDouble() const;

    /// Get as string (returns nullopt if not string)
    [[nodiscard]] std::optional<std::string> asString() const;

    /// Get string with default value
    [[nodiscard]] std::string getString(std::string_view defaultValue = "") const;

    /// Get int with default value
    [[nodiscard]] int64_t getInt(int64_t defaultValue = 0) const;

    /// Get uint with default value
    [[nodiscard]] uint64_t getUint(uint64_t defaultValue = 0) const;

    /// Get double with default value
    [[nodiscard]] double getDouble(double defaultValue = 0.0) const;

    /// Get bool with default value
    [[nodiscard]] bool getBool(bool defaultValue = false) const;

    /// Check if this is an object
    [[nodiscard]] bool isObject() const;

    /// Check if this is an array
    [[nodiscard]] bool isArray() const;

    /// Get nested value by key (for objects)
    [[nodiscard]] JsonValue operator[](std::string_view key) const;

    /// Get value by index (for arrays)
    [[nodiscard]] JsonValue operator[](size_t index) const;

    /// Check if object has key
    [[nodiscard]] bool hasKey(std::string_view key) const;

    /// Get array size (returns 0 if not array)
    [[nodiscard]] size_t size() const;

    /// Convert to nlohmann::json (for write operations)
    [[nodiscard]] nlohmann::json toNlohmann() const;

private:
    nlohmann::json nlohmann_;

#ifdef MAKINE_HAS_SIMDJSON
    simdjson::dom::element simdjson_;
    bool useSimdjson_ = false;
#endif
};

// =============================================================================
// JSON DOCUMENT - PARSED DOCUMENT
// =============================================================================

/**
 * @brief Parsed JSON document
 *
 * Owns the parsed data and provides access to the root value.
 */
class JsonDocument {
public:
    JsonDocument() = default;

    /// Parse JSON from string
    [[nodiscard]] static Result<JsonDocument> parse(std::string_view jsonStr);

    /// Parse JSON from file
    [[nodiscard]] static Result<JsonDocument> parseFile(const fs::path& path);

    /// Get root value
    [[nodiscard]] JsonValue root() const;

    /// Convenience accessors (delegates to root)
    [[nodiscard]] JsonValue operator[](std::string_view key) const { return root()[key]; }
    [[nodiscard]] JsonValue operator[](size_t index) const { return root()[index]; }
    [[nodiscard]] bool hasKey(std::string_view key) const { return root().hasKey(key); }

    /// Get string value at key
    [[nodiscard]] std::string getString(std::string_view key,
                                        std::string_view defaultValue = "") const {
        return root()[key].getString(defaultValue);
    }

    /// Get int value at key
    [[nodiscard]] int64_t getInt(std::string_view key, int64_t defaultValue = 0) const {
        return root()[key].getInt(defaultValue);
    }

    /// Get uint value at key
    [[nodiscard]] uint64_t getUint(std::string_view key, uint64_t defaultValue = 0) const {
        return root()[key].getUint(defaultValue);
    }

    /// Get double value at key
    [[nodiscard]] double getDouble(std::string_view key, double defaultValue = 0.0) const {
        return root()[key].getDouble(defaultValue);
    }

    /// Get bool value at key
    [[nodiscard]] bool getBool(std::string_view key, bool defaultValue = false) const {
        return root()[key].getBool(defaultValue);
    }

    /// Check if document is valid
    [[nodiscard]] bool isValid() const { return valid_; }

    /// Convert entire document to nlohmann::json
    [[nodiscard]] nlohmann::json toNlohmann() const { return root().toNlohmann(); }

private:
#ifdef MAKINE_HAS_SIMDJSON
    simdjson::dom::parser parser_;
    simdjson::dom::element element_;
    simdjson::padded_string paddedJson_;
    bool useSimdjson_ = false;
#endif
    nlohmann::json nlohmann_;
    bool valid_ = false;
};

// =============================================================================
// IMPLEMENTATION - JSON VALUE
// =============================================================================

inline JsonValue::Type JsonValue::type() const {
#ifdef MAKINE_HAS_SIMDJSON
    if (useSimdjson_) {
        switch (simdjson_.type()) {
            case simdjson::dom::element_type::NULL_VALUE: return Type::Null;
            case simdjson::dom::element_type::BOOL: return Type::Bool;
            case simdjson::dom::element_type::INT64: return Type::Int64;
            case simdjson::dom::element_type::UINT64: return Type::Uint64;
            case simdjson::dom::element_type::DOUBLE: return Type::Double;
            case simdjson::dom::element_type::STRING: return Type::String;
            case simdjson::dom::element_type::ARRAY: return Type::Array;
            case simdjson::dom::element_type::OBJECT: return Type::Object;
        }
    }
#endif
    if (nlohmann_.is_null()) return Type::Null;
    if (nlohmann_.is_boolean()) return Type::Bool;
    if (nlohmann_.is_number_integer()) return Type::Int64;
    if (nlohmann_.is_number_unsigned()) return Type::Uint64;
    if (nlohmann_.is_number_float()) return Type::Double;
    if (nlohmann_.is_string()) return Type::String;
    if (nlohmann_.is_array()) return Type::Array;
    if (nlohmann_.is_object()) return Type::Object;
    return Type::Null;
}

inline bool JsonValue::isNull() const {
#ifdef MAKINE_HAS_SIMDJSON
    if (useSimdjson_) {
        return simdjson_.type() == simdjson::dom::element_type::NULL_VALUE;
    }
#endif
    return nlohmann_.is_null();
}

inline std::optional<bool> JsonValue::asBool() const {
#ifdef MAKINE_HAS_SIMDJSON
    if (useSimdjson_) {
        if (auto result = simdjson_.get_bool(); result.error() == simdjson::SUCCESS) {
            return result.value();
        }
        return std::nullopt;
    }
#endif
    if (nlohmann_.is_boolean()) {
        return nlohmann_.get<bool>();
    }
    return std::nullopt;
}

inline std::optional<int64_t> JsonValue::asInt64() const {
#ifdef MAKINE_HAS_SIMDJSON
    if (useSimdjson_) {
        if (auto result = simdjson_.get_int64(); result.error() == simdjson::SUCCESS) {
            return result.value();
        }
        return std::nullopt;
    }
#endif
    if (nlohmann_.is_number_integer()) {
        return nlohmann_.get<int64_t>();
    }
    return std::nullopt;
}

inline std::optional<uint64_t> JsonValue::asUint64() const {
#ifdef MAKINE_HAS_SIMDJSON
    if (useSimdjson_) {
        if (auto result = simdjson_.get_uint64(); result.error() == simdjson::SUCCESS) {
            return result.value();
        }
        return std::nullopt;
    }
#endif
    if (nlohmann_.is_number_unsigned()) {
        return nlohmann_.get<uint64_t>();
    }
    return std::nullopt;
}

inline std::optional<double> JsonValue::asDouble() const {
#ifdef MAKINE_HAS_SIMDJSON
    if (useSimdjson_) {
        if (auto result = simdjson_.get_double(); result.error() == simdjson::SUCCESS) {
            return result.value();
        }
        return std::nullopt;
    }
#endif
    if (nlohmann_.is_number()) {
        return nlohmann_.get<double>();
    }
    return std::nullopt;
}

inline std::optional<std::string> JsonValue::asString() const {
#ifdef MAKINE_HAS_SIMDJSON
    if (useSimdjson_) {
        if (auto result = simdjson_.get_string(); result.error() == simdjson::SUCCESS) {
            return std::string(result.value());
        }
        return std::nullopt;
    }
#endif
    if (nlohmann_.is_string()) {
        return nlohmann_.get<std::string>();
    }
    return std::nullopt;
}

inline std::string JsonValue::getString(std::string_view defaultValue) const {
    auto val = asString();
    return val.value_or(std::string(defaultValue));
}

inline int64_t JsonValue::getInt(int64_t defaultValue) const {
    auto val = asInt64();
    return val.value_or(defaultValue);
}

inline uint64_t JsonValue::getUint(uint64_t defaultValue) const {
    auto val = asUint64();
    return val.value_or(defaultValue);
}

inline double JsonValue::getDouble(double defaultValue) const {
    auto val = asDouble();
    return val.value_or(defaultValue);
}

inline bool JsonValue::getBool(bool defaultValue) const {
    auto val = asBool();
    return val.value_or(defaultValue);
}

inline bool JsonValue::isObject() const {
    return type() == Type::Object;
}

inline bool JsonValue::isArray() const {
    return type() == Type::Array;
}

inline JsonValue JsonValue::operator[](std::string_view key) const {
#ifdef MAKINE_HAS_SIMDJSON
    if (useSimdjson_) {
        if (simdjson_.type() == simdjson::dom::element_type::OBJECT) {
            if (auto result = simdjson_[key]; result.error() == simdjson::SUCCESS) {
                return JsonValue(result.value());
            }
        }
        return JsonValue();
    }
#endif
    if (nlohmann_.is_object() && nlohmann_.contains(key)) {
        return JsonValue(nlohmann_[std::string(key)]);
    }
    return JsonValue();
}

inline JsonValue JsonValue::operator[](size_t index) const {
#ifdef MAKINE_HAS_SIMDJSON
    if (useSimdjson_) {
        if (simdjson_.type() == simdjson::dom::element_type::ARRAY) {
            if (auto result = simdjson_.at(index); result.error() == simdjson::SUCCESS) {
                return JsonValue(result.value());
            }
        }
        return JsonValue();
    }
#endif
    if (nlohmann_.is_array() && index < nlohmann_.size()) {
        return JsonValue(nlohmann_[index]);
    }
    return JsonValue();
}

inline bool JsonValue::hasKey(std::string_view key) const {
#ifdef MAKINE_HAS_SIMDJSON
    if (useSimdjson_) {
        if (simdjson_.type() == simdjson::dom::element_type::OBJECT) {
            auto result = simdjson_[key];
            return result.error() == simdjson::SUCCESS;
        }
        return false;
    }
#endif
    return nlohmann_.is_object() && nlohmann_.contains(key);
}

inline size_t JsonValue::size() const {
#ifdef MAKINE_HAS_SIMDJSON
    if (useSimdjson_) {
        if (simdjson_.type() == simdjson::dom::element_type::ARRAY) {
            return simdjson_.get_array().value().size();
        }
        if (simdjson_.type() == simdjson::dom::element_type::OBJECT) {
            return simdjson_.get_object().value().size();
        }
        return 0;
    }
#endif
    if (nlohmann_.is_array() || nlohmann_.is_object()) {
        return nlohmann_.size();
    }
    return 0;
}

inline nlohmann::json JsonValue::toNlohmann() const {
#ifdef MAKINE_HAS_SIMDJSON
    if (useSimdjson_) {
        // Convert simdjson to nlohmann
        std::string jsonStr = simdjson::to_string(simdjson_);
        return nlohmann::json::parse(jsonStr);
    }
#endif
    return nlohmann_;
}

// =============================================================================
// IMPLEMENTATION - JSON DOCUMENT
// =============================================================================

inline Result<JsonDocument> JsonDocument::parse(std::string_view jsonStr) {
    JsonDocument doc;

#ifdef MAKINE_HAS_SIMDJSON
    try {
        doc.paddedJson_ = simdjson::padded_string(jsonStr);
        doc.parser_.threaded = false;  // Disable background threading (deadlocks on MinGW)
        auto result = doc.parser_.parse(doc.paddedJson_);
        if (result.error() != simdjson::SUCCESS) {
            throw std::runtime_error(simdjson::error_message(result.error()));
        }
        doc.element_ = result.value();
        doc.useSimdjson_ = true;
        doc.valid_ = true;

        MAKINE_LOG_TRACE(log::CORE, "Parsed JSON with simdjson ({} bytes)", jsonStr.size());
        return doc;
    }
    catch (const std::exception& e) {
        // Fall through to nlohmann
        MAKINE_LOG_TRACE(log::CORE, "simdjson failed, falling back to nlohmann: {}", e.what());
    }
#endif

    try {
        doc.nlohmann_ = nlohmann::json::parse(jsonStr);
        doc.valid_ = true;
        return doc;
    }
    catch (const nlohmann::json::exception& e) {
        return std::unexpected(Error(ErrorCode::ParseError,
            std::string("JSON parse error: ") + e.what()));
    }
}

inline Result<JsonDocument> JsonDocument::parseFile(const fs::path& path) {
    if (!fs::exists(path)) {
        return std::unexpected(Error(ErrorCode::FileNotFound,
            "File not found: " + path.string()));
    }

    JsonDocument doc;

#ifdef MAKINE_HAS_SIMDJSON
    try {
        doc.parser_.threaded = false;  // Disable background threading (deadlocks on MinGW)
        auto result = doc.parser_.load(path.string());
        if (result.error() != simdjson::SUCCESS) {
            throw std::runtime_error(simdjson::error_message(result.error()));
        }
        doc.element_ = result.value();
        doc.useSimdjson_ = true;
        doc.valid_ = true;

        MAKINE_LOG_TRACE(log::CORE, "Parsed JSON file with simdjson: {}", path.string());
        return doc;
    }
    catch (const std::exception& e) {
        MAKINE_LOG_TRACE(log::CORE, "simdjson failed for file, falling back: {}", e.what());
    }
#endif

    try {
        std::ifstream file(path);
        if (!file) {
            return std::unexpected(Error(ErrorCode::FileAccessDenied,
                "Cannot open file: " + path.string()));
        }
        doc.nlohmann_ = nlohmann::json::parse(file);
        doc.valid_ = true;
        return doc;
    }
    catch (const nlohmann::json::exception& e) {
        return std::unexpected(Error(ErrorCode::ParseError,
            "JSON parse error in " + path.string() + ": " + e.what()));
    }
}

inline JsonValue JsonDocument::root() const {
#ifdef MAKINE_HAS_SIMDJSON
    if (useSimdjson_) {
        return JsonValue(element_);
    }
#endif
    return JsonValue(nlohmann_);
}

// =============================================================================
// CONVENIENCE FUNCTIONS
// =============================================================================

/**
 * @brief Parse JSON string
 */
inline Result<JsonDocument> parse(std::string_view jsonStr) {
    return JsonDocument::parse(jsonStr);
}

/**
 * @brief Parse JSON file
 */
inline Result<JsonDocument> parseFile(const fs::path& path) {
    return JsonDocument::parseFile(path);
}

/**
 * @brief Check if simdjson is available
 */
[[nodiscard]] inline constexpr bool hasSimdjson() {
#ifdef MAKINE_HAS_SIMDJSON
    return true;
#else
    return false;
#endif
}

/**
 * @brief Get JSON backend info
 */
[[nodiscard]] inline std::string backendInfo() {
#ifdef MAKINE_HAS_SIMDJSON
    return "simdjson (SIMD-accelerated)";
#else
    return "nlohmann-json";
#endif
}

// =============================================================================
// ARRAY ITERATION HELPERS
// =============================================================================

/**
 * @brief Iterate over JSON array elements
 *
 * Usage:
 *   for (auto elem : json::iterate(doc["items"])) {
 *       auto name = elem.getString("name");
 *   }
 */
class ArrayIterator {
public:
    explicit ArrayIterator(const JsonValue& arr) : array_(arr), index_(0), size_(arr.size()) {}

    class Iterator {
    public:
        Iterator(const JsonValue& arr, size_t idx) : array_(arr), index_(idx) {}

        JsonValue operator*() const { return array_[index_]; }
        Iterator& operator++() { ++index_; return *this; }
        bool operator!=(const Iterator& other) const { return index_ != other.index_; }

    private:
        const JsonValue& array_;
        size_t index_;
    };

    [[nodiscard]] Iterator begin() const { return Iterator(array_, 0); }
    [[nodiscard]] Iterator end() const { return Iterator(array_, size_); }

private:
    const JsonValue& array_;
    size_t index_;
    size_t size_;
};

inline ArrayIterator iterate(const JsonValue& arr) {
    return ArrayIterator(arr);
}

}  // namespace makine::json
