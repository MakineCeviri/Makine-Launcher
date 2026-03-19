/**
 * @file crypto_utils.hpp
 * @brief Cryptographic utilities with optional libsodium backend
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Provides modern cryptographic operations.
 * Uses libsodium when available for constant-time operations.
 *
 * Compile-time detection:
 * - MAKINE_HAS_LIBSODIUM - libsodium library available
 *
 * Fallback: OpenSSL-based implementation.
 */

#pragma once

#include "features.hpp"
#include "constants.hpp"
#include "error.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifdef MAKINE_HAS_LIBSODIUM
#include <sodium.h>
#else
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#endif

namespace makine::crypto {

// =============================================================================
// Constants
// =============================================================================

constexpr size_t HASH_SIZE = 32;      // SHA-256 / BLAKE2b-256
constexpr size_t NONCE_SIZE = 24;     // XChaCha20 nonce
constexpr size_t KEY_SIZE = 32;       // Symmetric key
constexpr size_t SIGNATURE_SIZE = 64; // Ed25519 signature
constexpr size_t PUBLIC_KEY_SIZE = 32;
constexpr size_t SECRET_KEY_SIZE = 64;

// =============================================================================
// Types
// =============================================================================

using Hash = std::array<uint8_t, HASH_SIZE>;
using Nonce = std::array<uint8_t, NONCE_SIZE>;
using Key = std::array<uint8_t, KEY_SIZE>;
using Signature = std::array<uint8_t, SIGNATURE_SIZE>;
using PublicKey = std::array<uint8_t, PUBLIC_KEY_SIZE>;
using SecretKey = std::array<uint8_t, SECRET_KEY_SIZE>;

// =============================================================================
// Initialization
// =============================================================================

/**
 * @brief Initialize crypto library (call once at startup)
 * @return true if initialized successfully
 */
inline bool initialize() noexcept {
#ifdef MAKINE_HAS_LIBSODIUM
    return sodium_init() >= 0;
#else
    // OpenSSL auto-initializes
    return true;
#endif
}

// =============================================================================
// Secure Memory
// =============================================================================

/**
 * @brief Zero memory securely (not optimized away)
 */
inline void secureZero(void* ptr, size_t size) noexcept {
#ifdef MAKINE_HAS_LIBSODIUM
    sodium_memzero(ptr, size);
#else
    volatile uint8_t* p = static_cast<volatile uint8_t*>(ptr);
    while (size--) {
        *p++ = 0;
    }
#endif
}

/**
 * @brief Constant-time memory comparison
 */
inline bool secureCompare(const void* a, const void* b, size_t size) noexcept {
#ifdef MAKINE_HAS_LIBSODIUM
    return sodium_memcmp(a, b, size) == 0;
#else
    volatile const uint8_t* pa = static_cast<volatile const uint8_t*>(a);
    volatile const uint8_t* pb = static_cast<volatile const uint8_t*>(b);
    uint8_t result = 0;
    while (size--) {
        result |= *pa++ ^ *pb++;
    }
    return result == 0;
#endif
}

// =============================================================================
// Random Generation
// =============================================================================

/**
 * @brief Generate cryptographically secure random bytes
 */
inline void randomBytes(std::span<uint8_t> buffer) noexcept {
#ifdef MAKINE_HAS_LIBSODIUM
    randombytes_buf(buffer.data(), buffer.size());
#else
    RAND_bytes(buffer.data(), static_cast<int>(buffer.size()));
#endif
}

/**
 * @brief Generate random bytes and return as vector
 */
inline std::vector<uint8_t> randomBytes(size_t size) {
    std::vector<uint8_t> result(size);
    randomBytes(result);
    return result;
}

/**
 * @brief Generate random nonce
 */
inline Nonce randomNonce() {
    Nonce nonce;
    randomBytes(nonce);
    return nonce;
}

/**
 * @brief Generate random key
 */
inline Key randomKey() {
    Key key;
    randomBytes(key);
    return key;
}

// =============================================================================
// Hashing
// =============================================================================

/**
 * @brief Compute hash of data (BLAKE2b with libsodium, SHA-256 with OpenSSL)
 */
inline Hash hash(std::span<const uint8_t> data) {
    Hash result;
#ifdef MAKINE_HAS_LIBSODIUM
    crypto_generichash(
        result.data(), result.size(),
        data.data(), data.size(),
        nullptr, 0
    );
#else
    SHA256(data.data(), data.size(), result.data());
#endif
    return result;
}

/**
 * @brief Compute hash of string
 */
inline Hash hash(std::string_view str) {
    return hash(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(str.data()),
        str.size()
    ));
}

/**
 * @brief Convert hash to hex string
 */
inline std::string hashToHex(const Hash& h) {
    static const char hexChars[] = "0123456789abcdef";
    std::string result;
    result.reserve(h.size() * 2);
    for (uint8_t b : h) {
        result += hexChars[b >> 4];
        result += hexChars[b & 0x0F];
    }
    return result;
}

/**
 * @brief Parse hash from hex string
 */
inline std::optional<Hash> hashFromHex(std::string_view hex) {
    if (hex.size() != HASH_SIZE * 2) {
        return std::nullopt;
    }

    Hash result;
    for (size_t i = 0; i < HASH_SIZE; ++i) {
        char hi = hex[i * 2];
        char lo = hex[i * 2 + 1];

        auto hexVal = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };

        int hiVal = hexVal(hi);
        int loVal = hexVal(lo);
        if (hiVal < 0 || loVal < 0) {
            return std::nullopt;
        }

        result[i] = static_cast<uint8_t>((hiVal << 4) | loVal);
    }

    return result;
}

// =============================================================================
// Symmetric Encryption (Authenticated)
// =============================================================================

/**
 * @brief Encrypt data with authenticated encryption
 *
 * Uses XChaCha20-Poly1305 with libsodium, AES-256-GCM with OpenSSL.
 *
 * @param plaintext Data to encrypt
 * @param key 32-byte key
 * @param nonce 24-byte nonce (must be unique per key)
 * @return Ciphertext with authentication tag, or empty on error
 */
inline std::vector<uint8_t> encrypt(
    std::span<const uint8_t> plaintext,
    const Key& key,
    const Nonce& nonce
) {
#ifdef MAKINE_HAS_LIBSODIUM
    std::vector<uint8_t> ciphertext(plaintext.size() + crypto_secretbox_MACBYTES);

    if (crypto_secretbox_easy(
            ciphertext.data(),
            plaintext.data(), plaintext.size(),
            nonce.data(),
            key.data()
        ) != 0) {
        return {};
    }

    return ciphertext;
#else
    // OpenSSL AES-256-GCM fallback
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    std::vector<uint8_t> ciphertext(plaintext.size() + 16);  // + GCM tag
    int len = 0;
    int ciphertextLen = 0;

    // Use first 12 bytes of nonce for GCM IV
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, key.data(), nonce.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, plaintext.data(), static_cast<int>(plaintext.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    ciphertextLen = len;

    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    ciphertextLen += len;

    // Get tag
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, ciphertext.data() + ciphertextLen) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    ciphertextLen += 16;

    EVP_CIPHER_CTX_free(ctx);
    ciphertext.resize(ciphertextLen);
    return ciphertext;
#endif
}

/**
 * @brief Decrypt authenticated ciphertext
 *
 * @param ciphertext Encrypted data with auth tag
 * @param key 32-byte key
 * @param nonce 24-byte nonce used during encryption
 * @return Plaintext, or empty on error/authentication failure
 */
inline std::vector<uint8_t> decrypt(
    std::span<const uint8_t> ciphertext,
    const Key& key,
    const Nonce& nonce
) {
#ifdef MAKINE_HAS_LIBSODIUM
    if (ciphertext.size() < crypto_secretbox_MACBYTES) {
        return {};
    }

    std::vector<uint8_t> plaintext(ciphertext.size() - crypto_secretbox_MACBYTES);

    if (crypto_secretbox_open_easy(
            plaintext.data(),
            ciphertext.data(), ciphertext.size(),
            nonce.data(),
            key.data()
        ) != 0) {
        return {};  // Authentication failed
    }

    return plaintext;
#else
    if (ciphertext.size() < 16) {
        return {};
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    std::vector<uint8_t> plaintext(ciphertext.size() - 16);
    int len = 0;
    int plaintextLen = 0;

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, key.data(), nonce.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(), static_cast<int>(ciphertext.size() - 16)) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    plaintextLen = len;

    // Set expected tag
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, const_cast<uint8_t*>(ciphertext.data() + ciphertext.size() - 16)) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};  // Authentication failed
    }
    plaintextLen += len;

    EVP_CIPHER_CTX_free(ctx);
    plaintext.resize(plaintextLen);
    return plaintext;
#endif
}

// =============================================================================
// Key Derivation
// =============================================================================

/**
 * @brief Derive key from password using Argon2id (libsodium) or PBKDF2 (OpenSSL)
 *
 * @param password User password
 * @param salt Salt (should be random, at least 16 bytes)
 * @return Derived key, or empty on error
 */
inline std::optional<Key> deriveKey(
    std::string_view password,
    std::span<const uint8_t> salt
) {
#ifdef MAKINE_HAS_LIBSODIUM
    if (salt.size() < crypto_pwhash_SALTBYTES) {
        return std::nullopt;
    }

    Key key;
    if (crypto_pwhash(
            key.data(), key.size(),
            password.data(), password.size(),
            salt.data(),
            crypto_pwhash_OPSLIMIT_MODERATE,
            crypto_pwhash_MEMLIMIT_MODERATE,
            crypto_pwhash_ALG_ARGON2ID13
        ) != 0) {
        return std::nullopt;
    }

    return key;
#else
    // OpenSSL PBKDF2 fallback
    Key key;
    if (PKCS5_PBKDF2_HMAC(
            password.data(), static_cast<int>(password.size()),
            salt.data(), static_cast<int>(salt.size()),
            kPBKDF2Iterations,
            EVP_sha256(),
            static_cast<int>(key.size()),
            key.data()
        ) != 1) {
        return std::nullopt;
    }

    return key;
#endif
}

// =============================================================================
// Digital Signatures
// =============================================================================

#ifdef MAKINE_HAS_LIBSODIUM

/**
 * @brief Generate Ed25519 key pair
 */
inline std::pair<PublicKey, SecretKey> generateKeyPair() {
    PublicKey pk;
    SecretKey sk;
    crypto_sign_keypair(pk.data(), sk.data());
    return {pk, sk};
}

/**
 * @brief Sign message with Ed25519
 */
inline Signature sign(std::span<const uint8_t> message, const SecretKey& sk) {
    Signature sig;
    crypto_sign_detached(sig.data(), nullptr, message.data(), message.size(), sk.data());
    return sig;
}

/**
 * @brief Verify Ed25519 signature
 */
inline bool verify(
    std::span<const uint8_t> message,
    const Signature& sig,
    const PublicKey& pk
) {
    return crypto_sign_verify_detached(sig.data(), message.data(), message.size(), pk.data()) == 0;
}

#endif // MAKINE_HAS_LIBSODIUM

// =============================================================================
// Statistics
// =============================================================================

/**
 * @brief Check if libsodium is available
 */
inline bool hasLibsodium() noexcept {
#ifdef MAKINE_HAS_LIBSODIUM
    return true;
#else
    return false;
#endif
}

/**
 * @brief Get crypto backend name
 */
inline const char* backendName() noexcept {
#ifdef MAKINE_HAS_LIBSODIUM
    return "libsodium";
#else
    return "OpenSSL";
#endif
}

} // namespace makine::crypto
