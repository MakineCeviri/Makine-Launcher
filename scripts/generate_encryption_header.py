#!/usr/bin/env python3
"""Generate encryption_key.h from a hex-encoded AES-256 key.

Usage:
    python generate_encryption_header.py <hex_key> [output_path]

    hex_key: 64-character hex string (32 bytes)
    output_path: defaults to qml/src/services/encryption_key.h

The key is XOR-obfuscated in the binary to resist trivial static analysis.
"""

import os
import sys
import secrets


def generate_header(hex_key: str, output_path: str) -> None:
    key_bytes = bytes.fromhex(hex_key.strip())
    if len(key_bytes) != 32:
        print(f"Error: key must be 32 bytes, got {len(key_bytes)}", file=sys.stderr)
        sys.exit(1)

    # Generate random XOR mask per byte
    mask = secrets.token_bytes(32)
    obfuscated = bytes(k ^ m for k, m in zip(key_bytes, mask))

    def to_hex_array(data: bytes) -> str:
        return ", ".join(f"0x{b:02X}" for b in data)

    header = f"""#pragma once
// Auto-generated — do NOT commit this file
#include <array>
#include <cstdint>

namespace crypto {{

constexpr uint8_t MKPK_MAGIC[] = {{'M', 'K', 'P', 'K'}};
constexpr uint8_t MKPK_VERSION = 1;
constexpr size_t  MKPK_NONCE_SIZE  = 12;
constexpr size_t  MKPK_TAG_SIZE    = 16;
constexpr size_t  MKPK_HEADER_SIZE = 4 + 1 + MKPK_NONCE_SIZE; // magic + version + nonce

namespace detail {{
constexpr uint8_t MASK[] = {{
    {to_hex_array(mask)}
}};
constexpr uint8_t OBF_KEY[] = {{
    {to_hex_array(obfuscated)}
}};
}} // namespace detail

inline std::array<uint8_t, 32> decryption_key() {{
    std::array<uint8_t, 32> key;
    for (size_t i = 0; i < 32; ++i)
        key[i] = detail::OBF_KEY[i] ^ detail::MASK[i];
    return key;
}}

}} // namespace crypto
"""

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w", newline="\n") as f:
        f.write(header)
    print(f"Generated {output_path}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: generate_encryption_header.py <hex_key> [output_path]")
        sys.exit(1)

    hex_key = sys.argv[1]
    output_path = sys.argv[2] if len(sys.argv) > 2 else "qml/src/services/encryption_key.h"
    generate_header(hex_key, output_path)
