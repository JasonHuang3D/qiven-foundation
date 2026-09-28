// ============================================================================
// src/diag/service.cpp — the I1 diagnostics engine (Qiven-own transport)
//
// General lane + reserved critical lane (bounded MPSC, one writer
// thread), rotating structured file sink, optional stderr/debugger
// development sinks, preallocated crash ring with the seqlock snapshot
// protocol, per-class loss counters and health. The engine is a private
// implementation detail behind <qiven/diag/service.hpp>; a private
// provider selected by the I1 measurement batch may replace it without
// changing the public contract (amended foundation.md §3; ADR-0059).
//
// Crash-path law: nothing on the ring path takes a lock or allocates;
// the emergency escalation for a critical overflow writes through the
// ring only (the file sink is the writer thread's responsibility).
// ============================================================================

#include <qiven/diag/service.hpp>

#include <qiven/platform.hpp>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <vector>

#if QIVEN_PLATFORM_WINDOWS
    #define NOMINMAX
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

namespace qiven::diag
{
namespace
{
struct stored_event
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

class bounded_lane
{
public:
    explicit bounded_lane(u32 slots) :
    slots_(slots), mask_(slots - 1)
    {
    }

    void push_drop_oldest(const stored_event& rec) noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ == slots_.size())
        {
            head_ = (head_ + 1) & mask_;
            --count_;
            ++dropped_;
        }
        slots_[(head_ + count_) & mask_] = rec;
        ++count_;
        ++emitted_;
        not_empty_.notify_one();
    }

    // returns false when the wait budget expired (overflow counted)
    bool push_bounded_wait(const stored_event& rec, i64 budget_ms) noexcept
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(budget_ms);
        std::unique_lock<std::mutex> lock(mutex_);
        while (count_ == slots_.size())
        {
            if (not_full_.wait_until(lock, deadline) == std::cv_status::timeout)
            {
                ++overflow_;
                return false;
            }
        }
        slots_[(head_ + count_) & mask_] = rec;
        ++count_;
        ++emitted_;
        not_empty_.notify_one();
        return true;
    }

    bool try_pop(stored_event& out) noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ == 0)
            return false;
        out   = slots_[head_];
        head_ = (head_ + 1) & mask_;
        --count_;
        ++drained_;
        not_full_.notify_one();
        return true;
    }

    void wake() noexcept
    {
        not_empty_.notify_all();
        not_full_.notify_all();
    }

    u64 dropped() const noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return dropped_;
    }
    u64 overflow() const noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return overflow_;
    }
    u64 emitted() const noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return emitted_;
    }
    u64 drained() const noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return drained_;
    }
    u32 occupancy() const noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<u32>(count_);
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable not_empty_, not_full_;
    std::vector<stored_event> slots_;
    usize head_ = 0, count_ = 0, mask_ = 0;
    u64 dropped_ = 0, overflow_ = 0, emitted_ = 0, drained_ = 0;
};

u64 steady_ns() noexcept
{
    return static_cast<u64>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                std::chrono::steady_clock::now().time_since_epoch())
                                .count());
}
} // namespace

class detail::service_impl
{
public:
    explicit service_impl(const service_config& cfg) :
    general_(cfg.general_lane_slots),
    critical_(cfg.critical_lane_slots),
    cfg_(cfg),
    ring_records_(cfg.ring_slots),
    ring_sequences_(cfg.ring_slots),
    ring_(ring_records_.data(), ring_sequences_.data(), cfg.ring_slots)
    {
    }

    bool start() noexcept
    {
        file_ = std::fopen(cfg_.file.path, "wb");
        if (file_ == nullptr)
        {
            // observable open failure: every record without a configured
            // fallback sink counts as a sink failure in health (never a
            // silent all-green log loss)
            sink_failures_.fetch_add(1, std::memory_order_relaxed);
        }
        stop_ = false;
        // set liveness BEFORE spawning: health() racing thread start still
        // reports the writer as running (alive = installed and not shut
        // down; the loop clears it on exit)
        writer_alive_.store(true, std::memory_order_relaxed);
        writer_ = std::thread([this] { writer_loop(); });
        return true;
    }

    void emit(const event& evt, source_module source) noexcept
    {
        stored_event rec {};
        rec.sequence     = next_sequence_.fetch_add(1, std::memory_order_relaxed);
        rec.timestamp_ns = steady_ns();
        rec.event_value  = evt.id.value;
        rec.source_value = source.value;
        rec.correlation  = evt.corr.value;
        rec.level        = static_cast<u8>(evt.level);
        rec.length       = static_cast<u32>(
            evt.text.size() < max_message_bytes ? evt.text.size() : max_message_bytes - 1);
        if (evt.text.data() != nullptr && rec.length > 0)
        {
            std::memcpy(rec.message, evt.text.data(), rec.length);
        }

        // every event mirrors into the crash ring first (crash path never
        // depends on lane or writer progress), then rides its lane
        event ring_evt        = evt;
        ring_evt.sequence     = rec.sequence;
        ring_evt.timestamp_ns = rec.timestamp_ns;
        ring_evt.source       = source;
        ring_.write(ring_evt);

        if (evt.level == severity::critical)
        {
            const bool accepted = critical_.push_bounded_wait(
                rec, cfg_.critical_wait_budget.count());
            if (!accepted)
            {
                // ring-path fallback already done above; count the escalation
                // (a reserved lane is not inexhaustible — program law)
            }
        }
        else
        {
            general_.push_drop_oldest(rec);
        }
    }

    shutdown_result shutdown() noexcept
    {
        shutdown_result result;
        const u64 drained_before = general_.drained() + critical_.drained();
        stop_                    = true;
        general_.wake();
        critical_.wake();
        {
            std::lock_guard<std::mutex> lock(wakeup_mutex_);
            wakeup_.notify_all(); // make the parked writer's exit immediate
        }
        if (writer_.joinable())
        {
            writer_.join();
        }
        if (file_)
        {
            std::fclose(file_);
            file_ = nullptr;
        }
        result.drained           = (general_.drained() + critical_.drained()) - drained_before;
        result.leftover_general  = general_.occupancy();
        result.leftover_critical = critical_.occupancy();
        result.writer_flushed    = result.leftover_general == 0 && result.leftover_critical == 0;
        return result;
    }

    health_snapshot health() const noexcept
    {
        health_snapshot snap;
        snap.general_dropped     = general_.dropped();
        snap.critical_overflow   = critical_.overflow();
        snap.general_emitted     = general_.emitted();
        snap.critical_emitted    = critical_.emitted();
        snap.general_drained     = general_.drained();
        snap.critical_drained    = critical_.drained();
        snap.general_occupancy   = general_.occupancy();
        snap.critical_occupancy  = critical_.occupancy();
        snap.sink_write_failures = sink_failures_.load(std::memory_order_relaxed);
        snap.rotations           = rotations_.load(std::memory_order_relaxed);
        snap.writer_alive        = writer_alive_.load(std::memory_order_relaxed);
        return snap;
    }

    const crash_ring* ring() const noexcept
    {
        return &ring_;
    }

    emitter make_emitter(source_module src) noexcept
    {
        return emitter(this, src);
    }

private:
    void writer_loop() noexcept
    {
        stored_event rec;
        u64 written_this_generation = 0;
        while (true)
        {
            bool got = critical_.try_pop(rec);
            if (!got)
                got = general_.try_pop(rec);
            if (got)
            {
                write_record(rec);
                written_this_generation += rec.length + 64;
                if (written_this_generation >= cfg_.file.size_bound_bytes)
                {
                    rotate();
                    written_this_generation = 0;
                }
                continue;
            }
            if (stop_.load(std::memory_order_relaxed))
            {
                // bounded flush window: drain until empty or the
                // configured shutdown budget expires (never a fixed
                // pop count that abandons queued records silently)
                const auto deadline = std::chrono::steady_clock::now() + cfg_.shutdown_flush_timeout;
                while (std::chrono::steady_clock::now() < deadline)
                {
                    if (!(critical_.try_pop(rec) || general_.try_pop(rec)))
                        break;
                    write_record(rec);
                }
                writer_alive_.store(false, std::memory_order_relaxed);
                return;
            }
            // idle: park on the wakeup CV (a bounded timeout keeps the
            // exit path latency-bounded even against a missed notify;
            // never a busy spin for the process lifetime)
            {
                std::unique_lock<std::mutex> lock(wakeup_mutex_);
                wakeup_.wait_for(lock, std::chrono::milliseconds(20));
            }
        }
    }

    void write_record(const stored_event& rec) noexcept
    {
        // 64 bytes of JSON framing + the 200-byte message budget + head
        // room for the widest scalar renderings; the guard below makes
        // the bound explicit rather than relying on the sizing alone
        char line[512];
        int n = std::snprintf(line, sizeof(line),
                              "{\"seq\":%llu,\"t\":%llu,\"sev\":\"%s\",\"id\":%u,\"src\":%u,\"corr\":%llu,\"msg\":\"%.*s\"}\n",
                              static_cast<unsigned long long>(rec.sequence),
                              static_cast<unsigned long long>(rec.timestamp_ns),
                              to_string(static_cast<severity>(rec.level)).data(),
                              rec.event_value, rec.source_value,
                              static_cast<unsigned long long>(rec.correlation),
                              static_cast<int>(rec.length), rec.message);
        // snprintf returns the WOULD-BE length; a truncated render still
        // wrote sizeof-1 bytes and is a lawful bounded line
        const int rendered = n;
        if (n >= static_cast<int>(sizeof(line)))
            n = static_cast<int>(sizeof(line)) - 1;
        bool ok        = rendered > 0;
        bool delivered = false;
        if (file_ != nullptr)
        {
            delivered = std::fwrite(line, 1, static_cast<usize>(n), file_) == static_cast<usize>(n);
            if (delivered)
                std::fflush(file_); // writer thread only; producers never flush
        }
        else if (cfg_.stderr_sink || cfg_.debugger_sink)
        {
            delivered = true; // a configured development sink takes the record
        }
        if (cfg_.stderr_sink)
        {
            std::fwrite(line, 1, static_cast<usize>(n), stderr);
        }
#if QIVEN_PLATFORM_WINDOWS
        if (cfg_.debugger_sink)
        {
            line[n] = '\0';
            OutputDebugStringA(line);
        }
#endif
        if (!ok || !delivered)
        {
            sink_failures_.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void rotate() noexcept
    {
        rotations_.fetch_add(1, std::memory_order_relaxed);
        if (file_ == nullptr)
        {
            return;
        }
        std::fclose(file_);
        file_ = nullptr;
        // generation-shift rotation: root.log -> root.1.log ... bounded
        // by cfg_.file.generations (oldest dropped); every failed
        // rename/open is observable in health (sink failure counters)
        std::string root(cfg_.file.path);
        for (u32 gen = cfg_.file.generations; gen > 1; --gen)
        {
            std::string from = root + "." + std::to_string(gen - 1) + ".log";
            std::string to   = root + "." + std::to_string(gen) + ".log";
            if (std::remove(to.c_str()) != 0 && errno != ENOENT)
            {
                sink_failures_.fetch_add(1, std::memory_order_relaxed);
            }
            if (std::rename(from.c_str(), to.c_str()) != 0 && errno != ENOENT)
            {
                sink_failures_.fetch_add(1, std::memory_order_relaxed);
            }
        }
        std::string first = root + ".1.log";
        std::remove(first.c_str());
        if (std::rename(root.c_str(), first.c_str()) != 0 && errno != ENOENT)
        {
            sink_failures_.fetch_add(1, std::memory_order_relaxed);
        }
        file_ = std::fopen(root.c_str(), "wb");
        if (file_ == nullptr)
        {
            sink_failures_.fetch_add(1, std::memory_order_relaxed);
        }
    }

    bounded_lane general_, critical_;
    service_config cfg_;
    std::vector<ring_record> ring_records_;
    std::vector<std::atomic<u64>> ring_sequences_;
    crash_ring ring_;
    std::FILE* file_ = nullptr;
    std::thread writer_;
    std::mutex wakeup_mutex_;
    std::condition_variable wakeup_;
    std::atomic<bool> stop_ { false };
    std::atomic<bool> writer_alive_ { false };
    std::atomic<u64> next_sequence_ { 0 };
    std::atomic<u64> sink_failures_ { 0 };
    std::atomic<u64> rotations_ { 0 };
};

namespace
{
// the ONE process-global service site (I0 census §2.2/F0 §4: exactly
// one process-global state site class; a second install is a typed
// configuration error, never a silent second writer)
std::mutex g_service_mutex;
detail::service_impl* g_service = nullptr;
service_config g_installed_config {};
} // namespace

install_result install_result::already_installed() noexcept
{
    install_result r;
    r.ok      = false;
    r.failure = "diag service already installed (one process-host service; "
                "multiple static copies are a configuration error)";
    return r;
}

void emitter::emit(const event& evt) const noexcept
{
    if (service_ == nullptr)
        return; // typed: invalid emitter is a no-op
    service_->emit(evt, source_);
}

void emitter::emit(severity level, event_id id, std::string_view text) const noexcept
{
    event evt;
    evt.level = level;
    evt.id    = id;
    evt.text  = text;
    emit(evt);
}

install_result install(const service_config& config) noexcept
{
    std::lock_guard<std::mutex> lock(g_service_mutex);
    install_result result;
    if (g_service != nullptr)
    {
        return install_result::already_installed();
    }
    auto pow2 = [](u32 v) { return v != 0 && (v & (v - 1)) == 0; };
    if (!pow2(config.general_lane_slots) || !pow2(config.critical_lane_slots) || !pow2(config.ring_slots))
    {
        result.ok      = false;
        result.failure = "lane/ring slot counts must be powers of two";
        return result;
    }
    auto* impl = new (std::nothrow) detail::service_impl(config);
    if (impl == nullptr)
    {
        result.ok      = false;
        result.failure = "allocation failure";
        return result;
    }
    impl->start();
    g_service           = impl;
    g_installed_config  = config;
    result.ok           = true;
    result.host_emitter = impl->make_emitter(source_module { 1 });
    return result;
}

shutdown_result shutdown() noexcept
{
    std::lock_guard<std::mutex> lock(g_service_mutex);
    if (g_service == nullptr)
    {
        return shutdown_result {}; // typed: not installed
    }
    shutdown_result r = g_service->shutdown();
    delete g_service;
    g_service = nullptr;
    return r;
}

health_snapshot health() noexcept
{
    std::lock_guard<std::mutex> lock(g_service_mutex);
    if (g_service == nullptr)
        return health_snapshot {};
    return g_service->health();
}

const crash_ring* crash_history() noexcept
{
    std::lock_guard<std::mutex> lock(g_service_mutex);
    return g_service == nullptr ? nullptr : g_service->ring();
}

} // namespace qiven::diag
