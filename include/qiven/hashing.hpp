#pragma once

// ============================================================================
// hashing.hpp — non-cryptographic 64-bit hashing and hex encoding
//
// FNV-1a 64 is a deterministic, allocation-free checksum primitive. It is
// suitable for content identity, hash tables and golden-vector pinning. It is
// NOT cryptographic: it must never be used where collision resistance against
// an adversary is required.
// ============================================================================

#include <qiven/types.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace qiven
{
inline constexpr u64 fnv1a64_offset_basis = 1469598103934665603ULL;
inline constexpr u64 fnv1a64_prime        = 1099511628211ULL;

// FNV-1a 64 over a byte range, continuing from `seed` (so calls compose).
[[nodiscard]] constexpr u64 fnv1a64(const std::byte* data, usize size,
                                    u64 seed = fnv1a64_offset_basis) noexcept
{
    for (usize i = 0; i < size; ++i)
    {
        seed ^= static_cast<u64>(data[i]);
        seed *= fnv1a64_prime;
    }
    return seed;
}

[[nodiscard]] constexpr u64 fnv1a64(std::span<const std::byte> bytes,
                                    u64 seed = fnv1a64_offset_basis) noexcept
{
    return fnv1a64(bytes.data(), bytes.size(), seed);
}

[[nodiscard]] constexpr u64 fnv1a64(std::string_view text,
                                    u64 seed = fnv1a64_offset_basis) noexcept
{
    return fnv1a64(reinterpret_cast<const std::byte*>(text.data()), text.size(), seed);
}

// Compose a hash over two parts (for example a parent identity and a payload)
// without concatenating buffers.
[[nodiscard]] constexpr u64 fnv1a64_chain(u64 seed, std::string_view text,
                                          std::span<const std::byte> bytes) noexcept
{
    for (const char ch : text)
    {
        seed ^= static_cast<u64>(static_cast<unsigned char>(ch));
        seed *= fnv1a64_prime;
    }
    return fnv1a64(bytes.data(), bytes.size(), seed);
}

// Lowercase, fixed-width 16-character hex encoding of a 64-bit value.
// `out` must have room for at least 16 characters.
inline void to_hex_u64(u64 value, char* out) noexcept
{
    static constexpr char digits[] = "0123456789abcdef";
    for (int shift = 60; shift >= 0; shift -= 4)
    {
        *out++ = digits[(value >> shift) & 0xFU];
    }
}

[[nodiscard]] inline std::string to_hex_u64(u64 value)
{
    std::string out(16, '0');
    to_hex_u64(value, out.data());
    return out;
}
} // namespace qiven
