# Test Yazma

Makine-Launcher test stratejisi ve örnekleri.

---

## Test Framework

- **Google Test (GTest)** - Unit testler
- **CTest** - Test runner
- **Qt Test** - UI testleri (planlanan)

---

## Test Yapısı

```
core/
└── tests/
    ├── CMakeLists.txt
    ├── test_main.cpp           # GTest main
    ├── test_game_detector.cpp
    ├── test_asset_parser.cpp
    ├── test_patch_engine.cpp
    └── testdata/               # Test verileri
        ├── unity_game/
        └── unreal_game/
```

> **Not:** Game-specific integration testler engine handler implementasyonu
> bekledigi icin devre disi birakilmistir. `makine_tests` unit test
> target'i aktiftir.

---

## Unit Test Örnekleri

### Temel Test

```cpp
#include <gtest/gtest.h>
#include <makine/core.hpp>

TEST(GameDetectorTest, DetectsSteamGames) {
    auto& core = makine::Core::instance();
    core.initialize();

    auto& detector = core.gameDetector();
    auto games = detector.scanSteam();

    // Steam kuruluysa oyun bulmali
    // (Test ortamina bagli)
    EXPECT_GE(games.size(), 0);
}
```

### Fixture Kullanımı

```cpp
class AssetParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        core_ = &makine::Core::instance();
        core_->initialize();
    }

    void TearDown() override {
        core_->shutdown();
    }

    makine::Core* core_;
};

TEST_F(AssetParserTest, DetectsUnityEngine) {
    auto& parser = core_->assetParser();

    auto result = parser.detectEngine("testdata/unity_game");

    ASSERT_TRUE(result.success());
    EXPECT_EQ(result.value(), makine::EngineType::Unity);
}

TEST_F(AssetParserTest, DetectsUnrealEngine) {
    auto& parser = core_->assetParser();

    auto result = parser.detectEngine("testdata/unreal_game");

    ASSERT_TRUE(result.success());
    EXPECT_EQ(result.value(), makine::EngineType::Unreal);
}
```

### Parameterized Test

```cpp
class EngineDetectionTest : public ::testing::TestWithParam<
    std::tuple<std::string, makine::EngineType>
> {};

TEST_P(EngineDetectionTest, DetectsEngine) {
    auto [path, expectedEngine] = GetParam();

    auto& parser = makine::Core::instance().assetParser();
    auto result = parser.detectEngine(path);

    ASSERT_TRUE(result.success());
    EXPECT_EQ(result.value(), expectedEngine);
}

INSTANTIATE_TEST_SUITE_P(
    Engines,
    EngineDetectionTest,
    ::testing::Values(
        std::make_tuple("testdata/unity_game", makine::EngineType::Unity),
        std::make_tuple("testdata/unreal_game", makine::EngineType::Unreal),
        std::make_tuple("testdata/rpgmaker_game", makine::EngineType::RpgMaker)
    )
);
```

---

## Mock Kullanımı

```cpp
#include <gmock/gmock.h>

class MockFileSystem : public IFileSystem {
public:
    MOCK_METHOD(bool, exists, (const std::string& path), (const, override));
    MOCK_METHOD(std::vector<std::byte>, read, (const std::string& path), (override));
};

TEST(PatchEngineTest, FailsWhenFileNotFound) {
    MockFileSystem mockFs;
    EXPECT_CALL(mockFs, exists(_))
        .WillOnce(Return(false));

    PatchEngine engine(&mockFs);
    auto result = engine.apply(game, package);

    EXPECT_FALSE(result.success());
    EXPECT_EQ(result.error().code(), ErrorCode::NotFound);
}
```

---

## Test Çalıştırma

### Tüm Testler

```bash
# just ile
just test

# veya CMake ile
cd build/dev
ctest --output-on-failure
```

### Spesifik Test

```bash
# Pattern ile
ctest -R GameDetector

# Verbose
ctest -V -R AssetParser
```

### Test Listesi

```bash
ctest -N  # Sadece listele, calistirma
```

---

## Coverage

### Coverage ile Build

```bash
# GCC/Clang
cmake -B build -DCOVERAGE=ON
cmake --build build
ctest --test-dir build

# Rapor olustur
gcovr --html coverage.html --html-details
```

### Coverage Hedefleri

| Modül | Hedef |
|-------|-------|
| Core | 80%+ |
| Services | 70%+ |
| Utils | 90%+ |

---

## CI'da Test

GitHub Actions'da otomatik test:

```yaml
- name: Run Tests
  run: |
    cd build/release
    ctest --output-on-failure --parallel 4
```

---

## Test Verileri

### Dizin Yapısı

```
testdata/
├── unity_game/
│   ├── UnityPlayer.dll
│   └── GameName_Data/
│       └── resources.assets
├── unreal_game/
│   ├── GameName.exe
│   └── Content/
│       └── Paks/
└── rpgmaker_game/
    └── www/
        └── data/
            └── System.json
```

### Test Verisi Oluşturma

```cpp
// Test helper
void createTestUnityGame(const std::string& path) {
    fs::create_directories(path + "/GameName_Data");
    // Minimal Unity dosyalari olustur
}
```

---

## Best Practices

### 1. Bağımsız Testler

```cpp
// Her test kendi setup'ini yapmali
TEST(MyTest, DoSomething) {
    auto core = createTestCore();  // Fresh instance
    // test...
}
```

### 2. Descriptive İsimler

```cpp
// YANLIS
TEST(Test1, Test2) { }

// DOGRU
TEST(GameDetector, ReturnsSteamGamesWhenSteamInstalled) { }
TEST(PatchEngine, CreatesBackupBeforeApplyingPatch) { }
```

### 3. Arrange-Act-Assert

```cpp
TEST(AssetParser, DetectsUnityMono) {
    // Arrange
    auto& parser = core.assetParser();
    std::string path = "testdata/unity_mono";

    // Act
    auto result = parser.detectEngine(path);

    // Assert
    ASSERT_TRUE(result.success());
    EXPECT_EQ(result.value(), EngineType::Unity);
}
```

---

## Sonraki Adımlar

- [Core Kütüphane](core-library.md)
- [Build Sistemi](build-system.md)
