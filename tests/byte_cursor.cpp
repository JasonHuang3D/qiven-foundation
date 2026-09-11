#include <qiven/byte_cursor.hpp>
#include <qiven/types.hpp>

#include <array>
#include <cstddef>
#include <limits>
#include <span>
#include <type_traits>

namespace
{
using qiven::ByteCursor;
using qiven::usize;

constexpr bool verify_constexpr() noexcept
{
    constexpr std::array<std::byte, 4> bytes {
        std::byte { 0x10 },
        std::byte { 0x20 },
        std::byte { 0x30 },
        std::byte { 0x40 },
    };

    ByteCursor cursor { std::span<const std::byte> { bytes } };

    const auto first = cursor.take(2);
    if (!first || first->size() != 2 || (*first)[0] != std::byte { 0x10 } || (*first)[1] != std::byte { 0x20 })
        return false;

    if (cursor.remaining() != 2 || cursor.empty())
        return false;

    const auto second = cursor.take(2);
    if (!second || second->size() != 2 || (*second)[0] != std::byte { 0x30 } || (*second)[1] != std::byte { 0x40 })
        return false;

    if (!cursor.empty() || cursor.remaining() != 0)
        return false;

    const auto zero = cursor.take(0);
    if (!zero || !zero->empty() || !cursor.empty())
        return false;

    return !cursor.take(1).has_value();
}

static_assert(verify_constexpr());
static_assert(std::is_copy_constructible_v<ByteCursor>);
static_assert(std::is_copy_assignable_v<ByteCursor>);
} // namespace

int main()
{
    const std::array<std::byte, 6> bytes {
        std::byte { 0x01 },
        std::byte { 0x02 },
        std::byte { 0x03 },
        std::byte { 0x04 },
        std::byte { 0x05 },
        std::byte { 0x06 },
    };

    ByteCursor cursor { std::span<const std::byte> { bytes } };

    if (cursor.remaining_bytes().data() != bytes.data() || cursor.remaining() != bytes.size() || cursor.empty())
        return 1;

    const auto prefix = cursor.take(2);
    if (!prefix || prefix->data() != bytes.data() || prefix->size() != 2)
        return 2;

    if (cursor.remaining_bytes().data() != bytes.data() + 2 || cursor.remaining() != 4)
        return 3;

    const auto before_failure = cursor.remaining_bytes();
    if (cursor.take(std::numeric_limits<usize>::max()))
        return 4;

    if (cursor.remaining_bytes().data() != before_failure.data() ||
        cursor.remaining_bytes().size() != before_failure.size())
        return 5;

    const usize before_zero = cursor.remaining();
    const auto zero         = cursor.take(0);
    if (!zero || !zero->empty() || cursor.remaining() != before_zero)
        return 6;

    ByteCursor trial = cursor;

    const auto trial_bytes = trial.take(3);
    if (!trial_bytes || trial.remaining() != 1)
        return 7;

    if (cursor.remaining() != 4)
        return 8;

    cursor = trial;

    if (cursor.remaining() != 1 || cursor.remaining_bytes().data() != bytes.data() + 5)
        return 9;

    const auto last = cursor.take(1);
    if (!last || last->data() != bytes.data() + 5 || !cursor.empty())
        return 10;

    const auto empty_take = cursor.take(0);
    if (!empty_take || !empty_take->empty() || !cursor.empty())
        return 11;

    if (cursor.take(1))
        return 12;

    ByteCursor empty { std::span<const std::byte> {} };

    if (!empty.empty() || empty.remaining() != 0)
        return 13;

    if (!empty.take(0).has_value())
        return 14;

    if (empty.take(1))
        return 15;

    return 0;
}
