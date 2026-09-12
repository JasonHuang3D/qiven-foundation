#include <qiven/checked_arithmetic.hpp>
#include <qiven/types.hpp>

#include <limits>
#include <optional>

namespace
{
using qiven::i16;
using qiven::i32;
using qiven::i64;
using qiven::i8;
using qiven::u8;
using qiven::usize;

constexpr usize usize_max = std::numeric_limits<usize>::max();
constexpr i8 i8_min       = std::numeric_limits<i8>::min();
constexpr i8 i8_max       = std::numeric_limits<i8>::max();
constexpr i16 i16_min     = std::numeric_limits<i16>::min();
constexpr i16 i16_max     = std::numeric_limits<i16>::max();
constexpr i32 i32_min     = std::numeric_limits<i32>::min();
constexpr i32 i32_max     = std::numeric_limits<i32>::max();
constexpr i64 i64_min     = std::numeric_limits<i64>::min();
constexpr i64 i64_max     = std::numeric_limits<i64>::max();

static_assert(qiven::checked_add<usize>(0, 0) == std::optional<usize> { 0 });
static_assert(qiven::checked_add<usize>(usize_max, 0) == std::optional<usize> { usize_max });
static_assert(!qiven::checked_add<usize>(usize_max, 1).has_value());

static_assert(qiven::checked_sub<usize>(0, 0) == std::optional<usize> { 0 });
static_assert(qiven::checked_sub<usize>(usize_max, usize_max) == std::optional<usize> { 0 });
static_assert(!qiven::checked_sub<usize>(0, 1).has_value());

static_assert(qiven::checked_mul<usize>(0, usize_max) == std::optional<usize> { 0 });
static_assert(qiven::checked_mul<usize>(usize_max, 1) == std::optional<usize> { usize_max });
static_assert(!qiven::checked_mul<usize>(usize_max, 2).has_value());

static_assert(qiven::checked_add<i8>(20, 30) == std::optional<i8> { static_cast<i8>(50) });
static_assert(qiven::checked_add<i8>(20, -30) == std::optional<i8> { static_cast<i8>(-10) });
static_assert(qiven::checked_add<i8>(-20, 30) == std::optional<i8> { static_cast<i8>(10) });
static_assert(qiven::checked_add<i8>(-20, -30) == std::optional<i8> { static_cast<i8>(-50) });
static_assert(qiven::checked_add<i8>(42, 0) == std::optional<i8> { static_cast<i8>(42) });
static_assert(qiven::checked_add<i8>(i8_max, 0) == std::optional<i8> { i8_max });
static_assert(qiven::checked_add<i8>(i8_min, 0) == std::optional<i8> { i8_min });
static_assert(!qiven::checked_add<i8>(i8_max, 1).has_value());
static_assert(!qiven::checked_add<i8>(i8_min, -1).has_value());
static_assert(qiven::checked_add<i8>(i8_max - 1, 1) == std::optional<i8> { i8_max });
static_assert(qiven::checked_add<i8>(i8_min + 1, -1) == std::optional<i8> { i8_min });
static_assert(!qiven::checked_add<i16>(i16_max, 1).has_value());
static_assert(!qiven::checked_add<i32>(i32_min, -1).has_value());
static_assert(qiven::checked_add<i64>(i64_max - 1, 1) == std::optional<i64> { i64_max });

static_assert(qiven::checked_sub<i8>(30, 20) == std::optional<i8> { static_cast<i8>(10) });
static_assert(qiven::checked_sub<i8>(20, -30) == std::optional<i8> { static_cast<i8>(50) });
static_assert(qiven::checked_sub<i8>(-20, 30) == std::optional<i8> { static_cast<i8>(-50) });
static_assert(qiven::checked_sub<i8>(-20, -30) == std::optional<i8> { static_cast<i8>(10) });
static_assert(qiven::checked_sub<i8>(42, 0) == std::optional<i8> { static_cast<i8>(42) });
static_assert(qiven::checked_sub<i8>(i8_max, 0) == std::optional<i8> { i8_max });
static_assert(qiven::checked_sub<i8>(i8_min, 0) == std::optional<i8> { i8_min });
static_assert(!qiven::checked_sub<i8>(i8_max, -1).has_value());
static_assert(!qiven::checked_sub<i8>(i8_min, 1).has_value());
static_assert(qiven::checked_sub<i8>(i8_max - 1, -1) == std::optional<i8> { i8_max });
static_assert(qiven::checked_sub<i8>(i8_min + 1, 1) == std::optional<i8> { i8_min });
static_assert(!qiven::checked_sub<i16>(i16_min, 1).has_value());
static_assert(!qiven::checked_sub<i32>(i32_max, -1).has_value());
static_assert(qiven::checked_sub<i64>(i64_min + 1, 1) == std::optional<i64> { i64_min });

[[nodiscard]] bool verify_i8_add_sub_exhaustively() noexcept
{
    for (int lhs = i8_min; lhs <= i8_max; ++lhs)
    {
        for (int rhs = i8_min; rhs <= i8_max; ++rhs)
        {
            const i8 lhs_value = static_cast<i8>(lhs);
            const i8 rhs_value = static_cast<i8>(rhs);

            const auto add       = qiven::checked_add(lhs_value, rhs_value);
            const int add_value  = lhs + rhs;
            const bool add_valid = add_value >= i8_min && add_value <= i8_max;

            if (add.has_value() != add_valid)
                return false;
            if (add_valid && static_cast<int>(*add) != add_value)
                return false;

            const auto sub       = qiven::checked_sub(lhs_value, rhs_value);
            const int sub_value  = lhs - rhs;
            const bool sub_valid = sub_value >= i8_min && sub_value <= i8_max;

            if (sub.has_value() != sub_valid)
                return false;
            if (sub_valid && static_cast<int>(*sub) != sub_value)
                return false;
        }
    }

    return true;
}

[[nodiscard]] bool verify_u8_exhaustively() noexcept
{
    constexpr unsigned int u8_max = std::numeric_limits<u8>::max();

    for (unsigned int lhs = 0; lhs <= u8_max; ++lhs)
    {
        for (unsigned int rhs = 0; rhs <= u8_max; ++rhs)
        {
            const u8 lhs_value = static_cast<u8>(lhs);
            const u8 rhs_value = static_cast<u8>(rhs);

            const auto add               = qiven::checked_add(lhs_value, rhs_value);
            const unsigned int add_value = lhs + rhs;
            const bool add_valid         = add_value <= u8_max;

            if (add.has_value() != add_valid)
                return false;
            if (add_valid && static_cast<unsigned int>(*add) != add_value)
                return false;

            const auto sub       = qiven::checked_sub(lhs_value, rhs_value);
            const bool sub_valid = rhs <= lhs;

            if (sub.has_value() != sub_valid)
                return false;
            if (sub_valid && static_cast<unsigned int>(*sub) != lhs - rhs)
                return false;

            const auto mul               = qiven::checked_mul(lhs_value, rhs_value);
            const unsigned int mul_value = lhs * rhs;
            const bool mul_valid         = mul_value <= u8_max;

            if (mul.has_value() != mul_valid)
                return false;
            if (mul_valid && static_cast<unsigned int>(*mul) != mul_value)
                return false;
        }
    }

    return true;
}
} // namespace

int main()
{
    if (!verify_i8_add_sub_exhaustively())
        return 1;

    if (!verify_u8_exhaustively())
        return 1;

    return 0;
}
