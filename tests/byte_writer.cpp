#include <qiven/byte_cursor.hpp>
#include <qiven/byte_writer.hpp>
#include <qiven/types.hpp>

#include <array>
#include <cstddef>
#include <limits>
#include <span>
#include <string>
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
    // ---- adversarial phase (2026-09-20): failed reserves must not consume
    // capacity, extreme counts must not trap, and zero is always valid ----

    const std::string eol(1, char(10));

    // reserve(0) succeeds on empty and non-empty writers (zero is always valid)
    {
        std::array<std::byte, 4> buffer {};
        qiven::ByteWriter empty { std::span<std::byte>(buffer.data(), 0) };
        if (!empty.reserve(0).has_value())
            return 21;
        if (!empty.empty())
            return 22;
        qiven::ByteWriter some { std::span<std::byte>(buffer) };
        if (!some.reserve(0).has_value())
            return 23;
        if (some.remaining() != buffer.size())
            return 24; // reserve(0) consumes nothing
    }

    // a failed reserve must NOT consume: retrying with a valid count works
    {
        std::array<std::byte, 4> buffer {};
        qiven::ByteWriter writer { std::span<std::byte>(buffer) };
        if (writer.reserve(buffer.size() + 1).has_value())
            return 25;
        if (writer.remaining() != buffer.size())
            return 26;
        const auto retry = writer.reserve(1);
        if (!retry.has_value() || retry->size() != 1)
            return 27;
        retry->front() = std::byte { 0x5A };
        if (buffer.front() != std::byte { 0x5A })
            return 28; // the reserved span writes through to the buffer
    }

    // extreme count: SIZE_MAX compares safely and refuses without overflow
    {
        std::array<std::byte, 4> buffer {};
        qiven::ByteWriter writer { std::span<std::byte>(buffer) };
        if (writer.reserve(std::numeric_limits<usize>::max()).has_value())
            return 29;
        if (writer.remaining() != buffer.size())
            return 30;
    }

    // write-through ordering: one-at-a-time reserves preserve content order,
    // and a reader over the same buffer sees exactly what was written
    {
        std::array<std::byte, 5> buffer {};
        qiven::ByteWriter writer { std::span<std::byte>(buffer) };
        for (std::size_t i = 0; i < buffer.size(); ++i)
        {
            const auto slot = writer.reserve(1);
            if (!slot.has_value())
                return 31;
            slot->front() = std::byte { static_cast<unsigned char>(0x10 * (i + 1)) };
        }
        if (!writer.empty())
            return 32;
        if (writer.reserve(1).has_value())
            return 33;
        qiven::ByteCursor readback { std::span<const std::byte>(buffer) };
        for (std::size_t i = 0; i < buffer.size(); ++i)
        {
            const auto byte_span = readback.take(1);
            if (!byte_span.has_value())
                return 34;
            if (byte_span->front() != std::byte { static_cast<unsigned char>(0x10 * (i + 1)) })
                return 35;
        }
    }

    // interleaved zero and non-zero reserves on one writer
    {
        std::array<std::byte, 3> buffer {};
        qiven::ByteWriter writer { std::span<std::byte>(buffer) };
        if (!writer.reserve(0).has_value())
            return 36;
        const auto two = writer.reserve(2);
        if (!two.has_value())
            return 37;
        two->front() = std::byte { 1 };
        if (!writer.reserve(0).has_value())
            return 38;
        const auto last = writer.reserve(1);
        if (!last.has_value())
            return 39;
        last->front() = std::byte { 2 };
        if (writer.reserve(1).has_value())
            return 40; // exactly full now
        if (buffer[0] != std::byte { 1 } || buffer[1] != std::byte { 0 } || buffer[2] != std::byte { 2 })
            return 41; // the reserved spans wrote exactly where the test wrote them
    }

    // constexpr: fill and exhaust sequences are evaluatable at compile time
    {
        constexpr bool fills_and_exhausts = [] {
            std::array<std::byte, 3> buffer {};
            qiven::ByteWriter writer { std::span<std::byte>(buffer) };
            for (std::size_t i = 0; i < buffer.size(); ++i)
            {
                const auto slot = writer.reserve(1);
                if (!slot.has_value())
                    return false;
                slot->front() = std::byte { static_cast<unsigned char>(i) };
            }
            return writer.empty() && !writer.reserve(1).has_value();
        }();
        static_assert(fills_and_exhausts);

        constexpr bool zero_reserve_on_empty_ok = [] {
            std::array<std::byte, 2> buffer {};
            qiven::ByteWriter writer { std::span<std::byte>(buffer.data(), 0) };
            return writer.reserve(0).has_value() && writer.empty();
        }();
        static_assert(zero_reserve_on_empty_ok);
    }

    std::printf("[ OK ] adversarial writer cases%s", eol.c_str());
    return 0;
}
