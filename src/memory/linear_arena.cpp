#include <qiven/memory/linear_arena.hpp>

#include <bit>
#include <memory>

#include <qiven/alignment.hpp>
#include <qiven/contracts.hpp>

namespace qiven::memory
{
LinearArena::LinearArena(void* memory, usize capacity) noexcept
:
begin_(static_cast<std::byte*>(memory)), current_(static_cast<std::byte*>(memory)), capacity_(capacity), remaining_(capacity)
{
    QIVEN_ASSERT(memory != nullptr || capacity == 0);
}

void* LinearArena::try_allocate(usize size, usize alignment) noexcept
{
    QIVEN_ASSERT(std::has_single_bit(alignment));

    if (size == 0)
        return nullptr;

    if (size > remaining_)
        return nullptr;

    void* candidate = current_;
    usize space     = remaining_;

    void* const memory = std::align(alignment, size, candidate, space);
    if (memory == nullptr)
        return nullptr;

    current_   = static_cast<std::byte*>(memory) + size;
    remaining_ = space - size;

    return memory;
}

void LinearArena::deallocate(void* memory, usize size, usize alignment) noexcept
{
    QIVEN_ASSERT(std::has_single_bit(alignment));

    if (memory == nullptr)
        return;

    QIVEN_ASSERT(size != 0);
    QIVEN_ASSERT(qiven::is_aligned(static_cast<usize>(reinterpret_cast<uptr>(memory)), alignment));

    static_cast<void>(size);
    static_cast<void>(alignment);
}

void LinearArena::reset() noexcept
{
    current_   = begin_;
    remaining_ = capacity_;
}

usize LinearArena::capacity() const noexcept
{
    return capacity_;
}

usize LinearArena::used() const noexcept
{
    return capacity_ - remaining_;
}

usize LinearArena::remaining() const noexcept
{
    return remaining_;
}
} // namespace qiven::memory
