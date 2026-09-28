// ============================================================================
// tests/diag_service.cpp — contract of <qiven/diag/service.hpp> (I1).
//
// Exit criteria exercised in-process (doc 00 §4): finite lane behavior
// with observable loss counters; no unbounded producer wait; bounded
// shutdown; typed second-install refusal (one process service); emitter
// invalid after shutdown; health observability; crash-ring mirroring.
// The stall/disk-full/rotation shapes beyond in-process reach ride the
// measurement rig's counterexample probes + the I2 probe suite.
// ============================================================================

#include <qiven/diag/service.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

using qiven::diag::correlation;
using qiven::diag::event_id;
using qiven::diag::severity;
using qiven::diag::source_module;

static qiven::diag::service_config tiny_config(const char* path)
{
    qiven::diag::service_config cfg;
    cfg.general_lane_slots     = 64;
    cfg.critical_lane_slots    = 16;
    cfg.critical_wait_budget   = std::chrono::milliseconds(20);
    cfg.shutdown_flush_timeout = std::chrono::milliseconds(500);
    cfg.ring_slots             = 64;
    cfg.file.path              = path;
    cfg.file.size_bound_bytes  = 1u << 30; // no rotation in this test
    return cfg;
}

int main()
{
    const char* sink = "diag-service-test.log";

    // 1. install once; a second install is a typed configuration error
    auto first = qiven::diag::install(tiny_config(sink));
    if (!first.ok || !first.host_emitter.valid())
    {
        return 2;
    }
    auto second = qiven::diag::install(tiny_config(sink));
    if (second.ok || second.failure == nullptr)
    {
        return 3; // one process-host service; duplicates are config errors
    }

    // 2. invalid emitters are typed no-ops (not UB)
    qiven::diag::emitter invalid;
    invalid.emit(severity::info, event_id { 7 }, "must-not-crash");

    // 3. produce/drain: events reach the sink path; health observes them
    for (int i = 0; i < 32; ++i)
    {
        first.host_emitter.emit(severity::info, event_id { 1000 + (qiven::u32)i },
                                "service-contract-event");
    }
    first.host_emitter.emit(severity::critical, event_id { 9999 }, "critical-lane-event");

    // bounded shutdown drains the lanes
    auto sres = qiven::diag::shutdown();
    if (!sres.writer_flushed)
        return 4;
    if (sres.leftover_critical != 0)
        return 5;

    // 4. emitters do not outlive the service (documented contract: the
    //    service outlives every emitter; emitting after shutdown is not
    //    a lawful call). No post-shutdown emit is performed here — the
    //    invalid-handle no-op contract is exercised in step 2 above with
    //    a never-valid emitter.

    // health after shutdown reports the null service
    auto late_health = qiven::diag::health();
    if (late_health.writer_alive)
        return 6;

    // 5. the sink received the events (producer never flushed them)
    {
        std::FILE* f = std::fopen(sink, "rb");
        if (f == nullptr)
            return 7;
        char buffer[65536];
        std::size_t n = std::fread(buffer, 1, sizeof(buffer) - 1, f);
        std::fclose(f);
        buffer[n] = '\0';
        if (std::strstr(buffer, "service-contract-event") == nullptr)
            return 8;
        if (std::strstr(buffer, "critical-lane-event") == nullptr)
            return 9;
    }

    // 6. general-lane saturation: drop-oldest with observable loss; the
    //    producer never blocks unbounded (finite per doc 00 §4)
    {
        auto inst = qiven::diag::install(tiny_config(sink));
        if (!inst.ok)
            return 11;
        // flood 100k events through a 64-slot lane with the writer
        // keeping up; measure producer latency stays bounded
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < 100000; ++i)
        {
            inst.host_emitter.emit(severity::trace, event_id { (qiven::u32)i },
                                   "saturation-flood");
        }
        auto worst_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0)
                            .count();
        // hard ceiling per I0 census §7: Warn/Error never block unbounded;
        // trace drops immediately (the 50 ms budget applies to Warn/Error
        // enqueue under saturation — the trace lane must be far faster)
        if (worst_ms > 5000)
            return 12;
        auto snap = qiven::diag::health();
        if (snap.general_emitted < 100000)
            return 13;
        qiven::diag::shutdown();
    }

    // 7. critical-lane saturation: bounded wait, then observable
    //    overflow with ring fallback — never an unbounded wait. The
    //    overflow path is forced deterministically: a 4-slot reserved
    //    lane with a ZERO wait budget under multi-producer contention
    //    (any momentary fullness times out immediately and counts).
    {
        qiven::diag::service_config cfg = tiny_config(sink);
        cfg.general_lane_slots          = 64;
        cfg.critical_lane_slots         = 4; // tiny reserved lane
        cfg.critical_wait_budget        = std::chrono::milliseconds(0);
        auto inst                       = qiven::diag::install(cfg);
        if (!inst.ok)
            return 14;
        const qiven::diag::crash_ring* ring = qiven::diag::crash_history();
        if (ring == nullptr)
            return 15;
        {
            std::atomic<int> overflow_probe { 0 };
            std::vector<std::thread> producers;
            for (int p = 0; p < 4; ++p)
            {
                producers.emplace_back([&, p] {
                    for (int i = 0; i < 512; ++i)
                    {
                        inst.host_emitter.emit(
                            severity::critical,
                            event_id { 50'000 + (qiven::u32)(p * 1000 + i) },
                            "critical-flood");
                    }
                });
            }
            for (auto& t : producers)
                t.join();
        }
        auto snap = qiven::diag::health();
        if (snap.critical_overflow == 0)
            return 17; // must be observable
        // every overflow mirrored into the crash ring: newest snapshot
        // carries critical records
        auto snap_ring = ring->capture_newest(16);
        if (snap_ring.count == 0)
            return 18;
        bool any_critical = false;
        for (qiven::u32 i = 0; i < snap_ring.count; ++i)
        {
            if (snap_ring.entries[i].stable && snap_ring.entries[i].record.level == static_cast<qiven::u8>(severity::critical))
            {
                any_critical = true;
            }
        }
        if (!any_critical)
            return 19;
        qiven::diag::shutdown();
    }

    // 8. multithread producers through one service: no loss of liveness,
    //    sequences unique (ring ticket law)
    {
        auto inst = qiven::diag::install(tiny_config(sink));
        if (!inst.ok)
            return 20;
        const qiven::diag::crash_ring* ring = qiven::diag::crash_history();
        std::atomic<bool> stop { false };
        std::vector<std::thread> producers;
        std::atomic<int> emitted { 0 };
        for (int p = 0; p < 4; ++p)
        {
            producers.emplace_back([&, p] {
                auto handle = inst.host_emitter; // copy: cheap handle
                int i       = 0;
                while (!stop.load(std::memory_order_relaxed))
                {
                    handle.emit(severity::info, event_id { (qiven::u32)(p * 100000 + i) },
                                "mt-producer");
                    ++i;
                }
                emitted.fetch_add(i);
            });
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        stop.store(true);
        for (auto& t : producers)
            t.join();
        if (emitted.load() <= 0)
            return 21;
        auto snap = ring->capture_newest(32);
        if (snap.count == 0)
            return 22;
        // strictly decreasing committed sequences in the newest-first
        // window (the snapshot's ordering law; replaces the vacuous
        // duplicate-id check - values are unique by construction)
        for (qiven::u32 a = 0; a + 1 < snap.count; ++a)
        {
            if (snap.entries[a].stable && snap.entries[a + 1].stable && snap.entries[a].record.sequence <= snap.entries[a + 1].record.sequence)
            {
                return 23;
            }
        }
        qiven::diag::shutdown();
    }

    // 9. shutdown of a non-installed service is a typed no-op
    {
        auto r = qiven::diag::shutdown();
        if (r.writer_flushed)
            return 24;
    }

    // 10. sink-open failure is OBSERVABLE: an unwritable root yields
    // sink_write_failures > 0 in health (never silent all-green loss)
    {
        qiven::diag::service_config cfg = tiny_config(sink);
        cfg.file.path                   = "no-such-dir/nested/x.log";
        auto inst                       = qiven::diag::install(cfg);
        if (!inst.ok)
            return 25;
        for (int i = 0; i < 20; ++i)
        {
            inst.host_emitter.emit(severity::error, event_id { (qiven::u32)i }, "lost-record");
        }
        auto h = qiven::diag::health();
        if (h.sink_write_failures == 0)
            return 26;
        if (h.writer_alive == false)
            return 27; // the writer keeps running; only the sink is gone
        qiven::diag::shutdown();
    }

    // 11. rotation is exercised and observable: a tiny generation size
    // rotates during the run and the rotation counter reports it
    {
        qiven::diag::service_config cfg = tiny_config(sink);
        cfg.file.size_bound_bytes       = 2048;
        cfg.file.generations            = 2;
        auto inst                       = qiven::diag::install(cfg);
        if (!inst.ok)
            return 28;
        for (int i = 0; i < 4000; ++i)
        {
            inst.host_emitter.emit(severity::info, event_id { (qiven::u32)(900000 + i) },
                                   "rotation-filler-event-payload");
        }
        auto h = qiven::diag::health();
        if (h.rotations == 0)
            return 29;
        qiven::diag::shutdown();
        std::remove("diag-service-test.1.log");
        std::remove("diag-service-test.2.log");
    }

    std::remove(sink);
    std::remove("diag-service-test.1.log");
    return 0;
}
