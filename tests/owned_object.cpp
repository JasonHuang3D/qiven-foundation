#include <qiven/memory/allocator.hpp>
#include <qiven/memory/owned_object.hpp>
#include <qiven/types.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace
{
using qiven::usize;
using qiven::memory::AllocatorRef;
using qiven::memory::OwnedObject;
using qiven::memory::try_make_owned_object;

struct LifetimeState
{
    usize constructor_count = 0;
    usize destructor_count  = 0;
    usize copy_count        = 0;
    usize move_count        = 0;
    usize sequence          = 0;
    usize destruction_order = 0;
};

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
        deallocation_order        = ++sequence;
    }

    static constexpr usize storage_alignment = 128;

    alignas(storage_alignment) std::array<std::byte, 256> storage {};
    bool fail_allocation            = false;
    usize allocate_count            = 0;
    usize deallocate_count          = 0;
    usize sequence                  = 0;
    usize deallocation_order        = 0;
    void* last_allocated            = nullptr;
    usize last_allocate_size        = 0;
    usize last_allocate_alignment   = 0;
    void* last_deallocated          = nullptr;
    usize last_deallocate_size      = 0;
    usize last_deallocate_alignment = 0;
};

struct TrackedObject
{
    TrackedObject(LifetimeState& state_value, TrackingAllocator& allocator_value, int first_value, int second_value) noexcept
    :
    state(&state_value), allocator(&allocator_value), first(first_value), second(second_value)
    {
        ++state->constructor_count;
    }

    ~TrackedObject() noexcept
    {
        ++state->destructor_count;
        state->destruction_order = ++allocator->sequence;
    }

    TrackedObject(const TrackedObject& other) noexcept
    :
    state(other.state), allocator(other.allocator), first(other.first), second(other.second)
    {
        ++state->copy_count;
    }

    TrackedObject(TrackedObject&& other) noexcept
    :
    state(other.state), allocator(other.allocator), first(other.first), second(other.second)
    {
        ++state->move_count;
    }

    LifetimeState* state;
    TrackingAllocator* allocator;
    int first;
    int second;
};

struct alignas(128) OverAligned
{
    explicit OverAligned(std::int32_t value) noexcept :
    value(value)
    {
    }
    std::array<std::byte, 124> padding {};
    std::int32_t value;
};

struct Immovable
{
    explicit Immovable(int value) noexcept :
    value(value)
    {
    }
    ~Immovable() noexcept       = default;
    Immovable(const Immovable&) = delete;
    Immovable(Immovable&&)      = delete;
    int value;
};

struct ThrowingConstruction
{
    ThrowingConstruction() noexcept(false)
    {
    }
};

template <typename T>
concept CanMakeWithoutArguments = requires(AllocatorRef allocator) {
    try_make_owned_object<T>(allocator);
};

static_assert(!std::is_copy_constructible_v<OwnedObject<Immovable>>);
static_assert(!std::is_copy_assignable_v<OwnedObject<Immovable>>);
static_assert(std::is_nothrow_move_constructible_v<OwnedObject<Immovable>>);
static_assert(std::is_nothrow_move_assignable_v<OwnedObject<Immovable>>);
static_assert(!CanMakeWithoutArguments<ThrowingConstruction>);

bool verify_success_and_access()
{
    TrackingAllocator backend;
    LifetimeState state;

    auto object = try_make_owned_object<TrackedObject>(AllocatorRef { backend }, state, backend, 17, 29);
    if (!object)
        return false;

    return object->get() == backend.last_allocated &&
           static_cast<bool>(*object) &&
           (*object)->first == 17 &&
           (**object).second == 29 &&
           state.constructor_count == 1 &&
           state.destructor_count == 0;
}

bool verify_over_alignment()
{
    TrackingAllocator backend;
    auto object = try_make_owned_object<OverAligned>(AllocatorRef { backend }, 41);

    if (!object)
        return false;

    const auto address = reinterpret_cast<std::uintptr_t>(object->get());
    return backend.last_allocate_size == sizeof(OverAligned) &&
           backend.last_allocate_alignment == alignof(OverAligned) &&
           address % alignof(OverAligned) == 0 &&
           object->get()->value == 41;
}

bool verify_allocation_failure()
{
    TrackingAllocator backend;
    LifetimeState state;
    backend.fail_allocation = true;

    {
        const auto object =
            try_make_owned_object<TrackedObject>(AllocatorRef { backend }, state, backend, 1, 2);
        if (object)
            return false;
    }

    return backend.allocate_count == 1 && backend.deallocate_count == 0 &&
           state.constructor_count == 0 && state.destructor_count == 0;
}

bool verify_destruction_order()
{
    TrackingAllocator backend;
    LifetimeState state;

    {
        auto object = try_make_owned_object<TrackedObject>(AllocatorRef { backend }, state, backend, 3, 4);
        if (!object)
            return false;
    }

    return state.destructor_count == 1 && backend.deallocate_count == 1 &&
           state.destruction_order != 0 && state.destruction_order < backend.deallocation_order;
}

bool verify_move_construction()
{
    TrackingAllocator backend;
    LifetimeState state;
    TrackedObject* original = nullptr;

    {
        auto source = try_make_owned_object<TrackedObject>(AllocatorRef { backend }, state, backend, 5, 6);
        if (!source)
            return false;

        original = source->get();
        OwnedObject<TrackedObject> destination { std::move(*source) };

        if (source->get() != nullptr || static_cast<bool>(*source) || destination.get() != original ||
            destination->first != 5 || state.copy_count != 0 || state.move_count != 0 ||
            state.destructor_count != 0 || backend.deallocate_count != 0)
            return false;
    }

    return state.destructor_count == 1 && backend.deallocate_count == 1 &&
           backend.last_deallocated == original;
}

bool verify_immovable_support()
{
    TrackingAllocator backend;
    auto source = try_make_owned_object<Immovable>(AllocatorRef { backend }, 73);
    if (!source)
        return false;

    Immovable* const original = source->get();
    OwnedObject<Immovable> destination { std::move(*source) };
    return source->get() == nullptr && destination.get() == original && destination->value == 73;
}

bool verify_move_assignment()
{
    TrackingAllocator destination_backend;
    TrackingAllocator source_backend;
    LifetimeState destination_state;
    LifetimeState source_state;

    TrackedObject* source_address = nullptr;
    {
        auto destination = try_make_owned_object<TrackedObject>(
            AllocatorRef { destination_backend }, destination_state, destination_backend, 7, 8);
        auto source = try_make_owned_object<TrackedObject>(
            AllocatorRef { source_backend }, source_state, source_backend, 9, 10);
        if (!destination || !source)
            return false;

        TrackedObject* const destination_address = destination->get();
        source_address                           = source->get();
        *destination                             = std::move(*source);

        if (destination_state.destructor_count != 1 || destination_backend.deallocate_count != 1 ||
            destination_state.destruction_order >= destination_backend.deallocation_order ||
            destination_backend.last_deallocated != destination_address || source->get() != nullptr ||
            static_cast<bool>(*source) || destination->get() != source_address || destination->get()->first != 9 ||
            source_state.copy_count != 0 || source_state.move_count != 0 || source_state.destructor_count != 0 ||
            source_backend.deallocate_count != 0)
            return false;
    }

    return destination_state.destructor_count == 1 && destination_backend.deallocate_count == 1 &&
           source_state.destructor_count == 1 && source_backend.deallocate_count == 1 &&
           source_backend.last_deallocated == source_address &&
           source_state.destruction_order < source_backend.deallocation_order;
}

bool verify_self_move()
{
    TrackingAllocator backend;
    LifetimeState state;
    auto object = try_make_owned_object<TrackedObject>(AllocatorRef { backend }, state, backend, 11, 12);
    if (!object)
        return false;

    TrackedObject* const address            = object->get();
    OwnedObject<TrackedObject>* const owner = &*object;
    *owner                                  = std::move(*owner);

    return owner->get() == address && owner->get()->first == 11 && owner->get()->second == 12 &&
           state.destructor_count == 0 && backend.deallocate_count == 0;
}
} // namespace

int main()
{
    if (!verify_success_and_access())
        return 1;
    if (!verify_over_alignment())
        return 2;
    if (!verify_allocation_failure())
        return 3;
    if (!verify_destruction_order())
        return 4;
    if (!verify_move_construction())
        return 5;
    if (!verify_immovable_support())
        return 6;
    if (!verify_move_assignment())
        return 7;
    if (!verify_self_move())
        return 8;
    return 0;
}
