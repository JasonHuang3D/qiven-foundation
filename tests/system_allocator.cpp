#include <qiven/alignment.hpp>
#include <qiven/memory/allocator.hpp>
#include <qiven/memory/system_allocator.hpp>

#include <cstddef>
#include <type_traits>

namespace
{
using qiven::uptr;
using qiven::usize;
using qiven::memory::AllocatorRef;
using qiven::memory::SystemAllocator;

struct AllocationCase
{
    usize size;
    usize alignment;
};

static_assert(std::is_empty_v<SystemAllocator>);
static_assert(std::is_constructible_v<AllocatorRef, SystemAllocator&>);

[[nodiscard]] bool pointer_is_aligned(void* memory, usize alignment) noexcept
{
    return qiven::is_aligned(static_cast<usize>(reinterpret_cast<uptr>(memory)), alignment);
}
} // namespace

int main()
{
    SystemAllocator allocator;

    if (allocator.try_allocate(0, 1) != nullptr)
        return 1;

    allocator.deallocate(nullptr, 0, 1);

    constexpr AllocationCase cases[] {
        {1, 1},
        {3, 2},
        {17, alignof(void*)},
        {37, 64},
        {257, 256},
    };

    for (const AllocationCase allocation : cases)
    {
        void* const memory = allocator.try_allocate(allocation.size, allocation.alignment);
        if (memory == nullptr)
            return 2;

        if (!pointer_is_aligned(memory, allocation.alignment))
            return 3;

        auto* const bytes = static_cast<std::byte*>(memory);
        bytes[0] = std::byte {0x5a};
        bytes[allocation.size - 1] = std::byte {0xa5};

        allocator.deallocate(memory, allocation.size, allocation.alignment);
    }

    AllocatorRef ref {allocator};

    void* const memory = ref.try_allocate(73, 128);
    if (memory == nullptr)
        return 4;

    if (!pointer_is_aligned(memory, 128))
        return 5;

    ref.deallocate(memory, 73, 128);

    return 0;
}
