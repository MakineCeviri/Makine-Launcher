/**
 * @file pickle_reader.cpp
 * @brief Minimal Python pickle protocol 2 decoder
 * @copyright (c) 2026 MakineAI Team
 */

#include "pickle_reader.hpp"

#include <cstring>
#include <stack>
#include <stdexcept>

namespace makineai::formats {

// Python pickle opcodes (protocol 0-2 subset used by Ren'Py)
namespace opcode {
    // Control
    constexpr uint8_t PROTO         = 0x80;
    constexpr uint8_t STOP          = '.';  // 0x2E
    constexpr uint8_t FRAME         = 0x95; // protocol 4 framing (skip)

    // Primitives
    constexpr uint8_t NONE          = 'N';  // 0x4E
    constexpr uint8_t NEWTRUE       = 0x88;
    constexpr uint8_t NEWFALSE      = 0x89;

    // Integers
    constexpr uint8_t INT           = 'I';  // 0x49 - text integer
    constexpr uint8_t BININT        = 'J';  // 0x4A - 4-byte signed LE
    constexpr uint8_t BININT1       = 'K';  // 0x4B - 1-byte unsigned
    constexpr uint8_t BININT2       = 'M';  // 0x4D - 2-byte unsigned LE
    constexpr uint8_t LONG1         = 0x8A; // 1-byte length + LE signed
    constexpr uint8_t LONG4         = 0x8B; // 4-byte length + LE signed

    // Strings (protocol 0/1)
    constexpr uint8_t SHORT_BINSTRING = 'U'; // 0x55 - 1-byte len + ASCII
    constexpr uint8_t BINSTRING     = 'T';   // 0x54 - 4-byte len + ASCII

    // Unicode strings
    constexpr uint8_t SHORT_BINUNICODE = 0x8C; // 1-byte len + UTF-8
    constexpr uint8_t BINUNICODE    = 'X';     // 0x58 - 4-byte len + UTF-8

    // Bytes
    constexpr uint8_t SHORT_BINBYTES = 'C';    // 0x43 - 1-byte len
    constexpr uint8_t BINBYTES      = 'B';     // 0x42 - 4-byte len
    constexpr uint8_t BINBYTES8     = 0x8E;    // 8-byte len (protocol 4)

    // Containers
    constexpr uint8_t EMPTY_LIST    = ']';  // 0x5D
    constexpr uint8_t EMPTY_TUPLE   = ')';  // 0x29
    constexpr uint8_t EMPTY_DICT    = '}';  // 0x7D
    constexpr uint8_t MARK          = '(';  // 0x28
    constexpr uint8_t TUPLE         = 't';  // 0x74 - from mark
    constexpr uint8_t TUPLE1        = 0x85;
    constexpr uint8_t TUPLE2        = 0x86;
    constexpr uint8_t TUPLE3        = 0x87;
    constexpr uint8_t LIST          = 'l';  // 0x6C - from mark

    // Append/Set
    constexpr uint8_t APPEND        = 'a';  // 0x61
    constexpr uint8_t APPENDS       = 'e';  // 0x65
    constexpr uint8_t SETITEM       = 's';  // 0x73
    constexpr uint8_t SETITEMS      = 'u';  // 0x75

    // Memo
    constexpr uint8_t BINPUT        = 'q';  // 0x71 - 1-byte memo index
    constexpr uint8_t LONG_BINPUT   = 'r';  // 0x72 - 4-byte memo index
    constexpr uint8_t BINGET        = 'h';  // 0x68 - 1-byte memo index
    constexpr uint8_t LONG_BINGET   = 'j';  // 0x6A - 4-byte memo index
    constexpr uint8_t MEMOIZE       = 0x94; // protocol 4

    // Duplicate
    constexpr uint8_t DUP           = '2';  // 0x32

    // Pop
    constexpr uint8_t POP           = '0';  // 0x30
    constexpr uint8_t POP_MARK      = '1';  // 0x31
}

namespace {

// Sentinel value on the stack to mark MARK positions
constexpr int MARK_SENTINEL = -1;

struct PickleReader {
    std::span<const uint8_t> buf;
    size_t pos = 0;

    bool hasBytes(size_t n) const { return pos + n <= buf.size(); }

    uint8_t readByte() {
        if (pos >= buf.size()) throw std::runtime_error("Pickle: unexpected end of data");
        return buf[pos++];
    }

    uint16_t readU16LE() {
        if (!hasBytes(2)) throw std::runtime_error("Pickle: unexpected end of data");
        uint16_t v = static_cast<uint16_t>(buf[pos]) |
                     (static_cast<uint16_t>(buf[pos + 1]) << 8);
        pos += 2;
        return v;
    }

    int32_t readI32LE() {
        if (!hasBytes(4)) throw std::runtime_error("Pickle: unexpected end of data");
        uint32_t v = static_cast<uint32_t>(buf[pos]) |
                     (static_cast<uint32_t>(buf[pos + 1]) << 8) |
                     (static_cast<uint32_t>(buf[pos + 2]) << 16) |
                     (static_cast<uint32_t>(buf[pos + 3]) << 24);
        pos += 4;
        return static_cast<int32_t>(v);
    }

    uint32_t readU32LE() {
        if (!hasBytes(4)) throw std::runtime_error("Pickle: unexpected end of data");
        uint32_t v = static_cast<uint32_t>(buf[pos]) |
                     (static_cast<uint32_t>(buf[pos + 1]) << 8) |
                     (static_cast<uint32_t>(buf[pos + 2]) << 16) |
                     (static_cast<uint32_t>(buf[pos + 3]) << 24);
        pos += 4;
        return v;
    }

    uint64_t readU64LE() {
        if (!hasBytes(8)) throw std::runtime_error("Pickle: unexpected end of data");
        uint64_t v = 0;
        for (int i = 0; i < 8; i++) {
            v |= static_cast<uint64_t>(buf[pos + i]) << (i * 8);
        }
        pos += 8;
        return v;
    }

    std::string readString(size_t len) {
        if (!hasBytes(len)) throw std::runtime_error("Pickle: unexpected end of data");
        std::string s(reinterpret_cast<const char*>(buf.data() + pos), len);
        pos += len;
        return s;
    }

    std::vector<uint8_t> readBytes(size_t len) {
        if (!hasBytes(len)) throw std::runtime_error("Pickle: unexpected end of data");
        std::vector<uint8_t> v(buf.begin() + pos, buf.begin() + pos + len);
        pos += len;
        return v;
    }

    // Read a variable-length signed integer (LONG1/LONG4 payload)
    int64_t readLongBytes(size_t len) {
        if (len == 0) return 0;
        if (!hasBytes(len)) throw std::runtime_error("Pickle: unexpected end of data");

        // Little-endian signed integer of arbitrary length
        int64_t result = 0;
        for (size_t i = 0; i < len && i < 8; i++) {
            result |= static_cast<int64_t>(buf[pos + i]) << (i * 8);
        }
        // Sign-extend if the high bit of the last byte is set
        if (len <= 8 && (buf[pos + len - 1] & 0x80)) {
            for (size_t i = len; i < 8; i++) {
                result |= static_cast<int64_t>(0xFF) << (i * 8);
            }
        }
        pos += len;
        return result;
    }
};

// Helper: move a PickleValue (clone-like for memo — we store indices)
// Since PickleValue is move-only, the memo stores the index into a separate arena.

struct StackEntry {
    int markOrIndex;  // MARK_SENTINEL for mark, otherwise index into arena
};

} // anonymous namespace

Result<PickleValue> parsePickle(std::span<const uint8_t> data) {
    if (data.empty()) {
        return std::unexpected(Error{ErrorCode::ParseError, "Pickle: empty data"});
    }

    try {
        PickleReader reader{data};

        // Arena: all values live here; stack/memo hold indices
        std::vector<PickleValue> arena;
        arena.reserve(256);

        std::vector<StackEntry> stack;
        stack.reserve(64);

        // Memo: pickle index -> arena index
        std::vector<int> memo(256, -1);

        auto pushValue = [&](PickleValue val) -> int {
            int idx = static_cast<int>(arena.size());
            arena.push_back(std::move(val));
            stack.push_back({idx});
            return idx;
        };

        auto popEntry = [&]() -> StackEntry {
            if (stack.empty()) throw std::runtime_error("Pickle: stack underflow");
            auto e = stack.back();
            stack.pop_back();
            return e;
        };

        // Find items back to the last MARK
        auto findMark = [&]() -> size_t {
            for (size_t i = stack.size(); i > 0; i--) {
                if (stack[i - 1].markOrIndex == MARK_SENTINEL) {
                    return i - 1;
                }
            }
            throw std::runtime_error("Pickle: MARK not found");
        };

        // Reconstruct a PickleValue by moving from arena (leaving monostate behind)
        auto takeFromArena = [&](int idx) -> PickleValue {
            return std::move(arena[idx]);
        };

        bool done = false;
        int resultIndex = -1;

        while (!done && reader.pos < reader.buf.size()) {
            uint8_t op = reader.readByte();

            switch (op) {

            // ===================== Control =====================
            case opcode::PROTO: {
                reader.readByte(); // protocol version, ignore
                break;
            }
            case opcode::FRAME: {
                reader.readU64LE(); // frame length, skip
                break;
            }
            case opcode::STOP: {
                if (stack.empty()) throw std::runtime_error("Pickle: STOP with empty stack");
                auto entry = popEntry();
                if (entry.markOrIndex == MARK_SENTINEL) {
                    throw std::runtime_error("Pickle: STOP on MARK");
                }
                resultIndex = entry.markOrIndex;
                done = true;
                break;
            }

            // ===================== Primitives =====================
            case opcode::NONE: {
                pushValue(PickleValue{});
                break;
            }
            case opcode::NEWTRUE: {
                pushValue(PickleValue{true});
                break;
            }
            case opcode::NEWFALSE: {
                pushValue(PickleValue{false});
                break;
            }

            // ===================== Integers =====================
            case opcode::INT: {
                // Text integer: read until \n
                std::string numStr;
                while (reader.pos < reader.buf.size()) {
                    uint8_t c = reader.readByte();
                    if (c == '\n') break;
                    numStr += static_cast<char>(c);
                }
                // Handle special boolean encoding: "00" = False, "01" = True
                if (numStr == "00") {
                    pushValue(PickleValue{false});
                } else if (numStr == "01") {
                    pushValue(PickleValue{true});
                } else {
                    pushValue(PickleValue{static_cast<int64_t>(std::stoll(numStr))});
                }
                break;
            }
            case opcode::BININT: {
                pushValue(PickleValue{static_cast<int64_t>(reader.readI32LE())});
                break;
            }
            case opcode::BININT1: {
                pushValue(PickleValue{static_cast<int64_t>(reader.readByte())});
                break;
            }
            case opcode::BININT2: {
                pushValue(PickleValue{static_cast<int64_t>(reader.readU16LE())});
                break;
            }
            case opcode::LONG1: {
                uint8_t len = reader.readByte();
                pushValue(PickleValue{reader.readLongBytes(len)});
                break;
            }
            case opcode::LONG4: {
                uint32_t len = reader.readU32LE();
                if (len > 8) throw std::runtime_error("Pickle: LONG4 too large");
                pushValue(PickleValue{reader.readLongBytes(len)});
                break;
            }

            // ===================== Strings =====================
            case opcode::SHORT_BINSTRING: {
                uint8_t len = reader.readByte();
                pushValue(PickleValue{reader.readString(len)});
                break;
            }
            case opcode::BINSTRING: {
                int32_t len = reader.readI32LE();
                if (len < 0) throw std::runtime_error("Pickle: negative BINSTRING length");
                pushValue(PickleValue{reader.readString(static_cast<size_t>(len))});
                break;
            }

            // ===================== Unicode =====================
            case opcode::SHORT_BINUNICODE: {
                uint8_t len = reader.readByte();
                pushValue(PickleValue{reader.readString(len)});
                break;
            }
            case opcode::BINUNICODE: {
                uint32_t len = reader.readU32LE();
                pushValue(PickleValue{reader.readString(len)});
                break;
            }

            // ===================== Bytes =====================
            case opcode::SHORT_BINBYTES: {
                uint8_t len = reader.readByte();
                pushValue(PickleValue{reader.readBytes(len)});
                break;
            }
            case opcode::BINBYTES: {
                uint32_t len = reader.readU32LE();
                pushValue(PickleValue{reader.readBytes(len)});
                break;
            }
            case opcode::BINBYTES8: {
                uint64_t len = reader.readU64LE();
                if (len > 0x7FFFFFFF) throw std::runtime_error("Pickle: BINBYTES8 too large");
                pushValue(PickleValue{reader.readBytes(static_cast<size_t>(len))});
                break;
            }

            // ===================== Containers =====================
            case opcode::EMPTY_LIST: {
                pushValue(PickleValue::makeList({}));
                break;
            }
            case opcode::EMPTY_TUPLE: {
                pushValue(PickleValue::makeList({}));
                break;
            }
            case opcode::EMPTY_DICT: {
                pushValue(PickleValue::makeDict({}));
                break;
            }
            case opcode::MARK: {
                stack.push_back({MARK_SENTINEL});
                break;
            }
            case opcode::TUPLE: {
                size_t markPos = findMark();
                PickleList items;
                for (size_t i = markPos + 1; i < stack.size(); i++) {
                    items.push_back(takeFromArena(stack[i].markOrIndex));
                }
                stack.resize(markPos);
                pushValue(PickleValue::makeList(std::move(items)));
                break;
            }
            case opcode::LIST: {
                size_t markPos = findMark();
                PickleList items;
                for (size_t i = markPos + 1; i < stack.size(); i++) {
                    items.push_back(takeFromArena(stack[i].markOrIndex));
                }
                stack.resize(markPos);
                pushValue(PickleValue::makeList(std::move(items)));
                break;
            }
            case opcode::TUPLE1: {
                auto e0 = popEntry();
                PickleList items;
                items.push_back(takeFromArena(e0.markOrIndex));
                pushValue(PickleValue::makeList(std::move(items)));
                break;
            }
            case opcode::TUPLE2: {
                auto e1 = popEntry();
                auto e0 = popEntry();
                PickleList items;
                items.push_back(takeFromArena(e0.markOrIndex));
                items.push_back(takeFromArena(e1.markOrIndex));
                pushValue(PickleValue::makeList(std::move(items)));
                break;
            }
            case opcode::TUPLE3: {
                auto e2 = popEntry();
                auto e1 = popEntry();
                auto e0 = popEntry();
                PickleList items;
                items.push_back(takeFromArena(e0.markOrIndex));
                items.push_back(takeFromArena(e1.markOrIndex));
                items.push_back(takeFromArena(e2.markOrIndex));
                pushValue(PickleValue::makeList(std::move(items)));
                break;
            }

            // ===================== Append / Set =====================
            case opcode::APPEND: {
                auto valEntry = popEntry();
                auto listEntry = popEntry();
                if (listEntry.markOrIndex == MARK_SENTINEL) {
                    throw std::runtime_error("Pickle: APPEND on MARK");
                }
                auto& listVal = arena[listEntry.markOrIndex];
                if (!listVal.isList()) throw std::runtime_error("Pickle: APPEND on non-list");
                listVal.asMutableList().push_back(takeFromArena(valEntry.markOrIndex));
                // Put list back on stack
                stack.push_back(listEntry);
                break;
            }
            case opcode::APPENDS: {
                size_t markPos = findMark();
                // The list is just below the MARK
                if (markPos == 0) throw std::runtime_error("Pickle: APPENDS without list");
                auto listEntry = stack[markPos - 1];
                auto& listVal = arena[listEntry.markOrIndex];
                if (!listVal.isList()) throw std::runtime_error("Pickle: APPENDS on non-list");
                for (size_t i = markPos + 1; i < stack.size(); i++) {
                    listVal.asMutableList().push_back(takeFromArena(stack[i].markOrIndex));
                }
                stack.resize(markPos); // Remove MARK and items, keep list
                break;
            }
            case opcode::SETITEM: {
                auto valEntry = popEntry();
                auto keyEntry = popEntry();
                auto dictEntry = popEntry();
                if (dictEntry.markOrIndex == MARK_SENTINEL) {
                    throw std::runtime_error("Pickle: SETITEM on MARK");
                }
                auto& dictVal = arena[dictEntry.markOrIndex];
                if (!dictVal.isDict()) throw std::runtime_error("Pickle: SETITEM on non-dict");
                dictVal.asMutableDict().emplace_back(
                    takeFromArena(keyEntry.markOrIndex),
                    takeFromArena(valEntry.markOrIndex)
                );
                stack.push_back(dictEntry);
                break;
            }
            case opcode::SETITEMS: {
                size_t markPos = findMark();
                if (markPos == 0) throw std::runtime_error("Pickle: SETITEMS without dict");
                auto dictEntry = stack[markPos - 1];
                auto& dictVal = arena[dictEntry.markOrIndex];
                if (!dictVal.isDict()) throw std::runtime_error("Pickle: SETITEMS on non-dict");
                // Items after MARK come in key, value pairs
                size_t itemCount = stack.size() - markPos - 1;
                if (itemCount % 2 != 0) throw std::runtime_error("Pickle: SETITEMS odd item count");
                for (size_t i = markPos + 1; i < stack.size(); i += 2) {
                    dictVal.asMutableDict().emplace_back(
                        takeFromArena(stack[i].markOrIndex),
                        takeFromArena(stack[i + 1].markOrIndex)
                    );
                }
                stack.resize(markPos);
                break;
            }

            // ===================== Memo =====================
            case opcode::BINPUT: {
                uint8_t idx = reader.readByte();
                if (stack.empty()) throw std::runtime_error("Pickle: BINPUT on empty stack");
                if (idx >= memo.size()) memo.resize(idx + 1, -1);
                memo[idx] = stack.back().markOrIndex;
                break;
            }
            case opcode::LONG_BINPUT: {
                uint32_t idx = reader.readU32LE();
                if (stack.empty()) throw std::runtime_error("Pickle: LONG_BINPUT on empty stack");
                if (idx >= memo.size()) memo.resize(idx + 1, -1);
                memo[idx] = stack.back().markOrIndex;
                break;
            }
            case opcode::MEMOIZE: {
                if (stack.empty()) throw std::runtime_error("Pickle: MEMOIZE on empty stack");
                size_t idx = 0;
                // Find next free memo slot
                for (size_t i = 0; i < memo.size(); i++) {
                    if (memo[i] == -1) { idx = i; break; }
                    if (i == memo.size() - 1) { idx = memo.size(); break; }
                }
                if (idx >= memo.size()) memo.resize(idx + 1, -1);
                memo[idx] = stack.back().markOrIndex;
                break;
            }
            case opcode::BINGET: {
                uint8_t idx = reader.readByte();
                if (idx >= memo.size() || memo[idx] == -1) {
                    throw std::runtime_error("Pickle: BINGET invalid memo index");
                }
                // Push reference to same arena slot (shared)
                stack.push_back({memo[idx]});
                break;
            }
            case opcode::LONG_BINGET: {
                uint32_t idx = reader.readU32LE();
                if (idx >= memo.size() || memo[idx] == -1) {
                    throw std::runtime_error("Pickle: LONG_BINGET invalid memo index");
                }
                stack.push_back({memo[idx]});
                break;
            }

            // ===================== Misc =====================
            case opcode::DUP: {
                if (stack.empty()) throw std::runtime_error("Pickle: DUP on empty stack");
                stack.push_back(stack.back());
                break;
            }
            case opcode::POP: {
                if (!stack.empty()) stack.pop_back();
                break;
            }
            case opcode::POP_MARK: {
                size_t markPos = findMark();
                stack.resize(markPos);
                break;
            }

            default:
                throw std::runtime_error(
                    std::string("Pickle: unsupported opcode 0x") +
                    "0123456789ABCDEF"[op >> 4] +
                    "0123456789ABCDEF"[op & 0xF]
                );
            }
        }

        if (resultIndex < 0) {
            return std::unexpected(Error{ErrorCode::ParseError, "Pickle: no STOP opcode found"});
        }

        return std::move(arena[resultIndex]);

    } catch (const std::exception& e) {
        return std::unexpected(Error{ErrorCode::ParseError, std::string("Pickle parse error: ") + e.what()});
    }
}

} // namespace makineai::formats
