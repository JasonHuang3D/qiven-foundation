#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <utility>

#include <qiven/checked_arithmetic.hpp>
#include <qiven/endian.hpp>
#include <qiven/memory/allocator.hpp>
#include <qiven/memory/owned_allocation.hpp>
#include <qiven/types.hpp>

namespace qiven
{
enum class append_status : u8
{
    ok                = 0,
    capacity_exceeded = 1,
    allocation_failed = 2,
};

// Owning, growing, bounded byte accumulator with scalar-put composition.
//
// Semantics are storage only (ADR-0024 admission,
// docs/architecture/byte-builder-admission.md): append bytes, append
// fixed-width LE/BE scalars, extract the accumulated span. Field order,
// framing, prefix width and versioning stay with each format owner.
//
// The failure channel is explicit and sticky: the first failed append
// latches its status, later appends are no-ops returning the same status,
// and ok()/status() report it. Growth is geometric (double, then clamp to
// max_capacity), lazy (nothing is allocated before the first append), and
// never hidden: capacity() is observable and monotonic non-decreasing
// between resets.
class ByteBuilder
{
public:
    [[nodiscard]] static std::optional<ByteBuilder> try_create(
        memory::AllocatorRef allocator,
        usize max_capacity) noexcept
    {
        ByteBuilder builder(allocator, max_capacity);
        return builder;
    }

    ~ByteBuilder() noexcept = default;

    ByteBuilder(const ByteBuilder&)            = delete;
    ByteBuilder& operator=(const ByteBuilder&) = delete;

    ByteBuilder(ByteBuilder&& other) noexcept
    :
    allocator_(other.allocator_),
    allocation_(std::move(other.allocation_)),
    size_(other.size_),
    max_capacity_(other.max_capacity_),
    status_(other.status_)
    {
        // A moved-from builder is a valid inert empty (zero size, capacity
        // and bound), never a stale-engaged optional with a pre-move size
        // (bytes() would span a null block).
        other.size_         = 0;
        other.max_capacity_ = 0;
        other.status_       = append_status::ok;
    }

    ByteBuilder& operator=(ByteBuilder&& other) noexcept
    {
        if (this == &other)
            return *this;

        allocation_   = std::move(other.allocation_);
        allocator_    = other.allocator_;
        size_         = other.size_;
        max_capacity_ = other.max_capacity_;
        status_       = other.status_;

        other.allocation_.reset();
        other.size_         = 0;
        other.max_capacity_ = 0;
        other.status_       = append_status::ok;
        return *this;
    }

    // Precondition: `bytes` must not reference storage owned by THIS
    // builder (e.g. bytes() of itself). Growth replaces the block BEFORE
    // the incoming bytes are copied, so a self-referencing span reads
    // freed storage. Extract-then-append of the builder's own contents
    // requires an intermediate copy at the call site. Scalar puts are
    // immune (they encode into a local array first).
    [[nodiscard]] append_status append(std::span<const std::byte> bytes) noexcept
    {
        if (status_ != append_status::ok)
            return status_;

        const auto required = checked_add(size_, bytes.size());
        if (!required)
            return status_ = append_status::capacity_exceeded;

        if (const append_status grown = ensure_capacity(*required); grown != append_status::ok)
            return status_ = grown;

        if (!bytes.empty())
        {
            std::span<std::byte> tail = free_tail().first(bytes.size());
            for (usize index = 0; index < bytes.size(); ++index)
                tail[index] = bytes[index];
            size_ += bytes.size();
        }
        return append_status::ok;
    }

    [[nodiscard]] append_status append_le_u16(u16 value) noexcept
    {
        return append_encoded_le(static_cast<u64>(value), sizeof(value));
    }

    [[nodiscard]] append_status append_le_u32(u32 value) noexcept
    {
        return append_encoded_le(static_cast<u64>(value), sizeof(value));
    }

    [[nodiscard]] append_status append_le_u64(u64 value) noexcept
    {
        return append_encoded_le(value, sizeof(value));
    }

    [[nodiscard]] append_status append_be_u16(u16 value) noexcept
    {
        return append_encoded_be(static_cast<u64>(value), sizeof(value));
    }

    [[nodiscard]] append_status append_be_u32(u32 value) noexcept
    {
        return append_encoded_be(static_cast<u64>(value), sizeof(value));
    }

    [[nodiscard]] append_status append_be_u64(u64 value) noexcept
    {
        return append_encoded_be(value, sizeof(value));
    }

    [[nodiscard]] append_status status() const noexcept
    {
        return status_;
    }

    [[nodiscard]] bool ok() const noexcept
    {
        return status_ == append_status::ok;
    }

    [[nodiscard]] std::span<const std::byte> bytes() const noexcept
    {
        if (!allocation_)
            return {};
        return { static_cast<const std::byte*>(allocation_->data()), size_ };
    }

    [[nodiscard]] usize size() const noexcept
    {
        return size_;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return size_ == 0;
    }

    [[nodiscard]] usize capacity() const noexcept
    {
        return allocation_ ? allocation_->size() : 0;
    }

    [[nodiscard]] usize max_capacity() const noexcept
    {
        return max_capacity_;
    }

    // Reuse the retained allocation for a new accumulation; clears the
    // latched status. Capacity is preserved (never shrinks).
    void reset() noexcept
    {
        size_   = 0;
        status_ = append_status::ok;
    }

private:
    ByteBuilder(memory::AllocatorRef allocator, usize max_capacity) noexcept
    :
    allocator_(allocator),
    max_capacity_(max_capacity)
    {
    }

    [[nodiscard]] std::span<std::byte> free_tail() noexcept
    {
        return {
            static_cast<std::byte*>(allocation_->data()) + size_,
            allocation_->size() - size_,
        };
    }

    [[nodiscard]] append_status ensure_capacity(usize required) noexcept
    {
        if (required <= capacity())
            return append_status::ok;

        if (required > max_capacity_)
            return append_status::capacity_exceeded;

        // Geometric growth, checked like every other size computation in
        // the file (a wrapped double would degrade growth to exact-fit).
        const auto doubled        = checked_mul(capacity(), usize { 2 });
        const usize doubled_value = doubled.value_or(required);
        const usize wanted        = required > doubled_value ? required : doubled_value;
        const usize target        = wanted > max_capacity_ ? max_capacity_ : wanted;

        auto grown = memory::OwnedAllocation::try_allocate(allocator_, target, alignof(std::byte));
        if (!grown)
            return append_status::allocation_failed;

        if (allocation_)
        {
            const std::span<const std::byte> live = bytes();
            std::span<std::byte> target_span { static_cast<std::byte*>(grown->data()), target };
            for (usize index = 0; index < size_; ++index)
                target_span[index] = live[index];
        }
        allocation_ = std::move(*grown);
        return append_status::ok;
    }

    [[nodiscard]] append_status append_encoded_le(u64 value, usize width) noexcept
    {
        if (status_ != append_status::ok)
            return status_;

        const auto required = checked_add(size_, width);
        if (!required)
            return status_ = append_status::capacity_exceeded;

        if (const append_status grown = ensure_capacity(*required); grown != append_status::ok)
            return status_ = grown;

        std::array<std::byte, 8> encoded {};
        if (!encode_le_u64(value, encoded))
            return status_ = append_status::capacity_exceeded;

        std::span<std::byte> tail = free_tail().first(width);
        for (usize index = 0; index < width; ++index)
            tail[index] = encoded[index];
        size_ += width;
        return append_status::ok;
    }

    [[nodiscard]] append_status append_encoded_be(u64 value, usize width) noexcept
    {
        if (status_ != append_status::ok)
            return status_;

        const auto required = checked_add(size_, width);
        if (!required)
            return status_ = append_status::capacity_exceeded;

        if (const append_status grown = ensure_capacity(*required); grown != append_status::ok)
            return status_ = grown;

        std::array<std::byte, 8> encoded {};
        if (!encode_be_u64(value, encoded))
            return status_ = append_status::capacity_exceeded;

        // BE scalars carry their high-order bytes last: a width-N BE put is
        // the LAST N bytes of the 8-byte BE encoding (LE takes the first N).
        std::span<std::byte> tail = free_tail().first(width);
        for (usize index = 0; index < width; ++index)
            tail[index] = encoded[encoded.size() - width + index];
        size_ += width;
        return append_status::ok;
    }

    memory::AllocatorRef allocator_;
    std::optional<memory::OwnedAllocation> allocation_;
    usize size_           = 0;
    usize max_capacity_   = 0;
    append_status status_ = append_status::ok;
};
} // namespace qiven
