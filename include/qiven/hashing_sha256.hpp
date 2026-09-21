#pragma once

// ============================================================================
// hashing_sha256.hpp — integrity-grade SHA-256 digests (FIPS 180-4)
//
// SHA-256 is the content-integrity primitive for decision binding and
// artifact identity (cpp-architecture §3.6). Non-cryptographic identity
// hashing of runtime strings stays on fnv1a64 in hashing.hpp; the two are
// different contracts and must not be substituted for each other.
//
// The core is constexpr, so golden vectors pin the implementation at compile
// time. The string_view overloads reinterpret bytes and are runtime-only.
// No allocation, no synchronization, no exceptions; cost is per-byte
// arithmetic over the input.
// ============================================================================

#include <qiven/contracts.hpp>
#include <qiven/types.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace qiven
{
using SHA256Digest = std::array<std::byte, 32>;

namespace detail
{
inline constexpr u32 sha256_initial_state[8] = { 0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                                 0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u };

inline constexpr u32 sha256_round_constants[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

[[nodiscard]] constexpr u32 sha256_load_be_u32(const std::byte* p) noexcept
{
    return (static_cast<u32>(p[0]) << 24) | (static_cast<u32>(p[1]) << 16) | (static_cast<u32>(p[2]) << 8) |
           static_cast<u32>(p[3]);
}

constexpr void sha256_store_be_u32(std::byte* p, u32 v) noexcept
{
    p[0] = static_cast<std::byte>(v >> 24);
    p[1] = static_cast<std::byte>(v >> 16);
    p[2] = static_cast<std::byte>(v >> 8);
    p[3] = static_cast<std::byte>(v);
}

constexpr void sha256_store_be_u64(std::byte* p, u64 v) noexcept
{
    for (usize i = 0; i < 8; ++i)
    {
        p[i] = static_cast<std::byte>(v >> (56 - 8 * i));
    }
}

constexpr void sha256_compress(u32 state[8], const std::byte* block) noexcept
{
    u32 w[64];
    for (usize i = 0; i < 16; ++i)
    {
        w[i] = sha256_load_be_u32(block + 4 * i);
    }
    for (usize i = 16; i < 64; ++i)
    {
        const u32 s0 = std::rotr(w[i - 15], 7) ^ std::rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const u32 s1 = std::rotr(w[i - 2], 17) ^ std::rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i]         = w[i - 16] + s0 + w[i - 7] + s1;
    }

    u32 a = state[0];
    u32 b = state[1];
    u32 c = state[2];
    u32 d = state[3];
    u32 e = state[4];
    u32 f = state[5];
    u32 g = state[6];
    u32 h = state[7];

    for (usize i = 0; i < 64; ++i)
    {
        const u32 s1  = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
        const u32 ch  = (e & f) ^ (~e & g);
        const u32 t1  = h + s1 + ch + sha256_round_constants[i] + w[i];
        const u32 s0  = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
        const u32 maj = (a & b) ^ (a & c) ^ (b & c);
        const u32 t2  = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

struct SHA256Core
{
    u32 state[8]         = { sha256_initial_state[0], sha256_initial_state[1], sha256_initial_state[2],
                             sha256_initial_state[3], sha256_initial_state[4], sha256_initial_state[5],
                             sha256_initial_state[6], sha256_initial_state[7] };
    u64 total_bytes      = 0;
    std::byte buffer[64] = {};
    usize buffered       = 0;

    constexpr void absorb(const std::byte* data, usize size) noexcept
    {
        total_bytes += size;
        while (size > 0)
        {
            const usize take = (64 - buffered) < size ? (64 - buffered) : size;
            for (usize i = 0; i < take; ++i)
            {
                buffer[buffered + i] = data[i];
            }
            buffered += take;
            data += take;
            size -= take;
            if (buffered == 64)
            {
                sha256_compress(state, buffer);
                buffered = 0;
            }
        }
    }

    [[nodiscard]] constexpr SHA256Digest finalize() noexcept
    {
        // SHA-256 counts message length in bits as u64; beyond 2^61 bytes the
        // counter would wrap. Such an input cannot be produced by a caller
        // that hashes real storage; treating it as a programming error.
        QIVEN_ASSERT((total_bytes >> 61) == 0);

        const u64 bit_length = total_bytes << 3;

        buffer[buffered] = static_cast<std::byte>(0x80);
        ++buffered;
        if (buffered > 56)
        {
            while (buffered < 64)
            {
                buffer[buffered] = std::byte { 0 };
                ++buffered;
            }
            sha256_compress(state, buffer);
            buffered = 0;
        }
        while (buffered < 56)
        {
            buffer[buffered] = std::byte { 0 };
            ++buffered;
        }
        sha256_store_be_u64(buffer + 56, bit_length);
        sha256_compress(state, buffer);

        SHA256Digest out {};
        for (usize i = 0; i < 8; ++i)
        {
            sha256_store_be_u32(out.data() + 4 * i, state[i]);
        }

        // Re-arm for the next message so one hasher can hash sequentially.
        for (usize i = 0; i < 8; ++i)
        {
            state[i] = sha256_initial_state[i];
        }
        total_bytes = 0;
        buffered    = 0;
        return out;
    }
};
} // namespace detail

// One-shot digests. The byte-range forms are constexpr; the string_view form
// reinterprets char storage and is runtime-only.
[[nodiscard]] constexpr SHA256Digest sha256(const std::byte* data, usize size) noexcept
{
    detail::SHA256Core core;
    core.absorb(data, size);
    return core.finalize();
}

[[nodiscard]] constexpr SHA256Digest sha256(std::span<const std::byte> bytes) noexcept
{
    return sha256(bytes.data(), bytes.size());
}

[[nodiscard]] inline SHA256Digest sha256(std::string_view text) noexcept
{
    return sha256(reinterpret_cast<const std::byte*>(text.data()), text.size());
}

// Streaming hasher for large or chunk-delivered content. finish() returns the
// digest and re-arms the hasher for a new message.
class SHA256Hasher
{
public:
    SHA256Hasher() noexcept = default;

    constexpr void update(const std::byte* data, usize size) noexcept
    {
        m_core.absorb(data, size);
    }

    constexpr void update(std::span<const std::byte> bytes) noexcept
    {
        m_core.absorb(bytes.data(), bytes.size());
    }

    inline void update(std::string_view text) noexcept
    {
        m_core.absorb(reinterpret_cast<const std::byte*>(text.data()), text.size());
    }

    [[nodiscard]] constexpr SHA256Digest finish() noexcept
    {
        return m_core.finalize();
    }

private:
    detail::SHA256Core m_core;
};

// Lowercase, fixed-width 64-character hex encoding of a digest.
// `out` must have room for at least 64 characters.
inline void to_hex_sha256(SHA256Digest digest, char* out) noexcept
{
    static constexpr char digits[] = "0123456789abcdef";
    for (usize i = 0; i < digest.size(); ++i)
    {
        const auto byte = static_cast<unsigned char>(digest[i]);
        *out++          = digits[byte >> 4];
        *out++          = digits[byte & 0x0FU];
    }
}

[[nodiscard]] inline std::string to_hex_sha256(SHA256Digest digest)
{
    std::string out(64, '0');
    to_hex_sha256(digest, out.data());
    return out;
}
} // namespace qiven
