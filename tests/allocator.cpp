#include <qiven/memory/allocator.hpp>
#include <qiven/memory/layout.hpp>

#include <cstddef>
#include <type_traits>

namespace
{
using qiven::usize;
using qiven::memory::AllocatorRef;
using qiven::memory::Layout;

struct TestAllocator
{
    alignas(64) std::byte storage[128] {};
    usize allocation_size        = 0;
    usize allocation_alignment   = 0;
    usize deallocation_size      = 0;
    usize deallocation_alignment = 0;
    void* deallocated_memory     = nullptr;
    int allocation_calls         = 0;
    int deallocation_calls       = 0;
    bool fail                    = false;

    void* try_allocate(usize size, usize alignment) noexcept
    {
        ++allocation_calls;
        allocation_size      = size;
        allocation_alignment = alignment;
        return fail ? nullptr : storage;
    }

    void deallocate(void* memory, usize size, usize alignment) noexcept
    {
        ++deallocation_calls;
        deallocated_memory     = memory;
        deallocation_size      = size;
        deallocation_alignment = alignment;
    }
};

struct ThrowingAllocator
{
    void* try_allocate(usize, usize)
    {
        return nullptr;
    }

    void deallocate(void*, usize, usize) noexcept
    {
    }
};

static_assert(sizeof(AllocatorRef) == 2 * sizeof(void*));
static_assert(std::is_trivially_copyable_v<AllocatorRef>);
static_assert(!std::is_default_constructible_v<AllocatorRef>);
static_assert(std::is_constructible_v<AllocatorRef, TestAllocator&>);
static_assert(!std::is_constructible_v<AllocatorRef, const TestAllocator&>);
static_assert(!std::is_constructible_v<AllocatorRef, TestAllocator&&>);
static_assert(!std::is_constructible_v<AllocatorRef, ThrowingAllocator&>);

[[nodiscard]] bool verify_layout_overloads() noexcept
{
    TestAllocator allocator;
    AllocatorRef ref { allocator };

    const Layout zero_layout = Layout::from_size_alignment(0, 32);
    if (ref.try_allocate(zero_layout) != nullptr || allocator.allocation_calls != 0)
        return false;

    const Layout layout = Layout::from_size_alignment(48, 64);
    void* const memory  = ref.try_allocate(layout);
    if (memory != allocator.storage ||
        allocator.allocation_calls != 1 ||
        allocator.allocation_size != layout.size() ||
        allocator.allocation_alignment != layout.alignment())
        return false;

    allocator.fail = true;
    if (ref.try_allocate(Layout::from_size_alignment(24, 8)) != nullptr || allocator.allocation_calls != 2)
        return false;

    ref.deallocate(nullptr, zero_layout);
    if (allocator.deallocation_calls != 0)
        return false;

    ref.deallocate(memory, layout);
    if (allocator.deallocation_calls != 1 ||
        allocator.deallocated_memory != memory ||
        allocator.deallocation_size != layout.size() ||
        allocator.deallocation_alignment != layout.alignment())
        return false;

    allocator.fail           = false;
    AllocatorRef copy        = ref;
    const Layout copy_layout = Layout::from_size_alignment(16, 16);
    if (copy.try_allocate(copy_layout) != allocator.storage ||
        allocator.allocation_calls != 3 ||
        allocator.allocation_size != copy_layout.size() ||
        allocator.allocation_alignment != copy_layout.alignment())
        return false;

    return true;
}
} // namespace

int main()
{
    TestAllocator allocator;
    AllocatorRef ref { allocator };

    if (ref.try_allocate(0, 16) != nullptr)
        return 1;
    if (allocator.allocation_calls != 0)
        return 2;

    void* const memory = ref.try_allocate(32, 64);
    if (memory != allocator.storage)
        return 3;
    if (allocator.allocation_calls != 1 || allocator.allocation_size != 32 || allocator.allocation_alignment != 64)
        return 4;

    AllocatorRef copy = ref;
    if (copy.try_allocate(16, 16) != allocator.storage)
        return 5;
    if (allocator.allocation_calls != 2 || allocator.allocation_size != 16 || allocator.allocation_alignment != 16)
        return 6;

    allocator.fail = true;
    if (ref.try_allocate(8, 8) != nullptr)
        return 7;
    if (allocator.allocation_calls != 3)
        return 8;

    ref.deallocate(nullptr, 0, 16);
    if (allocator.deallocation_calls != 0)
        return 9;

    copy.deallocate(memory, 32, 64);
    if (allocator.deallocation_calls != 1 || allocator.deallocated_memory != memory || allocator.deallocation_size != 32 ||
        allocator.deallocation_alignment != 64)
        return 10;

    if (!verify_layout_overloads())
        return 11;

    return 0;
}
