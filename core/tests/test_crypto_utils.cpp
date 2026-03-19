/**
 * @file test_crypto_utils.cpp
 * @brief Unit tests for crypto_utils.hpp
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <makine/crypto_utils.hpp>

#include <algorithm>
#include <cstring>
#include <numeric>
#include <set>
#include <string>
#include <vector>

namespace makine {
namespace testing {

class CryptoUtilsTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_TRUE(crypto::initialize()) << "Crypto backend initialization failed";
    }
};

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

TEST_F(CryptoUtilsTest, InitializeSucceeds) {
    // Repeated init should also succeed
    EXPECT_TRUE(crypto::initialize());
}

// ---------------------------------------------------------------------------
// Hashing
// ---------------------------------------------------------------------------

TEST_F(CryptoUtilsTest, HashDeterministic) {
    auto h1 = crypto::hash("hello world");
    auto h2 = crypto::hash("hello world");
    EXPECT_EQ(h1, h2);
}

TEST_F(CryptoUtilsTest, HashUniqueness) {
    auto h1 = crypto::hash("alpha");
    auto h2 = crypto::hash("bravo");
    EXPECT_NE(h1, h2);
}

TEST_F(CryptoUtilsTest, HashEmptyString) {
    auto h = crypto::hash("");
    // Should produce a valid hash (not all zeros)
    bool allZero = std::all_of(h.begin(), h.end(), [](uint8_t b) { return b == 0; });
    EXPECT_FALSE(allZero);
    EXPECT_EQ(h.size(), crypto::HASH_SIZE);
}

TEST_F(CryptoUtilsTest, HashSpanMatchesStringView) {
    std::string input = "test data";
    auto hStr = crypto::hash(std::string_view{input});

    std::vector<uint8_t> bytes(input.begin(), input.end());
    auto hSpan = crypto::hash(std::span<const uint8_t>{bytes});

    EXPECT_EQ(hStr, hSpan);
}

// ---------------------------------------------------------------------------
// Hex encoding / decoding
// ---------------------------------------------------------------------------

TEST_F(CryptoUtilsTest, HashToHexProduces64LowercaseHexChars) {
    auto h = crypto::hash("test");
    auto hex = crypto::hashToHex(h);

    EXPECT_EQ(hex.size(), 64u);
    for (char c : hex) {
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))
            << "Non-lowercase-hex character: " << c;
    }
}

TEST_F(CryptoUtilsTest, HashFromHexRoundTrip) {
    auto original = crypto::hash("round trip");
    auto hex = crypto::hashToHex(original);
    auto decoded = crypto::hashFromHex(hex);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, original);
}

TEST_F(CryptoUtilsTest, HashFromHexRejectsWrongLength) {
    // Too short
    EXPECT_FALSE(crypto::hashFromHex("abcdef").has_value());
    // Too long (66 chars)
    std::string tooLong(66, 'a');
    EXPECT_FALSE(crypto::hashFromHex(tooLong).has_value());
    // Empty
    EXPECT_FALSE(crypto::hashFromHex("").has_value());
}

TEST_F(CryptoUtilsTest, HashFromHexRejectsInvalidChars) {
    // 64 chars with invalid hex characters
    std::string bad(64, 'g');
    EXPECT_FALSE(crypto::hashFromHex(bad).has_value());

    // Valid length but one bad char
    std::string almostGood(64, 'a');
    almostGood[32] = 'z';
    EXPECT_FALSE(crypto::hashFromHex(almostGood).has_value());
}

TEST_F(CryptoUtilsTest, HashFromHexAcceptsUppercase) {
    auto h = crypto::hash("uppercase test");
    auto hex = crypto::hashToHex(h);

    // Convert to uppercase
    std::string upper;
    upper.reserve(hex.size());
    for (char c : hex) {
        upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    auto decoded = crypto::hashFromHex(upper);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, h);
}

// ---------------------------------------------------------------------------
// Secure memory
// ---------------------------------------------------------------------------

TEST_F(CryptoUtilsTest, SecureCompareEqualBuffers) {
    uint8_t a[16], b[16];
    std::memset(a, 0xAB, sizeof(a));
    std::memset(b, 0xAB, sizeof(b));

    EXPECT_TRUE(crypto::secureCompare(a, b, sizeof(a)));
}

TEST_F(CryptoUtilsTest, SecureCompareDifferentBuffers) {
    uint8_t a[16], b[16];
    std::memset(a, 0xAB, sizeof(a));
    std::memset(b, 0xCD, sizeof(b));

    EXPECT_FALSE(crypto::secureCompare(a, b, sizeof(a)));
}

TEST_F(CryptoUtilsTest, SecureCompareSingleByteDifference) {
    uint8_t a[32], b[32];
    std::memset(a, 0x00, sizeof(a));
    std::memcpy(b, a, sizeof(a));
    b[31] = 0x01; // differ only in last byte

    EXPECT_FALSE(crypto::secureCompare(a, b, sizeof(a)));
}

TEST_F(CryptoUtilsTest, SecureZeroMemory) {
    uint8_t buf[64];
    std::memset(buf, 0xFF, sizeof(buf));

    crypto::secureZero(buf, sizeof(buf));

    for (size_t i = 0; i < sizeof(buf); ++i) {
        EXPECT_EQ(buf[i], 0) << "Byte at index " << i << " not zeroed";
    }
}

// ---------------------------------------------------------------------------
// Random generation
// ---------------------------------------------------------------------------

TEST_F(CryptoUtilsTest, RandomBytesSpanProducesNonZeroData) {
    // 32 bytes of all zeros from a CSPRNG is astronomically unlikely
    std::array<uint8_t, 32> buf{};
    crypto::randomBytes(buf);

    bool hasNonZero = std::any_of(buf.begin(), buf.end(), [](uint8_t b) { return b != 0; });
    EXPECT_TRUE(hasNonZero) << "32 random bytes were all zero — extremely unlikely";
}

TEST_F(CryptoUtilsTest, RandomBytesVectorReturnsCorrectSize) {
    auto bytes = crypto::randomBytes(64);
    EXPECT_EQ(bytes.size(), 64u);
}

TEST_F(CryptoUtilsTest, RandomNonceTwoCallsDiffer) {
    auto n1 = crypto::randomNonce();
    auto n2 = crypto::randomNonce();
    EXPECT_NE(n1, n2) << "Two random nonces should differ (collision probability ~2^-96)";
}

TEST_F(CryptoUtilsTest, RandomKeyTwoCallsDiffer) {
    auto k1 = crypto::randomKey();
    auto k2 = crypto::randomKey();
    EXPECT_NE(k1, k2) << "Two random keys should differ (collision probability ~2^-128)";
}

TEST_F(CryptoUtilsTest, RandomNonceCorrectSize) {
    auto n = crypto::randomNonce();
    EXPECT_EQ(n.size(), crypto::NONCE_SIZE);
}

TEST_F(CryptoUtilsTest, RandomKeyCorrectSize) {
    auto k = crypto::randomKey();
    EXPECT_EQ(k.size(), crypto::KEY_SIZE);
}

// ---------------------------------------------------------------------------
// Encrypt / Decrypt
// ---------------------------------------------------------------------------

TEST_F(CryptoUtilsTest, EncryptDecryptRoundTrip) {
    auto key = crypto::randomKey();
    auto nonce = crypto::randomNonce();

    std::vector<uint8_t> plaintext = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"

    auto ciphertext = crypto::encrypt(plaintext, key, nonce);
    ASSERT_FALSE(ciphertext.empty()) << "Encryption should not return empty";

    auto decrypted = crypto::decrypt(ciphertext, key, nonce);
    EXPECT_EQ(decrypted, plaintext);
}

TEST_F(CryptoUtilsTest, EncryptDifferentKeysProduceDifferentCiphertext) {
    auto key1 = crypto::randomKey();
    auto key2 = crypto::randomKey();
    auto nonce = crypto::randomNonce();

    std::vector<uint8_t> plaintext = {1, 2, 3, 4, 5, 6, 7, 8};

    auto ct1 = crypto::encrypt(plaintext, key1, nonce);
    auto ct2 = crypto::encrypt(plaintext, key2, nonce);

    ASSERT_FALSE(ct1.empty());
    ASSERT_FALSE(ct2.empty());
    EXPECT_NE(ct1, ct2);
}

TEST_F(CryptoUtilsTest, DecryptWithWrongKeyReturnsEmpty) {
    auto key = crypto::randomKey();
    auto wrongKey = crypto::randomKey();
    auto nonce = crypto::randomNonce();

    std::vector<uint8_t> plaintext = {10, 20, 30};

    auto ciphertext = crypto::encrypt(plaintext, key, nonce);
    ASSERT_FALSE(ciphertext.empty());

    auto result = crypto::decrypt(ciphertext, wrongKey, nonce);
    EXPECT_TRUE(result.empty()) << "Decryption with wrong key should fail";
}

TEST_F(CryptoUtilsTest, DecryptWithWrongNonceReturnsEmpty) {
    auto key = crypto::randomKey();
    auto nonce = crypto::randomNonce();
    auto wrongNonce = crypto::randomNonce();

    std::vector<uint8_t> plaintext = {10, 20, 30};

    auto ciphertext = crypto::encrypt(plaintext, key, nonce);
    ASSERT_FALSE(ciphertext.empty());

    auto result = crypto::decrypt(ciphertext, key, wrongNonce);
    EXPECT_TRUE(result.empty()) << "Decryption with wrong nonce should fail";
}

TEST_F(CryptoUtilsTest, EncryptDecryptEmptyPlaintext) {
    auto key = crypto::randomKey();
    auto nonce = crypto::randomNonce();

    std::vector<uint8_t> plaintext; // empty

    auto ciphertext = crypto::encrypt(plaintext, key, nonce);
    // Ciphertext should contain at least the auth tag
    ASSERT_FALSE(ciphertext.empty());

    auto decrypted = crypto::decrypt(ciphertext, key, nonce);
    EXPECT_TRUE(decrypted.empty()) << "Decrypted empty plaintext should be empty";
}

TEST_F(CryptoUtilsTest, CiphertextLargerThanPlaintext) {
    auto key = crypto::randomKey();
    auto nonce = crypto::randomNonce();

    std::vector<uint8_t> plaintext(100, 0x42);

    auto ciphertext = crypto::encrypt(plaintext, key, nonce);
    // Ciphertext must be larger due to authentication tag
    EXPECT_GT(ciphertext.size(), plaintext.size());
}

// ---------------------------------------------------------------------------
// Backend info
// ---------------------------------------------------------------------------

TEST_F(CryptoUtilsTest, BackendNameNonEmpty) {
    const char* name = crypto::backendName();
    ASSERT_NE(name, nullptr);
    EXPECT_GT(std::strlen(name), 0u);
}

TEST_F(CryptoUtilsTest, HasLibsodiumConsistentWithBackendName) {
    if (crypto::hasLibsodium()) {
        EXPECT_STREQ(crypto::backendName(), "libsodium");
    } else {
        EXPECT_STREQ(crypto::backendName(), "OpenSSL");
    }
}

// ---------------------------------------------------------------------------
// Constants / type sizes
// ---------------------------------------------------------------------------

TEST_F(CryptoUtilsTest, ConstantSizes) {
    EXPECT_EQ(crypto::HASH_SIZE, 32u);
    EXPECT_EQ(crypto::NONCE_SIZE, 24u);
    EXPECT_EQ(crypto::KEY_SIZE, 32u);
}

TEST_F(CryptoUtilsTest, TypeAliasesSizes) {
    EXPECT_EQ(sizeof(crypto::Hash), crypto::HASH_SIZE);
    EXPECT_EQ(sizeof(crypto::Nonce), crypto::NONCE_SIZE);
    EXPECT_EQ(sizeof(crypto::Key), crypto::KEY_SIZE);
}

// ---------------------------------------------------------------------------
// ADDITIONAL EDGE CASES
// ---------------------------------------------------------------------------

TEST_F(CryptoUtilsTest, HashFromHexRejectsWhitespace) {
    // 64 chars with spaces
    std::string withSpace(64, 'a');
    withSpace[32] = ' ';
    EXPECT_FALSE(crypto::hashFromHex(withSpace).has_value());
}

TEST_F(CryptoUtilsTest, CryptoInitializeIdempotent) {
    // Multiple initializations should all succeed
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(crypto::initialize());
    }
}

TEST_F(CryptoUtilsTest, EncryptWithSameNonceDifferentKeys) {
    auto key1 = crypto::randomKey();
    auto key2 = crypto::randomKey();
    auto nonce = crypto::randomNonce();

    std::vector<uint8_t> plaintext = {1, 2, 3, 4, 5};

    auto ct1 = crypto::encrypt(plaintext, key1, nonce);
    auto ct2 = crypto::encrypt(plaintext, key2, nonce);

    ASSERT_FALSE(ct1.empty());
    ASSERT_FALSE(ct2.empty());
    EXPECT_NE(ct1, ct2);
}

TEST_F(CryptoUtilsTest, RandomBytesDistribution) {
    // Generate a larger buffer and check it has reasonable byte variety
    auto bytes = crypto::randomBytes(256);
    EXPECT_EQ(bytes.size(), 256u);

    // Count distinct byte values — extremely unlikely to have < 100 distinct
    // values in 256 random bytes (birthday paradox)
    std::set<uint8_t> distinctValues(bytes.begin(), bytes.end());
    EXPECT_GT(distinctValues.size(), 50u)
        << "256 random bytes should have more than 50 distinct values";
}

} // namespace testing
} // namespace makine
