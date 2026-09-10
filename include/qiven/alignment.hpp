#pragma once

#include <bit>
#include <limits>

#include <qiven/contracts.hpp>
#include <qiven/types.hpp>

namespace qiven
{
[[nodiscard]] constexpr bool is_aligned(usize value, usize alignment) noexcept
{
    QIVEN_ASSERT(std::has_single_bit(alignment));

    return (value & (alignment - 1)) == 0;
}

[[nodiscard]] constexpr usize align_down(usize value, usize alignment) noexcept
{
    QIVEN_ASSERT(std::has_single_bit(alignment));

    const usize mask = alignment - 1;
    return value & ~mask;
}

[[nodiscard]] constexpr usize align_up(usize value, usize alignment) noexcept
{
    QIVEN_ASSERT(std::has_single_bit(alignment));

    const usize mask = alignment - 1;
    QIVEN_ASSERT(value <= std::numeric_limits<usize>::max() - mask);

    return (value + mask) & ~mask;
}
} // namespace qiven
