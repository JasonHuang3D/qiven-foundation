// ============================================================================
// memory_observer_stress — aggressive adversarial testing for the allocation
// observer. Each section targets a specific failure mode:
//   1. large allocation/deallocation cycles (fragmentation patterns)
//   2. rapid interleaved alloc/free (thousands of ops)
//   3. zero-size edge cases at scale
//   4. peak tracking accuracy under varying patterns
//   5. interleaved watch scopes (nested)
//   6. data integrity through observer hooks
//   7. allocation failure accounting under stress
//   8. monotonicity of counters under stress
//
// Memory ceiling: all allocations stay well under 1/20 of system RAM.
// Largest single allocation: 1 MB; peak concurrent usage: ~2 MB.
// ============================================================================

#include <qiven/contracts.hpp>
#include <qiven/memory/observer.hpp>

#include <cstdio>
#include <cstring>
#include <numeric>
#include <vector>

using namespace qiven::memory;
using qiven::usize;
using qiven::u8;
using qiven::isize;
using qiven::u64;

namespace
{
// 1/20 of 16 GB = 800 MB; we stay far below with ~2 MB peak.
constexpr usize kMaxSingleAlloc = 1ULL << 20; // 1 MB
constexpr usize kMaxTotalUsage  = 2ULL << 20; // 2 MB
constexpr usize kStressRounds   = 1000;

void fill_pattern(std::byte* ptr, usize size, u8 seed)
{
    for (usize i = 0; i < size; ++i)
        ptr[i] = static_cast<std::byte>((seed + static_cast<u8>(i)) & 0xFF);
}

bool verify_pattern(const std::byte* ptr, usize size, u8 seed)
{
    for (usize i = 0; i < size; ++i)
        if (ptr[i] != static_cast<std::byte>((seed + static_cast<u8>(i)) & 0xFF))
            return false;
    return true;
}
} // namespace

int main()
{
    // ---- 1: large alloc/dealloc cycles with data integrity ----------------
    {
        AllocationObserver::reset();
        ScopedMemoryWatch watch;
        for (usize round = 0; round < 10; ++round)
        {
            const usize size = kMaxSingleAlloc - round * 1024;
            auto* ptr = new std::byte[size];
            fill_pattern(ptr, size, static_cast<u8>(round));
            if (!verify_pattern(ptr, size, static_cast<u8>(round)))
            {
                std::printf("[FAIL] data integrity corrupted at round %zu\n", round);
                return 1;
            }
            AllocationObserver::observe_allocate(size);
            AllocationObserver::observe_deallocate(size);
            delete[] ptr;
        }
        if (watch.has_leak())
        {
            std::printf("[FAIL] large cycle leak detected\n");
            return 1;
        }
    }
    std::printf("[ OK ] 1: large cycles with data integrity\n");

    // ---- 2: rapid interleaved alloc/free (1000 ops, varying sizes) --------
    {
        AllocationObserver::reset();
        ScopedMemoryWatch watch;
        std::vector<std::pair<std::byte*, usize>> live;
        for (usize i = 0; i < kStressRounds; ++i)
        {
            const usize size = (i % 64 + 1) * 16; // 16..1024 bytes
            auto* ptr = new std::byte[size];
            fill_pattern(ptr, size, static_cast<u8>(i));
            AllocationObserver::observe_allocate(size);
            live.emplace_back(ptr, size);

            // free every other one immediately (interleaved pattern)
            if (i % 2 == 1)
            {
                auto& [p, s] = live.back();
                if (!verify_pattern(p, s, static_cast<u8>(i)))
                {
                    std::printf("[FAIL] integrity at stress round %zu\n", i);
                    return 1;
                }
                AllocationObserver::observe_deallocate(s);
                delete[] p;
                live.pop_back();
            }
        }
        // clean up the rest
        for (auto& [p, s] : live)
        {
            AllocationObserver::observe_deallocate(s);
            delete[] p;
        }
        if (watch.has_leak())
        {
            std::printf("[FAIL] stress leak detected\n");
            return 1;
        }
    }
    std::printf("[ OK ] 2: %zu interleaved alloc/free ops\n", kStressRounds);

    // ---- 3: zero-size edge cases at scale ---------------------------------
    {
        AllocationObserver::reset();
        for (usize i = 0; i < 1000; ++i)
        {
            AllocationObserver::observe_allocate(0);
            AllocationObserver::observe_deallocate(0);
        }
        const auto s = AllocationObserver::snapshot();
        if constexpr (AllocationObserver::enabled())
        {
            if (s.total_allocations != 1000)
            {
                std::printf("[FAIL] zero-size alloc count: %zu != 1000\n", s.total_allocations);
                return 1;
            }
            if (s.current_usage != 0)
            {
                std::printf("[FAIL] zero-size usage: %zu != 0\n", s.current_usage);
                return 1;
            }
        }
    }
    std::printf("[ OK ] 3: 1000 zero-size ops\n");

    // ---- 4: peak tracking accuracy under varying patterns ----------------
    {
        AllocationObserver::reset();

        // pattern: 100, 200, 300, 400, free all, then 50, 50 -> peak should be 1000
        AllocationObserver::observe_allocate(100);
        AllocationObserver::observe_allocate(200);
        AllocationObserver::observe_allocate(300);
        AllocationObserver::observe_allocate(400);
        AllocationObserver::observe_deallocate(100);
        AllocationObserver::observe_deallocate(200);
        AllocationObserver::observe_deallocate(300);
        AllocationObserver::observe_deallocate(400);
        AllocationObserver::observe_allocate(50);
        AllocationObserver::observe_allocate(50);

        const auto s = AllocationObserver::snapshot();
        if constexpr (AllocationObserver::enabled())
        {
            if (s.peak_usage != 1000)
            {
                std::printf("[FAIL] peak: %zu != 1000\n", s.peak_usage);
                return 1;
            }
            if (s.current_usage != 100)
            {
                std::printf("[FAIL] current: %zu != 100\n", s.current_usage);
                return 1;
            }
        }
    }
    std::printf("[ OK ] 4: peak tracking accuracy\n");

    // ---- 5: nested watch scopes ------------------------------------------
    {
        AllocationObserver::reset();
        ScopedMemoryWatch outer;
        AllocationObserver::observe_allocate(100);
        {
            ScopedMemoryWatch inner;
            AllocationObserver::observe_allocate(200);
            {
                ScopedMemoryWatch innermost;
                AllocationObserver::observe_allocate(300);
                AllocationObserver::observe_deallocate(300);

                if constexpr (AllocationObserver::enabled())
                {
                    const auto d = innermost.delta();
                    QIVEN_VERIFY(d.total_allocations == 1);
                    QIVEN_VERIFY(d.total_deallocations == 1);
                }
            }
            const auto di = inner.delta();
            if constexpr (AllocationObserver::enabled())
            {
                QIVEN_VERIFY(di.total_allocations == 2); // 200 + 300
            }
        }
        if constexpr (AllocationObserver::enabled())
        {
            const auto d = outer.delta();
            QIVEN_VERIFY(d.total_allocations == 3); // 100 + 200 + 300
        }
    }
    std::printf("[ OK ] 5: nested watch scopes\n");

    // ---- 6: monotonicity of counters -------------------------------------
    {
        AllocationObserver::reset();
        usize prev_allocs = 0;
        usize prev_bytes = 0;
        for (usize i = 0; i < 100; ++i)
        {
            AllocationObserver::observe_allocate(i * 10);
            const auto s = AllocationObserver::snapshot();
            if (s.total_allocations < prev_allocs)
            {
                std::printf("[FAIL] allocation counter went backwards at %zu\n", i);
                return 1;
            }
            if (s.total_bytes_allocated < prev_bytes)
            {
                std::printf("[FAIL] byte counter went backwards at %zu\n", i);
                return 1;
            }
            prev_allocs = s.total_allocations;
            prev_bytes = s.total_bytes_allocated;
        }
    }
    std::printf("[ OK ] 6: counter monotonicity\n");

    // ---- 7: allocation failure accounting under stress -------------------
    {
        AllocationObserver::reset();
        for (usize i = 0; i < 500; ++i)
            AllocationObserver::observe_allocate_failure(i);
        const auto s = AllocationObserver::snapshot();
        if constexpr (AllocationObserver::enabled())
        {
            if (s.allocation_failures != 500)
            {
                std::printf("[FAIL] failure count: %zu != 500\n", s.allocation_failures);
                return 1;
            }
        }
    }
    std::printf("[ OK ] 7: failure accounting\n");

    // ---- 8: simultaneous alloc/dealloc with varying sizes (usage accuracy)
    {
        AllocationObserver::reset();
        usize expected_usage = 0;
        for (usize round = 0; round < 200; ++round)
        {
            const usize alloc_size = (round * 37) % 512 + 1;
            const usize free_size  = (round * 23) % 512 + 1;
            AllocationObserver::observe_allocate(alloc_size);
            expected_usage += static_cast<isize>(alloc_size);
            AllocationObserver::observe_deallocate(free_size);
            expected_usage -= static_cast<isize>(free_size);
        }
        const auto s = AllocationObserver::snapshot();
        if constexpr (AllocationObserver::enabled())
        {
            const auto signed_usage = static_cast<isize>(s.current_usage);
            const auto signed_expected = static_cast<isize>(expected_usage);
            // current_usage may underflow if we free more than we allocate;
            // the observer doesn't prevent this (that's the allocator's job)
            if (signed_expected >= 0 && signed_usage != signed_expected)
            {
                std::printf("[FAIL] usage: %zd != %zd\n",
                            static_cast<isize>(s.current_usage), signed_expected);
                return 1;
            }
        }
    }
    std::printf("[ OK ] 8: usage accuracy under varying sizes\n");

    std::printf("[ OK ] all stress tests passed\n");
    return 0;
}
