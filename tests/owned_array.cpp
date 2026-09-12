#include <qiven/memory/allocator.hpp>
#include <qiven/memory/owned_array.hpp>
#include <qiven/types.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

namespace
{
using qiven::usize;
using qiven::memory::AllocatorRef;
using qiven::memory::OwnedArray;
using qiven::memory::try_make_owned_array;

struct LifetimeState
{
    usize constructor_count      = 0;
    usize destructor_count       = 0;
    usize last_destruction_order = 0;
};

class TrackingAllocator
{
public:
    TrackingAllocator() noexcept
    {
        storage.fill(std::byte { 0xa5 });
    }

    [[nodiscard]] void* try_allocate(usize size, usize alignment) noexcept
    {
        ++allocation_count;
        last_allocate_size      = size;
        last_allocate_alignment = alignment;

        if (fail_allocation || size > storage.size() || alignment > storage_alignment)
            return nullptr;

        last_allocated = storage.data();
        return last_allocated;
    }

    void deallocate(void* memory, usize size, usize alignment) noexcept
    {
        ++deallocation_count;
        last_deallocated          = memory;
        last_deallocate_size      = size;
        last_deallocate_alignment = alignment;
        deallocation_order        = ++sequence;
    }

    static constexpr usize storage_alignment = 128;

    alignas(storage_alignment) std::array<std::byte, 2048> storage;
    bool fail_allocation            = false;
    usize allocation_count          = 0;
    usize deallocation_count        = 0;
    usize sequence                  = 0;
    usize deallocation_order        = 0;
    void* last_allocated            = nullptr;
    usize last_allocate_size        = 0;
    usize last_allocate_alignment   = 0;
    void* last_deallocated          = nullptr;
    usize last_deallocate_size      = 0;
    usize last_deallocate_alignment = 0;
};

struct TrackedElement
{
    TrackedElement() noexcept
    :
    state(current_state), allocator(current_allocator)
    {
        ++state->constructor_count;
        value = static_cast<int>(state->constructor_count);
    }

    ~TrackedElement() noexcept
    {
        ++state->destructor_count;
        state->last_destruction_order = ++allocator->sequence;
    }

    TrackedElement(const TrackedElement&) = delete;
    TrackedElement(TrackedElement&&)      = delete;

    static void configure(LifetimeState& state_value, TrackingAllocator& allocator_value) noexcept
    {
        current_state     = &state_value;
        current_allocator = &allocator_value;
    }

    inline static LifetimeState* current_state         = nullptr;
    inline static TrackingAllocator* current_allocator = nullptr;

    LifetimeState* state;
    TrackingAllocator* allocator;
    int value = 0;
};

struct alignas(128) OverAligned
{
    std::array<std::byte, 128> bytes {};
};

struct ThrowingDefaultConstruction
{
    ThrowingDefaultConstruction() noexcept(false)
    {
    }
};

template <typename T>
concept CanMakeOwnedArray = requires(AllocatorRef allocator, usize count) {
    try_make_owned_array<T>(allocator, count);
};

static_assert(!std::is_copy_constructible_v<OwnedArray<TrackedElement>>);
static_assert(!std::is_copy_assignable_v<OwnedArray<TrackedElement>>);
static_assert(std::is_nothrow_move_constructible_v<OwnedArray<TrackedElement>>);
static_assert(std::is_nothrow_move_assignable_v<OwnedArray<TrackedElement>>);
static_assert(!std::is_copy_constructible_v<TrackedElement>);
static_assert(!std::is_move_constructible_v<TrackedElement>);
static_assert(!CanMakeOwnedArray<ThrowingDefaultConstruction>);

[[nodiscard]] bool verify_success_access_and_destruction() noexcept
{
    TrackingAllocator backend;
    LifetimeState state;
    TrackedElement::configure(state, backend);
    TrackedElement* data = nullptr;

    {
        auto array = try_make_owned_array<TrackedElement>(AllocatorRef { backend }, 3);
        if (!array)
            return false;

        data = array->data();
        if (data == nullptr || array->size() != 3 || array->empty() ||
            state.constructor_count != 3 || state.destructor_count != 0 ||
            backend.allocation_count != 1 ||
            backend.last_allocate_size != sizeof(TrackedElement) * 3 ||
            backend.last_allocate_alignment != alignof(TrackedElement))
            return false;

        if (array->span().data() != data || array->span().size() != array->size() ||
            &(*array)[0] != data || &(*array)[1] != data + 1 || &(*array)[2] != data + 2 ||
            (*array)[0].value != 1 || (*array)[1].value != 2 || (*array)[2].value != 3)
            return false;
    }

    return state.destructor_count == 3 && backend.deallocation_count == 1 &&
           backend.last_deallocated == data &&
           backend.last_deallocate_size == sizeof(TrackedElement) * 3 &&
           backend.last_deallocate_alignment == alignof(TrackedElement) &&
           state.last_destruction_order < backend.deallocation_order;
}

[[nodiscard]] bool verify_value_initialization() noexcept
{
    TrackingAllocator backend;
    auto array = try_make_owned_array<int>(AllocatorRef { backend }, 4);
    if (!array || array->size() != 4)
        return false;

    for (int value : array->span())
    {
        if (value != 0)
            return false;
    }

    (*array)[2] = 47;
    return (*array)[2] == 47 && array->span()[2] == 47;
}

[[nodiscard]] bool verify_over_alignment() noexcept
{
    TrackingAllocator backend;
    auto array = try_make_owned_array<OverAligned>(AllocatorRef { backend }, 2);
    if (!array)
        return false;

    const auto address = reinterpret_cast<std::uintptr_t>(array->data());
    return backend.last_allocate_size == sizeof(OverAligned) * 2 &&
           backend.last_allocate_alignment == alignof(OverAligned) &&
           address % alignof(OverAligned) == 0;
}

[[nodiscard]] bool verify_zero_count() noexcept
{
    TrackingAllocator backend;
    LifetimeState state;
    TrackedElement::configure(state, backend);

    {
        auto array = try_make_owned_array<TrackedElement>(AllocatorRef { backend }, 0);
        if (!array || array->data() != nullptr || array->size() != 0 || !array->empty() ||
            !array->span().empty() || backend.allocation_count != 0 ||
            state.constructor_count != 0 || state.destructor_count != 0)
            return false;
    }

    return backend.allocation_count == 0 && backend.deallocation_count == 0 &&
           state.constructor_count == 0 && state.destructor_count == 0;
}

[[nodiscard]] bool verify_layout_overflow() noexcept
{
    TrackingAllocator backend;
    LifetimeState state;
    TrackedElement::configure(state, backend);

    constexpr usize max_count = std::numeric_limits<usize>::max() / sizeof(TrackedElement);
    const auto array          = try_make_owned_array<TrackedElement>(AllocatorRef { backend }, max_count + 1);

    return !array && backend.allocation_count == 0 && backend.deallocation_count == 0 &&
           state.constructor_count == 0 && state.destructor_count == 0;
}

[[nodiscard]] bool verify_allocation_failure() noexcept
{
    TrackingAllocator backend;
    LifetimeState state;
    TrackedElement::configure(state, backend);
    backend.fail_allocation = true;

    const auto array = try_make_owned_array<TrackedElement>(AllocatorRef { backend }, 2);
    return !array && backend.allocation_count == 1 && backend.deallocation_count == 0 &&
           state.constructor_count == 0 && state.destructor_count == 0;
}

[[nodiscard]] bool verify_move_construction() noexcept
{
    TrackingAllocator backend;
    LifetimeState state;
    TrackedElement::configure(state, backend);
    TrackedElement* original = nullptr;

    {
        auto source = try_make_owned_array<TrackedElement>(AllocatorRef { backend }, 3);
        if (!source)
            return false;

        original = source->data();
        OwnedArray<TrackedElement> destination { std::move(*source) };

        if (source->data() != nullptr || source->size() != 0 || !source->empty() ||
            destination.data() != original || destination.size() != 3 || destination.empty() ||
            state.destructor_count != 0 || backend.deallocation_count != 0)
            return false;
    }

    return state.constructor_count == 3 && state.destructor_count == 3 &&
           backend.deallocation_count == 1 && backend.last_deallocated == original;
}

[[nodiscard]] bool verify_move_assignment() noexcept
{
    TrackingAllocator destination_backend;
    TrackingAllocator source_backend;
    LifetimeState destination_state;
    LifetimeState source_state;

    TrackedElement* source_address = nullptr;
    {
        TrackedElement::configure(destination_state, destination_backend);
        auto destination =
            try_make_owned_array<TrackedElement>(AllocatorRef { destination_backend }, 2);

        TrackedElement::configure(source_state, source_backend);
        auto source = try_make_owned_array<TrackedElement>(AllocatorRef { source_backend }, 3);
        if (!destination || !source)
            return false;

        TrackedElement* const destination_address = destination->data();
        source_address                            = source->data();
        *destination                              = std::move(*source);

        if (destination_state.destructor_count != 2 ||
            destination_backend.deallocation_count != 1 ||
            destination_state.last_destruction_order >= destination_backend.deallocation_order ||
            destination_backend.last_deallocated != destination_address ||
            destination_backend.last_deallocate_size != sizeof(TrackedElement) * 2 ||
            destination_backend.last_deallocate_alignment != alignof(TrackedElement) ||
            source->data() != nullptr || source->size() != 0 || !source->empty() ||
            destination->data() != source_address || destination->size() != 3 ||
            source_state.destructor_count != 0 || source_backend.deallocation_count != 0)
            return false;
    }

    return destination_state.destructor_count == 2 &&
           destination_backend.deallocation_count == 1 && source_state.destructor_count == 3 &&
           source_backend.deallocation_count == 1 && source_backend.last_deallocated == source_address &&
           source_backend.last_deallocate_size == sizeof(TrackedElement) * 3 &&
           source_backend.last_deallocate_alignment == alignof(TrackedElement) &&
           source_state.last_destruction_order < source_backend.deallocation_order;
}

[[nodiscard]] bool verify_self_move() noexcept
{
    TrackingAllocator backend;
    LifetimeState state;
    TrackedElement::configure(state, backend);
    auto array = try_make_owned_array<TrackedElement>(AllocatorRef { backend }, 2);
    if (!array)
        return false;

    TrackedElement* const address           = array->data();
    OwnedArray<TrackedElement>* const owner = &*array;
    (*owner)[0].value                       = 17;
    (*owner)[1].value                       = 29;
    *owner                                  = std::move(*owner);

    return owner->data() == address && owner->size() == 2 && !owner->empty() &&
           (*owner)[0].value == 17 && (*owner)[1].value == 29 &&
           state.destructor_count == 0 && backend.deallocation_count == 0;
}
} // namespace

int main()
{
    if (!verify_success_access_and_destruction())
        return 1;
    if (!verify_value_initialization())
        return 2;
    if (!verify_over_alignment())
        return 3;
    if (!verify_zero_count())
        return 4;
    if (!verify_layout_overflow())
        return 5;
    if (!verify_allocation_failure())
        return 6;
    if (!verify_move_construction())
        return 7;
    if (!verify_move_assignment())
        return 8;
    if (!verify_self_move())
        return 9;
    return 0;
}
