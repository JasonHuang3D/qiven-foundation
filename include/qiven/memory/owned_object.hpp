#pragma once

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include <qiven/memory/allocator.hpp>
#include <qiven/memory/layout.hpp>
#include <qiven/memory/owned_allocation.hpp>

namespace qiven::memory
{
template <typename T>
class OwnedObject
{
    static_assert(std::is_object_v<T>);
    static_assert(!std::is_array_v<T>);
    static_assert(std::is_nothrow_destructible_v<T>);

public:
    ~OwnedObject() noexcept
    {
        if (get() != nullptr)
            std::destroy_at(get());
    }

    OwnedObject(const OwnedObject&)            = delete;
    OwnedObject& operator=(const OwnedObject&) = delete;

    OwnedObject(OwnedObject&&) noexcept = default;

    OwnedObject& operator=(OwnedObject&& other) noexcept
    {
        if (this == &other)
            return *this;

        if (get() != nullptr)
            std::destroy_at(get());

        allocation_ = std::move(other.allocation_);
        return *this;
    }

    [[nodiscard]] T* get() const noexcept
    {
        return static_cast<T*>(allocation_.data());
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return get() != nullptr;
    }

    T& operator*() const noexcept
    {
        return *get();
    }

    T* operator->() const noexcept
    {
        return get();
    }

private:
    explicit OwnedObject(OwnedAllocation&& allocation) noexcept
    :
    allocation_(std::move(allocation))
    {
    }

    template <typename U, typename... Args>
        requires std::is_nothrow_constructible_v<U, Args...>
    friend std::optional<OwnedObject<U>> try_make_owned_object(
        AllocatorRef allocator,
        Args&&... args) noexcept;

    OwnedAllocation allocation_;
};

template <typename T, typename... Args>
    requires std::is_nothrow_constructible_v<T, Args...>
[[nodiscard]] std::optional<OwnedObject<T>> try_make_owned_object(
    AllocatorRef allocator,
    Args&&... args) noexcept
{
    auto allocation = OwnedAllocation::try_allocate(allocator, Layout::of<T>());
    if (!allocation)
        return std::nullopt;

    std::construct_at(static_cast<T*>(allocation->data()), std::forward<Args>(args)...);
    return OwnedObject<T> { std::move(*allocation) };
}
} // namespace qiven::memory
