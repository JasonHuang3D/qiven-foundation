#include <qiven/alignment.hpp>
#include <qiven/memory/allocator.hpp>
#include <qiven/memory/linear_arena.hpp>

#include <cstddef>
#include <type_traits>

namespace
{
using qiven::uptr;
using qiven::usize;
using qiven::memory::AllocatorRef;
using qiven::memory::LinearArena;

[[nodiscard]] bool pointer_is_aligned(void* memory, usize alignment) noexcept
{
    return qiven::is_aligned(static_cast<usize>(reinterpret_cast<uptr>(memory)), alignment);
}

static_assert(std::is_constructible_v<AllocatorRef, LinearArena&>);
static_assert(!std::is_copy_constructible_v<LinearArena>);
static_assert(!std::is_copy_assignable_v<LinearArena>);
static_assert(!std::is_move_constructible_v<LinearArena>);
static_assert(!std::is_move_assignable_v<LinearArena>);
} // namespace

int main()
{
    LinearArena empty { nullptr, 0 };

    if (empty.capacity() != 0 || empty.used() != 0 || empty.remaining() != 0)
        return 1;

    if (empty.try_allocate(1, 1) != nullptr)
        return 2;

    alignas(64) std::byte storage[256] {};
    LinearArena arena { storage + 1, sizeof(storage) - 1 };

    if (arena.capacity() != sizeof(storage) - 1 || arena.used() != 0 || arena.remaining() != sizeof(storage) - 1)
        return 3;

    if (arena.try_allocate(0, 64) != nullptr)
        return 4;

    if (arena.used() != 0)
        return 5;

    void* const first = arena.try_allocate(7, 16);
    if (first != storage + 16)
        return 6;

    if (!pointer_is_aligned(first, 16))
        return 7;

    if (arena.used() != 22 || arena.remaining() != arena.capacity() - 22)
        return 8;

    void* const second = arena.try_allocate(5, 8);
    if (second != storage + 24)
        return 9;

    if (!pointer_is_aligned(second, 8))
        return 10;

    const usize used_before_deallocate = arena.used();
    arena.deallocate(first, 7, 16);

    if (arena.used() != used_before_deallocate)
        return 11;

    arena.reset();

    if (arena.used() != 0 || arena.remaining() != arena.capacity())
        return 12;

    if (arena.try_allocate(7, 16) != first)
        return 13;

    alignas(16) std::byte small_storage[32] {};
    LinearArena small { small_storage, sizeof(small_storage) };

    void* const large = small.try_allocate(24, 16);
    if (large == nullptr)
        return 14;

    const usize used_before_failure = small.used();
    if (small.try_allocate(16, 16) != nullptr)
        return 15;

    if (small.used() != used_before_failure)
        return 16;

    alignas(128) std::byte erased_storage[256] {};
    LinearArena erased_arena { erased_storage, sizeof(erased_storage) };
    AllocatorRef allocator { erased_arena };

    void* const erased = allocator.try_allocate(73, 128);
    if (erased == nullptr || !pointer_is_aligned(erased, 128))
        return 17;

    const usize erased_used = erased_arena.used();
    allocator.deallocate(erased, 73, 128);

    if (erased_arena.used() != erased_used)
        return 18;

    return 0;
}
