#pragma once

// ============================================================================
// memory/observer.hpp — thread-safe allocation observation
//
// Tracks allocation counts, byte totals, current usage, peak usage, and
// failure counts. Does NOT replace allocators — it observes them.
//
// Compile-time gate: when QIVEN_ENABLE_MEMORY_OBSERVER is 0, all observer
// calls compile to zero-cost no-ops. Default: on in Debug, off in Release.
//
// Thread safety: all counters are std::atomic. observe_* and snapshot()
// are safe to call from any thread.
// ============================================================================

#include <qiven/types.hpp>

#include <atomic>
#include <cstdint>

#ifndef QIVEN_ENABLE_MEMORY_OBSERVER
    #if defined(NDEBUG)
        #define QIVEN_ENABLE_MEMORY_OBSERVER 0
    #else
        #define QIVEN_ENABLE_MEMORY_OBSERVER 1
    #endif
#endif

namespace qiven::memory
{
struct MemoryStats
{
    usize total_allocations     = 0;
    usize total_deallocations   = 0;
    usize total_bytes_allocated = 0;
    usize total_bytes_freed     = 0;
    usize current_usage         = 0;
    usize peak_usage            = 0;
    usize allocation_failures   = 0;

    [[nodiscard]] constexpr bool operator==(const MemoryStats&) const noexcept = default;
};

class AllocationObserver
{
public:
    // ---- hooks (call from allocator code) ---------------------------------

    static void observe_allocate(usize size) noexcept
    {
#if QIVEN_ENABLE_MEMORY_OBSERVER
        total_allocations_.fetch_add(1, std::memory_order_relaxed);
        const usize new_bytes = total_bytes_allocated_.fetch_add(size, std::memory_order_relaxed) + size;
        current_usage_.fetch_add(size, std::memory_order_relaxed);

        // high-water mark: relaxed CAS loop
        usize current_peak = peak_usage_.load(std::memory_order_relaxed);
        while (new_bytes > current_peak && !peak_usage_.compare_exchange_weak(current_peak, new_bytes, std::memory_order_relaxed))
        {
        }
#else
        static_cast<void>(size);
#endif
    }

    static void observe_deallocate(usize size) noexcept
    {
#if QIVEN_ENABLE_MEMORY_OBSERVER
        total_deallocations_.fetch_add(1, std::memory_order_relaxed);
        total_bytes_freed_.fetch_add(size, std::memory_order_relaxed);
        current_usage_.fetch_sub(size, std::memory_order_relaxed);
#else
        static_cast<void>(size);
#endif
    }

    static void observe_allocate_failure(usize size) noexcept
    {
#if QIVEN_ENABLE_MEMORY_OBSERVER
        allocation_failures_.fetch_add(1, std::memory_order_relaxed);
#else
        static_cast<void>(size);
#endif
    }

    // ---- readback ----------------------------------------------------------

    [[nodiscard]] static MemoryStats snapshot() noexcept
    {
#if QIVEN_ENABLE_MEMORY_OBSERVER
        MemoryStats stats;
        stats.total_allocations     = total_allocations_.load(std::memory_order_relaxed);
        stats.total_deallocations   = total_deallocations_.load(std::memory_order_relaxed);
        stats.total_bytes_allocated = total_bytes_allocated_.load(std::memory_order_relaxed);
        stats.total_bytes_freed     = total_bytes_freed_.load(std::memory_order_relaxed);
        stats.current_usage         = current_usage_.load(std::memory_order_relaxed);
        stats.peak_usage            = peak_usage_.load(std::memory_order_relaxed);
        stats.allocation_failures   = allocation_failures_.load(std::memory_order_relaxed);
        return stats;
#else
        return MemoryStats {};
#endif
    }

    static void reset() noexcept
    {
#if QIVEN_ENABLE_MEMORY_OBSERVER
        total_allocations_.store(0, std::memory_order_relaxed);
        total_deallocations_.store(0, std::memory_order_relaxed);
        total_bytes_allocated_.store(0, std::memory_order_relaxed);
        total_bytes_freed_.store(0, std::memory_order_relaxed);
        current_usage_.store(0, std::memory_order_relaxed);
        peak_usage_.store(0, std::memory_order_relaxed);
        allocation_failures_.store(0, std::memory_order_relaxed);
#endif
    }

    [[nodiscard]] static constexpr bool enabled() noexcept
    {
        return QIVEN_ENABLE_MEMORY_OBSERVER != 0;
    }

private:
    AllocationObserver()                                     = delete;
    AllocationObserver(const AllocationObserver&)            = delete;
    AllocationObserver& operator=(const AllocationObserver&) = delete;

#if QIVEN_ENABLE_MEMORY_OBSERVER
    inline static std::atomic<usize> total_allocations_ { 0 };
    inline static std::atomic<usize> total_deallocations_ { 0 };
    inline static std::atomic<usize> total_bytes_allocated_ { 0 };
    inline static std::atomic<usize> total_bytes_freed_ { 0 };
    inline static std::atomic<usize> current_usage_ { 0 };
    inline static std::atomic<usize> peak_usage_ { 0 };
    inline static std::atomic<usize> allocation_failures_ { 0 };
#endif
};

// RAII guard: captures a snapshot at entry, exposes the delta at destruction.
class ScopedMemoryWatch
{
public:
    explicit ScopedMemoryWatch() noexcept
    :
    entry_(AllocationObserver::snapshot())
    {
    }

    ScopedMemoryWatch(const ScopedMemoryWatch&)            = delete;
    ScopedMemoryWatch& operator=(const ScopedMemoryWatch&) = delete;

    ~ScopedMemoryWatch() = default;

    [[nodiscard]] MemoryStats delta() const noexcept
    {
        const auto current = AllocationObserver::snapshot();
        MemoryStats d;
        d.total_allocations     = current.total_allocations - entry_.total_allocations;
        d.total_deallocations   = current.total_deallocations - entry_.total_deallocations;
        d.total_bytes_allocated = current.total_bytes_allocated - entry_.total_bytes_allocated;
        d.total_bytes_freed     = current.total_bytes_freed - entry_.total_bytes_freed;
        d.current_usage         = current.current_usage;
        d.peak_usage            = current.peak_usage;
        d.allocation_failures   = current.allocation_failures - entry_.allocation_failures;
        return d;
    }

    [[nodiscard]] bool has_leak() const noexcept
    {
        const auto d = delta();
        return d.total_allocations > d.total_deallocations;
    }

private:
    MemoryStats entry_;
};

} // namespace qiven::memory
