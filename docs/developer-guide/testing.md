# Test Yazma

Makine-Launcher test stratejisi ve örnekleri.

---

## Test Framework

- **Google Test (GTest)** - Core ve UI servis testleri (gmock `makine_tests`'e linkli, şu an kullanılmıyor)
- **CTest** - Test runner

---

## Test Yapısı

```
core/tests/                     # makine_tests — 231 test
├── test_main.cpp               # GTest main
├── test_error.cpp
├── test_path_utils.cpp
├── test_file_integrity.cpp
├── test_crash_recovery.cpp
└── test_package_catalog.cpp

tests/
├── ui/                         # Servis katmanı testleri, her dosya ayrı exe
│   ├── test_vdfparser.cpp
│   ├── test_catalogstore.cpp
│   └── ...                     # 17 dosya
├── integration/                # test_operationjournal_recover (core + servis)
└── plugins/dummy/              # Eklenti API'si için örnek DLL (ayrı derlenir)
```

> **Not:** `dev` preset `makine_tests` + `tests/ui/*` + `tests/integration` olmak üzere
> 19 test exe'si üretir. `core` preset yalnızca `makine_tests`'i derler.

---

## Unit Test Örnekleri

### Temel Test

```cpp
#include <gtest/gtest.h>
#include <makine/file_integrity.hpp>

TEST(FileIntegrity, RejectsShortHex) {
    EXPECT_FALSE(makine::integrity::isValidSha256Hex("abc123"));
}
```

### Fixture Kullanımı

```cpp
class FileIntegrityTest : public ::testing::Test {
protected:
    fs::path tempDir_;

    void SetUp() override {
        tempDir_ = fs::temp_directory_path() / "makine_integrity_tests";
        fs::create_directories(tempDir_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(tempDir_, ec);
    }
};

TEST_F(FileIntegrityTest, ComputeFileHash_NonexistentFile) {
    auto result = computeFileHash(tempDir_ / "does_not_exist.bin");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), makine::ErrorCode::FileNotFound);
}
```

### Parameterized Test

```cpp
class TraversalTest : public ::testing::TestWithParam<std::string> {};

TEST_P(TraversalTest, DetectsTraversal) {
    EXPECT_TRUE(makine::path::containsTraversalPattern(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    Patterns,
    TraversalTest,
    ::testing::Values("../etc/passwd", "a/../../b", "%2e%2e/secret")
);
```

---

## Test Çalıştırma

### Tüm Testler

```bash
# just ile (dev build + ctest --preset dev-tests)
just test

# Sadece core (core build + ctest --preset core-tests)
just test-core

# veya CMake ile
cd build/dev
ctest --output-on-failure
```

### Spesifik Test

```bash
# CTest adı (exe) ile
ctest -R test_vdfparser

# makine_tests içinde GTest filtresi ile
./build/dev/makine_tests --gtest_filter='CrashRecovery*'
```

### Test Listesi

```bash
ctest -N  # Sadece listele, calistirma
```

---

## Coverage

Projede tanımlı bir coverage seçeneği veya preset'i yok.

### Coverage Hedefleri

| Modül | Hedef |
|-------|-------|
| Core | 80%+ |
| Services | 70%+ |
| Utils | 90%+ |

---

## CI'da Test

`.github/workflows/ci.yml` her `dev` push'unda ve `dev`'e açılan PR'da `dev` preset'ini
derler ve `ctest --preset dev-tests --output-on-failure` çalıştırır (yerelde `just test` ile aynı).

---

## Test Verileri

Repoda sabit test verisi dizini yok. Testler ihtiyaç duydukları dosyaları
`SetUp()` içinde geçici dizine yazar ve `TearDown()` içinde siler:

```cpp
// Test helper (test_file_integrity.cpp)
fs::path writeFile(const std::string& name, const std::string& content) {
    auto path = tempDir_ / name;
    std::ofstream ofs(path, std::ios::binary);
    ofs << content;
    return path;
}
```

---

## Best Practices

### 1. Bağımsız Testler

```cpp
// Her test kendi setup'ini yapmali
TEST_F(CrashRecoveryTest, DoSomething) {
    makine::recovery::CrashRecoveryJournal journal(testDir_);  // Fresh instance
    // test...
}
```

### 2. Descriptive İsimler

```cpp
// YANLIS
TEST(Test1, Test2) { }

// DOGRU
TEST_F(FileIntegrityTest, ComputeFileHash_NonexistentFile) { }
TEST_F(CrashRecoveryTest, RecoverInstallRestoresOriginalsAndRemovesAddedFiles) { }
```

### 3. Arrange-Act-Assert

```cpp
TEST_F(FileIntegrityTest, ComputeFileHash_KnownContent) {
    // Arrange
    auto path = writeFile("hello.txt", "Hello, World!");

    // Act
    auto result = computeFileHash(path);

    // Assert
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, kHelloWorldHash);
}
```

---

## Sonraki Adımlar

- [Core Kütüphane](core-library.md)
- [Build Sistemi](build-system.md)
