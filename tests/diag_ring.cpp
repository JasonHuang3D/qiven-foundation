// ============================================================================
// tests/diag_ring.cpp — contract of <qiven/diag/ring.hpp> (I1).
//
// The seqlock snapshot protocol law: a torn record is never presented as
// complete (doc 00 §4). Covered: empty snapshot; single/multi-writer
// ordering; wraparound stability; concurrent write/snapshot stays valid
// or degrades to partial (never a silent torn-complete); per-writer
// distinct text makes a MIXED (cross-writer straddled) record detectable.
//
// The headless CRT primitive is the FIRST statement (the 2026-09-29
// modal-marathon law: a test that can abort must never open a modal CRT
// surface on any machine — aborts terminate headless with evidence +
// exit 3143 instead of awaiting a human click).
// ============================================================================

#include <qiven/crt_failure.hpp>
#include <qiven/diag/ring.hpp>

#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

using qiven::diag::crash_ring;
using qiven::diag::event;
using qiven::diag::ring_record;
using qiven::diag::severity;

static ring_record g_records[8];
static std::atomic<qiven::u64> g_sequences[8];
static crash_ring g_ring { g_records, g_sequences, 8 };

static event make_event(qiven::u64 seq, const char* text)
{
    event evt;
    evt.level = severity::info;
    evt.id    = qiven::diag::event_id { static_cast<qiven::u32>(seq) };
    evt.text  = text;
    return evt;
}

int main()
{
    qiven::install_headless_crt_failure_behavior();

    // 1. empty snapshot before any write
    {
        auto snap = g_ring.capture_newest(8);
        if (snap.status != crash_ring::snapshot_status::empty || snap.count != 0)
        {
            return 2;
        }
    }

    // 2. single writer, fewer than capacity: newest-first, stable
    for (qiven::u64 i = 0; i < 5; ++i)
    {
        g_ring.write(make_event(i, "stable-record"));
    }
    {
        auto snap = g_ring.capture_newest(8);
        if (snap.status != crash_ring::snapshot_status::valid || snap.count != 5)
        {
            return 3;
        }
        for (qiven::u32 i = 0; i < snap.count; ++i)
        {
            if (!snap.entries[i].stable)
                return 4;
            // newest first: sequence values 4,3,2,1,0
            if (snap.entries[i].record.event_value != 4 - i)
                return 5;
            if (std::strcmp(snap.entries[i].record.message, "stable-record") != 0)
                return 6;
        }
    }

    // 3. wraparound beyond capacity: only the newest `capacity` survive,
    //    all stable (no writer active during this snapshot)
    for (qiven::u64 i = 5; i < 40; ++i)
    {
        g_ring.write(make_event(i, "wrapped"));
    }
    {
        auto snap = g_ring.capture_newest(8);
        if (snap.status != crash_ring::snapshot_status::valid)
            return 7;
        if (snap.count != 8)
            return 8;
        if (snap.entries[0].record.event_value != 39)
            return 9;
        if (snap.entries[7].record.event_value != 32)
            return 10;
    }

    // 4. concurrent multi-writer + snapshotter: snapshot is valid or
    //    partial, every stable entry has a consistent record, and the
    //    run completes (bounded, no locks taken by writers). Writers use
    //    DISTINCT per-writer message text so a mixed (cross-writer
    //    straddled) record would be DETECTABLE: a stable record must
    //    carry exactly its own writer's tag, and its correlation must
    //    match the tag's writer.
    {
        ring_record concurrent_records[8];
        std::atomic<qiven::u64> concurrent_sequences[8];
        crash_ring concurrent_ring {
            concurrent_records, concurrent_sequences, 8
        };
        std::atomic<bool> stop { false };
        std::atomic<int> writes { 0 };
        std::vector<std::thread> writers;
        const char* tags[3] = { "writer-A-tag", "writer-B-tag", "writer-C-tag" };
        auto finish         = [&] {
            stop.store(true, std::memory_order_relaxed);
            for (auto& t : writers)
                t.join();
        };
        for (int w = 0; w < 3; ++w)
        {
            writers.emplace_back([&stop, &writes, w, &tags, &concurrent_ring] {
                qiven::u64 i = 0;
                while (!stop.load(std::memory_order_relaxed))
                {
                    event evt = make_event(i++, tags[w]);
                    evt.corr  = qiven::diag::correlation {
                        static_cast<qiven::u64>(w + 1)
                    };
                    concurrent_ring.write(evt);
                    writes.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        for (int wait = 0; wait < 100 && writes.load(std::memory_order_relaxed) == 0;
             ++wait)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (writes.load(std::memory_order_relaxed) == 0)
        {
            finish();
            return 11;
        }
        int valid = 0, partial = 0;
        for (int s = 0; s < 2000; ++s)
        {
            auto snap = concurrent_ring.capture_newest(16);
            if (snap.status == crash_ring::snapshot_status::empty)
            {
                finish();
                return 11;
            }
            for (qiven::u32 i = 0; i < snap.count; ++i)
            {
                if (!snap.entries[i].stable)
                {
                    finish();
                    return 12; // torn must never be stable
                }
                if (snap.entries[i].record.length == 0)
                {
                    finish();
                    return 13;
                }
                // per-writer consistency: the message tag selects the
                // writer; the correlation field must agree (a mixed
                // record carries one writer's tag with another's
                // scalars and fails here)
                const char* msg = snap.entries[i].record.message;
                int writer      = -1;
                for (int w = 0; w < 3; ++w)
                {
                    if (std::strcmp(msg, tags[w]) == 0)
                        writer = w;
                }
                if (writer < 0)
                {
                    finish();
                    return 20; // unknown/mixed message text
                }
                if (snap.entries[i].record.correlation != static_cast<qiven::u64>(writer + 1))
                {
                    finish();
                    return 21; // mixed record: tag/scalar mismatch
                }
            }
            if (snap.status == crash_ring::snapshot_status::valid)
                ++valid;
            else
                ++partial;
        }
        finish();
        if (valid + partial != 2000)
            return 14;
    }

    return 0;
}
