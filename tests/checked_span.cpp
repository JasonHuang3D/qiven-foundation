#include <qiven/checked_span.hpp>
#include <qiven/types.hpp>

#include <array>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <type_traits>

namespace
{
using qiven::usize;

constexpr bool verify_constexpr() noexcept
{
    std::array<int, 4> values { 10, 20, 30, 40 };

    const auto middle = qiven::checked_subspan(std::span<int, 4> { values }, 1, 2);
    if (!middle || middle->size() != 2 || (*middle)[0] != 20 || (*middle)[1] != 30)
        return false;

    const auto end = qiven::checked_subspan(std::span<int, 4> { values }, 4, 0);
    if (!end || !end->empty())
        return false;

    if (qiven::checked_subspan(std::span<int, 4> { values }, 5, 0))
        return false;

    if (qiven::checked_subspan(std::span<int, 4> { values }, 3, 2))
        return false;

    return true;
}

static_assert(verify_constexpr());

using MutableResult = decltype(qiven::checked_subspan(std::declval<std::span<int, 4>>(), usize {}, usize {}));
using ConstResult   = decltype(qiven::checked_subspan(std::declval<std::span<const int>>(), usize {}, usize {}));

static_assert(std::same_as<MutableResult, std::optional<std::span<int>>>);
static_assert(std::same_as<ConstResult, std::optional<std::span<const int>>>);
} // namespace

int main()
{
    std::array<int, 5> values { 1, 2, 3, 4, 5 };
    std::span<int> dynamic { values };

    auto middle = qiven::checked_subspan(dynamic, 1, 3);
    if (!middle || middle->data() != values.data() + 1 || middle->size() != 3)
        return 1;

    (*middle)[1] = 30;
    if (values[2] != 30)
        return 2;

    const std::span<const int> read_only { values };

    const auto tail = qiven::checked_subspan(read_only, 3, 2);
    if (!tail || tail->data() != values.data() + 3 || tail->size() != 2)
        return 3;

    const auto whole = qiven::checked_subspan(read_only, 0, read_only.size());
    if (!whole || whole->data() != read_only.data() || whole->size() != read_only.size())
        return 4;

    const auto empty_at_end = qiven::checked_subspan(read_only, read_only.size(), 0);
    if (!empty_at_end || !empty_at_end->empty())
        return 5;

    if (qiven::checked_subspan(read_only, read_only.size() + 1, 0))
        return 6;

    if (qiven::checked_subspan(read_only, read_only.size() - 1, 2))
        return 7;

    constexpr usize usize_max = std::numeric_limits<usize>::max();

    if (qiven::checked_subspan(read_only, usize_max, 1))
        return 8;

    if (qiven::checked_subspan(read_only, 1, usize_max))
        return 9;

    std::span<const int> empty;
    const auto empty_slice = qiven::checked_subspan(empty, 0, 0);
    if (!empty_slice || !empty_slice->empty())
        return 10;

    return 0;
}
