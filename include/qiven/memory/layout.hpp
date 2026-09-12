#pragma once

#include <bit>
#include <optional>

#include <qiven/checked_arithmetic.hpp>
#include <qiven/contracts.hpp>
#include <qiven/types.hpp>

namespace qiven::memory
{
class Layout
{
public:
    [[nodiscard]] static constexpr Layout from_size_alignment(usize size, usize alignment) noexcept
    {
        QIVEN_ASSERT(std::has_single_bit(alignment));

        return Layout { size, alignment };
    }

    template <typename T>
    [[nodiscard]] static constexpr Layout of() noexcept
    {
        return Layout { sizeof(T), alignof(T) };
    }

    template <typename T>
    [[nodiscard]] static constexpr std::optional<Layout> array(usize count) noexcept
    {
        const auto size = checked_mul<usize>(sizeof(T), count);
        if (!size)
            return std::nullopt;

        return Layout { *size, alignof(T) };
    }

    [[nodiscard]] constexpr usize size() const noexcept
    {
        return size_;
    }

    [[nodiscard]] constexpr usize alignment() const noexcept
    {
        return alignment_;
    }

private:
    constexpr Layout(usize size, usize alignment) noexcept
    :
    size_(size), alignment_(alignment)
    {
    }

    usize size_;
    usize alignment_;
};
} // namespace qiven::memory
