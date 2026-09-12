#include <qiven/memory/owned_allocation.hpp>

#include <utility>

namespace qiven::memory
{
std::optional<OwnedAllocation> OwnedAllocation::try_allocate(
    AllocatorRef allocator,
    usize size,
    usize alignment) noexcept
{
    return try_allocate(allocator, Layout::from_size_alignment(size, alignment));
}

std::optional<OwnedAllocation> OwnedAllocation::try_allocate(AllocatorRef allocator, Layout layout) noexcept
{
    void* const memory = allocator.try_allocate(layout);

    if (layout.size() != 0 && memory == nullptr)
        return std::nullopt;

    return OwnedAllocation { allocator, memory, layout };
}

OwnedAllocation::~OwnedAllocation() noexcept
{
    allocator_.deallocate(memory_, layout_);
}

OwnedAllocation::OwnedAllocation(OwnedAllocation&& other) noexcept
:
allocator_(other.allocator_),
memory_(std::exchange(other.memory_, nullptr)),
layout_(std::exchange(other.layout_, Layout::from_size_alignment(0, 1)))
{
}

OwnedAllocation& OwnedAllocation::operator=(OwnedAllocation&& other) noexcept
{
    if (this == &other)
        return *this;

    allocator_.deallocate(memory_, layout_);

    allocator_ = other.allocator_;
    memory_    = std::exchange(other.memory_, nullptr);
    layout_    = std::exchange(other.layout_, Layout::from_size_alignment(0, 1));

    return *this;
}

void* OwnedAllocation::data() const noexcept
{
    return memory_;
}

Layout OwnedAllocation::layout() const noexcept
{
    return layout_;
}

usize OwnedAllocation::size() const noexcept
{
    return layout_.size();
}

usize OwnedAllocation::alignment() const noexcept
{
    return layout_.alignment();
}

bool OwnedAllocation::empty() const noexcept
{
    return layout_.size() == 0;
}

OwnedAllocation::OwnedAllocation(AllocatorRef allocator, void* memory, Layout layout) noexcept
:
allocator_(allocator), memory_(memory), layout_(layout)
{
}
} // namespace qiven::memory
