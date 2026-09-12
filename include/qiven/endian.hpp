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

template <typename T>
[[nodiscard]] constexpr bool encode_le_unsigned(T value, std::span<std::byte> bytes) noexcept
{
    if (bytes.size() != sizeof(T))
        return false;

    for (usize index = 0; index < bytes.size(); ++index)
    {
        const usize shift = index * 8;
        bytes[index]      = static_cast<std::byte>((static_cast<u64>(value) >> shift) & u64 { 0xff });
    }
    return true;
}

template <typename T>
[[nodiscard]] constexpr bool encode_be_unsigned(T value, std::span<std::byte> bytes) noexcept
{
    if (bytes.size() != sizeof(T))
        return false;

    for (usize index = 0; index < bytes.size(); ++index)
    {
        const usize shift = (bytes.size() - 1 - index) * 8;
        bytes[index]      = static_cast<std::byte>((static_cast<u64>(value) >> shift) & u64 { 0xff });
    }
    return true;
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

[[nodiscard]] constexpr bool encode_le_u16(u16 value, std::span<std::byte> bytes) noexcept
{
    return detail::encode_le_unsigned(value, bytes);
}

[[nodiscard]] constexpr bool encode_le_u32(u32 value, std::span<std::byte> bytes) noexcept
{
    return detail::encode_le_unsigned(value, bytes);
}

[[nodiscard]] constexpr bool encode_le_u64(u64 value, std::span<std::byte> bytes) noexcept
{
    return detail::encode_le_unsigned(value, bytes);
}

[[nodiscard]] constexpr bool encode_be_u16(u16 value, std::span<std::byte> bytes) noexcept
{
    return detail::encode_be_unsigned(value, bytes);
}

[[nodiscard]] constexpr bool encode_be_u32(u32 value, std::span<std::byte> bytes) noexcept
{
    return detail::encode_be_unsigned(value, bytes);
}

[[nodiscard]] constexpr bool encode_be_u64(u64 value, std::span<std::byte> bytes) noexcept
{
    return detail::encode_be_unsigned(value, bytes);
}
} // namespace qiven
