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

    return 0;
}
