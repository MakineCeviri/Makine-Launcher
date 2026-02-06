/**
 * @file fuzz_translation_memory.cpp
 * @brief Fuzzer for Translation Memory string processing
 * @copyright (c) 2026 MakineAI Team
 *
 * Tests string normalization, hashing, n-gram generation, and similarity
 * calculations with malformed/adversarial input.
 *
 * Run:
 *   ./fuzz_translation_memory corpus/strings -max_len=65536
 */

#include <cstdint>
#include <cstddef>
#include <string>

#include <makineai/translation_memory.hpp>

using namespace makineai;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Need some content
    if (size < 1) {
        return 0;
    }

    // Limit size to prevent slow operations
    if (size > 100 * 1024) {  // 100 KB max per string
        return 0;
    }

    try {
        // Convert to string
        std::string input(reinterpret_cast<const char*>(data), size);

        // Test normalization
        // Should handle any byte sequence without crashing
        auto normalized = TranslationMemoryService::normalizeText(input);
        volatile size_t normLen = normalized.length();
        (void)normLen;

        // Test hashing
        // Should produce consistent hash for any input
        auto hash = TranslationMemoryService::hashText(input);
        volatile size_t hashLen = hash.length();
        (void)hashLen;

        // Test n-gram generation
        // Should handle very long strings and weird characters
        auto ngrams = TranslationMemoryService::generateNgrams(input, 3);
        volatile size_t ngramCount = ngrams.size();
        (void)ngramCount;

        // Test with different n-gram sizes
        for (int n = 2; n <= 5; ++n) {
            auto ng = TranslationMemoryService::generateNgrams(input, n);
            volatile size_t cnt = ng.size();
            (void)cnt;
        }

        // Test similarity calculations if we have enough data
        if (size >= 2) {
            // Split input into two parts
            size_t mid = size / 2;
            std::string part1(reinterpret_cast<const char*>(data), mid);
            std::string part2(reinterpret_cast<const char*>(data + mid), size - mid);

            // Levenshtein distance
            auto dist = TranslationMemoryService::levenshteinDistance(part1, part2);
            volatile int d = dist;
            (void)d;

            // Levenshtein ratio
            auto ratio = TranslationMemoryService::levenshteinRatio(part1, part2);
            volatile double r = ratio;
            (void)r;

            // Length similarity
            auto lenSim = TranslationMemoryService::lengthSimilarity(part1, part2);
            volatile double ls = lenSim;
            (void)ls;

            // Keyword match ratio
            auto kwRatio = TranslationMemoryService::keywordMatchRatio(part1, part2);
            volatile double kw = kwRatio;
            (void)kw;

            // Jaccard similarity
            auto ngrams1 = TranslationMemoryService::generateNgrams(part1, 3);
            auto ngrams2 = TranslationMemoryService::generateNgrams(part2, 3);
            auto jaccard = TranslationMemoryService::jaccardSimilarity(ngrams1, ngrams2);
            volatile double j = jaccard;
            (void)j;

            // Full similarity score
            auto score = TranslationMemoryService::calculateSimilarityScore(part1, part2);
            volatile double s = score;
            (void)s;
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

// Seed corpus with various string types
#ifdef GENERATE_CORPUS
#include <fstream>

void generateSeedCorpus(const char* outputDir) {
    // ASCII
    std::ofstream(std::string(outputDir) + "/seed_ascii.txt")
        << "Hello World! This is a test string.";

    // Turkish
    std::ofstream(std::string(outputDir) + "/seed_turkish.txt")
        << "Merhaba Dünya! Bu bir test metnidir. Şeker çörek.";

    // Japanese
    std::ofstream(std::string(outputDir) + "/seed_japanese.txt")
        << "こんにちは世界！これはテスト文字列です。";

    // Mixed
    std::ofstream(std::string(outputDir) + "/seed_mixed.txt")
        << "Hello 世界! Merhaba مرحبا 🎮";

    // Long repeated
    std::ofstream out(std::string(outputDir) + "/seed_repeated.txt");
    for (int i = 0; i < 1000; ++i) {
        out << "repeated text ";
    }

    // Special characters
    std::ofstream(std::string(outputDir) + "/seed_special.txt")
        << "Tab:\t Newline:\n Quote:\" Backslash:\\ Null:\0 End";

    // Very similar strings
    std::ofstream(std::string(outputDir) + "/seed_similar.txt")
        << "The quick brown fox jumps over the lazy dog."
        << "The quick brown cat jumps over the lazy dogs.";

    // Numbers and placeholders
    std::ofstream(std::string(outputDir) + "/seed_placeholders.txt")
        << "Player {0} scored {1} points! Only {2} remaining.";
}

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;
    generateSeedCorpus(argv[1]);
    return 0;
}
#endif
