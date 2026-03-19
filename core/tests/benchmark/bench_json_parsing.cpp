/**
 * @file bench_json_parsing.cpp
 * @brief Benchmark: JSON parsing performance comparison
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Compares performance of:
 * - nlohmann::json (baseline)
 * - simdjson (if available)
 * - Custom json_utils abstraction
 *
 * Build with Google Benchmark:
 *   cmake --build . --target bench_json_parsing
 *
 * Run:
 *   ./bench_json_parsing --benchmark_repetitions=5
 */

#include <benchmark/benchmark.h>

#include <makine/json_utils.hpp>
#include <makine/features.hpp>

#include <nlohmann/json.hpp>

#ifdef MAKINE_HAS_SIMDJSON
#include <simdjson.h>
#endif

#include <fstream>
#include <functional>
#include <random>
#include <sstream>

using namespace makine;

// ============================================================================
// TEST DATA GENERATION
// ============================================================================

/**
 * @brief Generate sample RPG Maker System.json content
 */
std::string generateRPGMakerSystemJson(size_t termsCount = 50) {
    nlohmann::json j;

    // Game title
    j["gameTitle"] = "Test RPG Game";
    j["versionId"] = 1234567890;
    j["locale"] = "en_US";

    // Terms (the main translation target)
    nlohmann::json terms;
    terms["basic"] = nlohmann::json::array({
        "Level", "Lv", "HP", "HP", "MP", "MP", "TP", "TP",
        "EXP", "EXP", "Max HP", "Max MP", "Attack", "Defense",
        "M.Attack", "M.Defense", "Agility", "Luck", "Hit", "Evasion"
    });

    nlohmann::json messages;
    for (size_t i = 0; i < termsCount; ++i) {
        messages["message" + std::to_string(i)] = "This is test message number " + std::to_string(i);
    }
    terms["messages"] = messages;

    nlohmann::json commands;
    commands["fight"] = "Fight";
    commands["escape"] = "Escape";
    commands["attack"] = "Attack";
    commands["guard"] = "Guard";
    commands["item"] = "Item";
    commands["skill"] = "Skill";
    commands["equip"] = "Equip";
    commands["status"] = "Status";
    commands["save"] = "Save";
    commands["gameEnd"] = "Game End";
    terms["commands"] = commands;

    j["terms"] = terms;

    // Other game data
    j["currencyUnit"] = "Gold";
    j["windowTone"] = nlohmann::json::array({0, 0, 0, 0});

    return j.dump();
}

/**
 * @brief Generate sample Epic Games manifest JSON
 */
std::string generateEpicManifestJson(size_t gamesCount = 10) {
    nlohmann::json j;
    j["formatVersion"] = 0;
    j["installationList"] = nlohmann::json::array();

    std::vector<std::string> sampleGames = {
        "Fortnite", "Rocket League", "Fall Guys", "Satisfactory",
        "Control", "Hades", "Celeste", "Outer Wilds", "Subnautica"
    };

    for (size_t i = 0; i < gamesCount; ++i) {
        nlohmann::json game;
        game["InstallLocation"] = "C:/Games/Epic/" + sampleGames[i % sampleGames.size()];
        game["NamespaceId"] = "namespace_" + std::to_string(i);
        game["ItemId"] = "item_" + std::to_string(i);
        game["ArtifactId"] = "artifact_" + std::to_string(i);
        game["AppVersion"] = "1.0." + std::to_string(i);
        game["AppName"] = sampleGames[i % sampleGames.size()];
        j["installationList"].push_back(game);
    }

    return j.dump();
}

/**
 * @brief Generate large JSON with nested structure
 */
std::string generateLargeNestedJson(size_t depth = 5, size_t breadth = 10) {
    nlohmann::json root;

    std::function<void(nlohmann::json&, size_t)> populate = [&](nlohmann::json& node, size_t level) {
        if (level == 0) {
            node["value"] = "leaf_value";
            node["number"] = 42;
            return;
        }

        for (size_t i = 0; i < breadth; ++i) {
            std::string key = "node_" + std::to_string(level) + "_" + std::to_string(i);
            node[key] = nlohmann::json::object();
            populate(node[key], level - 1);
        }
    };

    populate(root, depth);
    return root.dump();
}

// ============================================================================
// BENCHMARK FIXTURES
// ============================================================================

/**
 * @brief Fixture for JSON parsing benchmarks
 */
class JsonParsingFixture : public benchmark::Fixture {
public:
    void SetUp(const benchmark::State& state) override {
        // Generate test data based on state range
        size_t dataSize = static_cast<size_t>(state.range(0));

        smallJson_ = generateRPGMakerSystemJson(dataSize / 10);
        mediumJson_ = generateEpicManifestJson(dataSize);
        largeJson_ = generateLargeNestedJson(5, dataSize);
    }

    void TearDown(const benchmark::State&) override {
        smallJson_.clear();
        mediumJson_.clear();
        largeJson_.clear();
    }

protected:
    std::string smallJson_;
    std::string mediumJson_;
    std::string largeJson_;
};

// ============================================================================
// NLOHMANN JSON BENCHMARKS
// ============================================================================

BENCHMARK_DEFINE_F(JsonParsingFixture, NlohmannParse_Small)(benchmark::State& state) {
    for (auto _ : state) {
        auto result = nlohmann::json::parse(smallJson_);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * smallJson_.size());
}

BENCHMARK_DEFINE_F(JsonParsingFixture, NlohmannParse_Medium)(benchmark::State& state) {
    for (auto _ : state) {
        auto result = nlohmann::json::parse(mediumJson_);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * mediumJson_.size());
}

BENCHMARK_DEFINE_F(JsonParsingFixture, NlohmannParse_Large)(benchmark::State& state) {
    for (auto _ : state) {
        auto result = nlohmann::json::parse(largeJson_);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * largeJson_.size());
}

BENCHMARK_DEFINE_F(JsonParsingFixture, NlohmannAccess)(benchmark::State& state) {
    auto doc = nlohmann::json::parse(smallJson_);

    for (auto _ : state) {
        auto title = doc["gameTitle"].get<std::string>();
        auto terms = doc["terms"]["basic"];
        auto firstTerm = terms[0].get<std::string>();
        benchmark::DoNotOptimize(title);
        benchmark::DoNotOptimize(firstTerm);
    }
}

// ============================================================================
// SIMDJSON BENCHMARKS (if available)
// ============================================================================

#ifdef MAKINE_HAS_SIMDJSON

BENCHMARK_DEFINE_F(JsonParsingFixture, SimdjsonParse_Small)(benchmark::State& state) {
    simdjson::dom::parser parser;

    for (auto _ : state) {
        auto result = parser.parse(smallJson_);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * smallJson_.size());
}

BENCHMARK_DEFINE_F(JsonParsingFixture, SimdjsonParse_Medium)(benchmark::State& state) {
    simdjson::dom::parser parser;

    for (auto _ : state) {
        auto result = parser.parse(mediumJson_);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * mediumJson_.size());
}

BENCHMARK_DEFINE_F(JsonParsingFixture, SimdjsonParse_Large)(benchmark::State& state) {
    simdjson::dom::parser parser;

    for (auto _ : state) {
        auto result = parser.parse(largeJson_);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * largeJson_.size());
}

BENCHMARK_DEFINE_F(JsonParsingFixture, SimdjsonAccess)(benchmark::State& state) {
    simdjson::dom::parser parser;
    auto doc = parser.parse(smallJson_);

    for (auto _ : state) {
        auto title = doc["gameTitle"].get_string();
        auto terms = doc["terms"]["basic"];
        auto firstTerm = terms.at(0).get_string();
        benchmark::DoNotOptimize(title);
        benchmark::DoNotOptimize(firstTerm);
    }
}

#endif  // MAKINE_HAS_SIMDJSON

// ============================================================================
// JSON_UTILS ABSTRACTION BENCHMARKS
// ============================================================================

BENCHMARK_DEFINE_F(JsonParsingFixture, JsonUtils_Parse_Small)(benchmark::State& state) {
    for (auto _ : state) {
        auto result = json::parseString(smallJson_);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * smallJson_.size());
}

BENCHMARK_DEFINE_F(JsonParsingFixture, JsonUtils_Parse_Medium)(benchmark::State& state) {
    for (auto _ : state) {
        auto result = json::parseString(mediumJson_);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * mediumJson_.size());
}

BENCHMARK_DEFINE_F(JsonParsingFixture, JsonUtils_Access)(benchmark::State& state) {
    auto doc = json::parseString(smallJson_);
    if (!doc) {
        state.SkipWithError("Failed to parse JSON");
        return;
    }

    for (auto _ : state) {
        auto title = doc->getString("gameTitle");
        benchmark::DoNotOptimize(title);
    }
}

// ============================================================================
// REGISTER BENCHMARKS
// ============================================================================

BENCHMARK_REGISTER_F(JsonParsingFixture, NlohmannParse_Small)
    ->RangeMultiplier(2)
    ->Range(10, 100)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(JsonParsingFixture, NlohmannParse_Medium)
    ->RangeMultiplier(2)
    ->Range(10, 100)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(JsonParsingFixture, NlohmannParse_Large)
    ->RangeMultiplier(2)
    ->Range(5, 20)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(JsonParsingFixture, NlohmannAccess)
    ->Unit(benchmark::kNanosecond);

#ifdef MAKINE_HAS_SIMDJSON
BENCHMARK_REGISTER_F(JsonParsingFixture, SimdjsonParse_Small)
    ->RangeMultiplier(2)
    ->Range(10, 100)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(JsonParsingFixture, SimdjsonParse_Medium)
    ->RangeMultiplier(2)
    ->Range(10, 100)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(JsonParsingFixture, SimdjsonParse_Large)
    ->RangeMultiplier(2)
    ->Range(5, 20)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(JsonParsingFixture, SimdjsonAccess)
    ->Unit(benchmark::kNanosecond);
#endif

BENCHMARK_REGISTER_F(JsonParsingFixture, JsonUtils_Parse_Small)
    ->RangeMultiplier(2)
    ->Range(10, 100)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(JsonParsingFixture, JsonUtils_Parse_Medium)
    ->RangeMultiplier(2)
    ->Range(10, 100)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(JsonParsingFixture, JsonUtils_Access)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_MAIN();
