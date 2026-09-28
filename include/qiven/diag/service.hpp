#pragma once

// ============================================================================
// diag/service.hpp — the one process-host diagnostics service (I1, PR6
// doc 00 §4; amended foundation.md §10)
//
// Exactly one service per process: the host installs it before normal
// work; loaded modules acquire emitters (capability handles) instead of
// installing their own writer/ring/handler. A second install is a typed
// configuration error. The async transport (general lane + reserved
// critical lane), the rotating structured file sink, the optional
// stderr/debugger development sinks, the preallocated crash ring and the
// overflow/health counters are owned by the service and hidden behind
// this contract; a private provider may implement the engine (amended
// foundation.md §3) — no vendor type crosses this surface.
//
// Failure model: every severity has FINITE behavior when its lane is
// full (drop-oldest with loss counters for trace..error; bounded wait
// then ring-path fallback for critical — I0 census §7). A reserved lane
// is not assumed inexhaustible. Synchronous file flush on a normal log
// call is a contract violation (exit criterion).
// ============================================================================

#include <qiven/diag/event.hpp>
#include <qiven/diag/ring.hpp>
#include <qiven/types.hpp>

#include <chrono>
#include <cstddef>

namespace qiven::diag
{
namespace detail
{
class service_impl; // private engine behind this contract
}

struct install_result;

struct file_sink_config
{
    // Rotating structured file family (replaces unrotated --log targets;
    // retention = size_bound x generations, deployment-profile
    // configurable — I0 census §7 disk budget).
    const char* path     = "qiven-diag.log"; // path family root
    u32 size_bound_bytes = 64u << 20;        // 64 MiB per generation
    u32 generations      = 5;
};

struct service_config
{
    u32 general_lane_slots  = 8192; // power of two
    u32 critical_lane_slots = 256;  // power of two; reserved lane
    std::chrono::milliseconds critical_wait_budget { 10 };
    std::chrono::milliseconds shutdown_flush_timeout { 2000 };
    u32 ring_slots = 1024; // power of two; crash-visible
    file_sink_config file {};
    bool stderr_sink   = false; // development profile
    bool debugger_sink = false; // development profile (Windows)
};

// Overflow/health metrics (observable per severity class; exit
// criterion: overload and loss remain observable).
struct health_snapshot
{
    u64 general_dropped     = 0; // drop-oldest losses (trace..error lane)
    u64 critical_overflow   = 0; // critical wait-budget exhaustions
    u64 general_emitted     = 0;
    u64 critical_emitted    = 0;
    u64 general_drained     = 0;
    u32 general_occupancy   = 0;
    u32 critical_occupancy  = 0;
    u64 sink_write_failures = 0;
    u64 rotations           = 0;
    bool writer_alive       = false;
};

// Emitter capability handle. Non-owning: the service outlives every
// emitter. Copyable, cheap (a source identity + service pointer).
class emitter
{
public:
    emitter() noexcept = default;

    // Bounded produce. Never blocks unbounded, never throws, never
    // allocates (fixed record storage). Critical events ride the
    // reserved lane; after its wait budget they fall back to the crash
    // ring + emergency escalation counter.
    void emit(const event& evt) const noexcept;

    // Convenience form for the common single-line case.
    void emit(severity level, event_id id, std::string_view text) const noexcept;

    [[nodiscard]] source_module source() const noexcept
    {
        return source_;
    }
    [[nodiscard]] bool valid() const noexcept
    {
        return service_ != nullptr;
    }

private:
    friend struct install_result;
    friend class detail::service_impl;
    emitter(detail::service_impl* owner, source_module src) noexcept
    :
    service_(owner), source_(src)
    {
    }

    detail::service_impl* service_ = nullptr;
    source_module source_ {};
};

struct install_result
{
    bool ok             = false;
    const char* failure = nullptr; // typed reason when !ok
    emitter host_emitter {};       // valid when ok

    [[nodiscard]] static install_result already_installed() noexcept;
};

// Process install (host-owned, once). Returns a typed failure on a
// second install ("multiple static copies of the process service in one
// process are a configuration error" — amended foundation.md §10).
[[nodiscard]] install_result install(const service_config& config) noexcept;

// Shutdown with a bounded flush: drains both lanes until empty or the
// configured timeout, then stops the writer and destroys the service.
// Emitters are borrowing handles: they must not be used after shutdown
// (the service is required to outlive every emitter — same law as the
// crash-path capability handles; emitting afterwards is a caller
// contract violation, not a typed path).
struct shutdown_result
{
    u64 drained           = 0;
    u64 leftover_general  = 0; // still queued at timeout
    u64 leftover_critical = 0;
    bool writer_flushed   = false;
};
[[nodiscard]] shutdown_result shutdown() noexcept;

// Health of the installed service (nulls when not installed).
[[nodiscard]] health_snapshot health() noexcept;

// Direct access to the preallocated crash ring (the crash path and the
// future inspector snapshot through this; nullptr when not installed).
[[nodiscard]] const crash_ring* crash_history() noexcept;

} // namespace qiven::diag
