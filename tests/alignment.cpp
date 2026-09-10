#include <qiven/alignment.hpp>

#include <limits>

namespace
{
using qiven::usize;

static_assert(qiven::is_aligned(0, 1));
static_assert(qiven::is_aligned(32, 16));
static_assert(!qiven::is_aligned(33, 16));
static_assert(qiven::align_down(31, 16) == 16);
static_assert(qiven::align_up(16, 16) == 16);
static_assert(qiven::align_up(17, 16) == 32);
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

    return 0;
}
