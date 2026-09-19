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

    // ---- adversarial phase (2026-09-20): failure paths must leave the
    // cursor unchanged and never trap on extreme counts ----

    constexpr std::array<std::byte, 4> payload {
        std::byte { 0xA1 }, std::byte { 0xB2 }, std::byte { 0xC3 }, std::byte { 0xD4 }
    };

    // take(0) succeeds on empty and non-empty cursors (zero is always valid)
    {
        qiven::ByteCursor empty { std::span<const std::byte>() };
        if (!empty.take(0).has_value())
            return 30;
        if (!empty.empty())
            return 31;
        qiven::ByteCursor some { std::span<const std::byte>(payload.data(), payload.size()) };
        if (!some.take(0).has_value())
            return 32;
        if (some.remaining() != payload.size())
            return 33; // take(0) consumes nothing
    }

    // a failed take must NOT consume: retrying with a valid count still works
    {
        qiven::ByteCursor cursor { std::span<const std::byte>(payload.data(), payload.size()) };
        if (cursor.take(payload.size() + 1).has_value())
            return 34;
        if (cursor.remaining() != payload.size())
            return 35;
        const auto retry = cursor.take(1);
        if (!retry.has_value() || retry->front() != payload[0])
            return 36;
    }

    // extreme count: SIZE_MAX compares safely and refuses without overflow
    {
        qiven::ByteCursor cursor { std::span<const std::byte>(payload.data(), payload.size()) };
        if (cursor.take(std::numeric_limits<usize>::max()).has_value())
            return 37;
        if (cursor.remaining() != payload.size())
            return 38;
    }

    // exact consumption lands on empty; one-past is refused
    {
        qiven::ByteCursor cursor { std::span<const std::byte>(payload.data(), payload.size()) };
        if (!cursor.take(payload.size()).has_value())
            return 39;
        if (!cursor.empty())
            return 40;
        if (cursor.take(1).has_value())
            return 41;
    }

    // data integrity: taken spans view the original buffer bytes in order
    {
        qiven::ByteCursor cursor { std::span<const std::byte>(payload.data(), payload.size()) };
        for (std::size_t i = 0; i < payload.size(); ++i)
        {
            const auto byte_span = cursor.take(1);
            if (!byte_span.has_value())
                return 42;
            if (byte_span->front() != payload[i])
                return 43;
        }
        if (!cursor.empty())
            return 44;
    }

    // constexpr: full consume sequences are evaluatable at compile time
    {
        constexpr bool consumes_all = [] {
            constexpr std::array<std::byte, 3> bytes { std::byte { 1 }, std::byte { 2 }, std::byte { 3 } };
            qiven::ByteCursor cursor { std::span<const std::byte>(bytes) };
            return cursor.take(3).has_value() && cursor.empty();
        }();
        static_assert(consumes_all);

        constexpr bool zero_take_ok = [] {
            qiven::ByteCursor cursor { std::span<const std::byte>() };
            return cursor.take(0).has_value() && cursor.empty();
        }();
        static_assert(zero_take_ok);

        constexpr bool failed_take_preserves = [] {
            constexpr std::array<std::byte, 2> bytes { std::byte { 7 }, std::byte { 9 } };
            qiven::ByteCursor cursor { std::span<const std::byte>(bytes) };
            if (cursor.take(3).has_value())
                return false;
            return cursor.remaining() == 2 && cursor.take(2).has_value();
        }();
        static_assert(failed_take_preserves);
    }

    return 0;
}
