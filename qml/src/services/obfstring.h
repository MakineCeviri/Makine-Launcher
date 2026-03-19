/**
 * @file obfstring.h
 * @brief Compile-time XOR string encryption
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Every OBF("text") call produces a unique XOR-encrypted byte sequence
 * at compile time. The plaintext never appears in the binary's .rodata.
 */

#pragma once

#include <string>

namespace obf::detail {

constexpr unsigned char derive_key(unsigned seed, unsigned idx)
{
    // Simple PRNG — different key byte per character position
    unsigned h = seed ^ (idx * 2654435761u);
    h ^= h >> 16;
    h *= 0x45d9f3bu;
    h ^= h >> 16;
    return static_cast<unsigned char>(h | 1u); // never zero (avoids null terminator leak)
}

template <unsigned N, unsigned Seed>
class enc_str {
    unsigned char m_enc[N]{};

public:
    constexpr enc_str(const char (&s)[N])
    {
        for (unsigned i = 0; i < N; ++i)
            m_enc[i] = static_cast<unsigned char>(s[i]) ^ derive_key(Seed, i);
    }

    // Runtime decrypt — noinline prevents the compiler from constant-folding
    // the XOR away; volatile prevents dead-store elimination.
    __attribute__((noinline)) std::string get() const
    {
        char buf[N];
        for (unsigned i = 0; i < N; ++i) {
            volatile unsigned char c = m_enc[i] ^ derive_key(Seed, i);
            buf[i] = static_cast<char>(c);
        }
        return std::string(buf, N - 1); // exclude null terminator
    }
};

} // namespace obf::detail

// Produce an encrypted std::string — each call site gets a unique key via __LINE__
#define OBF(s) \
    (::obf::detail::enc_str<sizeof(s), __LINE__ * 7u + 0xA5E1u>(s).get())

// QString convenience wrapper
#define OBFQ(s) (QString::fromUtf8(OBF(s)))
