/**
 * @file fuzz_unity_bundle.cpp
 * @brief Fuzzer for Unity bundle parser
 * @copyright (c) 2026 MakineAI Team
 *
 * Tests Unity asset bundle parsing with malformed input.
 * Targets: buffer overflows, integer overflows, infinite loops.
 *
 * Run:
 *   ./fuzz_unity_bundle corpus/unity_bundle -max_len=65536 -max_total_time=3600
 */

#include <cstdint>
#include <cstddef>
#include <span>
#include <vector>

// Forward declare to avoid full include
namespace makineai::formats {
class UnityBundleParser;
}

#include <makineai/asset_parser.hpp>
#include <formats/unity_bundle.hpp>

using namespace makineai;

/**
 * @brief LibFuzzer entry point
 *
 * @param data Fuzzer-generated input data
 * @param size Size of input data
 * @return 0 on success (even if parsing fails gracefully)
 */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Skip very small inputs (can't be valid bundles)
    if (size < 16) {
        return 0;
    }

    // Limit maximum size to prevent OOM
    if (size > 10 * 1024 * 1024) {  // 10 MB max
        return 0;
    }

    try {
        // Create parser
        formats::UnityBundleParser parser;

        // Create span from input
        std::span<const uint8_t> inputSpan(data, size);

        // Try to detect if this looks like a Unity bundle
        // The parser should handle invalid data gracefully
        auto detectResult = parser.detect(inputSpan);
        if (!detectResult || !*detectResult) {
            // Not a Unity bundle - still valid fuzzing result
            return 0;
        }

        // Try to parse the bundle
        // This should NOT crash even with malformed input
        auto parseResult = parser.parse(inputSpan);

        // If parsing succeeded, try to extract strings
        if (parseResult) {
            auto& result = *parseResult;

            // Access parsed data to ensure it's valid
            for (const auto& entry : result.strings) {
                // Touch the data to catch memory errors
                volatile size_t len = entry.sourceText.length();
                (void)len;

                if (!entry.context.empty()) {
                    volatile char c = entry.context[0];
                    (void)c;
                }
            }
        }

    } catch (const std::bad_alloc&) {
        // OOM is acceptable for fuzzing
        return 0;
    } catch (const std::exception&) {
        // Other exceptions should be caught gracefully
        return 0;
    } catch (...) {
        // Unknown exceptions - parser should handle these
        return 0;
    }

    return 0;
}

/**
 * @brief Optional: Custom mutator for Unity-specific mutations
 */
extern "C" size_t LLVMFuzzerCustomMutator(
    uint8_t* data,
    size_t size,
    size_t maxSize,
    unsigned int seed
) {
    // Use default mutator
    return 0;
}

/**
 * @brief Optional: Seed corpus generator
 *
 * Creates minimal valid-looking Unity bundle headers for initial corpus.
 */
#ifdef GENERATE_CORPUS
#include <fstream>
#include <cstring>

void generateSeedCorpus(const char* outputDir) {
    // UnityFS signature
    const char* unityFsSig = "UnityFS";

    // Minimal header
    std::vector<uint8_t> seed1(64, 0);
    std::memcpy(seed1.data(), unityFsSig, 7);
    seed1[7] = 0;  // Null terminator
    seed1[8] = 0;  // Version (big endian)
    seed1[9] = 0;
    seed1[10] = 0;
    seed1[11] = 6;  // Version 6

    std::ofstream out1(std::string(outputDir) + "/seed_unityfs.bin", std::ios::binary);
    out1.write(reinterpret_cast<char*>(seed1.data()), seed1.size());

    // UnityRaw signature
    const char* unityRawSig = "UnityRaw";
    std::vector<uint8_t> seed2(64, 0);
    std::memcpy(seed2.data(), unityRawSig, 8);

    std::ofstream out2(std::string(outputDir) + "/seed_unityraw.bin", std::ios::binary);
    out2.write(reinterpret_cast<char*>(seed2.data()), seed2.size());

    // UnityWeb signature
    const char* unityWebSig = "UnityWeb";
    std::vector<uint8_t> seed3(64, 0);
    std::memcpy(seed3.data(), unityWebSig, 8);

    std::ofstream out3(std::string(outputDir) + "/seed_unityweb.bin", std::ios::binary);
    out3.write(reinterpret_cast<char*>(seed3.data()), seed3.size());
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <output_dir>\n";
        return 1;
    }
    generateSeedCorpus(argv[1]);
    return 0;
}
#endif
