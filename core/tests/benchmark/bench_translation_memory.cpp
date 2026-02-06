/**
 * @file bench_translation_memory.cpp
 * @brief Benchmark: Translation Memory performance
 * @copyright (c) 2026 MakineAI Team
 *
 * Benchmarks:
 * - N-gram generation
 * - Levenshtein distance calculation
 * - Fuzzy matching algorithms
 * - TM lookup performance
 *
 * Build:
 *   cmake --build . --target bench_translation_memory
 *
 * Run:
 *   ./bench_translation_memory --benchmark_repetitions=3
 */

#include <benchmark/benchmark.h>

#include <makineai/translation_memory.hpp>
#include <makineai/string_pool.hpp>

#include <random>
#include <unordered_set>

using namespace makineai;

// ============================================================================
// TEST DATA GENERATION
// ============================================================================

/**
 * @brief Generate random game dialogue strings
 */
std::vector<std::string> generateDialogueStrings(size_t count) {
    std::vector<std::string> templates = {
        "Welcome to {place}, adventurer!",
        "Have you seen the {item} around here?",
        "The {enemy} attacked the village last night.",
        "I need you to deliver this {item} to {person}.",
        "Be careful in the {place}, it's dangerous.",
        "Thank you for saving us from the {enemy}!",
        "Would you like to buy some {item}?",
        "The legend says that {person} once lived here.",
        "You have obtained {item}!",
        "Quest completed: Defeat the {enemy}",
        "{person} has joined your party!",
        "Your {stat} has increased by {number}!",
    };

    std::vector<std::string> places = {"forest", "dungeon", "castle", "village", "mountain"};
    std::vector<std::string> items = {"sword", "potion", "key", "scroll", "gem"};
    std::vector<std::string> enemies = {"dragon", "goblin", "skeleton", "demon", "wolf"};
    std::vector<std::string> persons = {"king", "wizard", "knight", "merchant", "elder"};
    std::vector<std::string> stats = {"strength", "agility", "wisdom", "luck", "defense"};

    std::vector<std::string> result;
    result.reserve(count);

    std::mt19937 rng(42);  // Fixed seed for reproducibility

    for (size_t i = 0; i < count; ++i) {
        std::string text = templates[rng() % templates.size()];

        // Replace placeholders
        size_t pos;
        while ((pos = text.find("{place}")) != std::string::npos) {
            text.replace(pos, 7, places[rng() % places.size()]);
        }
        while ((pos = text.find("{item}")) != std::string::npos) {
            text.replace(pos, 6, items[rng() % items.size()]);
        }
        while ((pos = text.find("{enemy}")) != std::string::npos) {
            text.replace(pos, 7, enemies[rng() % enemies.size()]);
        }
        while ((pos = text.find("{person}")) != std::string::npos) {
            text.replace(pos, 8, persons[rng() % persons.size()]);
        }
        while ((pos = text.find("{stat}")) != std::string::npos) {
            text.replace(pos, 6, stats[rng() % stats.size()]);
        }
        while ((pos = text.find("{number}")) != std::string::npos) {
            text.replace(pos, 8, std::to_string((rng() % 10) + 1));
        }

        result.push_back(text);
    }

    return result;
}

/**
 * @brief Generate similar strings (for fuzzy matching tests)
 */
std::pair<std::string, std::string> generateSimilarStrings(size_t length, double similarity) {
    std::string base;
    base.reserve(length);

    std::mt19937 rng(42);
    for (size_t i = 0; i < length; ++i) {
        base += 'a' + (rng() % 26);
    }

    std::string modified = base;
    size_t changesNeeded = static_cast<size_t>(length * (1.0 - similarity));

    for (size_t i = 0; i < changesNeeded && i < length; ++i) {
        size_t pos = rng() % length;
        modified[pos] = 'a' + (rng() % 26);
    }

    return {base, modified};
}

// ============================================================================
// BENCHMARK FIXTURES
// ============================================================================

class TMFixture : public benchmark::Fixture {
public:
    void SetUp(const benchmark::State& state) override {
        size_t count = static_cast<size_t>(state.range(0));
        strings_ = generateDialogueStrings(count);
    }

    void TearDown(const benchmark::State&) override {
        strings_.clear();
    }

protected:
    std::vector<std::string> strings_;
};

// ============================================================================
// N-GRAM BENCHMARKS
// ============================================================================

BENCHMARK_DEFINE_F(TMFixture, NgramGeneration)(benchmark::State& state) {
    for (auto _ : state) {
        for (const auto& str : strings_) {
            auto ngrams = TranslationMemoryService::generateNgrams(str, 3);
            benchmark::DoNotOptimize(ngrams);
        }
    }
    state.SetItemsProcessed(state.iterations() * strings_.size());
}

static void BM_NgramGeneration_VaryingLength(benchmark::State& state) {
    size_t length = static_cast<size_t>(state.range(0));
    std::string text(length, 'a');

    for (size_t i = 0; i < length; ++i) {
        text[i] = 'a' + (i % 26);
    }

    for (auto _ : state) {
        auto ngrams = TranslationMemoryService::generateNgrams(text, 3);
        benchmark::DoNotOptimize(ngrams);
    }
    state.SetBytesProcessed(state.iterations() * length);
}

BENCHMARK(BM_NgramGeneration_VaryingLength)
    ->RangeMultiplier(4)
    ->Range(16, 4096)
    ->Unit(benchmark::kMicrosecond);

// ============================================================================
// LEVENSHTEIN DISTANCE BENCHMARKS
// ============================================================================

static void BM_LevenshteinDistance_Short(benchmark::State& state) {
    std::string s1 = "Hello World";
    std::string s2 = "Hello World!";

    for (auto _ : state) {
        auto dist = TranslationMemoryService::levenshteinDistance(s1, s2);
        benchmark::DoNotOptimize(dist);
    }
}
BENCHMARK(BM_LevenshteinDistance_Short)->Unit(benchmark::kNanosecond);

static void BM_LevenshteinDistance_Medium(benchmark::State& state) {
    std::string s1 = "The quick brown fox jumps over the lazy dog";
    std::string s2 = "The quick brown cat jumps over the lazy dogs";

    for (auto _ : state) {
        auto dist = TranslationMemoryService::levenshteinDistance(s1, s2);
        benchmark::DoNotOptimize(dist);
    }
}
BENCHMARK(BM_LevenshteinDistance_Medium)->Unit(benchmark::kMicrosecond);

static void BM_LevenshteinDistance_Long(benchmark::State& state) {
    auto [s1, s2] = generateSimilarStrings(500, 0.9);

    for (auto _ : state) {
        auto dist = TranslationMemoryService::levenshteinDistance(s1, s2);
        benchmark::DoNotOptimize(dist);
    }
}
BENCHMARK(BM_LevenshteinDistance_Long)->Unit(benchmark::kMicrosecond);

static void BM_LevenshteinRatio(benchmark::State& state) {
    size_t length = static_cast<size_t>(state.range(0));
    auto [s1, s2] = generateSimilarStrings(length, 0.8);

    for (auto _ : state) {
        auto ratio = TranslationMemoryService::levenshteinRatio(s1, s2);
        benchmark::DoNotOptimize(ratio);
    }
}
BENCHMARK(BM_LevenshteinRatio)
    ->RangeMultiplier(2)
    ->Range(10, 500)
    ->Unit(benchmark::kMicrosecond);

// ============================================================================
// JACCARD SIMILARITY BENCHMARKS
// ============================================================================

static void BM_JaccardSimilarity(benchmark::State& state) {
    size_t ngramCount = static_cast<size_t>(state.range(0));

    std::vector<std::string> ngrams1, ngrams2;
    ngrams1.reserve(ngramCount);
    ngrams2.reserve(ngramCount);

    for (size_t i = 0; i < ngramCount; ++i) {
        ngrams1.push_back("ng" + std::to_string(i));
        ngrams2.push_back("ng" + std::to_string(i + ngramCount / 2));  // 50% overlap
    }

    for (auto _ : state) {
        auto sim = TranslationMemoryService::jaccardSimilarity(ngrams1, ngrams2);
        benchmark::DoNotOptimize(sim);
    }
}
BENCHMARK(BM_JaccardSimilarity)
    ->RangeMultiplier(2)
    ->Range(10, 1000)
    ->Unit(benchmark::kMicrosecond);

// ============================================================================
// HYBRID SIMILARITY SCORE BENCHMARKS
// ============================================================================

static void BM_HybridSimilarity_Simple(benchmark::State& state) {
    std::string source = "The player attacks the enemy";
    std::string candidate = "The player attacks the monster";

    for (auto _ : state) {
        auto score = TranslationMemoryService::calculateSimilarityScore(
            source, candidate);
        benchmark::DoNotOptimize(score);
    }
}
BENCHMARK(BM_HybridSimilarity_Simple)->Unit(benchmark::kMicrosecond);

static void BM_HybridSimilarity_WithContext(benchmark::State& state) {
    std::string source = "The player attacks the enemy";
    std::string candidate = "The player attacks the monster";

    TranslationMemoryService::MatchContext sourceCtx;
    sourceCtx.gameId = "game_123";
    sourceCtx.engineType = "Unity";
    sourceCtx.category = "Dialogue";

    TranslationMemoryService::MatchContext candidateCtx;
    candidateCtx.gameId = "game_123";  // Same game
    candidateCtx.engineType = "Unity";
    candidateCtx.category = "Dialogue";

    for (auto _ : state) {
        auto score = TranslationMemoryService::calculateSimilarityScore(
            source, candidate, sourceCtx, candidateCtx);
        benchmark::DoNotOptimize(score);
    }
}
BENCHMARK(BM_HybridSimilarity_WithContext)->Unit(benchmark::kMicrosecond);

// ============================================================================
// TEXT PROCESSING BENCHMARKS
// ============================================================================

static void BM_NormalizeText(benchmark::State& state) {
    std::string text = "  Hello,   World!  This is a TEST string...  ";

    for (auto _ : state) {
        auto normalized = TranslationMemoryService::normalizeText(text);
        benchmark::DoNotOptimize(normalized);
    }
}
BENCHMARK(BM_NormalizeText)->Unit(benchmark::kNanosecond);

static void BM_HashText(benchmark::State& state) {
    std::string text = "Hello World! This is a test string for hashing.";

    for (auto _ : state) {
        auto hash = TranslationMemoryService::hashText(text);
        benchmark::DoNotOptimize(hash);
    }
}
BENCHMARK(BM_HashText)->Unit(benchmark::kMicrosecond);

// ============================================================================
// STRING POOL BENCHMARKS
// ============================================================================

static void BM_StringPool_Intern_New(benchmark::State& state) {
    auto& pool = StringPool::instance();
    pool.clear();

    size_t i = 0;
    for (auto _ : state) {
        auto sv = pool.intern("unique_string_" + std::to_string(i++));
        benchmark::DoNotOptimize(sv);
    }
}
BENCHMARK(BM_StringPool_Intern_New)->Unit(benchmark::kNanosecond);

static void BM_StringPool_Intern_Existing(benchmark::State& state) {
    auto& pool = StringPool::instance();
    pool.clear();

    // Pre-populate
    std::string str = "repeated_string_for_interning";
    pool.intern(str);

    for (auto _ : state) {
        auto sv = pool.intern(str);
        benchmark::DoNotOptimize(sv);
    }
}
BENCHMARK(BM_StringPool_Intern_Existing)->Unit(benchmark::kNanosecond);

// ============================================================================
// BATCH MATCHING BENCHMARKS
// ============================================================================

BENCHMARK_DEFINE_F(TMFixture, BatchMatching)(benchmark::State& state) {
    // Create candidate strings (simulated TM)
    auto candidates = generateDialogueStrings(1000);

    for (auto _ : state) {
        for (const auto& source : strings_) {
            double bestScore = 0.0;
            for (const auto& candidate : candidates) {
                auto score = TranslationMemoryService::calculateSimilarityScore(
                    source, candidate);
                if (score > bestScore) {
                    bestScore = score;
                }
            }
            benchmark::DoNotOptimize(bestScore);
        }
    }
    state.SetItemsProcessed(state.iterations() * strings_.size());
}

BENCHMARK_REGISTER_F(TMFixture, NgramGeneration)
    ->RangeMultiplier(2)
    ->Range(10, 1000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(TMFixture, BatchMatching)
    ->Arg(10)
    ->Arg(50)
    ->Arg(100)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
