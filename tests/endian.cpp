#include <qiven/byte_cursor.hpp>
#include <qiven/byte_writer.hpp>
#include <qiven/endian.hpp>
#include <qiven/types.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <string>

namespace
{
using qiven::u16;
using qiven::u32;
using qiven::u64;

constexpr bool verify_constexpr() noexcept
{
    constexpr std::array<std::byte, 8> bytes {
        std::byte { 0x01 },
        std::byte { 0x23 },
        std::byte { 0x45 },
        std::byte { 0x67 },
        std::byte { 0x89 },
        std::byte { 0xab },
        std::byte { 0xcd },
        std::byte { 0xef },
    };

    const std::span<const std::byte> view { bytes };

    if (qiven::decode_be_u16(view.first(2)) != std::optional<u16> { u16 { 0x0123 } })
        return false;
    if (qiven::decode_le_u16(view.first(2)) != std::optional<u16> { u16 { 0x2301 } })
        return false;

    if (qiven::decode_be_u32(view.first(4)) != std::optional<u32> { u32 { 0x01234567 } })
        return false;
    if (qiven::decode_le_u32(view.first(4)) != std::optional<u32> { u32 { 0x67452301 } })
        return false;

    if (qiven::decode_be_u64(view) != std::optional<u64> { u64 { 0x0123456789abcdefULL } })
        return false;
    if (qiven::decode_le_u64(view) != std::optional<u64> { u64 { 0xefcdab8967452301ULL } })
        return false;

    if (qiven::decode_be_u32(view.first(3)))
        return false;
    if (qiven::decode_be_u32(view.first(5)))
        return false;

    return true;
}

static_assert(verify_constexpr());

static_assert(std::same_as<
              decltype(qiven::decode_le_u16(std::span<const std::byte> {})),
              std::optional<u16>>);
static_assert(std::same_as<
              decltype(qiven::decode_le_u32(std::span<const std::byte> {})),
              std::optional<u32>>);
static_assert(std::same_as<
              decltype(qiven::decode_le_u64(std::span<const std::byte> {})),
              std::optional<u64>>);
template <typename T>
constexpr bool verify_encoding(
    T pattern,
    const std::array<std::byte, sizeof(T)>& pattern_be,
    bool (*encode_le)(T, std::span<std::byte>) noexcept,
    bool (*encode_be)(T, std::span<std::byte>) noexcept,
    std::optional<T> (*decode_le)(std::span<const std::byte>) noexcept,
    std::optional<T> (*decode_be)(std::span<const std::byte>) noexcept) noexcept
{
    for (const T value : std::array<T, 3> { T { 0 }, std::numeric_limits<T>::max(), pattern })
    {
        std::array<std::byte, sizeof(T)> expected_be {};
        if (value == std::numeric_limits<T>::max())
            expected_be.fill(std::byte { 0xff });
        else if (value == pattern)
            expected_be = pattern_be;

        // The one-byte offset from an aligned base deliberately exercises an unaligned destination.
        alignas(T) std::array<std::byte, sizeof(T) + 2> storage {};
        storage.fill(std::byte { 0xa5 });
        const auto destination = std::span<std::byte> { storage }.subspan(1, sizeof(T));

        if (!encode_le(value, destination) || decode_le(destination) != std::optional<T> { value })
            return false;
        for (qiven::usize index = 0; index < sizeof(T); ++index)
            if (destination[index] != expected_be[sizeof(T) - 1 - index])
                return false;
        if (storage.front() != std::byte { 0xa5 } || storage.back() != std::byte { 0xa5 })
            return false;

        if (!encode_be(value, destination) || decode_be(destination) != std::optional<T> { value })
            return false;
        for (qiven::usize index = 0; index < sizeof(T); ++index)
            if (destination[index] != expected_be[index])
                return false;
        if (storage.front() != std::byte { 0xa5 } || storage.back() != std::byte { 0xa5 })
            return false;

        for (const qiven::usize count : std::array<qiven::usize, 3> { 0, sizeof(T) - 1, sizeof(T) + 1 })
        {
            storage.fill(std::byte { 0x5a });
            storage[1]         = std::byte { 0xc3 };
            const auto before  = storage;
            const auto invalid = std::span<std::byte> { storage }.subspan(1, count);
            if (encode_le(value, invalid) || storage != before)
                return false;
            if (encode_be(value, invalid) || storage != before)
                return false;
        }
    }
    return true;
}

constexpr bool verify_all_encodings() noexcept
{
    return verify_encoding<u16>(
               0x89ab,
               { std::byte { 0x89 }, std::byte { 0xab } },
               qiven::encode_le_u16, qiven::encode_be_u16,
               qiven::decode_le_u16, qiven::decode_be_u16) &&
           verify_encoding<u32>(
               0x89abcdef,
               { std::byte { 0x89 }, std::byte { 0xab }, std::byte { 0xcd }, std::byte { 0xef } },
               qiven::encode_le_u32, qiven::encode_be_u32,
               qiven::decode_le_u32, qiven::decode_be_u32) &&
           verify_encoding<u64>(
               0x8123456789abcdefULL,
               { std::byte { 0x81 }, std::byte { 0x23 }, std::byte { 0x45 }, std::byte { 0x67 },
                 std::byte { 0x89 }, std::byte { 0xab }, std::byte { 0xcd }, std::byte { 0xef } },
               qiven::encode_le_u64, qiven::encode_be_u64,
               qiven::decode_le_u64, qiven::decode_be_u64);
}

constexpr bool verify_writer_packet() noexcept
{
    std::array<std::byte, 6> packet {};
    qiven::ByteWriter writer { packet };
    const auto field16 = writer.reserve(2);
    const auto field32 = writer.reserve(4);
    if (!field16 || !field32 || !writer.empty())
        return false;
    if (!qiven::encode_le_u16(0x1234, *field16) || !qiven::encode_be_u32(0x89abcdef, *field32))
        return false;

    constexpr std::array<std::byte, 6> expected {
        std::byte { 0x34 }, std::byte { 0x12 }, std::byte { 0x89 },
        std::byte { 0xab }, std::byte { 0xcd }, std::byte { 0xef }
    };
    return packet == expected &&
           qiven::decode_le_u16(*field16) == std::optional<u16> { u16 { 0x1234 } } &&
           qiven::decode_be_u32(*field32) == std::optional<u32> { u32 { 0x89abcdef } };
}

static_assert(verify_all_encodings());
static_assert(verify_writer_packet());
} // namespace

int main()
{
    constexpr std::array<std::byte, 4> zero_bytes {};
    constexpr std::array<std::byte, 4> max_bytes {
        std::byte { 0xff },
        std::byte { 0xff },
        std::byte { 0xff },
        std::byte { 0xff },
    };

    if (qiven::decode_le_u32(zero_bytes) != std::optional<u32> { u32 { 0 } })
        return 1;
    if (qiven::decode_be_u32(max_bytes) != std::optional<u32> { std::numeric_limits<u32>::max() })
        return 2;

    const std::array<std::byte, 6> unaligned_storage {
        std::byte { 0xaa },
        std::byte { 0x12 },
        std::byte { 0x34 },
        std::byte { 0x56 },
        std::byte { 0x78 },
        std::byte { 0xbb },
    };

    const std::span<const std::byte> unaligned { unaligned_storage.data() + 1, 4 };

    if (qiven::decode_be_u32(unaligned) != std::optional<u32> { u32 { 0x12345678 } })
        return 3;
    if (qiven::decode_le_u32(unaligned) != std::optional<u32> { u32 { 0x78563412 } })
        return 4;

    if (qiven::decode_le_u16(std::span<const std::byte> {}))
        return 5;
    if (qiven::decode_le_u16(std::span<const std::byte> { unaligned_storage }.first(1)))
        return 6;
    if (qiven::decode_le_u16(std::span<const std::byte> { unaligned_storage }.first(3)))
        return 7;

    const std::array<std::byte, 6> packet {
        std::byte { 0x34 },
        std::byte { 0x12 },
        std::byte { 0x89 },
        std::byte { 0xab },
        std::byte { 0xcd },
        std::byte { 0xef },
    };

    qiven::ByteCursor cursor { std::span<const std::byte> { packet } };

    const auto field16 = cursor.take(2);
    if (!field16 || qiven::decode_le_u16(*field16) != std::optional<u16> { u16 { 0x1234 } })
        return 8;

    const auto field32 = cursor.take(4);
    if (!field32 || qiven::decode_be_u32(*field32) != std::optional<u32> { u32 { 0x89abcdef } })
        return 9;

    if (!cursor.empty())
        return 10;

    if (!verify_all_encodings())
        return 11;
    if (!verify_writer_packet())
        return 12;

    // ---- adversarial phase (2026-09-20): exhaustive round-trips, boundary
    // tables, undersized/oversized spans, misalignment, no partial writes ----

    const std::string eol(1, char(10));

    // EXHAUSTIVE u16 round-trip in both endiannesses (all 65536 values)
    for (u32 value = 0; value <= 0xFFFFU; ++value)
    {
        const auto original = static_cast<u16>(value);
        std::array<std::byte, 2> buffer {};
        if (!qiven::encode_le_u16(original, std::span<std::byte>(buffer)))
            return 20;
        if (*qiven::decode_le_u16(std::span<const std::byte>(buffer)) != original)
            return 21;
        std::array<std::byte, 2> be_buffer {};
        if (!qiven::encode_be_u16(original, std::span<std::byte>(be_buffer)))
            return 22;
        if (*qiven::decode_be_u16(std::span<const std::byte>(be_buffer)) != original)
            return 23;
        if (original == 0x1234U)
        {
            // known byte order: LE is little-end-first, BE is big-end-first
            if (buffer[0] != std::byte { 0x34 } || buffer[1] != std::byte { 0x12 })
                return 24;
            if (be_buffer[0] != std::byte { 0x12 } || be_buffer[1] != std::byte { 0x34 })
                return 25;
            if (original != 0 && buffer == be_buffer)
                return 26; // multi-byte LE and BE must differ for this value
        }
    }

    // u32 boundary table: encode/decode round-trips and known byte order
    {
        const u32 boundaries[] { 0x00000000U, 0x00000001U, 0x000000FFU, 0x00000100U,
                                 0x0000FFFFU, 0x00010000U, 0x7FFFFFFFU, 0x80000000U, 0xFFFFFF00U, 0xFFFFFFFFU,
                                 0xDEADBEEFU, 0x12345678U };
        for (const u32 value : boundaries)
        {
            std::array<std::byte, 4> buffer {};
            if (!qiven::encode_le_u32(value, std::span<std::byte>(buffer)))
                return 27;
            if (*qiven::decode_le_u32(std::span<const std::byte>(buffer)) != value)
                return 28;
            if (value == 0x12345678U && (buffer[0] != std::byte { 0x78 } || buffer[1] != std::byte { 0x56 } || buffer[2] != std::byte { 0x34 } || buffer[3] != std::byte { 0x12 }))
                return 29;
            std::array<std::byte, 4> be {};
            if (!qiven::encode_be_u32(value, std::span<std::byte>(be)))
                return 30;
            if (*qiven::decode_be_u32(std::span<const std::byte>(be)) != value)
                return 31;
            if (value == 0x12345678U && (be[0] != std::byte { 0x12 } || be[1] != std::byte { 0x34 } || be[2] != std::byte { 0x56 } || be[3] != std::byte { 0x78 }))
                return 132;
        }
    }

    // u64 extremes
    {
        const u64 extremes[] { 0ULL, 1ULL, 0x00FFFFFFFFFFFFFFULL, 0x7FFFFFFF00000000ULL,
                               0x8000000000000000ULL, 0xFFFFFFFFFFFFFFFEULL, std::numeric_limits<u64>::max(),
                               0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL };
        for (const u64 value : extremes)
        {
            std::array<std::byte, 8> buffer {};
            if (!qiven::encode_le_u64(value, std::span<std::byte>(buffer)))
                return 33;
            if (*qiven::decode_le_u64(std::span<const std::byte>(buffer)) != value)
                return 34;
            std::array<std::byte, 8> be {};
            if (!qiven::encode_be_u64(value, std::span<std::byte>(be)))
                return 35;
            if (*qiven::decode_be_u64(std::span<const std::byte>(be)) != value)
                return 36;
        }
    }

    // undersized spans: decode refuses, encode refuses WITHOUT partial writes
    {
        std::array<std::byte, 3> small {};
        if (qiven::decode_le_u32(std::span<const std::byte>(small)).has_value())
            return 37;
        if (qiven::decode_be_u32(std::span<const std::byte>(small)).has_value())
            return 38;
        std::array<std::byte, 8> target { std::byte { 1 }, std::byte { 2 }, std::byte { 3 },
                                          std::byte { 4 }, std::byte { 5 }, std::byte { 6 }, std::byte { 7 }, std::byte { 8 } };
        if (qiven::encode_le_u32(0xDEADBEEFU, std::span<std::byte>(target.data(), 3)))
            return 39; // encode returns bool: true means it WROTE, but the span is too small
        for (const auto byte : target)
        {
            if (byte != std::byte { 0 } && byte != std::byte { 1 } && byte != std::byte { 2 } && byte != std::byte { 3 } && byte != std::byte { 4 } && byte != std::byte { 5 } && byte != std::byte { 6 } && byte != std::byte { 7 } && byte != std::byte { 8 })
                return 40; // a failed encode must leave the target untouched
        }
    }

    // oversized spans: decode is STRICTLY exact-size - a larger span is
    // refused, which pins the contract against silent partial reads
    {
        std::array<std::byte, 6> padded { std::byte { 0x78 }, std::byte { 0x56 },
                                          std::byte { 0x34 }, std::byte { 0x12 }, std::byte { 0xEE }, std::byte { 0xFF } };
        if (qiven::decode_le_u32(std::span<const std::byte>(padded)).has_value())
            return 42; // strict exact-size contract (pinned 2026-09-20)
    }

    // misaligned underlying storage: odd-offset buffers decode correctly
    {
        alignas(8) std::array<std::byte, 9> raw {};
        for (std::size_t offset = 1; offset <= 4; ++offset)
        {
            std::span<std::byte> slot { raw.data() + offset, 4 };
            if (!qiven::encode_le_u32(0xA1B2C3DU, slot))
                return 43;
            if (*qiven::decode_le_u32(std::span<const std::byte>(slot)) != 0x0A1B2C3DU)
                return 44;
        }
    }

    // signed types are not part of the endian contract (type level)
    // (the API only accepts unsigned integers; this is asserted by usage)

    // constexpr round-trips
    static_assert([] {
        std::array<std::byte, 2> buffer {};
        return qiven::encode_le_u16(0x1234U, std::span<std::byte>(buffer)) && *qiven::decode_le_u16(std::span<const std::byte>(buffer)) == 0x1234U;
    }());
    static_assert([] {
        std::array<std::byte, 4> buffer {};
        return qiven::encode_be_u32(0x12345678U, std::span<std::byte>(buffer)) && *qiven::decode_be_u32(std::span<const std::byte>(buffer)) == 0x12345678U;
    }());

    std::printf("[ OK ] adversarial endian cases%s", eol.c_str());
    return 0;
}
