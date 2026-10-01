# Foundation Diagnostics Contract v1: freeze the C++ surface before I1/I2 implementation

> Proposal status: review required.

> Document ID: D00. The I2-only detailed design and acceptance/landing entry is
> [D10](README.md). D11-D13 complete
> its Windows-x64 I2 implementation design without changing the four public
> header declarations below. I1 execution design and I3/F2/F3 remain separate.
>
> This document revises qiven-docs PR #9 from an I2-entry recovery plan into the normative Foundation diagnostics contract that implementation work must obey. It refines, but does not reopen, the accepted PR #6 diagnostics architecture and the F0 boundary already published in qiven-foundation.
>
> Reviewed baseline: PR #9 `5b13540297649e0ef7ad81851ca02d2eafde6c07`; Foundation F0 `032a6152efa18407d7b8d29d3203a5def179385a`; accepted PR #6 documents at docs `2dc2f0f83668892b06959384dd4e2efdc009b3e2`. The now-published v48 accident branch is component/test evidence, not the implementation base. No project implementation or acceptance is performed by this proposal.

## 1. Decision

PR #6 correctly fixed the architecture but left too much API shape to the implementation window. That was acceptable while the design was still exploratory; it is no longer acceptable after the first logger implementation attempt reached a concurrent fatal-path defect and the development session entered a repeated native-failure loop.

The next diagnostics attempt therefore changes the development contract:

1. The public C++ source API, ownership, lifetime, call topology, failure vocabulary, crash-evidence vocabulary and cross-process wire invariants are frozen before I1/I2 implementation.
2. The implementation model owns private algorithms, private headers, queue structure, sink internals, platform adapters, provider adapters, tests and optimization, but it does not redesign the public contract while implementing it.
3. A conflict between the frozen contract and implementation evidence is surfaced as a CONTRACT-CONFLICT with the smallest proposed contract change. It is not repaired by silently changing a public header or introducing a parallel path.
4. The v48 I1/F1 branch is not resumed as an authority. It may be mined for individually mapped implementation/test components and incident evidence under section 13.
5. The old qiven-zcode-dev-plugin / Skill prerequisite in the previous PR #9 draft is removed. Foundation correctness is enforced by the C++ contract, compiler, tests, watchdogs and release probes, not by relying on a model to remember an operating instruction.
6. The legacy include/qiven/crt_failure.hpp remains a temporary compatibility surface, but its independent process-wide implementation ends when the common diagnostics service lands.

This PR intentionally freezes semantics and topology, not incidental implementation.

## 2. Current published Foundation state

At qiven-foundation main after F0:

- the repository has the amended Foundation architecture and the F0 diagnostics baseline;
- the delivered library is still static-only;
- no I1 logging header or implementation is published;
- no I2 crash-client or inspector contract is published;
- include/qiven/crt_failure.hpp plus src/crt_failure.cpp remain the only diagnostics-adjacent process-global mechanism;
- the current CRT helper still owns the Windows CRT handler set and one re-entrancy guard;
- the F0 baseline explicitly records I1/F1, I2 and F1-F3 as unlanded.

That clean published boundary is the implementation base for this contract.

The sealed v48 logger work proved something useful: a logger, crash ring and fatal path are not safe places to let the implementation session invent ownership and concurrency semantics while debugging. Those decisions are moved into this contract.

## 3. Authority boundary

### 3.1 Frozen by this contract

The following are normative for the first diagnostics implementation:

- public header names and declarations in section 5;
- one process diagnostics owner;
- explicit emitter capability flow;
- borrowed event-input lifetime;
- no hidden dynamic allocation on the installed producer path;
- bounded producer wait;
- typed, observable overflow/degradation;
- normal logger independence from fatal capture;
- crash callback restrictions;
- legacy CRT absorption;
- host-versus-module ownership;
- crash evidence classification;
- cross-process pointer-free/versioned wire rules;
- live crash-ring reader being out-of-process, not an unsynchronized in-process C++ reader;
- exact allowed/forbidden dependency topology;
- acceptance behavior in sections 8-10.

### 3.2 Left to implementation

The implementation may choose, without contract revision:

- MPSC versus qualified per-thread/SPSC transport;
- slot/block allocator structure;
- internal queue capacities derived from the configured memory budget;
- writer-thread structure;
- file implementation and rotation mechanics;
- private provider and platform adapter classes;
- source-file organization under src/diag;
- private helper APIs;
- internal caching;
- batching;
- Windows primitive selection where several primitives satisfy the same contract;
- Linux/macOS backend details when those platforms are separately qualified;
- optimization that preserves all observable behavior.

A third-party provider remains private. Provider replacement must not require public Runtime call-site changes.

### 3.3 CONTRACT-CONFLICT rule

An implementation session that concludes the frozen contract cannot be implemented correctly must stop that slice and produce:

~~~text
CONTRACT-CONFLICT
contract:
evidence:
why the current contract cannot be implemented correctly:
smallest proposed header/semantic change:
tests that discriminate old versus proposed contract:
~~~

The implementation session may prepare the proposed diff, but the public contract is not changed as part of an implementation/fix loop.

## 4. Normative topology

The process-level topology is:

~~~text
process host
    |
    v
install_process_diagnostics(config)
    |
    v
DiagnosticsHost ------------------------------+
    |                                         |
    | emitter()                               | health()/shutdown()
    v                                         |
Emitter                                       |
    |                                         |
    v                                         |
bounded event admission                       |
    |                                         |
    +----> bounded normal transport ----------+----> writer ----> bounded sinks
    |
    +----> crash-visible ring when policy requires

fatal callback / native exception
    |
    v
one Foundation crash client
    |
    +----> bounded emergency evidence
    |
    +----> crash capsule
               |
               v
        out-of-process inspector
               |
               v
      dump / ring snapshot / manifest

legacy install_headless_crt_failure_behavior()
    |
    v
the same Foundation process core
    |
    +----> legacy-compatible headless failure behavior
    +----> later promotion to the full DiagnosticsHost control capability

loaded lockstep module
    |
    +----> receives Emitter capability
    |
    X     must not install a second process service
~~~

The following direct edges are invalid:

~~~text
ordinary Emitter -> filesystem
ordinary Emitter -> synchronous file flush
ordinary Emitter -> stdio
ordinary Emitter -> unbounded wait

fatal callback -> normal writer thread
fatal callback -> heap allocation
fatal callback -> stdio / iostream
fatal callback -> CRT formatting / strlen
fatal callback -> application mutex
fatal callback -> recovery/continuation

loaded module -> install_process_diagnostics()
loaded module -> install_headless_crt_failure_behavior()
legacy CRT wrapper -> independent second handler stack
crash inspector -> process-local pointer trusted as wire data
~~~

## 5. Normative public C++ headers

The code in this section is the v1 source-level C++ contract. The first Foundation implementation transaction copies these declarations into the named headers before implementation expands.

They are source-level contracts for lockstep Qiven consumers. They are not an independently versioned C++ ABI. The future independently versioned module boundary, when a real consumer exists, remains a separate C-callable ABI under the accepted F0/PR #6 law.

### 5.1 include/qiven/diag/event.hpp

~~~cpp
#pragma once

#include <qiven/types.hpp>

#include <cstddef>
#include <source_location>
#include <span>
#include <string_view>

namespace qiven::diag
{

using EventId = u32;

inline constexpr EventId invalid_event_id = 0;

enum class Severity : u8
{
    trace = 0,
    debug,
    info,
    warn,
    error,
    critical
};

struct Correlation final
{
    // Zero means "not present".
    u64 session     = 0;
    u64 task        = 0;
    u64 transaction = 0;
    u64 generation  = 0;
    u64 action      = 0;
};

struct SourceLocationView final
{
    std::string_view file {};
    std::string_view function {};
    u32 line   = 0;
    u32 column = 0;

    [[nodiscard]] static constexpr SourceLocationView current(
        std::source_location location = std::source_location::current()) noexcept
    {
        return SourceLocationView {
            location.file_name(),
            location.function_name(),
            location.line(),
            location.column()
        };
    }
};

enum class FieldKind : u8
{
    signed_integer = 0,
    unsigned_integer,
    floating_point,
    boolean,
    text,
    bytes
};

struct FieldView final
{
    // All views are borrowed for the duration of Emitter::emit().
    std::string_view key {};
    FieldKind kind = FieldKind::text;

    i64 signed_value   = 0;
    u64 unsigned_value = 0;
    f64 floating_value = 0.0;

    std::string_view text_value {};
    std::span<const std::byte> bytes_value {};
};

struct EventView final
{
    EventId id = invalid_event_id;
    Severity severity = Severity::info;
    Correlation correlation {};
    SourceLocationView source {};
    std::string_view message {};
    std::span<const FieldView> fields {};
};

[[nodiscard]] constexpr EventView make_event(
    EventId id,
    Severity severity,
    std::string_view message,
    std::span<const FieldView> fields = {},
    Correlation correlation = {},
    std::source_location location = std::source_location::current()) noexcept
{
    return EventView {
        id,
        severity,
        correlation,
        SourceLocationView::current(location),
        message,
        fields
    };
}

} // namespace qiven::diag
~~~

Normative semantics:

- EventId zero is invalid and must be rejected.
- EventId is the stable machine identity. Human text is not the identity.
- The dotted rendering name for an EventId is owned by the vocabulary that owns that event family.
- EventView and every nested string/span are borrowed only until Emitter::emit() returns.
- An asynchronous implementation must copy/encode everything it needs before emit() returns.
- A loaded module may unload after its queued event has been copied; no queued pointer may reference module text/storage after emit() returns.
- Timestamps, process ID and thread ID are captured by the diagnostics service at admission, not supplied by callers.
- FieldKind selects the corresponding value member. signed_integer uses signed_value; unsigned_integer uses unsigned_value; floating_point uses floating_value; boolean uses unsigned_value and accepts only 0 or 1; text uses text_value; bytes uses bytes_value. Non-selected members have no semantic meaning.
- Truncation is marked in every published packet and counted. Normal admission of that packet returns accepted_truncated; normal filtering/dropping keeps its own disposition even if the ring publishes truncated content. EventId, severity, correlation and numeric field values never change silently.

Event validity and work bounds are also part of the contract:

- Caller-provided objects and the ranges actually accessed must be alive, readable and stable until emit returns. A dangling pointer or a span constructed outside the C++ object model cannot be made safe by this API. `rejected_invalid` covers inspectable values and lengths, not arbitrary pointer probing.
- Reject unknown Severity/FieldKind values, zero IDs, excessive field counts, empty/duplicate keys, unrepresentable key lengths, invalid retained UTF-8 and non-finite floating-point values. Keys are identities: never truncate or silently remove them or whole fields. Invalid events publish to neither destination.
- Check counts, lengths and checked size arithmetic before scanning/copying data. Read caller ranges at most up to the configured event-byte budget plus a fixed amount per admitted field; no `strlen`, traversal of an oversized span, or scan of a discarded text tail. Repeated key checks operate on the bounded owned copy. Retained text ends at a complete UTF-8 code point; bytes may end anywhere. A too-large fixed header/key/numeric skeleton is rejected before publishing either destination.
- The encoder first reserves that skeleton, then assigns remaining space in deterministic order: source file, source function, message, then variable field values in input order. Empty values caused by truncation remain present and typed. Section 6.1 records the truncation bit.
- Event IDs are unique across the process's participating vocabularies. Their build-pinned ID-to-name catalogs accompany offline decoding; the logger does not resolve a name by calling back into an unloadable module. No new runtime registry is implied by these headers.

### 5.2 include/qiven/diag/emitter.hpp

~~~cpp
#pragma once

#include <qiven/diag/event.hpp>
#include <qiven/types.hpp>

namespace qiven::diag
{

enum class EmitDisposition : u8
{
    accepted = 0,
    accepted_truncated,
    dropped_overflow,
    rejected_invalid,
    service_unavailable,
    filtered
};

enum class RingPublishState : u8
{
    not_attempted = 0,
    published,
    dropped,
    unavailable
};

struct EmitOutcome final
{
    EmitDisposition disposition = EmitDisposition::service_unavailable;
    RingPublishState ring = RingPublishState::unavailable;
};

class Emitter final
{
public:
    using EmitFn = EmitOutcome (*)(void*, const EventView&) noexcept;

    constexpr Emitter() noexcept = default;

    constexpr Emitter(void* context, EmitFn emit_fn) noexcept :
        m_context(context),
        m_emit_fn(emit_fn)
    {
    }

    [[nodiscard]] EmitOutcome emit(const EventView& event) const noexcept
    {
        if (m_emit_fn == nullptr)
        {
            return {};
        }
        return m_emit_fn(m_context, event);
    }

    [[nodiscard]] explicit constexpr operator bool() const noexcept
    {
        return m_emit_fn != nullptr;
    }

private:
    void* m_context = nullptr;
    EmitFn m_emit_fn = nullptr;
};

} // namespace qiven::diag
~~~

Normative semantics after successful installation:

- emit() is noexcept.
- emit() performs no filesystem operation and no synchronous file flush.
- emit() performs no dynamic allocation from the general heap.
- emit() does not use stdio or iostream.
- emit() has a finite producer-wait bound for every severity.
- all caller views are consumed or copied before emit() returns.
- service_unavailable is returned by an invalid/unbound Emitter and by a service that can no longer accept events.
- overflow is observable both in EmitOutcome and aggregate health counters.
- a critical event always attempts the crash-visible ring even when the normal queue is full.
- warn/error/critical ring behavior follows the configured ring floor.
- the ring attempt itself is bounded and never waits for the normal writer.

The two result dimensions describe different destinations. `disposition`
reports normal transport admission, not successful file writing or durability;
`ring` reports only the ring attempt. For a live logging service:

| Event/policy state | Normal disposition | Ring state |
| --- | --- | --- |
| Invalid event | rejected_invalid | not_attempted |
| Below persistent_floor | filtered | independently evaluated against crash_ring_floor |
| Admitted to normal transport | accepted or accepted_truncated | independently evaluated |
| Eligible normal event but queue admission fails | dropped_overflow | independently evaluated |
| Below ring floor, except critical | as above | not_attempted |
| Critical, at any persistent floor | as above | published, dropped or unavailable; always attempted |

Filtering is intentional and does not increment loss counters. A filtered
event may still publish a truncated ring packet; its packet flag and the
truncation counter disclose that fact. A stopped/disabled/unbound service
returns `service_unavailable` and `ring = unavailable`; it does not dereference
event payloads. Ring publication survives normal writer/sink failure while
the logging service still accepts events.

`operator bool()` tests binding, not readiness. Copies of a host-issued
Emitter may be used concurrently; the function/context pair is backed by the
process-lifetime core described below. A custom Emitter constructed by a
caller delegates those lifetime and concurrency obligations to that caller.

The v1 overflow policy is semantic, not algorithmic:

- trace/debug/info: normal-queue saturation may drop the new event immediately;
- warn/error: may wait only up to the configured warn_error_wait_us, then must return a drop/degradation result;
- critical: attempts the crash ring first, then may wait only up to critical_wait_us for normal transport;
- no severity may wait indefinitely for disk, writer progress or queue capacity;
- the configured interval covers the complete normal-admission wait/retry budget, not a fresh timeout per retry; zero permits one non-waiting attempt. Ring reservation and validation have separate fixed operation/byte bounds. No unbounded CAS loop or hidden blocking mutex satisfies this law.

These are algorithmic wait bounds. OS preemption, page faults and scheduling
can make observed wall time larger; I0 fixes separately measured end-to-end
latency thresholds and the external test deadline. A plain lock-free
designation is not proof of bounded per-call work.

### 5.3 include/qiven/diag/crash.hpp

~~~cpp
#pragma once

#include <qiven/types.hpp>

namespace qiven::diag
{

enum class CrashFailureClass : u16
{
    unknown = 0,
    ucrt_invalid_parameter = 1,
    abort_call = 2,
    cpp_terminate = 3,
    pure_virtual_call = 4,
    unhandled_native_exception = 5,
    contract_failure = 6,
    direct_fail_fast = 7,
    stack_cookie_failure = 8,
    pre_main_or_loader_failure = 9
};

enum class CrashContextOrigin : u8
{
    none = 0,
    callback_origin,
    native_exception,
    external_observer
};

enum class CrashContextQuality : u8
{
    none = 0,
    observer_only,
    reason_only,
    callback_context,
    native_context
};

enum class CrashCaptureState : u8
{
    unavailable = 0,
    emergency_only,
    inspector_ready,
    capture_in_progress,
    completed,
    partial,
    failed
};

enum class CrashEvidenceFlag : u32
{
    none             = 0,
    reason           = 1u << 0,
    callback_context = 1u << 1,
    native_context   = 1u << 2,
    external_dump    = 1u << 3,
    ring_tail        = 1u << 4,
    supervisor_exit  = 1u << 5,
    manifest         = 1u << 6
};

[[nodiscard]] constexpr CrashEvidenceFlag operator|(
    CrashEvidenceFlag a,
    CrashEvidenceFlag b) noexcept
{
    return static_cast<CrashEvidenceFlag>(
        static_cast<u32>(a) | static_cast<u32>(b));
}

[[nodiscard]] constexpr bool has_flag(
    CrashEvidenceFlag value,
    CrashEvidenceFlag flag) noexcept
{
    return (static_cast<u32>(value) & static_cast<u32>(flag)) != 0;
}

enum class CrashInstallIssue : u32
{
    none                    = 0,
    inspector_unavailable   = 1u << 0,
    emergency_sink_failed   = 1u << 1,
    handler_conflict        = 1u << 2,
    unsupported_fault_class = 1u << 3,
    unsupported_platform    = 1u << 4,
    artifact_storage_failed = 1u << 5
};

[[nodiscard]] constexpr CrashInstallIssue operator|(
    CrashInstallIssue a,
    CrashInstallIssue b) noexcept
{
    return static_cast<CrashInstallIssue>(
        static_cast<u32>(a) | static_cast<u32>(b));
}

[[nodiscard]] constexpr bool has_issue(
    CrashInstallIssue value,
    CrashInstallIssue issue) noexcept
{
    return (static_cast<u32>(value) & static_cast<u32>(issue)) != 0;
}

struct CrashCapabilities final
{
    // Bit N corresponds to static_cast<u16>(CrashFailureClass) == N.
    u64 callback_class_mask = 0;
    u64 native_class_mask   = 0;
    u64 external_class_mask = 0;
};

struct CrashHealth final
{
    CrashCaptureState state = CrashCaptureState::unavailable;
    CrashCapabilities capabilities {};
    CrashInstallIssue issues = CrashInstallIssue::none;

    bool emergency_sink_ready = false;
    bool inspector_registered = false;
};

struct CrashObservation final
{
    CrashFailureClass failure_class = CrashFailureClass::unknown;
    CrashContextOrigin context_origin = CrashContextOrigin::none;
    CrashContextQuality context_quality = CrashContextQuality::none;
    CrashEvidenceFlag evidence = CrashEvidenceFlag::none;
    CrashCaptureState capture_state = CrashCaptureState::unavailable;

    u32 native_exception_code = 0;
    u32 synthetic_exit_code   = 0;
};

} // namespace qiven::diag
~~~

This deliberately separates context quality from artifact presence.

A dump is not itself native context. A UCRT callback can have callback_context plus external_dump. An access violation can have native_context plus external_dump. Observer-only direct fail-fast may have external_dump and supervisor_exit without claiming callback/native context.

Normative classification rules:

- native_context is legal only when a genuine native exception record/context was supplied and preserved;
- UCRT invalid parameter, terminate, abort and purecall are callback-origin classes unless a platform supplies a separate genuine native exception;
- direct fail-fast, stack-cookie and pre-main/loader classes are never claimed as covered by the ordinary in-process callback layer merely because an external dump exists;
- inspector configured is not equivalent to inspector_registered;
- completed means the artifact state required by the selected profile is closed and minimally validated, not merely that a crash signal was observed.

The flag helpers test any overlap (and return false for none). A required
multi-bit evidence set must compare the entire mask, together with class and
context, rather than using one any-overlap test as proof of all requirements.

### 5.4 include/qiven/diag/process_diagnostics.hpp

~~~cpp
#pragma once

#include <qiven/diag/crash.hpp>
#include <qiven/diag/emitter.hpp>
#include <qiven/export.hpp>
#include <qiven/types.hpp>

#include <array>
#include <span>
#include <string_view>

namespace qiven::diag
{

enum class LoggerState : u8
{
    unavailable = 0,
    starting,
    ready,
    degraded,
    stopping,
    stopped
};

struct LoggingHealth final
{
    LoggerState state = LoggerState::unavailable;
    bool file_sink_ready = false;
    bool crash_ring_ready = false;
    bool counters_exact = true;

    // Indexed by static_cast<u8>(Severity).
    std::array<u64, 6> dropped_events {};
    std::array<u64, 6> filtered_events {};
    std::array<u64, 6> truncated_events {};
    std::array<u64, 6> lost_after_admission {};
    std::array<u64, 6> ring_dropped_events {};
    std::array<u64, 6> ring_unavailable_events {};
};

struct DiagnosticsHealth final
{
    LoggingHealth logging {};
    CrashHealth crash {};
};

enum class Requirement : u8
{
    disabled = 0,
    optional,
    required
};

struct RetentionPolicy final
{
    u64 max_file_bytes  = 0;
    u32 max_file_count  = 0;
    u64 max_total_bytes = 0;
};

struct LoggingPolicy final
{
    u32 event_byte_limit = 0;
    u8 field_count_limit = 0;

    u32 warn_error_wait_us = 0;
    u32 critical_wait_us = 0;

    Severity persistent_floor = Severity::info;
    Severity crash_ring_floor = Severity::warn;

    u64 memory_budget_bytes = 0;
    u64 crash_ring_budget_bytes = 0;

    RetentionPolicy retention {};
};

struct CrashClassRequirement final
{
    CrashFailureClass failure_class = CrashFailureClass::unknown;
    CrashContextOrigin context_origin = CrashContextOrigin::none;
    CrashContextQuality context_quality = CrashContextQuality::reason_only;
    CrashEvidenceFlag evidence = CrashEvidenceFlag::reason;
};

struct CrashStoragePolicy final
{
    // One artifact set includes dump, ring snapshot, capsule and manifest.
    u64 max_artifact_bytes = 0;
    u32 max_artifact_count = 0;
    u64 max_total_bytes = 0;
    u32 max_pending_captures = 0;
};

struct CrashPolicy final
{
    Requirement capture = Requirement::optional;

    // Borrowed only for installation; copied into bounded owned storage.
    std::span<const CrashClassRequirement> required_classes {};
    CrashStoragePolicy storage {};

    // Healthy-profile inspector acknowledgement budget.
    u32 acknowledgement_timeout_ms = 5000;

    // External supervisor deadline. This is not a permit for an
    // in-process handler to block for the whole duration.
    u32 supervisor_deadline_ms = 30000;
};

struct Identity128 final
{
    u64 high = 0;
    u64 low  = 0;
};

struct ProcessDiagnosticsConfig final
{
    // Borrowed only for install_process_diagnostics().
    std::string_view process_name {};
    std::string_view artifact_root_utf8 {};

    Identity128 build_identity {};
    Identity128 workspace_identity {};

    Requirement logging = Requirement::required;
    LoggingPolicy logging_policy {};
    CrashPolicy crash_policy {};
};

enum class InstallStatus : u8
{
    installed = 0,
    installed_degraded,
    upgraded_legacy_compat,

    invalid_config,
    already_installed,
    resource_exhausted,
    logging_unavailable,
    required_crash_unavailable,
    unsupported_platform
};

enum class ShutdownStatus : u8
{
    completed = 0,
    timed_out,
    already_stopped,
    invalid_host
};

struct ShutdownOutcome final
{
    ShutdownStatus status = ShutdownStatus::invalid_host;
    u64 flushed_events = 0;
    u64 lost_events = 0;
    u64 pending_events = 0;
};

struct InstallOutcome;

class DiagnosticsHost final
{
public:
    using HealthFn = DiagnosticsHealth (*)(void*) noexcept;
    using ShutdownFn = ShutdownOutcome (*)(void*, u32 timeout_ms) noexcept;

    DiagnosticsHost() noexcept = default;

    DiagnosticsHost(const DiagnosticsHost&) = delete;
    DiagnosticsHost& operator=(const DiagnosticsHost&) = delete;

    QIVEN_FOUNDATION_API DiagnosticsHost(DiagnosticsHost&& other) noexcept;
    QIVEN_FOUNDATION_API DiagnosticsHost& operator=(DiagnosticsHost&& other) noexcept;

    ~DiagnosticsHost() noexcept = default;

    [[nodiscard]] QIVEN_FOUNDATION_API explicit operator bool() const noexcept;

    [[nodiscard]] QIVEN_FOUNDATION_API Emitter emitter() const noexcept;
    [[nodiscard]] QIVEN_FOUNDATION_API DiagnosticsHealth health() const noexcept;

    // Explicit shutdown. Destruction of this small handle must not introduce
    // an unbounded flush. Process hosts call shutdown() when normal shutdown
    // ordering permits it.
    [[nodiscard]] QIVEN_FOUNDATION_API ShutdownOutcome shutdown(u32 timeout_ms) noexcept;

private:
    friend QIVEN_FOUNDATION_API InstallOutcome install_process_diagnostics(
        const ProcessDiagnosticsConfig&) noexcept;

    DiagnosticsHost(
        void* context,
        Emitter emitter,
        HealthFn health_fn,
        ShutdownFn shutdown_fn) noexcept;

    void* m_context = nullptr;
    Emitter m_emitter {};
    HealthFn m_health_fn = nullptr;
    ShutdownFn m_shutdown_fn = nullptr;
};

struct InstallOutcome final
{
    InstallStatus status = InstallStatus::invalid_config;
    DiagnosticsHost host {};
    DiagnosticsHealth health {};
};

[[nodiscard]] QIVEN_FOUNDATION_API InstallOutcome install_process_diagnostics(
    const ProcessDiagnosticsConfig& config) noexcept;

} // namespace qiven::diag
~~~

Normative install/lifetime semantics:

- exactly one DiagnosticsHost owns process diagnostics;
- a second full installation returns already_installed and creates no second writer/ring/handler set;
- an upgraded_legacy_compat result means the old CRT compatibility path had initialized the shared process core and the full install promoted that same core rather than creating a parallel core;
- ProcessDiagnosticsConfig views are borrowed only for the install call; implementation copies normalized values it needs;
- a successful host is move-only;
- modules receive only Emitter or a later versioned C ABI capability, never DiagnosticsHost;
- DiagnosticsHost destruction is not an implicit unbounded flush;
- shutdown() is explicit, bounded and idempotent;
- health() is a bounded snapshot and must not wait for disk or inspector progress;
- Requirement::disabled means that capability is not initialized and its health remains unavailable;
- a required capability that cannot be established prevents a normal installed result;
- an optional capability may yield installed_degraded with truthful health rather than fabricated readiness;
- field_count_limit is an 8-bit v1 bound and therefore cannot exceed the EventPacketV1 field_count representation;
- installation completes before ordinary worker threads, IPC and configuration paths that can invoke complex libraries.

The raw UTF-8 artifact path is a v1 transitional surface because Foundation does not yet publish the typed path primitive named by F0. The implementation must centralize conversion/canonicalization; it must not spread raw path comparison across the diagnostics subsystem. A future typed-path contract revision is explicit rather than silently changing path semantics inside I1.

#### Configuration and installation outcomes

- Zero initialization is a safe invalid configuration, not an undocumented production preset. Invalid enums, empty/embedded-NUL process names or artifact roots, non-absolute roots, invalid UTF-8, and zero build/workspace identities return `invalid_config`. The root is normalized once; inability to use a syntactically valid root is a capability/storage failure, not a syntax failure. Both capabilities disabled is invalid; disabled-capability policy fields are ignored.
- Enabled logging requires `event_byte_limit >= 112`, representable/aligned ring-slot arithmetic, nonzero normal/ring budgets, and nonzero retention limits with `max_total_bytes >= max_file_bytes`. The ring budget fits at least two full-size slots; normal memory includes encoding scratch, transport and writer staging. An event must fit the configured JSONL file limit after worst-case escaping/framing; installation either proves a conservative bound or rejects the policy. Zero `field_count_limit` admits field-free events only.
- The ring allocation is accounted separately by `crash_ring_budget_bytes`; it is available in I1 even when crash capture is disabled. Logging disabled leaves no Emitter/ring service; requesting `ring_tail` as required crash evidence in that configuration is invalid. Crash capture otherwise works without logging.
- Enabled crash capture requires nonzero storage limits, `max_total_bytes >= max_artifact_bytes`, and `0 < acknowledgement_timeout_ms < supervisor_deadline_ms`. The deployment's shared inspector enforces aggregate quotas across clients in addition to per-client limits, counting pending/partial artifacts. These limits do not authorize deleting another client's files.
- `required_classes` has at most one entry for each known non-unknown failure class. Reject duplicates, unknown bits and inconsistent context/evidence combinations. It must be nonempty for `capture = required`. Origin is exact; quality/evidence are tested for the named class and profile, never inferred merely from the existence of a dump or from the enum's numeric ordering. Optional capture with an empty list means best effort with truthful capabilities, not all-class coverage.
- The private deployment registration binds the inspector endpoint/profile, token/rights, allowed artifact root and predeclared per-class evidence. Its discovery, authentication, launch ownership and failure mapping are fixed in W0 before I2 code. Foundation neither guesses an executable from PATH nor silently changes machine-wide reporting settings.
- Success statuses are `installed`, `installed_degraded`, and `upgraded_legacy_compat`, each with a valid host. Promotion takes status precedence over optional degradation; callers must still inspect `health`. Any unmet required capability returns its failure status with an invalid host; optional failures preserve the working capability and report the precise health issue. I1 reports unimplemented I2 capture as unavailable/unsupported, never registered.
- Required logging means transport, writer, opened file sink and crash ring are ready together; failure returns logging_unavailable (or resource_exhausted for allocation failure). Optional logging may retain a usable subset with degraded health, including a live ring during sink failure. If every enabled optional capability is unavailable, a degraded control host may still report that state, but it exposes no fabricated usable Emitter.
- Installation is a serialized host startup transaction, before producer threads. Publish the full service only after required-capability checks succeed. A failed attempt unwinds only its own completed setup, leaves no writer or callback pointing at freed storage, and preserves a pre-existing legacy core. If bounded cleanup cannot finish, retain that setup storage and refuse a new install until cleanup finishes; never overlay it with a second core. After the first successful full install, restart/reconfiguration is outside v1, including after shutdown.

#### Lifetime, ordering, loss and shutdown

The DiagnosticsHost is the unique **control capability**, not the owner of
reclaimable callback memory. The core, thunk code, registered fatal callbacks,
and any storage they may still access remain resident until process exit.
This also applies to the shared library. A client module can unload only
after its own calls and workers quiesce; the service library cannot unload.

Moving the host transfers its fields and empties the source; self-move is a
no-op. Host move/destruction are externally serialized. A default/moved-from
host returns false, an empty Emitter, unavailable health and `invalid_host`.
Dropping a valid host performs no flush or reclamation; the host entrypoint
is responsible for explicit normal shutdown. A retained host-issued Emitter
remains safe after host move, destruction and completed/timed-out shutdown.

`emit()` is concurrent with other emits, `health()` and shutdown. Calls on
one host to `shutdown()` are externally serialized; health is concurrent.
Shutdown first closes normal admission. An emit overlapping this transition
either completes its already admitted bounded operation or returns unavailable.
Shutdown includes those in-flight operations in its drain deadline. No new
normal/ring event is admitted after the transition. Fatal evidence resources
remain installed and usable; normal shutdown is not crash-client uninstallation.

There is FIFO order for non-overlapping calls by one producer whose events
enter normal transport. Interleaved producers have no promised total order;
ring reservation order is separate from writer order. No v1 policy evicts an
already accepted event merely to admit a newer one.

`dropped_events` counts eligible normal events that fail admission;
`lost_after_admission` counts accepted events later definitely discarded due
to sink failure/shutdown. Ring drop/unavailability, filtering and truncation
have separate per-severity counters. Truncation counts once when any destination
publishes shortened content. Invalid inputs and unavailable calls are not
assigned a fabricated severity counter. Health counters are monotonic,
saturate at u64 maximum, and are individually coherent; a concurrent health
snapshot is not a transactional equation across counters.

Shutdown returns cumulative normal-service totals: `flushed_events` means a
complete JSONL record was written successfully (not power-loss durability),
`lost_events` means definitely discarded after admission, and `pending_events`
means its terminal fate is not yet known. A blocked in-flight write is pending,
not simultaneously counted as flushed or lost. A timeout closes admission but
retains all resources the writer/in-flight emits still use; no unbounded join,
thread destruction while joinable, use-after-free, or DLL unload follows it.
The writer may finish later; another shutdown can complete or time out against
its own deadline. `completed` requires quiescent producers/writer and zero
pending events; later calls return `already_stopped` with the same terminal
totals. Lost events can be nonzero even when shutdown completed. Zero timeout
requests a non-waiting close/status operation. The health state remains
stopping after timeout and becomes stopped only after safe quiescence.

#### Export support required at C0

`qiven/export.hpp` is a generated installed support header, created at C0
using CMake GenerateExportHeader (or equivalent mechanically verified output).
It defines `QIVEN_FOUNDATION_API`: empty for static consumers, dllexport/dllimport
for the Windows shared build/consumer, and default visibility for shared
platforms using hidden visibility. The selected exported CMake target propagates
the static/shared mode; consumers never guess it. This is a fifth packaging
dependency, not a provider header or an independently stable ABI.

The four source headers' declarations and export annotations are frozen.
Hash their canonical source bytes separately from the export generator recipe;
generated platform-specific bytes are verified per build tuple, not compared
to one universal digest. The existing CRT compatibility function also receives
the common export annotation at F1 without changing its C++ signature. A
module links only the client/header surface and calls the supplied Emitter;
host-role build/link gates reject embedded copies of the process service. A
runtime singleton in one image cannot detect or prevent every separately
statically linked copy by itself.

## 6. Internal wire contracts frozen before implementation

The following are not general public C++ APIs, but they cross process/thread ownership boundaries and therefore cannot be invented casually inside the implementation.

### 6.1 Event packet v1

The normal queue and crash ring may use different storage, but when a pointer-free encoded event is needed the logical packet is EventPacketV1.

All integer fields are little-endian. Current supported x64/ARM64 targets are little-endian; a future big-endian target requires an explicit new qualification.

Header layout:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | magic = 0x31564551 ("QEV1") |
| 4 | 2 | major = 1 |
| 6 | 2 | minor = 0 |
| 8 | 4 | total_size |
| 12 | 4 | EventId |
| 16 | 1 | Severity |
| 17 | 1 | field_count |
| 18 | 2 | flags |
| 20 | 4 | process_id |
| 24 | 4 | thread_id |
| 28 | 4 | source line |
| 32 | 4 | source column |
| 36 | 4 | reserved = 0 |
| 40 | 8 | monotonic_ns |
| 48 | 8 | wall_time_unix_ns |
| 56 | 8 | correlation.session |
| 64 | 8 | correlation.task |
| 72 | 8 | correlation.transaction |
| 80 | 8 | correlation.generation |
| 88 | 8 | correlation.action |
| 96 | 2 | source_file_size |
| 98 | 2 | source_function_size |
| 100 | 4 | message_size |
| 104 | 4 | fields_size |
| 108 | 4 | reserved = 0 |
| 112 | ... | file, function, message, field TLVs |

Each field TLV is:

~~~text
u16 key_size
u8  FieldKind
u8  reserved = 0
u32 value_size
key bytes
value bytes
~~~

Numeric values use exactly eight little-endian bytes; boolean uses one byte 0/1; text/bytes use value_size bytes.

Signed integers use two's-complement i64 representation; floating point is
IEEE-754 binary64 (qualified by a compile-time platform check). Header flags
bit 0 means a source/message/value was truncated; all other v1 bits are zero.
No key or numeric value is truncated. `fields_size` is the complete TLV region,
including TLV headers and keys. Require exactly `field_count` TLVs and
`total_size = 112 + source_file_size + source_function_size + message_size +
fields_size`, using checked arithmetic, with no trailing bytes. Reject unknown
major/minor versions, reserved bits, invalid enum/numeric widths, and malformed
retained text before consuming variable fields. New minor versions require
an explicitly compatible decoder; unknown is not silently compatible.

JSONL preserves integer precision, represents byte fields as a declared hex
or base64 encoding, and serializes fields as ordered typed entries so a text
value and bytes never become indistinguishable. Non-finite floats are rejected
at admission. The writer emits the packet truncation flag, not just the
in-memory EmitOutcome. The build-pinned decoder/schema, escaping, numeric
round-trip rules and literal interoperability fixtures are delivered at W0;
round-tripping one implementation through its own decoder is insufficient.

The encoder:

- never writes beyond the configured event_byte_limit;
- never allocates on emit after installation;
- preserves EventId, severity and correlation;
- marks every shortened source/message/text/bytes payload with the packet truncation bit and follows the destination-specific outcome rules in section 5.2;
- rejects structurally invalid fields rather than serializing ambiguous data;
- emits a packet that the writer and inspector validate before reading variable data.

### 6.2 Crash ring v1

The ring is preallocated at installation in a restricted shared mapping.
One authorized inspector may read live slots. Both sides use the selected
OS/ISA's documented interprocess atomics on aligned scalar words; an object
dump of `std::atomic`, its size, or C++ release/acquire alone is not a portable
wire specification. Windows v1 uses shared-memory Interlocked operations
with full barriers; another platform must qualify its own adapter before
claiming live-reader support.

Header (64 bytes, little-endian; mapping and slots aligned to 64 bytes):

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | magic = 0x314E5251 ("QRN1") |
| 4 | 2 | major = 1 |
| 6 | 2 | minor = 0 |
| 8 | 8 | total_bytes |
| 16 | 4 | slot_count |
| 20 | 4 | slot_stride |
| 24 | 4 | payload_capacity = event_byte_limit |
| 28 | 4 | atomic flags: bit 0 accounting degraded; other bits zero |
| 32 | 8 | atomic last_reserved_sequence, initially 0 |
| 40 | 8 | atomic dropped_records, initially 0 |
| 48 | 8 | process_instance |
| 56 | 8 | reserved = 0 |

Each slot has this fixed prefix, followed by its payload and zero padding:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | atomic access_state: 0 free, 1 writer, 2 inspector |
| 4 | 4 | reserved = 0 |
| 8 | 8 | commit_sequence, initially 0 |
| 16 | 4 | payload_size |
| 20 | 4 | flags = 0 |
| 24 | payload_capacity | EventPacketV1 and unused zero bytes |

`slot_stride = align_up(24 + payload_capacity, 64)` and
`total_bytes = 64 + slot_count * slot_stride`; arithmetic must be checked
against the mapping size and configured budget before any slot access.
`process_instance` is bound by authenticated registration, not PID alone.

Writer protocol:

1. Reserve a unique sequence in `[1, 2^63 - 1]`. A bounded reservation
   attempt may drop on contention; no retry-until-success loop is permitted.
   At exhaustion the ring becomes unavailable until process exit; counters
   never wrap a sequence into a live generation.
2. Select `(sequence - 1) % slot_count`. Try exactly one atomic claim from
   access_state 0 to 1. If busy, count a dropped ring attempt and return.
3. While holding that claim, reject an already committed sequence greater
   than or equal to this reservation. A delayed old producer cannot overwrite
   a newer generation. Set commit_sequence to `(sequence << 1) | 1`.
4. Write payload_size, flags and the validated packet; zero unused bytes.
   Set commit_sequence to `sequence << 1`, then release access_state to 0.
   No payload or metadata writes occur after releasing the claim.

Inspector protocol:

1. Fix an upper reservation watermark at pass start (or use the committed
   crash capsule's watermark). Scan at most slot_count slots once, with at most one
   held claim. Try one atomic 0-to-2 claim per slot; busy slots are skipped
   and recorded as partial, without waiting or retrying them in this pass.
2. While owning a slot, read its sequence/size, copy only within capacity,
   validate EventPacketV1, then release the claim on every normal/error exit.
   Accept only a nonzero even sequence at or below that watermark, with the
   correct slot mapping and validated size; sort accepted records by sequence
   for the output. Report overwritten/skipped generations as missing, not complete.
3. Report copied, busy, malformed, empty and after-watermark counts separately,
   plus detected history gaps. Do not invent exact missing/overwritten history
   counts when overwritten generations are no longer observable. D12 freezes
   this bounded snapshot artifact for the I2 profile.
   A set of complete records is not a simultaneous whole-ring snapshot or
   proof of complete history. An initially empty ring differs from a failed
   snapshot. Snapshot storage/work is bounded by the declared ring budget.

The exclusion claim, not equal sequence observations, protects the plain
payload. A writer or inspector that dies while holding a slot leaves that
slot unavailable for the rest of this process instance. No age-based reclaim
is safe: a suspended owner might resume. A restarted inspector skips orphaned
claims; degradation is visible through busy/loss counts. This sacrifices a
slot rather than permitting races or unbounded producer waiting. Tests cover
death on both sides and quantify contention loss against I0 budgets.

An out-of-process ReadProcessMemory copy bracketed by equal sequence words
is not this protocol. That API documents a memory copy, not a transactional
snapshot or acquire synchronization with the target. It remains usable for
a separately qualified immutable/quiescent dump, but cannot replace slot
ownership for live reads. Unsynchronized in-process live payload reads remain
invalid; a test using the same ownership law is race-free but does not by
itself qualify the real interprocess adapter.

All diagnostic counters use fixed-operation or bounded-attempt accounting.
If an exact saturation-safe counter update cannot complete within its bound,
set a sticky atomic accounting-degraded bit (`counters_exact = false` in
LoggingHealth; bit 0 in the ring header and the inspector receipt);
its last numeric value is explicitly a lower bound. Never spin indefinitely
to preserve a telemetry counter. Such degradation fails a gate that requires
exact accounting and cannot be hidden as a successful measurement.

### 6.3 Crash capsule v1

The crash capsule is a fixed 8192-byte little-endian buffer. It is pointer-free.

Logical offsets:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 8 | publication sequence |
| 8 | 4 | magic = 0x4E564451 ("QDVN") |
| 12 | 2 | major = 1 |
| 14 | 2 | minor = 0 |
| 16 | 4 | encoded_size = 8192 |
| 20 | 4 | process_id |
| 24 | 4 | thread_id |
| 28 | 2 | CrashFailureClass |
| 30 | 1 | CrashContextOrigin |
| 31 | 1 | CrashContextQuality |
| 32 | 4 | CrashEvidenceFlag bits |
| 36 | 1 | CrashCaptureState |
| 37 | 1 | architecture tag |
| 38 | 2 | reserved = 0 |
| 40 | 4 | native_exception_code |
| 44 | 4 | synthetic_exit_code |
| 48 | 8 | process_instance |
| 56 | 8 | build_identity.high |
| 64 | 8 | build_identity.low |
| 72 | 8 | workspace_identity.high |
| 80 | 8 | workspace_identity.low |
| 88 | 4 | reason_size |
| 92 | 4 | context_size |
| 96 | 8 | crash_ring_sequence |
| 104 | 2 | platform: 0 unknown, 1 Windows, 2 Linux, 3 macOS |
| 106 | 2 | context_encoding, 0 when context_size is 0 |
| 108 | 20 | reserved = 0 |
| 128 | 512 | reason bytes |
| 640 | 4096 | context bytes |
| 4736 | 3456 | reserved for v1, zero when emitted |

The capsule is in a registered shared mapping and has exactly one publication
per process instance. The first fatal entrant claims 0-to-1 with the qualified
interprocess atomic. It writes the initialized buffer, then publishes 2 with
release/full-barrier semantics and signals the inspector. The inspector uses
the matching qualified acquire/full-barrier load and copies only after
observing 2. D12 defines Windows-x64 read-only publication loads; an Interlocked
read/modify/write operation cannot be used on a read-only mapping.
Published bytes never change; observer results go to a separate receipt.
Secondary/reentrant faults never rewrite or publish over the first capsule.
They take the bounded emergency/termination path; if they shorten the primary
capture, the final result is partial. This is not a multiwriter seqlock.

Architecture tag values are frozen as 0 = unknown, 1 = x86_64 and 2 = arm64. Reason bytes are UTF-8. Context bytes are opaque platform-qualified data.

The platform, architecture, context_encoding, origin and exact build identity
select a decoder. W0 freezes that decoder's byte schema, register/exception
record fields, context flags, limits and literal fixtures before I2 writes it.
Copying a native `CONTEXT` or `EXCEPTION_POINTERS` object wholesale is not a
schema. Register values/fault addresses may be encoded as numeric evidence;
linked-record or address-space pointers are not references the decoder may
dereference. The inspector reconstructs its own native exception structures
and explicitly selects the matching provider/ClientPointers mode. If the
required context cannot fit, report partial/reason-only evidence and the
failed requirement; never silently truncate context while claiming native_context.

`crash_ring_sequence` is the atomically observed last_reserved_sequence at
fatal publication, or zero if no ring exists. It bounds the requested history,
not the last successfully committed record. Later reservations are excluded
from the crash tail; missing, still-owned or overwritten earlier slots remain
explicit gaps. A reserved sequence is never itself proof of an event.

At publication, evidence flags describe only evidence already present:
reason/context, not a future external dump, ring snapshot or final manifest.
Capture acknowledgement means required target-dependent dump/snapshot data
was written, closed and minimally validated; it permits bounded target exit.
It does not mean final manifest publication/durability succeeded. Only the
inspector's final CrashObservation/manifest may report completed and artifact
bits after all selected-profile requirements close. Hashing/publication
failure after acknowledgement leaves a partial final receipt.

The inspector rejects:

- unknown major version;
- unknown minor/platform/context-encoding combination or nonzero reserved bits;
- encoded_size other than the v1 size;
- impossible enum values;
- reason/context lengths beyond their fixed capacity;
- odd/torn publication;
- stale process-instance identity;
- PID reuse;
- mismatched build identity;
- a future revision that introduces a process-local pointer into the wire contract.

## 7. Legacy CRT compatibility contract

include/qiven/crt_failure.hpp keeps its current public signature during migration:

~~~cpp
namespace qiven
{
inline constexpr int crt_failure_exit_code = 3143;
void install_headless_crt_failure_behavior();
}
~~~

No new legacy capability is added. F1 may include the common export support
and annotate this function as specified in section 5.4; its signature remains.

Its implementation changes as follows when the common service lands:

- calling it first creates/uses only the legacy-compatible state of the same Foundation process core;
- a later full install promotes that state and returns upgraded_legacy_compat;
- calling it after the full service exists is idempotent with respect to process-global callbacks and creates no second writer/ring/handler set;
- it continues to suppress modal CRT behavior during migration;
- it stops using CRT formatting/length routines and a possibly broken stderr dependency on the qualified fatal path;
- the fixed 3143 exit code may remain as a compatibility discriminator but is not the only evidence;
- new production hosts use install_process_diagnostics(), not this wrapper.

The implementation must not preserve two separately maintained callback stacks merely to keep the legacy function alive.

Disabling new crash capture does not undo headless compatibility behavior
already installed through the legacy entrypoint. Record this legacy-only mode
explicitly; it grants no inspector/native-context coverage. There is no v1
uninstall/restore operation during ordinary logging shutdown.

## 8. I1 implementation contract

I1 starts only after C0 headers compile and I0-R has fixed the revised
workload/budgets. Native probes require S0; consumers of private wire schemas
require W0. These are the section-11 entry gates, not implied delivered work.

The implementation must satisfy:

### 8.1 Producer

- no general-heap allocation in Emitter::emit() after installation;
- no filesystem, synchronous flush, stdio or iostream;
- bounded validation and encoding;
- explicit drop/truncation outcome;
- stable event identity;
- event/caller views never retained;
- no dependency on writer liveness;
- no unbounded retry.

### 8.2 Normal transport/writer

- finite memory fixed by the installed budget;
- one process writer service;
- bounded disk usage;
- versioned JSONL output with at minimum schema version, event id, severity, timestamps, pid/tid, correlations, source, message and fields;
- sink failure changes health and never converts producer calls into unbounded waits;
- shutdown obeys its caller-provided timeout;
- health counters remain readable while the writer is degraded.

### 8.3 Crash ring

- uses the section-6.2 publication law;
- accounts for inspector contention, orphaned claims and sequence exhaustion;
- no target-side concurrent writers to the same slot;
- no in-process unsynchronized live payload reader;
- external snapshots never present a torn record as complete;
- critical ring attempt does not depend on normal queue capacity or writer progress.

### 8.4 F1 packaging

The same source contract is built as:

- current static/lockstep form; and
- one lockstep shared-library form with explicit intended exports and hidden private symbols where supported.

The first F1 transaction also repairs the F0-recorded public-header file-set omission mechanically. Installed-package tests must consume the installed target from outside the source tree.

The C0 export support is exercised with hidden visibility enabled, explicit
symbol inventory, and host-plus-client-module consumers. A separate static
host executable and a separate shared host executable both pass; loading two
static service copies into one process is rejected as a packaging error.

A shared build remains a lockstep C++ contract, not a promise of independently stable C++ ABI.

## 9. I2 implementation contract

I2 reuses the same process core; it does not add a second process-global diagnostics owner.

Before the private crash provider is selected, the PR #6/I0 comparison still runs for the required Windows classes. Crashpad, a Qiven client+inspector and platform facilities remain implementation/provider candidates behind the frozen public contract.

The I2-only annex now nominates the Qiven client/external DbgHelp inspector
for implementation, with the predeclared comparison/decision gate in D13.
D11-D12 freeze its internal ownership, launch/rights, context and artifact
contracts. Nomination is not a measured provider-selection result.

Fatal callback invariants:

- no heap allocation;
- no application lock;
- no normal logger/writer dependency;
- no stdio or iostream;
- no CRT formatting or strlen;
- no complex JSON;
- no recovery attempt;
- bounded fixed memory;
- reentrancy guarded;
- independent emergency evidence path;
- bounded handoff to the inspector;
- the external supervisor owns the ultimate stuck-process deadline.

Context truth rules are those in crash.hpp. Provider behavior must map into that vocabulary rather than changing the vocabulary to fit a provider.

Installation of callbacks is qualified per linked CRT instance and for
thread-local/later overrides. One process core does not imply one CRT handler
table. Lockstep client modules use the qualified shared-CRT tuple; an additional
CRT instance needs a host-coordinated private adapter and explicit coverage,
or is marked uncovered. A Foundation `QIVEN_ASSERT`/contract-failure path
joins the same fatal core with a constant bounded reason; it must not pass
through Emitter or instantiate another callback subsystem. The public legacy
and contracts entrypoints are source-compatible adapters, not independent owners.

Crash storage includes partial files and final artifact sets; a dump that
would exceed quota is failed/partial and terminated by the qualified provider
or external deadline. An emergency handle write is best effort, not a promise
that the OS/storage will respond within an in-process timeout. The external
supervisor runs independently of the failing target, captures its exit, and
enforces the deadline. Failure of capture never becomes a recovery/continuation
claim.

## 10. Mechanical acceptance matrix

### 10.1 Contract/header gates

| ID | Gate |
| --- | --- |
| H01 | Every new public header is self-contained when compiled alone. |
| H02 | No public diagnostics header includes a private provider header. |
| H03 | No public source-level diagnostics type is presented as an independently stable C++ ABI. |
| H04 | Asynchronous service state retains no borrowed EventView payload pointer after emit() returns. |
| H05 | DiagnosticsHost is move-only; modules receive Emitter, not host ownership. |
| H06 | A second install cannot create a second writer/ring/fatal-handler set. |
| H07 | Legacy CRT-first then full-install promotes one process core. |
| H08 | Any implementation commit that changes a section-5 declaration is flagged for CONTRACT-CONFLICT review before further implementation. |
| H09 | Extract the four header blocks mechanically; compile them against the pinned real Foundation types.hpp and generated export support, individually, in static/shared and no-exceptions/no-RTTI modes. No manually retyped header fixture. |
| H10 | Default/invalid configs, every required/optional/disabled combination, failed-install cleanup, legacy promotion with degraded capture, and reinstallation after shutdown follow the specified result table. |
| H11 | Host move/self-move/moved-from operations, saved emitters, concurrent emit/health/shutdown and timeout followed by late writer completion have no dangling state. |
| H12 | Static/shared installed symbol inventory and host-plus-module linkage reject duplicate service copies. Function-call signatures alone do not enforce process-role ownership. |

At the Foundation C0 contract-landing commit, record SHA-256 for the four public
headers and the export generator recipe, bound to the accepted PR #9 document
commit. I1/I2 gates compare against that independent accepted baseline, not
an expected hash editable inside the same implementation change. Semantic
probes remain required: identical declarations do not prove correct behavior.

### 10.2 I1 logger gates

| ID | Probe | Required result |
| --- | --- | --- |
| L01 | multi-thread producer stress | no corruption/deadlock; all waits bounded |
| L02 | instrumented allocator around emit | zero general-heap allocations after install |
| L03 | queue full at trace/debug/info | immediate typed drop allowed; counter advances |
| L04 | queue full at warn/error | one total configured wait budget; typed outcome; predeclared wall-time tolerance recorded |
| L05 | queue full at critical | crash-ring attempt still occurs; normal queue may drop after bound |
| L06 | writer deliberately stalled | producers remain bounded; health reports degradation/loss |
| L07 | sink write failure | no producer hang; health changes |
| L08 | disk full | bounded failure and bounded storage accounting |
| L09 | rotation/retention | file count/bytes remain within declared policy |
| L10 | readable but malformed EventView values/lengths | rejected_invalid before any OOB access; dangling ranges are caller precondition violations |
| L11 | oversized text/bytes | valid bounded packet or rejected event; truncation is explicit |
| L12 | caller storage destroyed after emit | queued event remains valid |
| L13 | module source strings disappear after module unload following completed emit | queued record uses copied/encoded bytes, not dangling module pointers |
| L14 | crash-ring multiwriter wrap | one writer per slot; conflicting reuse drops instead of waiting |
| L15 | live ring snapshot during writes | external reader accepts only stable committed records |
| L16 | shutdown healthy | bounded drain; completed only after quiescence; timeout is truthful if deadline is insufficient |
| L17 | shutdown with blocked sink | returns timed_out within supplied timeout; no indefinite join |
| L18 | Debug/Release event identity | EventId and correlation semantics identical |
| L19 | installed static consumer | external source-tree consumer links/runs |
| L20 | installed shared consumer | external consumer loads/runs with one process service |
| L21 | no-exceptions/no-RTTI contract build | remains buildable |
| L22 | Linux/macOS compile path | portable headers/core compile; this alone makes no crash-coverage claim |
| L23 | persistent/ring floor cross-product, including critical | typed filtering distinct from loss; ring-only outcomes and truncation flags truthful |
| L24 | writer I/O succeeds/fails after shutdown timeout | cumulative flushed/lost/pending totals settle without double count or UAF; saved Emitter stays safe |
| L25 | writer/inspector dies holding ring slot; delayed old reservation | bounded drops, explicit partial snapshot, no reclaim race or stale-generation overwrite |
| L26 | ring sequence/counter boundary fixtures | no ABA/wrap; exhaustion/degraded accounting visible without unbounded retries |
| L27 | byte-level independent event/ring fixtures | exact offsets, version/flags, size/overflow/UTF-8/numeric validation; no self-round-trip-only proof |
| L28 | source-derived concurrency oracle intentionally fails | workers are signaled and joined; original test failure survives instead of terminate/hang |
| L29 | revised event workload versus baseline/private providers | I0 producer, memory, CPU, startup and static/shared budgets met on the exact contract candidate; old narrow-event numbers do not qualify it |

### 10.3 I2 crash/failure gates

The previous PR #9 crash matrix remains useful evidence and is retained with the corrected context/artifact vocabulary:

| ID | Probe | Required result |
| --- | --- | --- |
| C01 | Release UCRT invalid parameter, healthy inspector | no modal; callback_context; completed selected-profile dump; matching Release symbols |
| C02 | Debug UCRT invalid parameter | no Debug Assertion modal |
| C03 | abort() | no modal; bounded reason; callback-origin classification |
| C04 | uncaught exception -> std::terminate | no modal; terminating-thread callback context |
| C05 | pure virtual call | no modal; typed callback-origin class |
| C06 | unhandled access violation | native_context; actual faulting instruction resolves |
| C07 | stdout/stderr closed/aliased | evidence survives through independent path |
| C08 | normal writer stalled | crash capture remains independent |
| C09 | allocator unavailable/failing fixture | qualified callback path performs no heap allocation |
| C10 | recursive crash-handler failure | reentrancy guard terminates without loop/deadlock/second modal |
| C11 | inspector absent before install | truthful degraded health; no completed claim |
| C12 | inspector dies after registration | partial/failed state; no completed claim |
| C13 | destination unwritable | typed storage failure |
| C14 | disk full | bounded failure; no false durable claim |
| C15 | target exits before capture completion | partial unless qualified capture already completed |
| C16 | direct fail-fast | no false in-process coverage claim; external evidence only if separately qualified |
| C17 | pre-main/static-init failure | observer/external profile only |
| C18 | mismatched symbols | symbolization rejects mismatch |
| C19 | correct Release symbols | build/image identity matches and required frame resolves |
| C20 | stale PID/process registration | rejected |
| C21 | malformed capsule lengths/version | rejected without OOB read |
| C22 | torn capsule publication | never accepted as committed |
| C23 | crash-ring snapshot concurrent with overwrite | only stable committed records emitted |
| C24 | two simultaneous crashing clients | artifacts stay process-bound; provider serialization requirements obeyed |
| C25 | repeated full install | already_installed; no duplicate service |
| C26 | loaded module attempts host install | rejected by ownership gate |
| C27 | legacy wrapper before full install | later full install upgrades same process core |
| C28 | legacy wrapper after full install | no second callback stack |
| C29 | static/shared packaged consumers | one process service |
| C30 | crash storm | bounded retention; loss/cleanup visible |
| C31 | healthy capture timing | acknowledgement meets the accepted <=5 s budget for the selected profile |
| C32 | hung capture/storage | external supervisor classifies by the accepted 30 s deadline |
| C33 | installation timing | meets the accepted <=5 ms one-shot profile budget |
| C34 | qualified fatal-path import/call audit | no snprintf/strlen/stdio/general heap/ordinary logger lock/application mutex |
| C35 | modal detector around every callback probe | zero interactive child-owned windows |
| C36 | thread-local invalid-parameter-handler interference | claimed coverage still holds or is explicitly uncovered |
| C37 | Crashpad/Qiven A/B on required classes | same candidate/profile; evidence/cost/degradation recorded |
| C38 | WER/direct-fail-fast profile | separate external capability; no unsupported coexistence claim |
| C39 | two fatal threads in one process; recursive failure during publication | one immutable capsule/identity; secondary failure never overwrites it; shortened capture remains partial |
| C40 | registered inspector authentication/rights and stale mapping | untrusted registrations rejected; shared slots and process instance cannot be substituted by PID alone |
| C41 | capture acknowledged, final manifest/hash publication fails | target may exit; final receipt remains partial and never claims final durability |
| C42 | zero/insufficient artifact quota, concurrent clients and partial files | per-client and aggregate storage/pending limits enforced; failures visible |
| C43 | original Release stderr-corruption regression and Foundation contract failure | minimal fixture works without I1; common fatal core used; old-fail/new-pass evidence retained |
| C44 | context schema/decoder literal fixtures and over-capacity native context | inspector reconstructs correct context; incompatible/oversized representation cannot claim native quality |
| C45 | disabled/optional/required class and artifact requirements | required startup failure and optional degradation reflect the actual named profile, including CRT-instance limitations |

### 10.4 Native-probe safety gate

Every native diagnostics test binary, including ordinary concurrency/packaging
tests that could terminate unexpectedly, runs under the external watchdog.
Deliberate crash/abort/invalid-parameter tests additionally assert their exact
expected exit and evidence; an ordinary test uses a zero-success exit policy.

On Windows that harness must:

- enforce a hard process deadline;
- detect child-owned interactive top-level windows during the probe;
- track the child process tree and correlate OS crash-report windows when a
  reporter owns the modal; never terminate unrelated windows/processes;
- classify and terminate a modal/hung child;
- collect the one run's evidence;
- return control to CTest/operator without an automatic blind retry loop.

The gate proves the native failure once per case. Retry policy belongs above the test binary and is never implemented as an uncontrolled loop in the failing process.

The watchdog/modal fixture is qualified before the first concurrency or fault
probe. CTest's outer timeout is a second containment layer, not a substitute
for per-child deadline, process-tree cleanup and failure classification.

This is the mechanical replacement for relying on an agent session to remember "stop after the first crash".

## 11. Implementation sequence after PR #9 acceptance

The source contract lands first. PR #6/ADR-0059 still permit I1 and I2 to
proceed independently after I0/F0; a completed logger is not an I2 evidence
prerequisite. The original recovery sequence accidentally serialized the
urgent fatal-path proof behind all I1/F1 work. The corrected dependencies are:

~~~text
C0  land four exact public headers + generated export support
    self-containment checks + accepted-baseline SHA-256 records
    no logger/crash implementation yet

I0-R
    reconcile the existing census/comparison with the revised payload/API
    freeze workloads, provider adapters, budgets and failure oracles
    retain valid prior evidence; rerun only invalidated measurements

S0
    qualify watchdog/modal/process-tree containment and test thread cleanup
    before any native diagnostics probe

W0
    land private codec/registration schemas, independent wire fixtures
    and source-backed interprocess atomic adapters before their consumers

E1 (after I0-R/S0/W0; without requiring I1/F1)
    minimal original Release regression + callback/native-context proof
    predeclared I2 provider comparison behind the frozen public vocabulary

I1-A
    implement or reuse the common process core + legacy ownership bridge
    Emitter admission + bounded transport

I1-B
    writer + JSONL sink + retention + health + bounded shutdown

I1-C
    crash-visible ring using W0 and the frozen slot-ownership protocol

F1
    static/shared packaging + installed consumers + one-service proof

I1 review
    fresh review against the frozen contract and L01-L29

I2-A..E (after E1/W0; shares the core contract, not logger liveness)
    crash capsule/emergency path
    qualified Windows capture callbacks + complete legacy fatal-path absorption
    inspector
    degradation/bypass qualification
    Foundation fixtures + exact Runtime/Devkit handoff; real host migration is I3

I2 review
    C01-C45 and exact Release receipts
~~~

No step requires recovering v48 as code authority. Work scheduling may be
serial; these are dependency boundaries, not a requirement to run parallel
agents or concurrent implementation sessions.

D10-D13 refine the I2 lane into B00-B06. B00-B05 deliver a useful crash-only
profile, including the common core if I1 has not landed it. B06 alone depends
on the genuine I1 writer/ring. The I2 annex fixes the context/registration/
acknowledgement/artifact portions of W0; their literal fixtures and qualification
remain implementation work. It does not complete the I1 JSONL design or
turn design documents into runtime evidence.

W0 is a bounded design/fixture deliverable, not permission for the implementation
model to improvise another public API. It specifies: exact JSONL keys/schema
and typed byte rendering; the ring's qualified atomic adapter; platform context
encoding and literal fixtures; authenticated registration and process-instance
binding; and acknowledgement versus final-manifest records. The inspector's
registration includes mappings/rights, extents, schema/build identity and
process-creation identity. Header bytes alone cannot authenticate a client.
Missing W0 evidence blocks the affected ring/I2 slice. Changes to section 5 or
the frozen section-6 semantics use CONTRACT-CONFLICT, not a W0 escape hatch.

I0-R fixes memory/retention, producer p50/p95/p99 and maximum observed wait,
writer CPU, startup, shutdown, inspector timing and packaging metrics before
measurement. Reusing the old comparison as provenance is appropriate; its
narrower event payload and invalidated bad-root/memory probes cannot establish
the revised contract's cost or correctness. The existing 5 ms startup, 5 s
healthy acknowledgement and 30 s external deadline are profile budgets to
reconcile with the pinned census, not universal OS guarantees. Startup timing
must say whether the inspector was already running and include real registration.

## 12. What the implementation model may and may not change

Without reopening this contract, the implementation model may create:

- src/diag private classes;
- private queue/ring helpers;
- platform adapter headers;
- provider adapter headers;
- sink implementations;
- test-only executables;
- watchdog/modal-detection test harnesses;
- benchmarks;
- CMake implementation details needed to satisfy F1.

It may not silently:

- change the declarations in section 5;
- add a second global diagnostics singleton;
- route a fatal callback through the normal logger;
- make Emitter own caller memory;
- expose provider types publicly;
- make a loaded module a process-service owner;
- turn the old CRT helper into a second subsystem;
- claim in-process callback/native context that was not actually supplied;
- treat a nonempty dump as proof of correct context;
- treat successful Windows probes as Linux/macOS crash coverage.

## 13. v48 accident branch salvage map

The previously local v48 work is now available remotely as
`jason-extended-cognition/v48-f1-packaging`, sealed head
`7e128ab1321c59764b76b72d6bc48bd55f619542`. The branch records
`4edc08b9...` as its historical green I1 review base and `3e893cd4...`
as its historical green F1 packaging base; these are prior test verdicts,
not a fresh correctness endorsement. The sealed head is intentionally red.

This branch is evidence and a component mine, not a branch to merge or
continue wholesale.

### 13.1 Directly reusable or adaptable material

The following work has independent value and should be inspected when
implementing the frozen contract rather than recreated blindly:

- the tracked out-of-tree F1 smoke-consumer project;
- the install/export/relocation and Windows DLL-loader proof structure;
- static-versus-shared measurement plumbing, subject to revalidating the
  metric and budget against the new emitter contract;
- JSONL escaping and one-physical-line structural oracle;
- rotation/retention failure accounting tests;
- installed-package Debug/Release matrix;
- header self-containment test wiring;
- typed/owned failure-message lesson from the dangling `e.what()` fix;
- bounded native-test timeout patterns;
- per-writer distinct-payload oracle as a useful negative test for mixed
  records.

These are implementation/test assets. They do not preserve the old
public declarations.

### 13.2 Material that must not be copied as architecture

The old public `event.hpp`, `service.hpp` and `ring.hpp` declarations
do not govern the new implementation. In particular:

- the old `emitter::emit()` returns void and hides admission/drop
  disposition, conflicting with the frozen typed EmitOutcome;
- service ownership is exposed through process-global free functions and
  borrowing implementation pointers rather than the frozen move-only
  DiagnosticsHost capability;
- raw `const char*` path/config and chrono-bearing public configuration
  do not match the v1 frozen contract;
- `crash_history()` exposes a live in-process ring object to arbitrary
  readers, conflicting with the new out-of-process live-reader rule;
- the old event model is materially narrower than the frozen field,
  correlation and source-location contract;
- all events are mirrored into the crash ring in the old service rather
  than obeying the configured ring floor;
- the old general lane uses mutex/condition-variable admission and
  drop-oldest semantics chosen during implementation; these remain
  implementation candidates only where they satisfy the new producer
  contract and measured budgets.

### 13.3 Concrete accident-chain evidence

The sealed branch contains three separate defects that interact.

First, `crash_ring::capture_newest()` copies a plain
`ring_record` while target-process writers concurrently modify the same
plain object. Atomic sequence words around that copy do not make the
payload accesses race-free under the C++ memory model. The branch itself
already acknowledges that a mixed record cannot be excluded by the
seqlock shape. The v1 contract therefore does not admit an
unsynchronized in-process live reader. Merely moving the same copy to
ReadProcessMemory does not establish a consistency proof; section 6.2
now supplies actual reader/writer exclusion.

Second, the concurrent test starts joinable `std::thread` writers and
then performs multiple direct `return` statements from inside the
validation loop. If any mixed/torn oracle fails, stack unwinding destroys
the vector while those threads are still joinable; the C++ standard
requires `std::thread::~thread()` to call `std::terminate()` in that
state. That converts the first diagnostic assertion into a fatal CRT
path. Before the headless primitive was installed first, repeated test
reruns could therefore surface repeated modal abort dialogs rather than
the original ring-validation failure.

This does not prove that every popup in the incident had one identical
origin, but it provides a concrete, source-level reproducer class for
the observed `std::terminate` behavior and explains why treating the
CRT popup itself as the primary defect sent debugging in the wrong
direction.

Third, `crash_ring::write()` stores a new odd sequence and writes the
slot payload **without acquiring exclusive slot ownership**. Its CAS
is at the end. Two writers whose reservations wrap to the same slot can
therefore write the same plain payload simultaneously; a failed final CAS
cannot undo that data race. The fault is not confined to the snapshot reader.
The new protocol claims the slot before any payload/metadata write and also
rejects delayed stale reservations.

The replacement tests use RAII cleanup that both requests the stop condition
the workers actually observe and joins them. Replacing `std::thread` with
`std::jthread` alone is insufficient if workers still poll an unrelated
atomic `stop` that an early return never sets. Force an oracle failure as
a negative test and verify prompt cleanup with the original failure code.
All native diagnostics tests stay under the watchdog; intentional crash
probes remain separate child processes.

### 13.4 Reuse rule

The implementation session starts from F0 main plus the C0 frozen
headers. It may copy or adapt a v48 implementation/test fragment only
after mapping it to a current contract requirement.

No v48 public header is copied wholesale, and the v48 branch is never
merged/cherry-picked as the I1 base. Test/packaging assets may be ported
in small reviewable commits with their original provenance retained.

## 14. Relationship to accepted PR #6

This proposal does not reverse PR #6.

PR #6 remains the architectural authority for:

- Foundation ownership;
- Runtime/Devkit division;
- I0-I3/F0-F3 program boundaries;
- provider comparison law;
- cross-platform qualification;
- static/shared versus independent module ABI distinction;
- honest crash-evidence claims;
- Release receipts and performance budgets.

PR #9 supplies what PR #6 intentionally did not freeze: the first C++ source
contract and the mechanical rule that implementation cannot redesign it
during a fix loop. Private W0 schemas remain explicit entry deliverables;
source declarations alone do not mean the inspector protocol is implemented
or qualified. I0's measured mechanism selection still governs private transport
choices. The safe ring protocol is a reference contract whose performance
must be measured, not an asserted optimal algorithm.

Where this document is more precise than an illustrative PR #6 sketch, this document governs the I1/I2 implementation surface while preserving PR #6 semantics.

## 15. Acceptance meaning

Accepting PR #9 means:

- the old PR #9 I2-entry/local-recovery/Skill plan is superseded;
- published F0 main is the clean implementation base;
- the section-5 headers become the normative first diagnostics C++ contract;
- C0 lands those headers before logger implementation;
- I0-R/S0/W0/E1 have the explicit dependencies above; I2 proof is not blocked on a complete general logger;
- GLM/ZCode implementation work is constrained to the allowed implementation space;
- a public-header conflict becomes an explicit design transaction instead of an implementation-side edit;
- logger/crash correctness is decided by compiler, tests, watchdogs, platform probes and exact receipts.

Acceptance does not claim that I1, I2, shared-library packaging, a crash provider, Linux/macOS crash coverage or a stable independent module ABI has landed.

## 16. Evidence

Read-only evidence used in the 2026-10-01 review:

- [PR #9 reviewed document](https://github.com/JasonHuang3D/qiven-docs/blob/5b13540297649e0ef7ad81851ca02d2eafde6c07/proposal/2026-09-29/00-foundation-diagnostics-contract-v1.md).
- [Accepted PR #6 program](https://github.com/JasonHuang3D/qiven-docs/blob/2dc2f0f83668892b06959384dd4e2efdc009b3e2/accepted/2026-09-29/00-cpp-diagnostics-infrastructure-program.md), its adjacent diagnostics design and Foundation-boundary amendment.
- Foundation at `032a6152efa18407d7b8d29d3203a5def179385a`: [architecture](https://github.com/JasonHuang3D/qiven-foundation/blob/032a6152efa18407d7b8d29d3203a5def179385a/docs/architecture/foundation.md), [F0 baseline](https://github.com/JasonHuang3D/qiven-foundation/blob/032a6152efa18407d7b8d29d3203a5def179385a/docs/architecture/cpp-diagnostics-f0-baseline.md), CMakeLists, types.hpp, crt_failure.hpp and crt_failure.cpp. F0 has no published I1/I2 implementation.
- Context at `f68ea826bda52a38c6a125469ff1eda492929c7a`: [ADR-0059](https://github.com/JasonHuang3D/qiven-context/blob/f68ea826bda52a38c6a125469ff1eda492929c7a/decisions/ADR-0059.md), [I1 measurement audit](https://github.com/JasonHuang3D/qiven-context/blob/f68ea826bda52a38c6a125469ff1eda492929c7a/evidence/audits/2026-09-29-i1-measurement-batch.md), and [modal incident audit](https://github.com/JasonHuang3D/qiven-context/blob/f68ea826bda52a38c6a125469ff1eda492929c7a/evidence/audits/2026-09-29-v48-diag-ring-modal-incident.md). These are retrieved repository evidence, not implementation authority or a reconstruction of prior conversations.
- Sealed v48 `7e128ab1321c59764b76b72d6bc48bd55f619542`: [ring.hpp](https://github.com/JasonHuang3D/qiven-foundation/blob/7e128ab1321c59764b76b72d6bc48bd55f619542/include/qiven/diag/ring.hpp) and [diag_ring.cpp](https://github.com/JasonHuang3D/qiven-foundation/blob/7e128ab1321c59764b76b72d6bc48bd55f619542/tests/diag_ring.cpp). Targeted source inspection supports the three defect classes in section 13; it does not identify every popup's exact runtime origin.

Platform/specification checks (retrieved 2026-10-01):

- [Microsoft ReadProcessMemory](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-readprocessmemory) specifies copying accessible memory and transferred-byte/error results, not snapshot consistency. The rejection of sequence-only live-copy proof is a design inference from that documented boundary.
- [Microsoft Interlocked Variable Access](https://learn.microsoft.com/en-us/windows/win32/sync/interlocked-variable-access) documents shared-memory use by different processes, alignment and barrier semantics; this supports the qualified Windows slot-ownership adapter.
- [C++ thread destructor specification](https://eel.is/c++draft/thread.thread.destr) specifies terminate on a joinable thread's destruction.
- [CMake GenerateExportHeader](https://cmake.org/cmake/help/latest/module/GenerateExportHeader.html) documents generated static/shared visibility support and installation with public headers.

## 17. Review disposition (2026-10-01)

The contract-first direction is retained. Review found implementation-relevant
holes rather than a reason to replace the diagnostics architecture:

| Finding | Disposition |
| --- | --- |
| Source freeze omitted export support needed by F1 | Freeze annotations and the generated support contract at C0. |
| Filtering, ring-only truncation and post-admission loss were ambiguous | Add filtered disposition, destination-specific counters and byte/encoding validity rules. |
| A small host handle plus timed-out shutdown left callback/storage lifetimes unspecified | Pin the process core/code, specify admission close, in-flight work, pending totals and repeat shutdown. |
| Crash requirements/retention could not be expressed by the frozen config | Add per-class evidence requirements and bounded crash storage policy; fail required installation truthfully. |
| External sequence checks were asserted to establish consistency | Use explicit shared-slot exclusion, bounded contention loss, orphan handling and no sequence wrap. |
| Capsule rewrites, native layout and capture/final completion were unspecified | Publish once; require a private codec/registration deliverable; distinguish acknowledgement from final receipt. |
| I2 comparison was placed after the whole logger/F1 | Restore the PR #6 independent I2 proof boundary and reconcile I0 measurements with the new contract. |
| Incident analysis missed competing writers and stop-before-join | Record both source-level defects and require a forced-oracle-failure cleanup test. |

Verification for this revision is document/header validation, not an I1/I2
acceptance receipt. The four literal header blocks plus type/noexcept/move
checks compiled with GCC 13.3 in C++20 static and hidden-visibility shared
syntax modes, both without exceptions/RTTI (ten compilations). The local
export macro was a Linux syntax fixture; CMake generation and shared linking
were not claimed. Event/ring/slot/capsule table offsets, fixed sizes, gate IDs
and fence/signature consistency were also checked. Cross-platform compilation, native crash probes,
installed DLL loading, IPC atomics, latency and retention remain the named
implementation gates. No accepted document or other repository is changed.

## 18. I2-only design extension (2026-10-01)

Owner direction expands PR9 toward the remaining PR6 program, with this
revision restricted to I2. The entry/landing map is
[D10](README.md), capture/internal
design [D11](i2-capture.md), private wire and
artifact contract [D12](i2-wire.md),
and batches/gates [D13](i2-execution.md).

The operational copies belong under Foundation's
`docs/architecture/diagnostics/`; qiven-context carries decision/progress refs,
not duplicated technical specifications. D10 defines the owner acceptance,
dated migration, exact five-file transfer and independent provenance check.

This extension preserves the section-5 declaration bytes. It specifies the
one-core crash-only route, prestarted inspector/supervisor, actual process
capabilities, read-only reply publication, Windows context codec, bounded
artifact/rate policy, callback/legacy/contracts absorption, provider comparison
and every C01-C45 implementation oracle. Core delivery and later real-I1
integration have distinct qualification labels. No Foundation/context/Runtime
implementation or acceptance result is claimed by this design-only revision.

Codex (model not-introspectable; reasoning not-introspectable)
