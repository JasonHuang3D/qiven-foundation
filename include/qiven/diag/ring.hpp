#pragma once

// ============================================================================
// diag/ring.hpp — preallocated crash-visible ring + snapshot protocol
// (I1, PR6 doc 00 §4; doc 01 §5)
//
// A fixed-capacity ring of slots written by multiple producers and
// snapshotted concurrently. An access-state claim protects each plain
// payload from concurrent writers and inspectors; an odd commit sequence
// records an in-progress writer and every snapshot reports degradation
// instead of presenting an unowned record as complete.
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
inline constexpr u64 max_ring_sequence = (u64 { 1 } << 63) - 1;

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
    crash_ring(ring_record* payloads, std::atomic<u64>* sequences,
               std::atomic<u8>* access_states, u32 capacity) noexcept
    :
    capacity_(capacity), records_(payloads),
    sequences_(sequences), access_states_(access_states)
    {
        for (u32 i = 0; i < capacity; ++i)
        {
            sequences_[i].store(0, std::memory_order_relaxed);
            access_states_[i].store(0, std::memory_order_relaxed);
        }
    }

    // Multiwriter-safe, lock-free, allocation-free. A reservation has one
    // fixed slot and one bounded claim attempt; contention drops the record.
    // The access-state claim protects the plain payload from delayed writers
    // and inspectors, including owners that are later suspended or killed.
    u64 write(const event& evt) noexcept
    {
        u64 expected_reservation = last_reserved_sequence_.load(
            std::memory_order_relaxed);
        if (expected_reservation >= max_ring_sequence || !last_reserved_sequence_.compare_exchange_strong(
                                                             expected_reservation, expected_reservation + 1,
                                                             std::memory_order_relaxed, std::memory_order_relaxed))
        {
            dropped_records_.fetch_add(1, std::memory_order_relaxed);
            return 0;
        }

        const u64 reservation   = expected_reservation + 1;
        const u32 index         = static_cast<u32>((reservation - 1) % capacity_);
        std::atomic<u8>& access = access_states_[index];
        u8 expected_access      = 0;
        if (!access.compare_exchange_strong(expected_access, 1,
                                            std::memory_order_acq_rel,
                                            std::memory_order_relaxed))
        {
            dropped_records_.fetch_add(1, std::memory_order_relaxed);
            return 0;
        }

        const u64 committed = sequences_[index].load(std::memory_order_relaxed);
        if ((committed & 1ULL) != 0 || (committed != 0 && (committed >> 1) >= reservation))
        {
            access.store(0, std::memory_order_release);
            dropped_records_.fetch_add(1, std::memory_order_relaxed);
            return 0;
        }

        const u64 commit_sequence = reservation << 1;
        sequences_[index].store(commit_sequence | 1ULL, std::memory_order_relaxed);
        ring_record& rec = records_[index];
        rec.sequence     = commit_sequence;
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
        if (rec.length < max_message_bytes)
        {
            std::memset(rec.message + rec.length, 0, max_message_bytes - rec.length);
        }
        rec.message[rec.length] = '\0';
        sequences_[index].store(commit_sequence, std::memory_order_release);
        access.store(0, std::memory_order_release);
        return commit_sequence;
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
        snapshot_status status    = snapshot_status::empty;
        u32 count                 = 0; // entries populated (newest first)
        u64 highest_sequence      = 0;
        u32 busy_slots            = 0;
        u32 malformed_slots       = 0;
        u32 empty_slots           = 0;
        u32 after_watermark_slots = 0;
        u64 dropped_records       = 0;
        snapshot_entry entries[64]; // bounded
    };

    // Claims each slot at most once as an inspector, copies only while that
    // claim is held, and returns stable records newest first. Busy, malformed
    // and after-watermark slots remain visible in the bounded receipt.
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
        const u64 watermark   = last_reserved_sequence_.load(std::memory_order_acquire);
        snap.highest_sequence = watermark;
        snap.dropped_records  = dropped_records_.load(std::memory_order_relaxed);
        if (watermark == 0)
        {
            snap.status = snapshot_status::empty;
            return snap;
        }
        snap.status = snapshot_status::valid;
        u32 copied  = 0;

        // Walk every slot once. An inspector that cannot claim a slot does
        // not wait or retry; the slot stays partial for this pass.
        for (u32 i = 0; i < capacity_; ++i)
        {
            std::atomic<u8>& access = access_states_[i];
            u8 expected_access      = 0;
            if (!access.compare_exchange_strong(expected_access, 2,
                                                std::memory_order_acq_rel,
                                                std::memory_order_relaxed))
            {
                ++snap.busy_slots;
                continue;
            }

            const u64 commit_sequence = sequences_[i].load(std::memory_order_acquire);
            if (commit_sequence == 0)
            {
                ++snap.empty_slots;
                access.store(0, std::memory_order_release);
                continue;
            }

            const u64 reservation = commit_sequence >> 1;
            ring_record copy      = records_[i];
            const bool valid      = (commit_sequence & 1ULL) == 0 && reservation != 0 && reservation <= watermark && static_cast<u32>((reservation - 1) % capacity_) == i && copy.sequence == commit_sequence && copy.length < max_message_bytes;
            if (!valid)
            {
                if ((commit_sequence & 1ULL) != 0 || copy.length >= max_message_bytes || reservation == 0)
                    ++snap.malformed_slots;
                else if (reservation > watermark)
                    ++snap.after_watermark_slots;
                access.store(0, std::memory_order_release);
                continue;
            }

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
            access.store(0, std::memory_order_release);
        }
        snap.count = copied;
        if (snap.busy_slots != 0 || snap.malformed_slots != 0 || snap.after_watermark_slots != 0)
            snap.status = snapshot_status::partial;
        if (copied == 0 && snap.busy_slots == 0 && snap.malformed_slots == 0 && snap.after_watermark_slots == 0)
            snap.status = snapshot_status::empty;
        return snap;
    }

    u32 capacity() const noexcept
    {
        return capacity_;
    }

    u64 dropped() const noexcept
    {
        return dropped_records_.load(std::memory_order_relaxed);
    }

private:
    u32 capacity_                   = 0;
    ring_record* records_           = nullptr;
    std::atomic<u64>* sequences_    = nullptr;
    std::atomic<u8>* access_states_ = nullptr;
    std::atomic<u64> last_reserved_sequence_ { 0 };
    std::atomic<u64> dropped_records_ { 0 };
};

} // namespace qiven::diag
