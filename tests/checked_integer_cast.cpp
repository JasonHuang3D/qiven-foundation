#include <qiven/checked_integer_cast.hpp>
#include <qiven/types.hpp>

#include <concepts>
#include <limits>
#include <optional>

namespace
{
using qiven::i16;
using qiven::i32;
using qiven::i64;
using qiven::i8;
using qiven::u16;
using qiven::u32;
using qiven::u64;
using qiven::u8;
using qiven::usize;

template <typename To, typename From>
concept can_checked_integer_cast = requires(From value) {
    qiven::checked_integer_cast<To>(value);
};

constexpr bool verify_constexpr() noexcept
{
    if (qiven::checked_integer_cast<u32>(i32 { 0 }) != std::optional<u32> { u32 { 0 } })
        return false;

    if (qiven::checked_integer_cast<u32>(i32 { -1 }))
        return false;

    if (qiven::checked_integer_cast<i32>(u32 { 0 }) != std::optional<i32> { i32 { 0 } })
        return false;

    constexpr u32 i32_max_as_u32 = static_cast<u32>(std::numeric_limits<i32>::max());

    if (qiven::checked_integer_cast<i32>(i32_max_as_u32) !=
        std::optional<i32> { std::numeric_limits<i32>::max() })
        return false;

    if (qiven::checked_integer_cast<i32>(i32_max_as_u32 + u32 { 1 }))
        return false;

    if (qiven::checked_integer_cast<u64>(std::numeric_limits<u32>::max()) !=
        std::optional<u64> { u64 { std::numeric_limits<u32>::max() } })
        return false;

    if (qiven::checked_integer_cast<u32>(std::numeric_limits<u64>::max()))
        return false;

    if (qiven::checked_integer_cast<i64>(std::numeric_limits<i32>::min()) !=
        std::optional<i64> { i64 { std::numeric_limits<i32>::min() } })
        return false;

    if (qiven::checked_integer_cast<i32>(std::numeric_limits<i64>::min()))
        return false;

    return true;
}

static_assert(verify_constexpr());

static_assert(can_checked_integer_cast<u32, i32>);
static_assert(can_checked_integer_cast<usize, u64>);
static_assert(!can_checked_integer_cast<bool, u32>);
static_assert(!can_checked_integer_cast<u32, bool>);
static_assert(!can_checked_integer_cast<char, u32>);
static_assert(!can_checked_integer_cast<u32, char>);
} // namespace

int main()
{
    if (qiven::checked_integer_cast<i8>(u8 { 127 }) != std::optional<i8> { i8 { 127 } })
        return 1;

    if (qiven::checked_integer_cast<i8>(u8 { 128 }))
        return 2;

    if (qiven::checked_integer_cast<u8>(i8 { -1 }))
        return 3;

    if (qiven::checked_integer_cast<u8>(std::numeric_limits<i8>::max()) !=
        std::optional<u8> { u8 { 127 } })
        return 4;

    if (qiven::checked_integer_cast<i16>(std::numeric_limits<i8>::min()) !=
        std::optional<i16> { i16 { std::numeric_limits<i8>::min() } })
        return 5;

    if (qiven::checked_integer_cast<u16>(std::numeric_limits<u8>::max()) !=
        std::optional<u16> { u16 { std::numeric_limits<u8>::max() } })
        return 6;

    const auto local_size = qiven::checked_integer_cast<usize>(u64 { 42 });
    if (!local_size || *local_size != usize { 42 })
        return 7;

    return 0;
}
