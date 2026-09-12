#include <qiven/alignment.hpp>

#include <limits>
#include <optional>

namespace
{
using qiven::usize;

static_assert(qiven::is_aligned(0, 1));
static_assert(qiven::is_aligned(32, 16));
static_assert(!qiven::is_aligned(33, 16));
static_assert(qiven::align_down(31, 16) == 16);
static_assert(qiven::align_up(16, 16) == 16);
static_assert(qiven::align_up(17, 16) == 32);
static_assert(qiven::checked_align_up(0, 1) == std::optional<usize> { 0 });
static_assert(qiven::checked_align_up(37, 1) == std::optional<usize> { 37 });
static_assert(qiven::checked_align_up(32, 16) == std::optional<usize> { 32 });
static_assert(qiven::checked_align_up(33, 16) == std::optional<usize> { 48 });
static_assert(qiven::checked_align_up(63, 64) == std::optional<usize> { 64 });
static_assert(qiven::checked_align_up(std::numeric_limits<usize>::max(), 1) ==
              std::optional<usize> { std::numeric_limits<usize>::max() });
static_assert(!qiven::checked_align_up(std::numeric_limits<usize>::max(), 2).has_value());
} // namespace

int main()
{
    constexpr usize alignment         = 8;
    constexpr usize max_value         = std::numeric_limits<usize>::max();
    constexpr usize max_aligned_value = max_value & ~usize { alignment - 1 };

    if (!qiven::is_aligned(64, alignment))
        return 1;

    if (qiven::is_aligned(65, alignment))
        return 2;

    if (qiven::align_down(15, alignment) != 8)
        return 3;

    if (qiven::align_up(15, alignment) != 16)
        return 4;

    if (qiven::align_up(max_aligned_value, alignment) != max_aligned_value)
        return 5;

    if (qiven::align_down(max_value, alignment) != max_aligned_value)
        return 6;

    if (qiven::checked_align_up(0, alignment) != std::optional<usize> { 0 })
        return 7;

    if (qiven::checked_align_up(24, alignment) != std::optional<usize> { 24 })
        return 8;

    if (qiven::checked_align_up(25, alignment) != std::optional<usize> { 32 })
        return 9;

    if (qiven::checked_align_up(max_aligned_value, alignment) !=
        std::optional<usize> { max_aligned_value })
        return 10;

    if (qiven::checked_align_up(max_aligned_value - 1, alignment) !=
        std::optional<usize> { max_aligned_value })
        return 11;

    if (qiven::checked_align_up(max_aligned_value + 1, alignment).has_value())
        return 12;

    if (qiven::checked_align_up(max_value - 1, alignment).has_value())
        return 13;

    if (qiven::checked_align_up(max_value, alignment).has_value())
        return 14;

    return 0;
}
