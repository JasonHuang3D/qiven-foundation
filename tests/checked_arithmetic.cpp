#include <qiven/checked_arithmetic.hpp>
#include <qiven/types.hpp>

#include <limits>
#include <optional>

namespace
{
using qiven::u8;
using qiven::usize;

constexpr usize usize_max = std::numeric_limits<usize>::max();

static_assert(qiven::checked_add<usize>(0, 0) == std::optional<usize> { 0 });
static_assert(qiven::checked_add<usize>(usize_max, 0) == std::optional<usize> { usize_max });
static_assert(!qiven::checked_add<usize>(usize_max, 1).has_value());

static_assert(qiven::checked_sub<usize>(0, 0) == std::optional<usize> { 0 });
static_assert(qiven::checked_sub<usize>(usize_max, usize_max) == std::optional<usize> { 0 });
static_assert(!qiven::checked_sub<usize>(0, 1).has_value());

static_assert(qiven::checked_mul<usize>(0, usize_max) == std::optional<usize> { 0 });
static_assert(qiven::checked_mul<usize>(usize_max, 1) == std::optional<usize> { usize_max });
static_assert(!qiven::checked_mul<usize>(usize_max, 2).has_value());

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
    if (!verify_u8_exhaustively())
        return 1;

    return 0;
}
