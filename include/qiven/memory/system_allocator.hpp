#pragma once

#include <qiven/types.hpp>

namespace qiven::memory
{
class SystemAllocator
{
public:
    [[nodiscard]] void* try_allocate(usize size, usize alignment) noexcept;
    void deallocate(void* memory, usize size, usize alignment) noexcept;
};
} // namespace qiven::memory
