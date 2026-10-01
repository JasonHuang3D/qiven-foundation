# Foundation I2: process roles, capture path and internal contract

> Document ID: D11. Read D10 for scope/landing; D00 owns the public headers.
> No public header declaration changes are required by this I2 design.

## 1. Nominated mechanism and evidence gate

The nominated Windows-x64 implementation is a small Foundation callback
client plus an external Foundation inspector using Windows DbgHelp. A
Foundation supervisor launches and bounds the fixture/deployed process set.
Runtime later chooses when/where to launch it; Foundation supplies the
reusable executable and mechanism, not Runtime restart/business policy.

This nomination freezes the implementation candidate to make work concrete;
it is not a measured provider victory. B02 compares a pinned Crashpad adapter
on the same required UCRT and access-violation fixtures and deployment tuple.
WER is a separately operated external-evidence profile, never silently enabled
alongside the custom reporter. D13 defines the comparison and decision rule.
If the nominated mechanism fails its required evidence or cost gates, GLM
returns the evidence to design authority instead of inventing a substitute.

No third-party type appears in D00. DbgHelp/DbgCore versions and paths are
pinned in the receipt and loaded from the qualified absolute Windows/system
location; DLL search through the current directory or PATH is not a profile.
Crashpad source/dependency/license identities are pinned before comparison.

## 2. Executables and process ownership

| Component | Responsibility | Lifetime and forbidden dependency |
| --- | --- | --- |
| Target host + one Foundation process core | Install callbacks; publish fixed evidence; emit normal logs only if I1 is enabled | Core/thunks remain resident until process exit; fatal path never calls normal logger |
| `qiven-diag-inspector` | One target's registration, capsule validation, ring snapshot, dump and manifest | Separate process; one DbgHelp calling thread; no provider calls in target |
| `qiven-diag-supervisor` | Launch authorization, process handles, resource reservations, deadline/modal detection and missing-evidence receipt | Independent process/event loop; never waits synchronously on artifact storage or inspector work |
| `qiven-diag-verify` | Offline dump/symbol/context and receipt verification | After capture, with exact PE/PDB identities; no network symbol download |
| Native test driver | Declarative fixtures and expected evidence; CTest integration | Invokes supervisor; never launches an uncontained failing child directly |

A supervisor run holds exclusive ownership of one artifact root and admits
at most the profile's pending-client limit. Each armed target reserves one
artifact set and has its own inspector process. Two simultaneous targets
therefore do not share DbgHelp state. The root reservation accounts for all
live inspectors, pending artifacts and prior retained artifacts. Another
supervisor using the same root receives `root_busy`; it cannot race retention.

The I2 reference launcher takes a finite launch manifest (D12). A dynamic
Runtime launch broker is I3 integration; this design does not add an ad hoc
public network service to solve that later problem.

### Launch and registration chain

1. Supervisor validates its profile/launch manifest and absolute image paths;
   validates/pins target, inspector, provider and symbol identities; obtains
   the exclusive root lease and quota reservation in its storage worker.
2. Create an unnamed read-only bootstrap mapping, target-owned request/capsule
   mapping, broker-owned reply mapping, notification events and an independent
   preopened emergency file. Restrict handle rights as D12 specifies.
3. Create the target suspended with `STARTUPINFOEX` and an explicit handle
   inheritance list. Bind its actual process handle, PID, creation time,
   image identity and nonzero per-instance token in the launch record.
4. Create that target's inspector suspended with its own D12 bootstrap and
   explicit handle list, including the real target handle. Duplicate the
   inspector/supervisor synchronization handles needed by the target into it.
   Finalize both bootstraps with the correct process-local handle values;
   neither bootstrap is mutated after either child resumes.
5. Assign target/inspector to supervisor-owned jobs with kill-on-close and
   no permitted breakaway. If confinement/rights/bootstrap setup fails, kill
   the still-suspended children and return setup failure. Resume inspector;
   it validates its inputs, preloads the pinned provider and signals its
   separate bootstrap-ready event. Resume target only after that event and
   a live-inspector check. The supervisor retains all required handles.
6. The host's first ordinary startup call is `install_process_diagnostics()`.
   Its private I2 adapter consumes the explicitly inherited bootstrap token
   once; it does not discover a server by PID, filename or PATH. Validate
   D00 config, D12 binding and profile equality; map resources and register.
7. Inspector verifies the actual launched process and immutable request,
   artifact reservation and supported requirement list. If a ring was supplied,
   duplicate its target-owned mapping handle into the inspector using the held
   target process handle and validate its geometry before replying ready.
8. Target receives ready, installs the qualified callback set and commits the
   one process core. Only then return successful installation. A callback
   firing during callback installation already has the prepared capsule/IPC
   resources; registration failure cannot leave callbacks pointing at freed memory.

The environment variable in D12 transports a child-local inherited mapping
handle; it is neither endpoint discovery nor authentication by an environment
string. Authority is the supervisor-created process/handle set and its bound
launch record. A copied token without those capabilities fails installation.
Pre-main faults precede step 6 and receive observer-only evidence in this profile.

## 3. Private module/file seams

| Owning path | Fixed responsibility |
| --- | --- |
| `src/diag/process_core.hpp/.cpp` | The one process core, install transaction, host capability and pinned lifetime; shared with future I1 |
| `src/diag/crash/core_contract.hpp` | Exact private seam below; no Windows/provider types |
| `src/diag/crash/core.cpp` | Preparation/commit/rollback, requirement evaluation and bounded health |
| `src/diag/crash/wire.hpp/.cpp` | D12 validated scalar codecs and immutable publications |
| `src/diag/crash/platform/windows_client.hpp/.cpp` | Win32 handles, callbacks, first-fault gate, native context codec |
| `src/diag/crash/platform/unsupported.cpp` | Truthful unsupported capability result on unqualified platforms |
| `src/diag/crash/provider/windows_dump.cpp` | Inspector-only DbgHelp calls and quota-aware dump I/O |
| `tools/diag/inspector/` | Healthy-process artifact capture and final manifest |
| `tools/diag/supervisor/` | Launch/identity registry, watchdog, root/credit owner |
| `tools/diag/verify/` | Offline symbol and context receipts |
| `tests/diag/crash/` | Contained fixture binaries, codec negatives and receipt oracles |

These are semantic/file ownership boundaries, not a requirement to create
large files. Smaller private helpers may be placed under the same owner.
There is one definition of each protocol/constant; tests derive declarations
but use independently authored literal byte fixtures as their oracle.

Exact `src/diag/crash/core_contract.hpp`:

~~~cpp
#pragma once

#include <qiven/diag/process_diagnostics.hpp>
#include <qiven/types.hpp>

namespace qiven::diag::detail
{
struct ProcessCore;

enum class CrashPrepareCode : u8
{
    disabled = 0,
    ready,
    degraded,
    failed
};

struct CrashPrepareResult final
{
    CrashPrepareCode code = CrashPrepareCode::failed;
    CrashHealth health {};
};

// Called once in the serialized normal-startup installation transaction.
// Owns/copies all needed input before returning; never publishes host success.
[[nodiscard]] CrashPrepareResult prepare_crash(
    ProcessCore& core, const ProcessDiagnosticsConfig& config) noexcept;

// Installs the callback set after successful preparation. Failure is reflected
// in health; common install enforces required/optional policy before success.
[[nodiscard]] CrashPrepareResult commit_crash(ProcessCore& core) noexcept;

// Returns false when safe cleanup is pending; storage/code must stay pinned.
[[nodiscard]] bool abandon_crash_preparation(ProcessCore& core) noexcept;

[[nodiscard]] CrashHealth crash_health(const ProcessCore& core) noexcept;

// Legacy entrypoint uses this same core in headless-only mode.
void ensure_legacy_headless_core() noexcept;

// Only named, compile-time failure classes/reasons enter this path.
// Captures callback-origin machine context on the terminating thread.
[[noreturn]] void fatal_callback(CrashFailureClass failure_class) noexcept;
} // namespace qiven::diag::detail
~~~

Native SEH has a separate Windows-private entry taking borrowed
`EXCEPTION_POINTERS` from the filter. It validates/copies the supplied native
record/context into D12's codec under the same first-fault gate; it never calls
`fatal_callback()` and relabels a newly captured callback context as native.
The provider seam is inspector-side `capture_validated_request -> dump file +
CaptureAck + manifest`; its exact data boundary is D12, not a target-side
plugin callback or virtual logger interface.

### Common installation outcomes

`prepare_crash` allocates/maps/preopens and registers on normal startup;
`commit_crash` installs callbacks only after these resources are usable.
Disabled capture returns disabled/unavailable and does not create an inspector
connection. Legacy headless behavior, if already installed, remains in the
same core without an inspector coverage claim.

For required capture, any unmet class, artifact, storage, registration or
callback requirement produces an invalid host and the applicable D00 failure
status. For optional capture, preserve working logging and report degraded
crash health. If only crash is enabled and optional but unavailable, a degraded
control host may report the failure; its Emitter remains unbound. Unqualified
platforms report `unsupported_platform` in CrashInstallIssue and no class masks.
I2 does not implement a fake successful logger: logging-required installation
fails until the genuine I1 service exists.

`crash_health` performs bounded scalar snapshots and zero-time process-handle
checks. Inspector/supervisor death clears operational readiness and records
the corresponding unavailable/failed capture state. No polling thread is
required in the target. Stored callback/core resources remain pinned after
normal shutdown; neither successful shutdown nor a timeout unloads the DLL.

## 4. Target fatal path

The qualified path is fixed:

1. Atomically claim the process-wide first-fault gate before context capture,
   string access, formatting or emergency I/O. Use the already initialized
   core or the static headless-only core; never lazily allocate a full service.
2. A secondary/reentrant entry performs direct process termination with the
   fixed reentrancy exit code; it does not touch primary payload or wait on it.
   The supervisor records any shortened primary capture as partial.
3. First entry stores its monotonic tick, then publishes `fatal_started`.
   Capture callback registers with `RtlCaptureContext`, or copy the genuine
   filter-supplied native context. Use preallocated scratch and D12 codec.
4. Build the D00 capsule: constant bounded reason, real PID/TID and instance,
   exact build/workspace identity, honest context origin/quality and ring
   reservation watermark. Publish 0 -> 1 -> 2 once, with the qualified barriers.
5. Signal the inspector. Then attempt one bounded-size write to the independent
   emergency file handle. This write may block inside Windows/storage; the
   inspector already received the publication and the supervisor still runs.
6. Wait on capture-ack, inspector-exit and supervisor-exit handles, using the
   remaining acknowledgement budget from the first-fault tick, never a fresh
   full timeout after a stalled step. Validate the immutable acknowledgement.
   A failure/death/timeout is evidence degradation, not permission to recover.
7. Terminate directly through the Windows process API, preserving the selected
   exit discriminator. No C++ unwinding, `exit`, `abort`, stdio, logger call,
   application mutex, allocator, symbol lookup or JSON occurs on this path.

Fatal callback code lives in the pinned Foundation service image. The normal
host handle, a loaded emitter client, a logger writer or an atexit destructor
never owns its storage. The fatal path has bounded instructions/buffer sizes;
kernel I/O and scheduling are not asserted to have a hard in-process wall bound.
The independent deadline is what bounds the whole failing process attempt.

### Callback adapter table

| Entry | Capsule class and context | Exact treatment |
| --- | --- | --- |
| UCRT invalid-parameter callback | ucrt_invalid_parameter; callback_origin/callback_context | Ignore possibly corrupt expression/file pointers; constant reason; never return to `_invoke_watson` |
| `signal(SIGABRT, ...)` handler | abort_call; callback_origin/callback_context | Suppress CRT abort/report UI at installation; callback terminates without calling abort again |
| `std::set_terminate` handler | cpp_terminate; callback_origin/callback_context | Do not inspect `current_exception`, rethrow or read `what()` |
| `_set_purecall_handler` | pure_virtual_call; callback_origin/callback_context | Constant reason; terminate through common core |
| Debug narrow/wide CRT report hook | Report policy only; no fatal capsule | Set report result to 0, return handled; do not inspect/format message or enter a debugger; let a following UCRT invalid-parameter dispatch reach its fatal callback |
| Foundation `detail::contract_fail` | contract_failure; callback_origin/callback_context | Preserve signature/macros; replace snprintf/stderr/debug-break/CRT-exit implementation with common fatal entry |
| top-level unhandled SEH filter, qualified native faults | unhandled_native_exception; native_exception/native_context | Use supplied primary exception record + control/integer register context; preserve original code/address |
| direct fail-fast, stack-cookie, pre-main/loader, unqualified stack overflow | external observer only | No promise that an installed callback runs; unknown cause stays unknown if only exit is observed |

Debug CRT report hooks suppress the report with result 0 for warning/error/assert
and set report modes to zero. They are not generic fatal classifiers: a Debug
invalid-parameter assertion can precede its actual invalid-parameter handler.
Terminating in that report hook would steal the class/exit/context from UCRT.
Standalone raw CRT reports therefore have non-modal report-policy coverage
only; Foundation assertions/contracts enter the fatal core directly. This is
not recovery from an entered fatal callback. Set narrow and wide report hooks
where the qualified CRT uses both. Install the invalid-parameter handler,
abort behavior (`_WRITE_ABORT_MSG` and `_CALL_REPORTFAULT` cleared), SIGABRT,
terminate, purecall, report policy and SEH filter as one owned operation.
Use process-local headless error mode; do not edit machine-wide WER policy.
An unexpected existing handler is a recorded handler conflict under the
exclusive profile, not an invitation to chain arbitrary callbacks on a fault.

The UCRT compatibility exit remains 3143. Foundation contract failure retains
exit 3. Other callback classes use `0xE0510000 | class_number`; a recursive
handler uses `0xE05100FE`. Native-SEH termination uses the original native
exception code; the capsule's synthetic_exit_code stays zero for that case.
These are process exit discriminators, not proof of a native exception.
The old contract_fail signature's expression/kind strings are not copied on
the fatal path; source/context and the fixed reason provide safe attribution.

Qualification covers the declared shared CRT instance and no foreign
thread-local/later handler replacement. Enumerate and test interference.
Additional CRT copies or replacement handlers are uncovered until a separately
qualified host-coordinated adapter exists. A singleton is not proof of CRT scope.

## 5. Inspector capture and completion

Inspector waits on the request hint, target exit and its supervisor handle.
Hints may coalesce; on every wake/poll inspect immutable install/capsule
publication state. Never wait for the target's logger or a target mutex.
An odd/uncommitted capsule is missing/partial, not a record to repair.

For committed capsule 2: validate all identity, schema and bounds; persist the
exact capsule; acquire/copy ring slots by D00 when a real ring is registered;
reconstruct local Windows structures from D12; call MiniDumpWriteDump once.
Set `ClientPointers = FALSE` because these reconstructed structures are in
the inspector. No raw target pointer from a wire buffer is dereferenced.
For callback capture, the dump exception record is explicitly synthetic and
its code is distinct from `native_exception_code = 0` in the capsule/manifest.
The original callback thread remains alive until acknowledgement/deadline.

The reference dump flags are `MiniDumpNormal | MiniDumpWithThreadInfo |
MiniDumpWithUnloadedModules`; full-memory, handle-data and arbitrary user
streams are not enabled by this profile. Keep the qualified faulting thread's
stack/module/context. D13 tests seeded-secret exposure and records remaining
stack-memory sensitivity; this design does not promise perfect redaction.

Dump output uses DbgHelp I/O callbacks and a pre-reserved file extent budget.
IoStart selects alternate I/O; each IoWriteAll validates offset+size and writes
all bytes or fails; IoFinish succeeds only if the previous writes did. No
write can exceed the reservation, including sparse seeks. Read-memory failures
are not converted to success for the required fault context. The worker's
blocking I/O is contained by the supervisor's deadline/job, not cancellation
of arbitrary C++ threads. All DbgHelp operations in that inspector are serial.

Before successful CaptureAck: close dump/capsule/required ring files, reopen
for minimal structural validation, check matching process/thread and the
required context stream. `ring_tail` means a valid bounded snapshot with
explicit gaps, not lossless history; an actually empty ring is a valid empty
snapshot, whereas an unreadable/malformed ring is partial/failed.
Then publish the immutable acknowledgement and signal it. The target may exit.
Hashing, the final manifest and symbol verification remain separate steps.

Only a complete final manifest closes all selected-profile artifact requirements.
On failure after acknowledgement, keep a partial final receipt and the earlier
ack fact. The inspector cannot retroactively claim that the target stayed alive
until final hashing. Exact symbol verification is an offline qualification
receipt bound to artifact hashes; capture success alone is not symbol success.

## 6. Supervisor deadline and failure states

The event loop never calls dump, hash, blocking filesystem write or cleanup.
Storage tasks run in killable workers. At first observed fatal progress/event,
use its validated tick or the supervisor's first-observation tick when absent.
The latter basis and observation delay are recorded. Enforce 30,000 ms from
that basis for capture/postprocessing; target acknowledgement budget is 5,000 ms.
Poll registered progress at most every 10 ms in the reference profile, subject
to recorded OS scheduling delay; a deadline is enforced when the supervisor
next runs, not a hard real-time claim. The outer CTest/process deadline is 40 s
for each deliberate fault attempt.

The per-attempt deadline starts at admission, before storage/setup; bootstrap
readiness has a 5 s setup bound within it. Neither fault detection nor a worker
restart resets the 40 s total. The effective capture cutoff is the earlier of
the 30 s fatal deadline and that existing attempt deadline. A native driver
contains the supervisor itself in a kill-on-close job and keeps its independent
watch; killing a blocked supervisor cannot orphan suspended descendants.

Before any fault, the launch manifest's finite test lifetime applies. A target
dying without a capsule yields an observer receipt with actual exit status,
instance/build and absent-context flags. It does not synthesize a native cause
from a shared Windows exit code. A fixture's expected fault class is stored
separately from the observed class.

On inspector/target/modal/deadline failure: record the typed in-memory outcome,
terminate the affected contained process tree, and ask a storage worker to
publish the bounded observer receipt. If storage is also unavailable, return
the supervisor result to the driver through its bounded result channel and
mark artifact evidence unavailable. No design can guarantee a durable file on
failed storage; a missing artifact is never a successful capture.

Monitor target/descendant windows and correlated OS report windows on the
qualified interactive desktop. Correlation uses process handles/creation
identity and reporter attribution, not window title alone. In noninteractive
CI, use the separately qualified noninteractive profile plus deadlines; do
not report that a desktop window scan occurred when it could not. Unrelated
processes/windows are never terminated. No automatic retry follows an unexpected native
failure; a next attempt requires an explicit changed-candidate/test receipt.

## 7. Primary technical references

- [MiniDumpWriteDump](https://learn.microsoft.com/en-us/windows/win32/api/minidumpapiset/nf-minidumpapiset-minidumpwritedump): separate-process capture, rights, and serial DbgHelp use.
- [MINIDUMP_EXCEPTION_INFORMATION](https://learn.microsoft.com/en-us/windows/win32/api/minidumpapiset/ns-minidumpapiset-minidump_exception_information): local versus target ExceptionPointers semantics.
- [MINIDUMP_CALLBACK_TYPE](https://learn.microsoft.com/en-us/windows/win32/api/minidumpapiset/ne-minidumpapiset-minidump_callback_type): alternate I/O and failure callbacks.
- [Windows Job Objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects): process-group containment; actual inherited/nested-job behavior remains a probe.
- [CRT parameter validation](https://learn.microsoft.com/en-us/cpp/c-runtime-library/parameter-validation) and [report hook results](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/crtsetreporthook): the Debug report can precede invalid-parameter dispatch; the non-modal report hook must allow that dispatch to retain its class.
- [Crashpad design at the I0 source pin](https://chromium.googlesource.com/crashpad/crashpad/+/ce308a86daa85e65df219ff5fb385095b0a1c467/doc/overview_design.md): B02 retains the census pin and records the dependency closure before measurement.

Codex (model not-introspectable; reasoning not-introspectable)
