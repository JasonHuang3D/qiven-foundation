#include <qiven/hashing_sha256.hpp>

#include <qiven/contracts.hpp>

#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using qiven::usize;

constexpr std::byte operator""_b(unsigned long long value)
{
    return static_cast<std::byte>(value);
}

// FIPS 180-4 example digests, pinned at compile time where the input is a
// constant byte range.
constexpr qiven::SHA256Digest empty_input_digest = { 0xE3_b, 0xB0_b, 0xC4_b, 0x42_b, 0x98_b, 0xFC_b, 0x1C_b, 0x14_b,
                                                     0x9A_b, 0xFB_b, 0xF4_b, 0xC8_b, 0x99_b, 0x6F_b, 0xB9_b, 0x24_b,
                                                     0x27_b, 0xAE_b, 0x41_b, 0xE4_b, 0x64_b, 0x9B_b, 0x93_b, 0x4C_b,
                                                     0xA4_b, 0x95_b, 0x99_b, 0x1B_b, 0x78_b, 0x52_b, 0xB8_b, 0x55_b };

constexpr qiven::SHA256Digest abc_digest = { 0xBA_b, 0x78_b, 0x16_b, 0xBF_b, 0x8F_b, 0x01_b, 0xCF_b, 0xEA_b,
                                             0x41_b, 0x41_b, 0x40_b, 0xDE_b, 0x5D_b, 0xAE_b, 0x22_b, 0x23_b,
                                             0xB0_b, 0x03_b, 0x61_b, 0xA3_b, 0x96_b, 0x17_b, 0x7A_b, 0x9C_b,
                                             0xB4_b, 0x10_b, 0xFF_b, 0x61_b, 0xF2_b, 0x00_b, 0x15_b, 0xAD_b };

constexpr std::byte abc_bytes[] { std::byte { 'a' }, std::byte { 'b' }, std::byte { 'c' } };

// NIST "million a" vector and the two longer FIPS examples, compared as hex
// at runtime (variable-length inputs).
constexpr std::string_view fips_448_message = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
constexpr std::string_view fips_448_hex     = "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1";
constexpr std::string_view fips_896_message =
    "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";
constexpr std::string_view fips_896_hex  = "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1";
constexpr std::string_view million_a_hex = "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0";
} // namespace

static_assert(qiven::sha256(static_cast<const std::byte*>(nullptr), 0) == empty_input_digest);
static_assert(qiven::sha256(abc_bytes, sizeof abc_bytes) == abc_digest);

int main()
{
    // runtime one-shot agrees with the compile-time pinning
    QIVEN_VERIFY(qiven::sha256(abc_bytes, sizeof abc_bytes) == abc_digest);
    QIVEN_VERIFY(qiven::sha256("") == qiven::sha256(static_cast<const std::byte*>(nullptr), 0));

    // span, pointer and string_view overloads hash the same bytes
    {
        const std::string text = "abc";
        QIVEN_VERIFY(qiven::sha256(text) == abc_digest);
        QIVEN_VERIFY(qiven::sha256(std::span<const std::byte>(abc_bytes, sizeof abc_bytes)) == abc_digest);
    }

    // golden vectors for the longer FIPS messages
    QIVEN_VERIFY(qiven::to_hex_sha256(qiven::sha256(fips_448_message)) == fips_448_hex);
    QIVEN_VERIFY(qiven::to_hex_sha256(qiven::sha256(fips_896_message)) == fips_896_hex);

    // the million-'a' NIST stress vector, one-shot
    QIVEN_VERIFY(qiven::to_hex_sha256(qiven::sha256(std::string(1'000'000, 'a'))) == million_a_hex);

    // streaming equals one-shot across chunk patterns, including the padding
    // boundaries at 55/56/63/64/65 bytes
    {
        const std::string long_text = std::string(2000, 'x') + std::string(fips_896_message);
        const auto expected         = qiven::sha256(long_text);

        const usize chunk_sizes[] = { 1, 3, 55, 56, 63, 64, 65, 77, 510, 1000 };
        for (const usize chunk : chunk_sizes)
        {
            qiven::SHA256Hasher hasher;
            for (usize offset = 0; offset < long_text.size(); offset += chunk)
            {
                const usize take = (long_text.size() - offset) < chunk ? long_text.size() - offset : chunk;
                hasher.update(std::string_view(long_text).substr(offset, take));
            }
            QIVEN_VERIFY(hasher.finish() == expected);
        }

        // empty updates are no-ops; mixed overload shapes agree
        qiven::SHA256Hasher hasher;
        hasher.update("");
        hasher.update(static_cast<const std::byte*>(nullptr), 0);
        hasher.update(std::span<const std::byte> {});
        hasher.update(fips_448_message);
        QIVEN_VERIFY(qiven::to_hex_sha256(hasher.finish()) == fips_448_hex);
    }

    // finish() re-arms the hasher: sequential digests match one-shot each
    {
        qiven::SHA256Hasher hasher;
        hasher.update("abc");
        QIVEN_VERIFY(hasher.finish() == abc_digest);
        hasher.update(fips_448_message);
        QIVEN_VERIFY(qiven::to_hex_sha256(hasher.finish()) == fips_448_hex);
        hasher.update("");
        QIVEN_VERIFY(hasher.finish() == empty_input_digest);
    }

    // streaming the million-'a' vector in 1000-byte chunks
    {
        const std::string million(1'000'000, 'a');
        qiven::SHA256Hasher hasher;
        for (usize offset = 0; offset < million.size(); offset += 1000)
        {
            hasher.update(std::string_view(million).substr(offset, 1000));
        }
        QIVEN_VERIFY(qiven::to_hex_sha256(hasher.finish()) == million_a_hex);
    }

    // hex encoding: fixed width, lowercase, extremes
    {
        const qiven::SHA256Digest zeros {};
        QIVEN_VERIFY(qiven::to_hex_sha256(zeros) == std::string(64, '0'));
        QIVEN_VERIFY(qiven::to_hex_sha256(abc_digest) ==
                     "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

        char text[64];
        qiven::to_hex_sha256(zeros, text);
        QIVEN_VERIFY(std::string(text, 64) == std::string(64, '0'));
    }

    // different content digests differently; identical content is stable
    {
        QIVEN_VERIFY(qiven::sha256("a") != qiven::sha256("b"));
        QIVEN_VERIFY(qiven::sha256("a") == qiven::sha256("a"));
    }

    std::printf("[ OK ] hashing_sha256\n");
    return 0;
}
