#include <qiven/byte_writer.hpp>
#include <qiven/types.hpp>

#include <array>
#include <cstddef>
#include <limits>
#include <span>
#include <type_traits>

namespace
{
using qiven::ByteWriter;
using qiven::usize;

constexpr bool verify_constexpr() noexcept
{
    std::array<std::byte, 4> bytes {
        std::byte { 0x10 },
        std::byte { 0x20 },
        std::byte { 0x30 },
        std::byte { 0x40 },
    };

    ByteWriter writer { std::span<std::byte> { bytes } };

    const auto first = writer.reserve(2);
    if (!first || first->size() != 2 || (*first)[0] != std::byte { 0x10 } || (*first)[1] != std::byte { 0x20 })
        return false;

    if (writer.remaining() != 2 || writer.empty())
        return false;

    const auto second = writer.reserve(2);
    if (!second || second->size() != 2 || (*second)[0] != std::byte { 0x30 } || (*second)[1] != std::byte { 0x40 })
        return false;

    if (!writer.empty() || writer.remaining() != 0)
        return false;

    const auto zero = writer.reserve(0);
    if (!zero || !zero->empty() || !writer.empty())
        return false;

    (*first)[0] = std::byte { 0xab };
    return bytes[0] == std::byte { 0xab } && !writer.reserve(1).has_value();
}

static_assert(verify_constexpr());
static_assert(std::is_copy_constructible_v<ByteWriter>);
static_assert(std::is_copy_assignable_v<ByteWriter>);
} // namespace

int main()
{
    std::array<std::byte, 6> bytes {
        std::byte { 0x01 },
        std::byte { 0x02 },
        std::byte { 0x03 },
        std::byte { 0x04 },
        std::byte { 0x05 },
        std::byte { 0x06 },
    };

    ByteWriter writer { std::span<std::byte> { bytes } };

    if (writer.remaining_bytes().data() != bytes.data() || writer.remaining() != bytes.size() || writer.empty())
        return 1;

    const auto prefix = writer.reserve(2);
    if (!prefix || prefix->data() != bytes.data() || prefix->size() != 2)
        return 2;

    if (writer.remaining_bytes().data() != bytes.data() + 2 || writer.remaining() != 4)
        return 3;

    const auto before_failure = writer.remaining_bytes();
    if (writer.reserve(std::numeric_limits<usize>::max()))
        return 4;

    if (writer.remaining_bytes().data() != before_failure.data() ||
        writer.remaining_bytes().size() != before_failure.size())
        return 5;

    const usize before_zero = writer.remaining();
    const auto zero         = writer.reserve(0);
    if (!zero || !zero->empty() || writer.remaining() != before_zero)
        return 6;

    ByteWriter trial = writer;

    const auto trial_bytes = trial.reserve(3);
    if (!trial_bytes || trial.remaining() != 1)
        return 7;

    if (writer.remaining() != 4)
        return 8;

    writer = trial;

    if (writer.remaining() != 1 || writer.remaining_bytes().data() != bytes.data() + 5)
        return 9;

    const auto last = writer.reserve(1);
    if (!last || last->data() != bytes.data() + 5 || !writer.empty())
        return 10;

    const auto empty_take = writer.reserve(0);
    if (!empty_take || !empty_take->empty() || !writer.empty())
        return 11;

    if (writer.reserve(1))
        return 12;

    ByteWriter empty { std::span<std::byte> {} };

    if (!empty.empty() || empty.remaining() != 0)
        return 13;

    if (!empty.reserve(0).has_value())
        return 14;

    if (empty.reserve(1))
        return 15;

    if (trial.remaining() != 1 || trial.remaining_bytes().data() != bytes.data() + 5)
        return 16;
    (*last)[0] = std::byte { 0xa5 };
    if (bytes[5] != std::byte { 0xa5 } || trial.remaining_bytes()[0] != std::byte { 0xa5 })
        return 17;
    (*prefix)[0] = std::byte { 0xb6 };
    if (bytes[0] != std::byte { 0xb6 })
        return 18;
    ByteWriter bounded { std::span<std::byte> { bytes } };
    if (bounded.reserve(bytes.size() + 1) || bounded.remaining() != bytes.size() ||
        bounded.remaining_bytes().data() != bytes.data())
        return 19;
    const auto whole = bounded.reserve(bytes.size());
    if (!whole || whole->data() != bytes.data() || whole->size() != bytes.size() || !bounded.empty())
        return 20;
    return 0;
}
