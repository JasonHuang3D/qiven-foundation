#pragma once

// ============================================================================
// diag/ring.hpp — preallocated crash-visible ring + snapshot protocol
// (I1, PR6 doc 00 §4; doc 01 §5)
//
// A fixed-capacity ring of slots written by multiple producers and
// snapshotted concurrently (including by out-of-process readers over a
// crash dump). Safety rule (program law): a commit sequence ALONE does
// not make concurrent payload reads safe — every slot is a seqlock
// (odd sequence = writer in progress; sequence change across a snapshot
// copy = torn) and every snapshot reports a validity state; a torn
// record is never presented as complete.
//
// Writers never allocate and never take locks (crash-path shaped). The
// payload record is trivially copyable (the pointer-free I2 artifact
// carrier); the sequence word is a lock-free atomic whose in-memory
// form is a plain u64 for out-of-process readers.
// ============================================================================

#include <qiven/diag/event.hpp>
#include <qiven/types.hpp>

#include <atomic>
#include <cstring>
#include <type_traits>

namespace qiven::diag
{
inline constexpr u32 max_message_bytes = 200;

// The fixed crash-visible record (pointer-free by construction; the
// cross-process artifact format is the I2 contract — this is its
// in-process carrier).
struct ring_record
{
    u64 sequence     = 0;
    u64 timestamp_ns = 0;
    u32 event_value  = 0;
    u32 source_value = 0;
    u64 correlation  = 0;
    u8 level         = 0;
    u32 length       = 0;
    char message[max_message_bytes] {};
};
static_assert(std::is_trivially_copyable_v<ring_record>);

class crash_ring
{
public:
    // capacity must be a power of two; slots must outlive all writers
    // (preallocated by the service; typically never freed).
    crash_ring(ring_record* payloads, std::atomic<u64>* sequences, u32 capacity) noexcept
    :
    capacity_(capacity), mask_(capacity - 1), records_(payloads),
    sequences_(sequences)
    {
        for (u32 i = 0; i < capacity; ++i)
        {
            sequences_[i].store(0, std::memory_order_relaxed);
        }
    }

    // Multiwriter-safe, lock-free, allocation-free. Returns the claimed
    // global sequence. A writer preempted mid-write leaves an odd
    // (in-progress) sequence that snapshots report as torn, never as
    // complete.
    //
    // Commit guard: the closing store lands only while the slot still
    // carries THIS writer's odd mark (compare_exchange). A writer
    // preempted past a full ring wrap whose slot a later writer already
    // re-committed cannot regress that slot's sequence — its commit is
    // abandoned instead. Known envelope edge (recorded, bounded): the
    // abandoned writer's payload stores may already have dirtied the
    // successor's slot in the window before its CAS failure; the
    // snapshot validation (sequence equality before/after + payload
    // sequence cross-check) rejects every such mix EXCEPT the
    // astronomically narrow case of a straddled copy that carries the
    // successor's sequence field together with the abandoned writer's
    // message bytes — a mixed record cannot be excluded in pure
    // seqlock form; the I2 cross-process snapshot validator
    // (digest-bound capsule) is the closing backstop, and the
    // concurrent oracle below uses per-writer distinct text so any mix
    // is detectable in testing.
    u64 write(const event& evt) noexcept
    {
        const u64 ticket                   = ticket_.fetch_add(1, std::memory_order_relaxed);
        const u64 commit_seq               = seq_source_.fetch_add(2, std::memory_order_relaxed) + 2;
        std::atomic<u64>* claimed_sequence = nullptr;
        u32 index                          = 0;

        // Claim a slot with CAS before touching its payload. A writer that
        // loses a race or sees another writer in progress probes the next
        // bounded slot; if every slot is busy, this record is dropped rather
        // than corrupting a record owned by another writer.
        for (u32 probe = 0; probe < capacity_; ++probe)
        {
            const u32 candidate                  = static_cast<u32>((ticket + probe) & mask_);
            std::atomic<u64>& candidate_sequence = sequences_[candidate];
            u64 expected                         = candidate_sequence.load(
                std::memory_order_relaxed);
            if ((expected & 1ULL) != 0)
                continue;
            if (candidate_sequence.compare_exchange_weak(
                    expected, commit_seq - 1, std::memory_order_acquire,
                    std::memory_order_relaxed))
            {
                claimed_sequence = &candidate_sequence;
                index            = candidate;
                break;
            }
        }
        if (claimed_sequence == nullptr)
            return 0;

        std::atomic<u64>& seq = *claimed_sequence;
        // release fence between the odd-marking store and the payload
        // stores (standard seqlock writer idiom): without it, on weakly
        // ordered targets a snapshotter can observe the new payload under
        // the OLD committed sequence and validate a torn record
        std::atomic_thread_fence(std::memory_order_release);
        ring_record& rec = records_[index];
        rec.sequence     = commit_seq;
        rec.timestamp_ns = evt.timestamp_ns;
        rec.event_value  = evt.id.value;
        rec.source_value = evt.source.value;
        rec.correlation  = evt.corr.value;
        rec.level        = static_cast<u8>(evt.level);
        rec.length       = static_cast<u32>(
            evt.text.size() < max_message_bytes ? evt.text.size() : max_message_bytes - 1);
        if (evt.text.data() != nullptr && rec.length > 0)
        {
            std::memcpy(rec.message, evt.text.data(), rec.length);
        }
        rec.message[rec.length] = '\0';
        seq.store(commit_seq, std::memory_order_release);
        return commit_seq;
    }

    // Snapshot validity states (never present torn as complete; partial
    // results are first-class).
    enum class snapshot_status : u8
    {
        valid   = 0, // every copied record was stable
        partial = 1, // some slots torn/in-progress; the rest copied
        empty   = 2, // no committed record has ever been written
    };

    struct snapshot_entry
    {
        ring_record record;
        bool stable = false;
    };

    struct snapshot
    {
        snapshot_status status = snapshot_status::empty;
        u32 count              = 0; // entries populated (newest first)
        u64 highest_sequence   = 0;
        snapshot_entry entries[64]; // bounded
    };

    // Copies up to max_entries stable records, newest sequence first.
    // A slot whose sequence is odd, zero, or changed across the copy is
    // torn: not copied, degrades the status to partial.
    snapshot capture_newest(u32 max_entries) const noexcept
    {
        snapshot snap;
        if (max_entries > 64)
            max_entries = 64;
        if (max_entries == 0)
        {
            snap.status = snapshot_status::empty; // degenerate window: nothing to copy
            return snap;
        }
        const u64 tickets = ticket_.load(std::memory_order_relaxed);
        if (tickets == 0)
        {
            snap.status = snapshot_status::empty;
            return snap;
        }
        snap.highest_sequence = tickets; // at least this many writes claimed
        snap.status           = snapshot_status::valid;
        u32 copied            = 0;
        u32 torn              = 0;

        // insertion window: walk every slot once; keep the newest
        // max_entries stable records (bounded capacity, bounded entries)
        for (u32 i = 0; i < capacity_; ++i)
        {
            const std::atomic<u64>& seq = sequences_[i];
            const u64 before            = seq.load(std::memory_order_acquire);
            if (before == 0)
            {
                continue; // never written: unoccupied, not torn
            }
            if ((before & 1ULL) != 0)
            {
                ++torn; // in-progress
                continue;
            }
            ring_record copy = records_[i];
            // acquire on the validating load: prevents the compiler/CPU
            // from hoisting it above the payload copy on weakly-ordered
            // targets (a torn payload must never validate)
            const u64 after = seq.load(std::memory_order_acquire);
            if (before != after || copy.sequence != before)
            {
                ++torn; // changed under us
                continue;
            }
            if (before > snap.highest_sequence)
                snap.highest_sequence = before;

            if (copied < max_entries)
            {
                snap.entries[copied].record = copy;
                snap.entries[copied].stable = true;
                ++copied;
                // keep window ordered newest-first via insertion
                for (u32 j = copied - 1; j > 0; --j)
                {
                    if (snap.entries[j].record.sequence > snap.entries[j - 1].record.sequence)
                    {
                        snapshot_entry tmp  = snap.entries[j];
                        snap.entries[j]     = snap.entries[j - 1];
                        snap.entries[j - 1] = tmp;
                    }
                    else
                    {
                        break;
                    }
                }
            }
            else if (copy.sequence > snap.entries[max_entries - 1].record.sequence)
            {
                // replace the oldest in the window, re-insert ordered
                snap.entries[max_entries - 1].record = copy;
                snap.entries[max_entries - 1].stable = true;
                for (u32 j = max_entries - 1; j > 0; --j)
                {
                    if (snap.entries[j].record.sequence > snap.entries[j - 1].record.sequence)
                    {
                        snapshot_entry tmp  = snap.entries[j];
                        snap.entries[j]     = snap.entries[j - 1];
                        snap.entries[j - 1] = tmp;
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }
        snap.count = copied;
        if (torn > 0)
            snap.status = snapshot_status::partial;
        if (copied == 0 && torn > 0)
            snap.status = snapshot_status::partial;
        if (copied == 0 && torn == 0)
            snap.status = snapshot_status::empty;
        return snap;
    }

    u32 capacity() const noexcept
    {
        return capacity_;
    }

private:
    u32 capacity_                = 0;
    u32 mask_                    = 0;
    ring_record* records_        = nullptr;
    std::atomic<u64>* sequences_ = nullptr;
    std::atomic<u64> ticket_ { 0 };
    std::atomic<u64> seq_source_ { 0 };
};

} // namespace qiven::diag
