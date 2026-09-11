#pragma once

#include <concepts>
#include <optional>
#include <utility>

namespace qiven
{
namespace detail
{
template <typename T>
concept checked_integer =
    std::same_as<T, signed char> ||
    std::same_as<T, unsigned char> ||
    std::same_as<T, short> ||
    std::same_as<T, unsigned short> ||
    std::same_as<T, int> ||
    std::same_as<T, unsigned int> ||
    std::same_as<T, long> ||
    std::same_as<T, unsigned long> ||
    std::same_as<T, long long> ||
    std::same_as<T, unsigned long long>;
} // namespace detail

template <detail::checked_integer To, detail::checked_integer From>
[[nodiscard]] constexpr std::optional<To> checked_integer_cast(From value) noexcept
{
    if (!std::in_range<To>(value))
        return std::nullopt;

    return static_cast<To>(value);
}
} // namespace qiven
