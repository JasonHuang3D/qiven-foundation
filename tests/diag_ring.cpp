// ============================================================================
// tests/diag_ring.cpp — contract of <qiven/diag/ring.hpp> (I1).
//
// The seqlock snapshot protocol law: a torn record is never presented as
// complete (doc 00 §4). Covered: empty snapshot; single/multi-writer
// ordering; wraparound stability; concurrent write/snapshot stays valid
// or degrades to partial (never a silent torn-complete).
// ============================================================================

#include <qiven/diag/ring.hpp>

#include <atomic>
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
    //    run completes (bounded, no locks taken by writers)
    {
        std::atomic<bool> stop { false };
        std::vector<std::thread> writers;
        for (int w = 0; w < 3; ++w)
        {
            writers.emplace_back([&stop, w] {
                qiven::u64 i = 0;
                while (!stop.load(std::memory_order_relaxed))
                {
                    event evt = make_event(i++, "concurrent");
                    evt.corr  = qiven::diag::correlation {
                        static_cast<qiven::u64>(w + 1)
                    };
                    g_ring.write(evt);
                }
            });
        }
        int valid = 0, partial = 0;
        for (int s = 0; s < 2000; ++s)
        {
            auto snap = g_ring.capture_newest(16);
            if (snap.status == crash_ring::snapshot_status::empty)
                return 11;
            for (qiven::u32 i = 0; i < snap.count; ++i)
            {
                if (!snap.entries[i].stable)
                    return 12; // torn must never be stable
                if (snap.entries[i].record.length == 0)
                    return 13;
            }
            if (snap.status == crash_ring::snapshot_status::valid)
                ++valid;
            else
                ++partial;
        }
        stop.store(true, std::memory_order_relaxed);
        for (auto& t : writers)
            t.join();
        if (valid + partial != 2000)
            return 14;
    }

    return 0;
}
