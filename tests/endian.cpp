#include <qiven/byte_cursor.hpp>
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

    return 0;
}
