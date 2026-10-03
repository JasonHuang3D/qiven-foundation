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

// F1 packaging: the shared variant exports the public diag entry points
// through the CMake-generated <qiven/foundation_export.hpp> (dllexport
// while building the DLL, dllimport while consuming the installed
// package). The static target and its consumers define
// QIVEN_FOUNDATION_NO_EXPORT_HEADER, so the generated header is skipped
// and QIVEN_FOUNDATION_API falls back to empty below — the static
// path's codegen is unchanged from the pre-F1 build. SCOPE LIMITATION:
// this slice wires the macro onto the free functions
// (install/shutdown/health/crash_history) plus the two emitter::emit
// overloads (an external consumer of the DLL cannot link without
// them); the full export surface (class members across the public
// headers) is the F2 module-ABI slice.
#ifndef QIVEN_FOUNDATION_NO_EXPORT_HEADER
    #include <qiven/foundation_export.hpp>
#endif

#ifndef QIVEN_FOUNDATION_API
    #define QIVEN_FOUNDATION_API
#endif

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
    u64 critical_drained    = 0;
    u32 general_occupancy   = 0;
    u32 critical_occupancy  = 0;
    u64 sink_write_failures = 0;
    u64 rotations           = 0;
    u64 ring_dropped        = 0; // bounded ring claim/reservation losses
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
    QIVEN_FOUNDATION_API void emit(const event& evt) const noexcept;

    // Convenience form for the common single-line case.
    QIVEN_FOUNDATION_API void emit(severity level, event_id id, std::string_view text) const noexcept;

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
    bool ok = false;
    // typed reason when !ok — OWNED bounded storage (never a borrowed
    // exception string: the exception object dies at the catch boundary
    // and a dangling failure pointer is UB for the caller)
    char failure_text[160] {};
    const char* failure = nullptr; // points into failure_text when set
    emitter host_emitter {};       // valid when ok

    void set_failure(const char* text) noexcept
    {
        u32 i = 0;
        while (text != nullptr && text[i] != '\0' && i < sizeof(failure_text) - 1)
        {
            failure_text[i] = text[i];
            ++i;
        }
        failure_text[i] = '\0';
        failure         = failure_text;
    }

    [[nodiscard]] static install_result already_installed() noexcept;
};

// Process install (host-owned; ONE LIVE INSTANCE AT A TIME — a fresh
// install after a completed shutdown is lawful). Returns a typed
// failure when an instance is already live ("multiple static copies of
// the process service in one process are a configuration error" —
// amended foundation.md §10), on a non-power-of-two capacity, or on
// engine resource exhaustion.
[[nodiscard]] QIVEN_FOUNDATION_API install_result install(const service_config& config);

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
[[nodiscard]] QIVEN_FOUNDATION_API shutdown_result shutdown() noexcept;

// Health of the installed service (nulls when not installed).
[[nodiscard]] QIVEN_FOUNDATION_API health_snapshot health() noexcept;

// Direct access to the preallocated crash ring (the crash path and the
// future inspector snapshot through this; nullptr when not installed).
[[nodiscard]] QIVEN_FOUNDATION_API const crash_ring* crash_history() noexcept;

} // namespace qiven::diag
