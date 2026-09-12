#pragma once

#include <concepts>
#include <limits>
#include <optional>
#include <type_traits>

namespace qiven
{
namespace detail
{
template <typename T>
concept checked_integer = std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool>;

template <typename T>
concept checked_unsigned_integer = std::unsigned_integral<T> && checked_integer<T>;
} // namespace detail

template <detail::checked_integer T>
[[nodiscard]] constexpr std::optional<T> checked_add(T lhs, T rhs) noexcept
{
    if constexpr (std::signed_integral<T>)
    {
        if ((rhs > 0 && lhs > std::numeric_limits<T>::max() - rhs) ||
            (rhs < 0 && lhs < std::numeric_limits<T>::min() - rhs))
            return std::nullopt;
    }
    else
    {
        if (rhs > std::numeric_limits<T>::max() - lhs)
            return std::nullopt;
    }

    return static_cast<T>(lhs + rhs);
}

template <detail::checked_integer T>
[[nodiscard]] constexpr std::optional<T> checked_sub(T lhs, T rhs) noexcept
{
    if constexpr (std::signed_integral<T>)
    {
        if ((rhs > 0 && lhs < std::numeric_limits<T>::min() + rhs) ||
            (rhs < 0 && lhs > std::numeric_limits<T>::max() + rhs))
            return std::nullopt;
    }
    else
    {
        if (rhs > lhs)
            return std::nullopt;
    }

    return static_cast<T>(lhs - rhs);
}

template <detail::checked_unsigned_integer T>
[[nodiscard]] constexpr std::optional<T> checked_mul(T lhs, T rhs) noexcept
{
    if (lhs != 0 && rhs > std::numeric_limits<T>::max() / lhs)
        return std::nullopt;

    return static_cast<T>(lhs * rhs);
}
} // namespace qiven
