#pragma once

#include <cstddef>

#include <qiven/types.hpp>

namespace qiven::memory
{
class LinearArena
{
public:
    LinearArena(void* memory, usize capacity) noexcept;

    LinearArena(const LinearArena&)            = delete;
    LinearArena& operator=(const LinearArena&) = delete;
    LinearArena(LinearArena&&)                 = delete;
    LinearArena& operator=(LinearArena&&)      = delete;

    [[nodiscard]] void* try_allocate(usize size, usize alignment) noexcept;
    void deallocate(void* memory, usize size, usize alignment) noexcept;

    void reset() noexcept;

    [[nodiscard]] usize capacity() const noexcept;
    [[nodiscard]] usize used() const noexcept;
    [[nodiscard]] usize remaining() const noexcept;

private:
    std::byte* begin_;
    std::byte* current_;
    usize capacity_;
    usize remaining_;
};
} // namespace qiven::memory
