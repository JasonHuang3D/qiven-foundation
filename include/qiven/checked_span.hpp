#pragma once

#include <optional>
#include <span>

#include <qiven/types.hpp>

namespace qiven
{
template <typename T, usize Extent>
[[nodiscard]] constexpr std::optional<std::span<T>> checked_subspan(
    std::span<T, Extent> source,
    usize offset,
    usize count) noexcept
{
    if (offset > source.size())
        return std::nullopt;

    if (count > source.size() - offset)
        return std::nullopt;

    return source.subspan(offset, count);
}
} // namespace qiven
