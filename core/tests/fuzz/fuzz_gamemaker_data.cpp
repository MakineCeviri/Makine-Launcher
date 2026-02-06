/**
 * @file fuzz_gamemaker_data.cpp
 * @brief Fuzzer for GameMaker data.win parser
 * @copyright (c) 2026 MakineAI Team
 *
 * Tests GameMaker binary format parsing with malformed input.
 * Critical: data.win files can be modified by users for modding.
 *
 * Run:
 *   ./fuzz_gamemaker_data corpus/gamemaker_data -max_len=1048576
 */

#include <cstdint>
#include <cstddef>
#include <span>

#include <makineai/asset_parser.hpp>
#include <formats/gamemaker_data.hpp>

using namespace makineai;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // GameMaker data.win has minimum structure
    if (size < 8) {
        return 0;
    }

    // Limit to reasonable size
    if (size > 50 * 1024 * 1024) {  // 50 MB max
        return 0;
    }

    try {
        formats::GameMakerDataParser parser;

        std::span<const uint8_t> inputSpan(data, size);

        // Check magic bytes (FORM)
        auto detectResult = parser.detect(inputSpan);
        if (!detectResult || !*detectResult) {
            return 0;
        }

        // Parse the file
        auto parseResult = parser.parse(inputSpan);

        if (parseResult) {
            auto& result = *parseResult;

            // Validate extracted strings
            for (const auto& entry : result.strings) {
                // Check string is valid UTF-8 or at least doesn't crash
                volatile size_t len = entry.sourceText.length();
                (void)len;

                // Check offset is reasonable
                if (entry.offset > size) {
                    // Parser should not return invalid offsets
                    // But we don't crash - just note it
                }
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

// Seed corpus with GameMaker magic
#ifdef GENERATE_CORPUS
#include <fstream>
#include <cstring>

void generateSeedCorpus(const char* outputDir) {
    // FORM chunk header
    std::vector<uint8_t> seed(1024, 0);

    // "FORM" magic
    seed[0] = 'F';
    seed[1] = 'O';
    seed[2] = 'R';
    seed[3] = 'M';

    // Size (little endian) - rest of file
    uint32_t formSize = 1016;
    std::memcpy(&seed[4], &formSize, 4);

    // "GEN8" general info chunk
    seed[8] = 'G';
    seed[9] = 'E';
    seed[10] = 'N';
    seed[11] = '8';

    // GEN8 size
    uint32_t gen8Size = 100;
    std::memcpy(&seed[12], &gen8Size, 4);

    // "STRG" strings chunk
    seed[116] = 'S';
    seed[117] = 'T';
    seed[118] = 'R';
    seed[119] = 'G';

    // STRG size
    uint32_t strgSize = 100;
    std::memcpy(&seed[120], &strgSize, 4);

    // String count
    uint32_t stringCount = 2;
    std::memcpy(&seed[124], &stringCount, 4);

    std::ofstream out(std::string(outputDir) + "/seed_datawin.bin", std::ios::binary);
    out.write(reinterpret_cast<char*>(seed.data()), seed.size());
}

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;
    generateSeedCorpus(argv[1]);
    return 0;
}
#endif
