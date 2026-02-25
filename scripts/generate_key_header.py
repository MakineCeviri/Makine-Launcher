#!/usr/bin/env python3
"""
Generate encryption_key.h from .encryption_key file.

Reads the hex-encoded AES-256 key and produces a C++ header with
compile-time XOR-obfuscated bytes matching the PRNG in encryption_key.h.

Usage:
    python scripts/generate_key_header.py
"""

from pathlib import Path

SCRIPT_DIR = Path(__file__).parent
KEY_FILE = SCRIPT_DIR / ".encryption_key"
OUTPUT = SCRIPT_DIR.parent / "qml" / "src" / "services" / "encryption_key.h"


def key_mask(idx: int) -> int:
    """Must match detail::key_mask() in encryption_key.h exactly."""
    h = 0x7A3B9E1D ^ (idx * 0x9E3779B9 & 0xFFFFFFFF)
    h &= 0xFFFFFFFF
    h ^= h >> 13
    h &= 0xFFFFFFFF
    h *= 0x5BD1E995
    h &= 0xFFFFFFFF
    h ^= h >> 15
    h &= 0xFFFFFFFF
    return h & 0xFF


def main():
    key_hex = KEY_FILE.read_text().strip()
    if len(key_hex) != 64:
        raise ValueError(f"Key must be 64 hex chars, got {len(key_hex)}")

    key_bytes = bytes.fromhex(key_hex)
    encrypted = [key_bytes[i] ^ key_mask(i) for i in range(32)]

    # Format as C array lines (8 bytes per line)
    lines = []
    for i in range(0, 32, 8):
        chunk = encrypted[i:i + 8]
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in chunk))
    array_body = ",\n".join(lines)

    header = f'''/**
 * @file encryption_key.h
 * @brief Compile-time obfuscated AES-256 decryption key for MKPK packages
 *
 * The 32-byte key is XOR-encrypted at compile time so it never appears
 * as a contiguous plaintext blob in the binary's .rodata / .rdata section.
 *
 * Regenerate with: python scripts/generate_key_header.py
 */

#pragma once

#include <array>
#include <cstdint>

namespace makineai::crypto {{

namespace detail {{

// PRNG mask — intentionally different constants than obfstring.h
// to prevent cross-correlation analysis
constexpr uint8_t key_mask(unsigned idx)
{{
    unsigned h = 0x7A3B9E1Du ^ (idx * 0x9E3779B9u);
    h ^= h >> 13;
    h *= 0x5BD1E995u;
    h ^= h >> 15;
    return static_cast<uint8_t>(h);
}}

// XOR-encrypted key bytes (compile-time constant)
// The plaintext key is NEVER present in the binary
constexpr uint8_t ENC_KEY[32] = {{
{array_body}
}};

}} // namespace detail

// Runtime-only decryption.
// noinline + volatile prevent the compiler from constant-folding the XOR
// and placing the plaintext key back into the binary.
#ifdef _MSC_VER
__declspec(noinline)
#else
__attribute__((noinline))
#endif
inline std::array<uint8_t, 32> decryption_key()
{{
    std::array<uint8_t, 32> key{{}};
    for (unsigned i = 0; i < 32; ++i) {{
        volatile uint8_t b = detail::ENC_KEY[i] ^ detail::key_mask(i);
        key[i] = b;
    }}
    return key;
}}

// MKPK format constants — shared between C++ and Python pipeline
constexpr uint8_t  MKPK_MAGIC[4]   = {{'M', 'K', 'P', 'K'}};
constexpr uint8_t  MKPK_VERSION     = 0x01;
constexpr unsigned MKPK_NONCE_SIZE  = 12;  // AES-GCM standard
constexpr unsigned MKPK_TAG_SIZE    = 16;  // GCM auth tag
constexpr unsigned MKPK_HEADER_SIZE = 4 + 1 + MKPK_NONCE_SIZE; // 17 bytes

}} // namespace makineai::crypto
'''

    OUTPUT.write_text(header)
    print(f"Generated: {OUTPUT}")
    print(f"Key SHA-256 prefix: {key_hex[:16]}...")


if __name__ == "__main__":
    main()
