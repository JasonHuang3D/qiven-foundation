#include <qiven/hashing.hpp>

#include <qiven/contracts.hpp>

#include <cstddef>
#include <limits>
#include <string>

namespace
{
using qiven::u64;

constexpr std::byte operator""_b(unsigned long long value)
{
    return static_cast<std::byte>(value);
}

// FNV-1a 64 reference values: the offset basis is the hash of the empty
// input, and one step is hand-computed from the prime 1099511628211.
constexpr u64 offset_basis = 1469598103934665603ULL;
constexpr u64 prime        = 1099511628211ULL;

constexpr u64 reference_step(u64 seed, std::byte byte)
{
    return (seed ^ static_cast<u64>(byte)) * prime;
}
} // namespace

int main()
{
    // the empty input hashes to the offset basis
    constexpr u64 empty_hash = qiven::fnv1a64(static_cast<const std::byte*>(nullptr), 0);
    static_assert(empty_hash == offset_basis);

    // single-byte steps follow the reference recurrence
    const std::byte one { 0x01 };
    QIVEN_VERIFY(qiven::fnv1a64(&one, 1) == reference_step(offset_basis, one));
    QIVEN_VERIFY(qiven::fnv1a64(&one, 1, 42) == reference_step(42, one));

    // seeded calls compose: chaining equals hashing the concatenation
    {
        const std::byte first[] { 0x0A_b, 0x0B_b, 0x0C_b };
        const std::byte second[] { 0x1D_b, 0x1E_b };
        const u64 chained = qiven::fnv1a64(second, 2, qiven::fnv1a64(first, 3));
        std::byte both[] { 0x0A_b, 0x0B_b, 0x0C_b, 0x1D_b, 0x1E_b };
        QIVEN_VERIFY(chained == qiven::fnv1a64(both, sizeof both));
        QIVEN_VERIFY(qiven::fnv1a64_chain(qiven::fnv1a64(first, 3), "", second) == chained);
    }

    // span and string_view overloads agree on the same bytes
    {
        const std::string text = "qiven";
        const u64 from_text    = qiven::fnv1a64(text);
        const u64 from_bytes   = qiven::fnv1a64(
            std::span<const std::byte>(reinterpret_cast<const std::byte*>(text.data()), text.size()));
        QIVEN_VERIFY(from_text == from_bytes);
        QIVEN_VERIFY(from_text != offset_basis);
    }

    // different content hashes differently; identical content is stable
    {
        const std::byte a[] { 0x00_b };
        const std::byte b[] { 0x01_b };
        QIVEN_VERIFY(qiven::fnv1a64(a, 1) != qiven::fnv1a64(b, 1));
        QIVEN_VERIFY(qiven::fnv1a64(a, 1) == qiven::fnv1a64(a, 1));
    }

    // hex encoding: fixed width, lowercase, round-trip of the extremes
    {
        char text[16];
        qiven::to_hex_u64(0, text);
        QIVEN_VERIFY(std::string(text, 16) == "0000000000000000");
        qiven::to_hex_u64(std::numeric_limits<u64>::max(), text);
        QIVEN_VERIFY(std::string(text, 16) == "ffffffffffffffff");
        qiven::to_hex_u64(0x0123456789ABCDEFull, text);
        QIVEN_VERIFY(std::string(text, 16) == "0123456789abcdef");
        QIVEN_VERIFY(qiven::to_hex_u64(0xDEADBEEFCAFEBABEull) == "deadbeefcafebabe");
    }

    std::printf("[ OK ] hashing\n");
    return 0;
}
