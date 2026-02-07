/**
 * @file pickle_reader.hpp
 * @brief Minimal Python pickle protocol 2 decoder (internal)
 * @copyright (c) 2026 MakineAI Team
 *
 * Decodes the subset of pickle opcodes used by Ren'Py's RPA index.
 * Security: No REDUCE/GLOBAL/INST/BUILD/OBJ opcodes — no code execution.
 */

#pragma once

#include "makineai/error.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace makineai::formats {

// Forward declarations for recursive types
struct PickleValue;
using PickleList = std::vector<PickleValue>;
using PickleDict = std::vector<std::pair<PickleValue, PickleValue>>;

/**
 * @brief A dynamically-typed value decoded from a pickle stream
 *
 * Supports: None, bool, int64_t, string, bytes, list/tuple, dict
 * Uses unique_ptr for recursive containers to avoid infinite-size type.
 */
struct PickleValue {
    std::variant<
        std::monostate,                     // None
        bool,                               // True/False
        int64_t,                            // Integer
        std::string,                        // Unicode string
        std::vector<uint8_t>,               // Bytes
        std::unique_ptr<PickleList>,        // List or tuple
        std::unique_ptr<PickleDict>         // Dict
    > data;

    // Constructors
    PickleValue() : data(std::monostate{}) {}
    explicit PickleValue(bool v) : data(v) {}
    explicit PickleValue(int64_t v) : data(v) {}
    explicit PickleValue(std::string v) : data(std::move(v)) {}
    explicit PickleValue(std::vector<uint8_t> v) : data(std::move(v)) {}

    static PickleValue makeList(PickleList list) {
        PickleValue v;
        v.data = std::make_unique<PickleList>(std::move(list));
        return v;
    }

    static PickleValue makeDict(PickleDict dict) {
        PickleValue v;
        v.data = std::make_unique<PickleDict>(std::move(dict));
        return v;
    }

    // Move-only (unique_ptr in variant)
    PickleValue(PickleValue&&) = default;
    PickleValue& operator=(PickleValue&&) = default;
    PickleValue(const PickleValue&) = delete;
    PickleValue& operator=(const PickleValue&) = delete;

    // Type checks
    [[nodiscard]] bool isNone() const { return std::holds_alternative<std::monostate>(data); }
    [[nodiscard]] bool isBool() const { return std::holds_alternative<bool>(data); }
    [[nodiscard]] bool isInt() const { return std::holds_alternative<int64_t>(data); }
    [[nodiscard]] bool isString() const { return std::holds_alternative<std::string>(data); }
    [[nodiscard]] bool isBytes() const { return std::holds_alternative<std::vector<uint8_t>>(data); }
    [[nodiscard]] bool isList() const { return std::holds_alternative<std::unique_ptr<PickleList>>(data); }
    [[nodiscard]] bool isDict() const { return std::holds_alternative<std::unique_ptr<PickleDict>>(data); }

    // Accessors (caller must check type first)
    [[nodiscard]] int64_t asInt() const { return std::get<int64_t>(data); }
    [[nodiscard]] const std::string& asString() const { return std::get<std::string>(data); }
    [[nodiscard]] const std::vector<uint8_t>& asBytes() const { return std::get<std::vector<uint8_t>>(data); }
    [[nodiscard]] const PickleList& asList() const { return *std::get<std::unique_ptr<PickleList>>(data); }
    [[nodiscard]] const PickleDict& asDict() const { return *std::get<std::unique_ptr<PickleDict>>(data); }

    // Mutable accessors for building
    [[nodiscard]] PickleList& asMutableList() { return *std::get<std::unique_ptr<PickleList>>(data); }
    [[nodiscard]] PickleDict& asMutableDict() { return *std::get<std::unique_ptr<PickleDict>>(data); }
};

/**
 * @brief Parse a pickle byte stream into a PickleValue
 * @param data Raw pickle bytes
 * @return The top-of-stack value after STOP opcode
 */
[[nodiscard]] Result<PickleValue> parsePickle(std::span<const uint8_t> data);

} // namespace makineai::formats
