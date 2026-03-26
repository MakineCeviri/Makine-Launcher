/**
 * @file test_integration_main.cpp
 * @brief Main entry point for Makine integration tests
 *
 * Integration tests verify complete workflows across multiple modules.
 * They test real-world scenarios like:
 * - Full game scanning → translation extraction → patching → verification
 * - Handler workflows for each supported engine
 * - Package installation and removal
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {
    /**
     * @brief Get the fixtures directory path
     */
    fs::path getFixturesDir() {
        #ifdef MAKINE_TEST_FIXTURES_DIR
            return MAKINE_TEST_FIXTURES_DIR;
        #else
            return fs::current_path() / "fixtures";
        #endif
    }

    /**
     * @brief Get the temp directory path for test outputs
     */
    fs::path getTempDir() {
        #ifdef MAKINE_TEST_TEMP_DIR
            return MAKINE_TEST_TEMP_DIR;
        #else
            return fs::temp_directory_path() / "makine_tests";
        #endif
    }

    /**
     * @brief Check if a fixture exists
     */
    bool hasFixture(const std::string& name) {
        return fs::exists(getFixturesDir() / name);
    }

    /**
     * @brief Skip test if fixture is missing
     */
    void skipIfNoFixture(const std::string& name) {
        if (!hasFixture(name)) {
            GTEST_SKIP() << "Fixture not available: " << name;
        }
    }
}

/**
 * @brief Global test environment for integration tests
 *
 * Sets up and tears down shared resources for all integration tests.
 */
class IntegrationTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        // Create temp directory
        auto tempDir = getTempDir();
        if (!fs::exists(tempDir)) {
            fs::create_directories(tempDir);
        }

        // Log fixtures status
        std::cout << "\n=== Makine Integration Tests ===" << std::endl;
        std::cout << "Fixtures dir: " << getFixturesDir() << std::endl;
        std::cout << "Temp dir: " << tempDir << std::endl;

        // Check available fixtures
        if (fs::exists(getFixturesDir())) {
            std::cout << "\nAvailable fixtures:" << std::endl;
            for (const auto& entry : fs::directory_iterator(getFixturesDir())) {
                if (entry.is_directory()) {
                    std::cout << "  - " << entry.path().filename().string() << std::endl;
                }
            }
        } else {
            std::cout << "\nWARNING: Fixtures directory not found!" << std::endl;
            std::cout << "Some integration tests will be skipped." << std::endl;
        }
        std::cout << std::endl;
    }

    void TearDown() override {
        // Clean up temp directory
        auto tempDir = getTempDir();
        if (fs::exists(tempDir)) {
            std::error_code ec;
            fs::remove_all(tempDir, ec);
            // Ignore errors - temp cleanup is best-effort
        }
    }
};

// Register global environment
testing::Environment* const integrationEnv =
    testing::AddGlobalTestEnvironment(new IntegrationTestEnvironment);

/**
 * @brief Base class for integration test fixtures
 *
 * Provides common utilities for all integration tests.
 */
class IntegrationTestBase : public ::testing::Test {
protected:
    void SetUp() override {
        // Each test gets its own temp subdirectory
        testTempDir_ = getTempDir() / ::testing::UnitTest::GetInstance()
            ->current_test_info()->name();
        fs::create_directories(testTempDir_);
    }

    void TearDown() override {
        // Clean up test temp directory
        if (fs::exists(testTempDir_)) {
            std::error_code ec;
            fs::remove_all(testTempDir_, ec);
        }
    }

    /**
     * @brief Get the fixtures directory
     */
    fs::path fixturesDir() const {
        return getFixturesDir();
    }

    /**
     * @brief Get this test's temp directory
     */
    fs::path tempDir() const {
        return testTempDir_;
    }

    /**
     * @brief Copy fixture to temp directory
     */
    fs::path copyFixture(const std::string& name) {
        auto src = fixturesDir() / name;
        auto dst = tempDir() / name;

        if (fs::is_directory(src)) {
            fs::copy(src, dst, fs::copy_options::recursive);
        } else {
            fs::copy_file(src, dst);
        }

        return dst;
    }

    /**
     * @brief Check if fixture exists
     */
    bool hasFixture(const std::string& name) const {
        return fs::exists(fixturesDir() / name);
    }

    /**
     * @brief Skip test if fixture is missing
     */
    void requireFixture(const std::string& name) {
        if (!hasFixture(name)) {
            GTEST_SKIP() << "Fixture not available: " << name;
        }
    }

    /**
     * @brief Create a test file with content
     */
    fs::path createTestFile(const std::string& name, const std::string& content) {
        auto path = tempDir() / name;
        fs::create_directories(path.parent_path());
        std::ofstream ofs(path);
        ofs << content;
        return path;
    }

    /**
     * @brief Read file content
     */
    std::string readFile(const fs::path& path) {
        std::ifstream ifs(path);
        return std::string(std::istreambuf_iterator<char>(ifs),
                          std::istreambuf_iterator<char>());
    }

private:
    fs::path testTempDir_;
};

// Main is provided by gtest_main, but we can customize if needed
// int main(int argc, char **argv) {
//     testing::InitGoogleTest(&argc, argv);
//     return RUN_ALL_TESTS();
// }
