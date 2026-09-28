#pragma once

// ============================================================================
// diag/event.hpp — typed diagnostics event vocabulary (I1, PR6 doc 00 §4)
//
// Bounded, allocation-free producer representation: every field is a
// fixed-width scalar or a bounded borrowed view. No vendor type crosses
// this surface (amended foundation.md §3); the payload view is valid for
// the duration of the emit call only (asynchronous transports copy).
// ============================================================================

#include <qiven/types.hpp>

#include <cstdint>
#include <string_view>

namespace qiven::diag
{
// Ordered severity lanes. Trace..Error share the general lane policy
// (drop-oldest under pressure with loss counters); Critical owns a
// reserved lane with a bounded wait budget and ring-path fallback
// (I0 census §7 saturation budget).
enum class severity : u8
{
    trace    = 0,
    debug    = 1,
    info     = 2,
    warn     = 3,
    error    = 4,
    critical = 5,
};

[[nodiscard]] constexpr std::string_view to_string(severity level) noexcept
{
    switch (level)
    {
    case severity::trace: return "trace";
    case severity::debug: return "debug";
    case severity::info: return "info";
    case severity::warn: return "warn";
    case severity::error: return "error";
    case severity::critical: return "critical";
    }
    return "unknown";
}

// Stable event identity from a per-site compile-time constant. The value
// is opaque to the transport; sinks render it verbatim.
struct event_id
{
    u32 value = 0;
};

// Cross-event correlation (one per causal operation; 0 = none).
struct correlation
{
    u64 value = 0;
};

// Identifies the emitting image/module in one process (the host installs
// the service; modules acquire emitters with their own identity —
// amended foundation.md §10).
struct source_module
{
    u32 value = 0;
};

// The bounded producer payload. `text` is a borrowed view: transports
// copy at most max_message_bytes (service_config) into fixed storage.
struct event
{
    severity level = severity::info;
    event_id id {};
    correlation corr {};
    source_module source {};
    u64 sequence     = 0; // assigned by the service
    u64 timestamp_ns = 0;
    std::string_view text {};
};

} // namespace qiven::diag
