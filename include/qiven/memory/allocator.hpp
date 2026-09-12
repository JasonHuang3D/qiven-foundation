#pragma once

#include <concepts>
#include <memory>
#include <type_traits>

#include <qiven/contracts.hpp>
#include <qiven/memory/layout.hpp>
#include <qiven/types.hpp>

namespace qiven::memory
{
namespace detail
{
template <typename Allocator>
concept allocator_backend =
    !std::is_const_v<Allocator> && !std::is_volatile_v<Allocator> &&
    requires(Allocator& allocator, void* memory, usize size, usize alignment) {
        { allocator.try_allocate(size, alignment) } noexcept -> std::same_as<void*>;
        { allocator.deallocate(memory, size, alignment) } noexcept -> std::same_as<void>;
    };
} // namespace detail

class AllocatorRef
{
public:
    template <detail::allocator_backend Allocator>
        requires(!std::same_as<Allocator, AllocatorRef>)
    explicit AllocatorRef(Allocator& allocator) noexcept
    :
    context_(std::addressof(allocator)), vtable_(std::addressof(vtable_for<Allocator>))
    {
    }

    [[nodiscard]] void* try_allocate(usize size, usize alignment) const noexcept
    {
        return try_allocate(Layout::from_size_alignment(size, alignment));
    }

    [[nodiscard]] void* try_allocate(Layout layout) const noexcept
    {
        if (layout.size() == 0)
            return nullptr;

        void* const memory = vtable_->try_allocate(context_, layout.size(), layout.alignment());

        QIVEN_ASSERT(
            memory == nullptr ||
            (reinterpret_cast<uptr>(memory) & static_cast<uptr>(layout.alignment() - 1)) == 0);

        return memory;
    }

    void deallocate(void* memory, usize size, usize alignment) const noexcept
    {
        deallocate(memory, Layout::from_size_alignment(size, alignment));
    }

    void deallocate(void* memory, Layout layout) const noexcept
    {
        if (memory == nullptr)
            return;

        QIVEN_ASSERT(layout.size() != 0);
        QIVEN_ASSERT(
            (reinterpret_cast<uptr>(memory) & static_cast<uptr>(layout.alignment() - 1)) == 0);

        vtable_->deallocate(context_, memory, layout.size(), layout.alignment());
    }

private:
    struct VTable
    {
        using TryAllocateFn = void* (*)(void* context, usize size, usize alignment) noexcept;
        using DeallocateFn  = void (*)(void* context, void* memory, usize size, usize alignment) noexcept;

        constexpr VTable(TryAllocateFn try_allocate_fn, DeallocateFn deallocate_fn) noexcept
        :
        try_allocate(try_allocate_fn), deallocate(deallocate_fn)
        {
        }

        TryAllocateFn try_allocate;
        DeallocateFn deallocate;
    };

    template <typename Allocator>
    [[nodiscard]] static void* try_allocate_erased(void* context, usize size, usize alignment) noexcept
    {
        return static_cast<Allocator*>(context)->try_allocate(size, alignment);
    }

    template <typename Allocator>
    static void deallocate_erased(void* context, void* memory, usize size, usize alignment) noexcept
    {
        static_cast<Allocator*>(context)->deallocate(memory, size, alignment);
    }

    template <typename Allocator>
    inline static constexpr VTable vtable_for {
        &try_allocate_erased<Allocator>,
        &deallocate_erased<Allocator>,
    };

    void* context_;
    const VTable* vtable_;
};
} // namespace qiven::memory
