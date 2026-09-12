#pragma once

#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

#include <qiven/contracts.hpp>
#include <qiven/memory/allocator.hpp>
#include <qiven/memory/layout.hpp>
#include <qiven/memory/owned_allocation.hpp>
#include <qiven/types.hpp>

namespace qiven::memory
{
template <typename T>
class OwnedArray
{
    static_assert(std::is_object_v<T>);
    static_assert(!std::is_array_v<T>);
    static_assert(std::is_nothrow_destructible_v<T>);

public:
    ~OwnedArray() noexcept
    {
        destroy_elements();
    }

    OwnedArray(const OwnedArray&)            = delete;
    OwnedArray& operator=(const OwnedArray&) = delete;

    OwnedArray(OwnedArray&&) noexcept = default;

    OwnedArray& operator=(OwnedArray&& other) noexcept
    {
        if (this == &other)
            return *this;

        destroy_elements();
        allocation_ = std::move(other.allocation_);
        return *this;
    }

    [[nodiscard]] T* data() const noexcept
    {
        return static_cast<T*>(allocation_.data());
    }

    [[nodiscard]] usize size() const noexcept
    {
        return allocation_.size() / sizeof(T);
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return size() == 0;
    }

    [[nodiscard]] std::span<T> span() const noexcept
    {
        return { data(), size() };
    }

    T& operator[](usize index) const noexcept
    {
        QIVEN_ASSERT(index < size());

        return data()[index];
    }

private:
    explicit OwnedArray(OwnedAllocation&& allocation) noexcept
    :
    allocation_(std::move(allocation))
    {
    }

    void destroy_elements() noexcept
    {
        if (data() != nullptr)
            std::destroy_n(data(), size());
    }

    template <typename U>
        requires std::is_nothrow_default_constructible_v<U>
    friend std::optional<OwnedArray<U>> try_make_owned_array(AllocatorRef allocator, usize count) noexcept;

    OwnedAllocation allocation_;
};

template <typename T>
    requires std::is_nothrow_default_constructible_v<T>
[[nodiscard]] std::optional<OwnedArray<T>> try_make_owned_array(AllocatorRef allocator, usize count) noexcept
{
    const auto layout = Layout::array<T>(count);
    if (!layout)
        return std::nullopt;

    auto allocation = OwnedAllocation::try_allocate(allocator, *layout);
    if (!allocation)
        return std::nullopt;

    T* const data = static_cast<T*>(allocation->data());
    for (usize index = 0; index < count; ++index)
        std::construct_at(data + index);

    return OwnedArray<T> { std::move(*allocation) };
}
} // namespace qiven::memory
