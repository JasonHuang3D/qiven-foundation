#pragma once

#include <cstddef>
#include <optional>
#include <span>

#include <qiven/types.hpp>

namespace qiven
{
class ByteWriter
{
public:
    explicit constexpr ByteWriter(std::span<std::byte> bytes) noexcept
    :
    remaining_(bytes)
    {
    }

    [[nodiscard]] constexpr std::optional<std::span<std::byte>> reserve(usize count) noexcept
    {
        if (count > remaining_.size())
            return std::nullopt;

        const std::span<std::byte> result = remaining_.first(count);
        remaining_                        = remaining_.subspan(count);
        return result;
    }

    [[nodiscard]] constexpr std::span<std::byte> remaining_bytes() const noexcept
    {
        return remaining_;
    }

    [[nodiscard]] constexpr usize remaining() const noexcept
    {
        return remaining_.size();
    }

    [[nodiscard]] constexpr bool empty() const noexcept
    {
        return remaining_.empty();
    }

private:
    std::span<std::byte> remaining_;
};
} // namespace qiven
