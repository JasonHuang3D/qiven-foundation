#include <qiven/memory/allocator.hpp>
#include <qiven/memory/owned_allocation.hpp>
#include <qiven/types.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>

namespace
{
using qiven::usize;
using qiven::memory::AllocatorRef;
using qiven::memory::OwnedAllocation;

class TrackingAllocator
{
public:
    [[nodiscard]] void* try_allocate(usize size, usize alignment) noexcept
    {
        ++allocate_count;
        last_allocate_size      = size;
        last_allocate_alignment = alignment;

        if (fail_allocation || size > storage.size() || alignment > storage_alignment)
            return nullptr;

        last_allocated = storage.data();
        return last_allocated;
    }

    void deallocate(void* memory, usize size, usize alignment) noexcept
    {
        ++deallocate_count;
        last_deallocated          = memory;
        last_deallocate_size      = size;
        last_deallocate_alignment = alignment;
    }

    static constexpr usize storage_alignment = 64;

    alignas(storage_alignment) std::array<std::byte, 256> storage {};
    bool fail_allocation            = false;
    usize allocate_count            = 0;
    usize deallocate_count          = 0;
    void* last_allocated            = nullptr;
    usize last_allocate_size        = 0;
    usize last_allocate_alignment   = 0;
    void* last_deallocated          = nullptr;
    usize last_deallocate_size      = 0;
    usize last_deallocate_alignment = 0;
};

static_assert(!std::is_copy_constructible_v<OwnedAllocation>);
static_assert(!std::is_copy_assignable_v<OwnedAllocation>);
static_assert(std::is_nothrow_move_constructible_v<OwnedAllocation>);
static_assert(std::is_nothrow_move_assignable_v<OwnedAllocation>);

bool verify_success_and_destruction()
{
    TrackingAllocator backend;
    void* allocated = nullptr;

    {
        auto allocation = OwnedAllocation::try_allocate(AllocatorRef { backend }, 96, 32);
        if (!allocation)
            return false;

        allocated = allocation->data();

        if (allocated == nullptr ||
            allocation->size() != 96 ||
            allocation->alignment() != 32 ||
            allocation->empty())
            return false;

        if (backend.allocate_count != 1 ||
            backend.deallocate_count != 0 ||
            backend.last_allocate_size != 96 ||
            backend.last_allocate_alignment != 32)
            return false;
    }

    return backend.deallocate_count == 1 &&
           backend.last_deallocated == allocated &&
           backend.last_deallocate_size == 96 &&
           backend.last_deallocate_alignment == 32;
}

bool verify_failure()
{
    TrackingAllocator backend;
    backend.fail_allocation = true;

    const auto allocation = OwnedAllocation::try_allocate(AllocatorRef { backend }, 64, 16);

    return !allocation &&
           backend.allocate_count == 1 &&
           backend.deallocate_count == 0;
}

bool verify_zero_size()
{
    TrackingAllocator backend;

    {
        auto allocation = OwnedAllocation::try_allocate(AllocatorRef { backend }, 0, 64);
        if (!allocation)
            return false;

        if (allocation->data() != nullptr ||
            allocation->size() != 0 ||
            allocation->alignment() != 64 ||
            !allocation->empty())
            return false;

        if (backend.allocate_count != 0 || backend.deallocate_count != 0)
            return false;
    }

    return backend.allocate_count == 0 && backend.deallocate_count == 0;
}

bool verify_move_construction()
{
    TrackingAllocator backend;
    void* allocated = nullptr;

    {
        auto source = OwnedAllocation::try_allocate(AllocatorRef { backend }, 48, 16);
        if (!source)
            return false;

        allocated = source->data();

        OwnedAllocation destination { std::move(*source) };

        if (!source->empty() ||
            source->data() != nullptr ||
            source->size() != 0 ||
            source->alignment() != 1)
            return false;

        if (destination.data() != allocated ||
            destination.size() != 48 ||
            destination.alignment() != 16 ||
            destination.empty())
            return false;

        if (backend.deallocate_count != 0)
            return false;
    }

    return backend.deallocate_count == 1 &&
           backend.last_deallocated == allocated &&
           backend.last_deallocate_size == 48 &&
           backend.last_deallocate_alignment == 16;
}

bool verify_move_assignment_origins()
{
    TrackingAllocator destination_backend;
    TrackingAllocator source_backend;

    void* destination_memory = nullptr;
    void* source_memory      = nullptr;

    {
        auto destination = OwnedAllocation::try_allocate(AllocatorRef { destination_backend }, 32, 8);
        auto source      = OwnedAllocation::try_allocate(AllocatorRef { source_backend }, 80, 32);

        if (!destination || !source)
            return false;

        destination_memory = destination->data();
        source_memory      = source->data();

        *destination = std::move(*source);

        if (destination_backend.deallocate_count != 1 ||
            destination_backend.last_deallocated != destination_memory ||
            destination_backend.last_deallocate_size != 32 ||
            destination_backend.last_deallocate_alignment != 8)
            return false;

        if (source_backend.deallocate_count != 0)
            return false;

        if (!source->empty() ||
            source->data() != nullptr ||
            source->size() != 0 ||
            source->alignment() != 1)
            return false;

        if (destination->data() != source_memory ||
            destination->size() != 80 ||
            destination->alignment() != 32 ||
            destination->empty())
            return false;
    }

    return destination_backend.deallocate_count == 1 &&
           source_backend.deallocate_count == 1 &&
           source_backend.last_deallocated == source_memory &&
           source_backend.last_deallocate_size == 80 &&
           source_backend.last_deallocate_alignment == 32;
}

bool verify_self_move()
{
    TrackingAllocator backend;

    {
        auto allocation = OwnedAllocation::try_allocate(AllocatorRef { backend }, 40, 8);
        if (!allocation)
            return false;

        void* const memory           = allocation->data();
        OwnedAllocation* const owner = &*allocation;

        *owner = std::move(*owner);

        if (owner->data() != memory ||
            owner->size() != 40 ||
            owner->alignment() != 8 ||
            owner->empty())
            return false;

        if (backend.deallocate_count != 0)
            return false;
    }

    return backend.deallocate_count == 1;
}
} // namespace

int main()
{
    if (!verify_success_and_destruction())
        return 1;

    if (!verify_failure())
        return 2;

    if (!verify_zero_size())
        return 3;

    if (!verify_move_construction())
        return 4;

    if (!verify_move_assignment_origins())
        return 5;

    if (!verify_self_move())
        return 6;

    return 0;
}
