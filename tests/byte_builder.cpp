#include <qiven/byte_builder.hpp>
#include <qiven/endian.hpp>
#include <qiven/memory/allocator.hpp>
#include <qiven/memory/system_allocator.hpp>
#include <qiven/types.hpp>

#include <array>
#include <cstddef>
#include <cstdio>
#include <span>
#include <string>
#include <type_traits>
#include <utility>

namespace
{
using qiven::append_status;
using qiven::ByteBuilder;
using qiven::u16;
using qiven::u32;
using qiven::u64;
using qiven::usize;
using qiven::memory::AllocatorRef;

// Multi-slot bump allocator: ByteBuilder growth allocates a new block
// while the previous allocation is still live, so a single fixed storage
// (as in the allocator contract test) would alias. Slots are handed out
// from one arena; `fail` forces the explicit allocation-failed channel.
struct SlotAllocator
{
    alignas(16) std::byte storage[8192] {};
    usize offset    = 0;
    int allocations = 0;
    bool fail       = false;

    void* try_allocate(usize size, usize alignment) noexcept
    {
        ++allocations;
        if (fail)
            return nullptr;

        const usize mask = alignment - 1;
        const usize base = (offset + mask) & ~mask;
        if (base + size > sizeof(storage))
            return nullptr;

        offset = base + size;
        return storage + base;
    }

    void deallocate(void*, usize, usize) noexcept
    {
    }
};

static_assert(!std::is_copy_constructible_v<ByteBuilder>);
static_assert(!std::is_copy_assignable_v<ByteBuilder>);
static_assert(std::is_nothrow_move_constructible_v<ByteBuilder>);
static_assert(std::is_nothrow_move_assignable_v<ByteBuilder>);

constexpr std::array<std::byte, 4> bytes_of(u32 value)
{
    return {
        std::byte { static_cast<unsigned char>(value & 0xffu) },
        std::byte { static_cast<unsigned char>((value >> 8) & 0xffu) },
        std::byte { static_cast<unsigned char>((value >> 16) & 0xffu) },
        std::byte { static_cast<unsigned char>((value >> 24) & 0xffu) },
    };
}

template <usize N>
constexpr std::span<const std::byte> view(const std::array<std::byte, N>& bytes) noexcept
{
    return { bytes.data(), bytes.size() };
}
} // namespace

int main()
{
    const std::string eol(1, char(10));
    qiven::memory::SystemAllocator system;

    // ---- scalar puts against endian.hpp golden vectors ----
    {
        auto builder = ByteBuilder::try_create(AllocatorRef { system }, 64);
        if (!builder)
            return 1;

        if (builder->append_le_u32(0xDEADBEEFu) != append_status::ok)
            return 2;

        const auto golden = bytes_of(0xDEADBEEFu);
        if (builder->size() != 4 || builder->bytes()[0] != golden[0] || builder->bytes()[1] != golden[1] ||
            builder->bytes()[2] != golden[2] || builder->bytes()[3] != golden[3])
            return 3;

        // LE u64: reference bytes produced by endian.hpp itself
        std::array<std::byte, 8> reference {};
        if (!qiven::encode_le_u64(0x0102030405060708ull, reference))
            return 4;
        if (builder->append_le_u64(0x0102030405060708ull) != append_status::ok)
            return 5;
        for (usize index = 0; index < reference.size(); ++index)
            if (builder->bytes()[4 + index] != reference[index])
                return 6;

        // BE u32/u16 against endian.hpp references
        std::array<std::byte, 4> be_reference {};
        if (!qiven::encode_be_u32(0xAABBCCDDu, be_reference))
            return 7;
        std::array<std::byte, 2> be16_reference {};
        if (!qiven::encode_be_u16(u16 { 0xCAFE }, be16_reference))
            return 8;
        if (builder->append_be_u32(0xAABBCCDDu) != append_status::ok ||
            builder->append_be_u16(u16 { 0xCAFE }) != append_status::ok)
            return 9;
        for (usize index = 0; index < be_reference.size(); ++index)
            if (builder->bytes()[12 + index] != be_reference[index])
                return 10;
        for (usize index = 0; index < be16_reference.size(); ++index)
            if (builder->bytes()[16 + index] != be16_reference[index])
                return 11;

        // LE u16 against reference
        std::array<std::byte, 2> le16_reference {};
        if (!qiven::encode_le_u16(u16 { 0x1234 }, le16_reference))
            return 12;
        if (builder->append_le_u16(u16 { 0x1234 }) != append_status::ok)
            return 13;
        if (builder->bytes()[18] != le16_reference[0] || builder->bytes()[19] != le16_reference[1])
            return 14;

        // BE u64 against reference
        std::array<std::byte, 8> be64_reference {};
        if (!qiven::encode_be_u64(0xFEDCBA9876543210ull, be64_reference))
            return 15;
        if (builder->append_be_u64(0xFEDCBA9876543210ull) != append_status::ok)
            return 16;
        for (usize index = 0; index < be64_reference.size(); ++index)
            if (builder->bytes()[20 + index] != be64_reference[index])
                return 17;

        if (builder->size() != 28 || !builder->ok())
            return 18;

        std::printf("[ OK ] scalar puts match endian.hpp golden vectors%s", eol.c_str());
    }

    // ---- raw append, ownership, growth preserves data ----
    {
        auto builder = ByteBuilder::try_create(AllocatorRef { system }, 4096);
        if (!builder || !builder->empty() || builder->capacity() != 0)
            return 19;

        for (u32 round = 0; round < 512; ++round)
        {
            const auto pattern = bytes_of(round * 2654435761u);
            if (builder->append(view(pattern)) != append_status::ok)
                return 20;
        }

        if (builder->size() != 2048)
            return 21;

        for (u32 round = 0; round < 512; ++round)
        {
            const auto pattern = bytes_of(round * 2654435761u);
            for (usize index = 0; index < pattern.size(); ++index)
                if (builder->bytes()[usize { round } * 4 + index] != pattern[index])
                    return 22;
        }

        // growth actually happened (capacity grew past the first block)
        if (builder->capacity() < builder->size())
            return 23;

        std::printf("[ OK ] growth preserves accumulated data%s", eol.c_str());
    }

    // ---- geometric growth law: double, clamp to max ----
    {
        SlotAllocator allocator;
        auto builder = ByteBuilder::try_create(AllocatorRef { allocator }, 96);
        if (!builder)
            return 24;

        const std::array<std::byte, 3> three { std::byte { 1 }, std::byte { 2 }, std::byte { 3 } };
        const std::array<std::byte, 80> eighty {};

        if (builder->append(view(three)) != append_status::ok)
            return 25;
        if (builder->capacity() != 3 || allocator.allocations != 1) // first allocation = required (lazy)
            return 26;

        if (builder->append(view(three)) != append_status::ok)
            return 27;
        if (builder->capacity() != 6 || allocator.allocations != 2) // 2x double
            return 28;

        if (builder->append(view(eighty)) != append_status::ok)
            return 29;
        if (builder->capacity() != 86 || allocator.allocations != 3) // max(required, 2x)
            return 30;

        if (builder->append_le_u64(1) != append_status::ok)
            return 31;
        if (builder->capacity() != 96) // clamped to max_capacity
            return 32;
        if (builder->size() != 94)
            return 33;

        std::printf("[ OK ] geometric growth doubles then clamps to max%s", eol.c_str());
    }

    // ---- bounds: capacity_exceeded latches, later appends are no-ops ----
    {
        auto builder = ByteBuilder::try_create(AllocatorRef { system }, 8);
        if (!builder)
            return 34;

        if (builder->append_le_u64(1) != append_status::ok || builder->size() != 8)
            return 35;

        const std::array<std::byte, 1> one { std::byte { 0 } };
        if (builder->append(view(one)) != append_status::capacity_exceeded)
            return 36;
        if (builder->append_le_u16(2) != append_status::capacity_exceeded) // sticky, no size change
            return 37;
        if (builder->size() != 8 || builder->ok())
            return 38;

        builder->reset();
        if (!builder->ok() || builder->size() != 0 || builder->capacity() != 8) // capacity retained
            return 39;
        if (builder->append_le_u32(7) != append_status::ok || builder->size() != 4) // reusable
            return 40;

        // empty span append is always ok and consumes nothing
        builder->reset();
        if (builder->append(std::span<const std::byte> {}) != append_status::ok || builder->size() != 0)
            return 41;

        // zero max: every non-empty append fails capacity_exceeded
        auto zero = ByteBuilder::try_create(AllocatorRef { system }, 0);
        if (!zero)
            return 42;
        if (zero->append(view(one)) != append_status::capacity_exceeded)
            return 43;

        std::printf("[ OK ] bounds latch, reset reuses, zero-max degenerates%s", eol.c_str());
    }

    // ---- growth failure channel: allocation_failed via failing allocator ----
    {
        SlotAllocator allocator;
        auto builder = ByteBuilder::try_create(AllocatorRef { allocator }, 4096);
        if (!builder)
            return 44;

        if (builder->append_le_u32(0x11223344u) != append_status::ok)
            return 45;

        allocator.fail = true;
        if (builder->append(view(std::array<std::byte, 64> {})) != append_status::allocation_failed)
            return 46;
        if (builder->append_le_u16(9) != append_status::allocation_failed) // sticky
            return 47;
        if (builder->size() != 4) // failed appends consume nothing
            return 48;

        allocator.fail = false;
        builder->reset();
        if (builder->append(view(std::array<std::byte, 64> {})) != append_status::ok || builder->size() != 64)
            return 49;

        // allocator that can never serve: try_create still succeeds (lazy),
        // first append reports allocation_failed
        SlotAllocator exhausted;
        exhausted.fail = true;
        auto starved   = ByteBuilder::try_create(AllocatorRef { exhausted }, 16);
        if (!starved)
            return 50;
        if (starved->append_le_u32(1) != append_status::allocation_failed)
            return 51;

        std::printf("[ OK ] allocation failure is explicit and sticky%s", eol.c_str());
    }

    // ---- move semantics: bytes + limits transfer, source is reusable-empty ----
    {
        SlotAllocator allocator;
        auto builder = ByteBuilder::try_create(AllocatorRef { allocator }, 128);
        if (!builder)
            return 52;

        if (builder->append_le_u64(0x0F0E0D0C0B0A0908ull) != append_status::ok)
            return 53;

        ByteBuilder moved { std::move(*builder) };
        if (moved.size() != 8 || moved.max_capacity() != 128 || !moved.ok())
            return 54;
        if (moved.bytes()[0] != std::byte { 0x08 } || moved.bytes()[7] != std::byte { 0x0F })
            return 55;

        auto target = ByteBuilder::try_create(AllocatorRef { allocator }, 4);
        if (!target)
            return 56;
        if (target->append(view(std::array<std::byte, 2> {})) != append_status::ok)
            return 57;

        target = std::move(moved);
        if (target->size() != 8 || target->max_capacity() != 128)
            return 58;
        if (target->bytes()[7] != std::byte { 0x0F })
            return 59;

        // self move-assign is guarded and changes nothing
        target = std::move(*target);
        if (target->size() != 8)
            return 60;

        std::printf("[ OK ] move semantics transfer ownership%s", eol.c_str());
    }

    return 0;
}
