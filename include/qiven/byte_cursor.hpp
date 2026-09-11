#pragma once

#include <cstddef>
#include <optional>
#include <span>

#include <qiven/types.hpp>

namespace qiven
{
class ByteCursor
{
public:
    explicit constexpr ByteCursor(std::span<const std::byte> bytes) noexcept
        : remaining_(bytes)
    {
    }

    [[nodiscard]] constexpr std::optional<std::span<const std::byte>> take(usize count) noexcept
    {
        if (count > remaining_.size())
            return std::nullopt;

        const std::span<const std::byte> result = remaining_.first(count);
        remaining_                             = remaining_.subspan(count);
        return result;
    }

    [[nodiscard]] constexpr std::span<const std::byte> remaining_bytes() const noexcept
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
    std::span<const std::byte> remaining_;
};
} // namespace qiven
