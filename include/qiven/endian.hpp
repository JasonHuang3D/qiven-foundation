#pragma once

#include <cstddef>
#include <optional>
#include <span>

#include <qiven/types.hpp>

namespace qiven
{
namespace detail
{
template <typename T>
[[nodiscard]] constexpr std::optional<T> decode_le_unsigned(std::span<const std::byte> bytes) noexcept
{
    if (bytes.size() != sizeof(T))
        return std::nullopt;

    u64 value = 0;

    for (usize index = 0; index < bytes.size(); ++index)
    {
        const usize shift = index * 8;
        value |= static_cast<u64>(std::to_integer<u8>(bytes[index])) << shift;
    }

    return static_cast<T>(value);
}

template <typename T>
[[nodiscard]] constexpr std::optional<T> decode_be_unsigned(std::span<const std::byte> bytes) noexcept
{
    if (bytes.size() != sizeof(T))
        return std::nullopt;

    u64 value = 0;

    for (const std::byte byte : bytes)
    {
        value <<= 8;
        value |= static_cast<u64>(std::to_integer<u8>(byte));
    }

    return static_cast<T>(value);
}
} // namespace detail

[[nodiscard]] constexpr std::optional<u16> decode_le_u16(std::span<const std::byte> bytes) noexcept
{
    return detail::decode_le_unsigned<u16>(bytes);
}

[[nodiscard]] constexpr std::optional<u32> decode_le_u32(std::span<const std::byte> bytes) noexcept
{
    return detail::decode_le_unsigned<u32>(bytes);
}

[[nodiscard]] constexpr std::optional<u64> decode_le_u64(std::span<const std::byte> bytes) noexcept
{
    return detail::decode_le_unsigned<u64>(bytes);
}

[[nodiscard]] constexpr std::optional<u16> decode_be_u16(std::span<const std::byte> bytes) noexcept
{
    return detail::decode_be_unsigned<u16>(bytes);
}

[[nodiscard]] constexpr std::optional<u32> decode_be_u32(std::span<const std::byte> bytes) noexcept
{
    return detail::decode_be_unsigned<u32>(bytes);
}

[[nodiscard]] constexpr std::optional<u64> decode_be_u64(std::span<const std::byte> bytes) noexcept
{
    return detail::decode_be_unsigned<u64>(bytes);
}
} // namespace qiven
