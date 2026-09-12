#pragma once

#include <optional>

#include <qiven/memory/allocator.hpp>
#include <qiven/types.hpp>

namespace qiven::memory
{
class OwnedAllocation
{
public:
    [[nodiscard]] static std::optional<OwnedAllocation> try_allocate(
        AllocatorRef allocator,
        usize size,
        usize alignment) noexcept;

    ~OwnedAllocation() noexcept;

    OwnedAllocation(const OwnedAllocation&)            = delete;
    OwnedAllocation& operator=(const OwnedAllocation&) = delete;

    OwnedAllocation(OwnedAllocation&& other) noexcept;
    OwnedAllocation& operator=(OwnedAllocation&& other) noexcept;

    [[nodiscard]] void* data() const noexcept;
    [[nodiscard]] usize size() const noexcept;
    [[nodiscard]] usize alignment() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

private:
    OwnedAllocation(AllocatorRef allocator, void* memory, usize size, usize alignment) noexcept;

    AllocatorRef allocator_;
    void* memory_;
    usize size_;
    usize alignment_;
};
} // namespace qiven::memory
