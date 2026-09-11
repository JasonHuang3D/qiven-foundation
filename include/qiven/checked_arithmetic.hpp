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
concept checked_unsigned_integer =
    std::unsigned_integral<T> && !std::same_as<std::remove_cv_t<T>, bool>;
} // namespace detail

template <detail::checked_unsigned_integer T>
[[nodiscard]] constexpr std::optional<T> checked_add(T lhs, T rhs) noexcept
{
    if (rhs > std::numeric_limits<T>::max() - lhs)
        return std::nullopt;

    return static_cast<T>(lhs + rhs);
}

template <detail::checked_unsigned_integer T>
[[nodiscard]] constexpr std::optional<T> checked_sub(T lhs, T rhs) noexcept
{
    if (rhs > lhs)
        return std::nullopt;

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
