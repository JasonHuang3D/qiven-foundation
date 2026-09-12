#include <qiven/memory/layout.hpp>
#include <qiven/types.hpp>

#include <limits>
#include <optional>
#include <type_traits>

namespace
{
using qiven::u32;
using qiven::usize;
using qiven::memory::Layout;

struct Ordinary
{
    u32 first;
    u32 second;
};

struct alignas(64) OverAligned
{
    char value[64];
};

static_assert(!std::is_default_constructible_v<Layout>);
static_assert(!std::is_constructible_v<Layout, usize, usize>);

constexpr Layout zero_layout = Layout::from_size_alignment(0, 1);
static_assert(zero_layout.size() == 0);
static_assert(zero_layout.alignment() == 1);

constexpr Layout ordinary_layout = Layout::from_size_alignment(96, 32);
static_assert(ordinary_layout.size() == 96);
static_assert(ordinary_layout.alignment() == 32);

constexpr Layout large_layout =
    Layout::from_size_alignment(std::numeric_limits<usize>::max(), 64);
static_assert(large_layout.size() == std::numeric_limits<usize>::max());
static_assert(large_layout.alignment() == 64);

constexpr Layout char_layout = Layout::of<char>();
static_assert(char_layout.size() == sizeof(char));
static_assert(char_layout.alignment() == alignof(char));

constexpr Layout object_layout = Layout::of<Ordinary>();
static_assert(object_layout.size() == sizeof(Ordinary));
static_assert(object_layout.alignment() == alignof(Ordinary));

constexpr Layout over_aligned_layout = Layout::of<OverAligned>();
static_assert(over_aligned_layout.size() == sizeof(OverAligned));
static_assert(over_aligned_layout.alignment() == alignof(OverAligned));

constexpr auto empty_array_layout = Layout::array<Ordinary>(0);
static_assert(empty_array_layout.has_value());
static_assert(empty_array_layout->size() == 0);
static_assert(empty_array_layout->alignment() == alignof(Ordinary));

constexpr auto single_array_layout = Layout::array<Ordinary>(1);
static_assert(single_array_layout.has_value());
static_assert(single_array_layout->size() == sizeof(Ordinary));
static_assert(single_array_layout->alignment() == alignof(Ordinary));

constexpr auto ordinary_array_layout = Layout::array<Ordinary>(7);
static_assert(ordinary_array_layout.has_value());
static_assert(ordinary_array_layout->size() == sizeof(Ordinary) * 7);
static_assert(ordinary_array_layout->alignment() == alignof(Ordinary));

constexpr auto over_aligned_array_layout = Layout::array<OverAligned>(3);
static_assert(over_aligned_array_layout.has_value());
static_assert(over_aligned_array_layout->size() == sizeof(OverAligned) * 3);
static_assert(over_aligned_array_layout->alignment() == alignof(OverAligned));

constexpr usize max_ordinary_count = std::numeric_limits<usize>::max() / sizeof(Ordinary);
constexpr auto max_array_layout    = Layout::array<Ordinary>(max_ordinary_count);
static_assert(max_array_layout.has_value());
static_assert(max_array_layout->size() == sizeof(Ordinary) * max_ordinary_count);
static_assert(max_array_layout->alignment() == alignof(Ordinary));
static_assert(!Layout::array<Ordinary>(max_ordinary_count + 1).has_value());
} // namespace

int main()
{
    const Layout layout = Layout::from_size_alignment(128, 16);
    if (layout.size() != 128 || layout.alignment() != 16)
        return 1;

    const auto array_layout = Layout::array<OverAligned>(4);
    if (!array_layout ||
        array_layout->size() != sizeof(OverAligned) * 4 ||
        array_layout->alignment() != alignof(OverAligned))
        return 2;

    if (Layout::array<Ordinary>(max_ordinary_count + 1).has_value())
        return 3;

    return 0;
}
