/**
 * @file fuzz_json_parser.cpp
 * @brief Fuzzer for JSON parsing (simdjson/nlohmann abstraction)
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Tests JSON parsing robustness with malformed input.
 * Important for RPG Maker JSON files which users may edit.
 *
 * Run:
 *   ./fuzz_json_parser corpus/json -max_len=1048576
 */

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>

#include <makine/json_utils.hpp>

using namespace makine;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Need some content
    if (size < 2) {
        return 0;
    }

    // Limit size
    if (size > 10 * 1024 * 1024) {
        return 0;
    }

    try {
        // Convert to string
        std::string jsonStr(reinterpret_cast<const char*>(data), size);

        // Try parsing with our abstraction
        auto result = json::parseString(jsonStr);

        if (result) {
            auto& doc = *result;

            // Try various access patterns
            // These should not crash even with weird JSON

            // Try to get as object
            auto obj = doc.asObject();
            if (obj) {
                // Iterate keys (limited)
                int count = 0;
                for (const auto& [key, value] : *obj) {
                    volatile size_t keyLen = key.length();
                    (void)keyLen;

                    // Try to get value as string
                    auto str = value.asString();
                    if (str) {
                        volatile size_t strLen = str->length();
                        (void)strLen;
                    }

                    if (++count > 100) break;  // Limit iteration
                }
            }

            // Try to get as array
            auto arr = doc.asArray();
            if (arr) {
                int count = 0;
                for (const auto& elem : *arr) {
                    auto str = elem.asString();
                    if (str) {
                        volatile size_t strLen = str->length();
                        (void)strLen;
                    }

                    if (++count > 100) break;
                }
            }

            // Try path access
            auto nested = doc.at("data").at("items").at(0).asString();
            if (nested) {
                volatile size_t len = nested->length();
                (void)len;
            }
        }

    } catch (const std::bad_alloc&) {
        return 0;
    } catch (const std::exception&) {
        return 0;
    } catch (...) {
        return 0;
    }

    return 0;
}

// Seed corpus with various JSON structures
#ifdef GENERATE_CORPUS
#include <fstream>

void generateSeedCorpus(const char* outputDir) {
    // Empty object
    std::ofstream(std::string(outputDir) + "/seed_empty_obj.json") << "{}";

    // Empty array
    std::ofstream(std::string(outputDir) + "/seed_empty_arr.json") << "[]";

    // Simple object
    std::ofstream(std::string(outputDir) + "/seed_simple.json")
        << R"({"name":"test","value":42})";

    // Nested structure
    std::ofstream(std::string(outputDir) + "/seed_nested.json")
        << R"({"data":{"items":[{"id":1,"text":"hello"}]}})";

    // RPG Maker style
    std::ofstream(std::string(outputDir) + "/seed_rpgmaker.json")
        << R"({"gameTitle":"Test","terms":{"basic":["HP","MP"]}})";

    // Unicode
    std::ofstream(std::string(outputDir) + "/seed_unicode.json")
        << R"({"text":"Merhaba Dünya 世界 🎮"})";

    // Large array
    std::ofstream out(std::string(outputDir) + "/seed_large_array.json");
    out << "[";
    for (int i = 0; i < 1000; ++i) {
        if (i > 0) out << ",";
        out << "\"item" << i << "\"";
    }
    out << "]";
}

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;
    generateSeedCorpus(argv[1]);
    return 0;
}
#endif
