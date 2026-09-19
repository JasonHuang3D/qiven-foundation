#include <qiven/contracts.hpp>
#include <qiven/memory/observer.hpp>

#include <array>
#include <cstdio>

using namespace qiven::memory;

int main()
{
    // reset for a clean slate
    AllocationObserver::reset();
    const auto baseline = AllocationObserver::snapshot();

    // observe a single allocation
    AllocationObserver::observe_allocate(64);
    {
        const auto s = AllocationObserver::snapshot();
        if constexpr (AllocationObserver::enabled())
        {
            QIVEN_VERIFY(s.total_allocations == baseline.total_allocations + 1);
            QIVEN_VERIFY(s.total_bytes_allocated >= 64);
            QIVEN_VERIFY(s.current_usage >= 64);
        }
    }

    // observe a matching deallocation: usage returns to baseline
    AllocationObserver::observe_deallocate(64);
    {
        const auto s = AllocationObserver::snapshot();
        if constexpr (AllocationObserver::enabled())
        {
            QIVEN_VERIFY(s.current_usage == baseline.current_usage);
        }
    }

    // observe a failure
    AllocationObserver::observe_allocate_failure(1024);
    {
        const auto s = AllocationObserver::snapshot();
        if constexpr (AllocationObserver::enabled())
        {
            QIVEN_VERIFY(s.allocation_failures == baseline.allocation_failures + 1);
        }
    }

    // peak usage tracks the high-water mark (not the current level)
    {
        AllocationObserver::reset();
        AllocationObserver::observe_allocate(100);
        AllocationObserver::observe_allocate(200);
        AllocationObserver::observe_deallocate(200);
        AllocationObserver::observe_deallocate(100);
        const auto s = AllocationObserver::snapshot();
        if constexpr (AllocationObserver::enabled())
        {
            QIVEN_VERIFY(s.peak_usage >= 300);
            QIVEN_VERIFY(s.current_usage == 0);
            QIVEN_VERIFY(s.total_allocations == 2);
            QIVEN_VERIFY(s.total_deallocations == 2);
        }
    }

    // ScopedMemoryWatch: delta across a scope
    {
        AllocationObserver::reset();
        {
            ScopedMemoryWatch watch;
            AllocationObserver::observe_allocate(50);
            AllocationObserver::observe_allocate(50);
            AllocationObserver::observe_deallocate(50);
            const auto d = watch.delta();
            if constexpr (AllocationObserver::enabled())
            {
                QIVEN_VERIFY(d.total_allocations == 2);
                QIVEN_VERIFY(d.total_deallocations == 1);
            }
        }
        // after the scope, the watch is destroyed but the global state persists
        if constexpr (AllocationObserver::enabled())
        {
            const auto s = AllocationObserver::snapshot();
            QIVEN_VERIFY(s.total_allocations == 2);
        }
    }

    // ScopedMemoryWatch leak detection
    {
        AllocationObserver::reset();
        {
            ScopedMemoryWatch watch;
            AllocationObserver::observe_allocate(100);
            // no deallocate: this is a "leak" within the watch scope
            if constexpr (AllocationObserver::enabled())
            {
                QIVEN_VERIFY(watch.has_leak());
            }
        }
        {
            ScopedMemoryWatch watch;
            AllocationObserver::observe_allocate(100);
            AllocationObserver::observe_deallocate(100);
            if constexpr (AllocationObserver::enabled())
            {
                QIVEN_VERIFY(!watch.has_leak());
            }
        }
    }

    // zero-size allocation is still tracked
    {
        AllocationObserver::reset();
        AllocationObserver::observe_allocate(0);
        const auto s = AllocationObserver::snapshot();
        if constexpr (AllocationObserver::enabled())
        {
            QIVEN_VERIFY(s.total_allocations == 1);
        }
    }

    // enabled() reflects the compile-time flag
    {
        // in Debug: enabled; in Release: disabled (but tests still pass because
        // the counters return zeros)
        const auto s = AllocationObserver::snapshot();
        if constexpr (AllocationObserver::enabled())
        {
            QIVEN_VERIFY(s.total_allocations > 0); // we observed above
        }
        else
        {
            QIVEN_VERIFY(s.total_allocations == 0); // no-op in Release
        }
    }

    std::printf("[ OK ] memory observer\n");
    return 0;
}
