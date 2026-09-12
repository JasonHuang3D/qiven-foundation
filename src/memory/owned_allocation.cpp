#include <qiven/memory/owned_allocation.hpp>

#include <utility>

namespace qiven::memory
{
std::optional<OwnedAllocation> OwnedAllocation::try_allocate(
    AllocatorRef allocator,
    usize size,
    usize alignment) noexcept
{
    void* const memory = allocator.try_allocate(size, alignment);

    if (size != 0 && memory == nullptr)
        return std::nullopt;

    return OwnedAllocation { allocator, memory, size, alignment };
}

OwnedAllocation::~OwnedAllocation() noexcept
{
    allocator_.deallocate(memory_, size_, alignment_);
}

OwnedAllocation::OwnedAllocation(OwnedAllocation&& other) noexcept
:
allocator_(other.allocator_),
memory_(std::exchange(other.memory_, nullptr)),
size_(std::exchange(other.size_, 0)),
alignment_(std::exchange(other.alignment_, 1))
{
}

OwnedAllocation& OwnedAllocation::operator=(OwnedAllocation&& other) noexcept
{
    if (this == &other)
        return *this;

    allocator_.deallocate(memory_, size_, alignment_);

    allocator_ = other.allocator_;
    memory_    = std::exchange(other.memory_, nullptr);
    size_      = std::exchange(other.size_, 0);
    alignment_ = std::exchange(other.alignment_, 1);

    return *this;
}

void* OwnedAllocation::data() const noexcept
{
    return memory_;
}

usize OwnedAllocation::size() const noexcept
{
    return size_;
}

usize OwnedAllocation::alignment() const noexcept
{
    return alignment_;
}

bool OwnedAllocation::empty() const noexcept
{
    return size_ == 0;
}

OwnedAllocation::OwnedAllocation(AllocatorRef allocator, void* memory, usize size, usize alignment) noexcept
:
allocator_(allocator), memory_(memory), size_(size), alignment_(alignment)
{
}
} // namespace qiven::memory
