# Foundation I2: Windows bootstrap, context and artifact wire contract v1

> Document ID: D12. Completes W0 for D11's Windows-x64 I2 profile.
> D00 still owns EventPacketV1, CrashRingV1 and the 8192-byte CrashCapsuleV1.
> These are encoded byte formats, not C++ struct layouts or independent module ABIs.

## 1. Common encoding and ownership rules

All integers are unsigned little-endian unless explicitly stated otherwise.
All byte offsets below are relative to the named region. Validate fixed
extent, exact version, lengths, enum/bit ranges and checked arithmetic before
variable access. Reserved bytes are emitted as zero and rejected if nonzero.
Strings are UTF-8 byte ranges without a terminator; unused capacity is zero.
Embedded NUL, invalid UTF-8, duplicate requirements and unknown schema fields
are invalid. A decode result never contains a pointer into unvalidated input.

Shared atomic words are naturally aligned 32/64-bit Windows words. Writers
and read/modify/write users use full-barrier Interlocked operations. A reader
of a read-only mapping uses the Windows-x64 adapter: one aligned volatile
scalar load compiled explicitly with MSVC `/volatile:ms`, then `MemoryBarrier()`
before payload access. Never use compare/exchange as a load on a read-only view.
This platform adapter relies on documented aligned Win64 atomic loads and
Microsoft acquire semantics, not portable C++ volatile; isolate its translation
unit, qualify generated instructions and test with actual read-only mappings.
Ring ownership still requires a writable view and D00 Interlocked claims.
The bootstrap is immutable before either child resumes. Request,
InstallReply, Capsule and CaptureAck each have one publication: 0 empty,
1 writing, 2 immutable/committed. Readers acquire publication 2 before copying;
other values do not authorize a payload copy. Notifications are hints and
do not replace publication checks. An immutable reply is never overwritten
to represent later health; live peer-process handles determine liveness.

Native handle numbers below are valid only in the specifically designated
process's inherited/duplicated handle table. They are capability references
resolved through Windows, not addresses or persistent artifact pointers.
No native handle number is used to reopen an arbitrary PID or dereference
target memory. The persisted capsule/context contains no such handle.

## 2. Explicit bootstrap channel

Target environment contains exactly:

~~~text
QIVEN_DIAG_BOOTSTRAP_V1=qdb1:<16-lowercase-hex bootstrap-handle>:<32-lowercase-hex nonce>
~~~

The handle denotes a 4096-byte read-only mapping inherited through the explicit
launch list. Parse fixed length/alphabet with checked conversion; reject null,
pseudo and invalid handles. The token nonce must equal the mapping nonce.
Read it once during normal installation, clear it from the target environment
after copying, and clear inheritability on all consumed handles before workers
or descendants start. Duplicate required handles into the core's owned lifetime;
only handles explicitly passed to a new child may be inherited again.

With capture enabled and no token, optional installation becomes degraded;
required installation fails. No named-pipe fallback, PID scanning, executable
search, registry modification or implicit reporter launch occurs in the target.

### BootstrapV1: 4096 bytes

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | magic = 0x31424451 (QDB1) |
| 4 | 2 | major = 1 |
| 6 | 2 | minor = 0 |
| 8 | 4 | encoded_size = 4096 |
| 12 | 4 | profile_id = 1 (win-x64-minimal-v1) |
| 16 | 4 | target_pid |
| 20 | 4 | reserved |
| 24 | 8 | target_creation_filetime |
| 32 | 8 | nonzero process_instance |
| 40 | 8 | launch_nonce.high |
| 48 | 8 | launch_nonce.low |
| 56 | 8 | build_identity.high |
| 64 | 8 | build_identity.low |
| 72 | 8 | workspace_identity.high |
| 80 | 8 | workspace_identity.low |
| 88 | 4 | supervisor_pid |
| 92 | 4 | inspector_pid |
| 96 | 8 | target-local client_mapping_handle |
| 104 | 8 | target-local reply_mapping_handle |
| 112 | 8 | target-local request_event_handle |
| 120 | 8 | target-local install_ready_event_handle |
| 128 | 8 | target-local capture_ack_event_handle |
| 136 | 8 | target-local emergency_file_handle |
| 144 | 8 | target-local supervisor_process_handle |
| 152 | 8 | target-local inspector_process_handle |
| 160 | 4 | emergency_capacity = 4096 |
| 164 | 4 | install_reply_timeout_ms = 50 |
| 168 | 4 | acknowledgement_timeout_ms = 5000 |
| 172 | 4 | supervisor_deadline_ms = 30000 |
| 176 | 8 | max_artifact_bytes |
| 184 | 8 | max_total_bytes |
| 192 | 4 | max_artifact_count |
| 196 | 4 | max_pending_captures |
| 200 | 2 | artifact_root_size, 1..1024 |
| 202 | 2 | process_name_size, 1..128 |
| 204 | 52 | reserved |
| 256 | 1024 | normalized absolute artifact_root_utf8 |
| 1280 | 128 | process_name_utf8 |
| 1408 | 2688 | reserved |

The supervisor verifies the image SHA-256 and PE identity against its trusted
launch manifest before resume; build/workspace IDs are not substitutes for
that verification. Target compares PID and creation time with its own kernel
identity and all supplied config identities/name/root/limits with bootstrap.
Inspector compares them with its supervisor-owned registry and actual target
handle. Root equality uses the one normalized path plus opened-root identity;
per-file cleanup never relies on string prefixes alone. Reject substituted
handles, stale process instances and ring mappings from a different client.

`process_instance` is a nonzero launch-generated 64-bit token; a separate
128-bit launch nonce is generated with the OS random service. Actual process
handle and creation identity remain part of every check, so token/PID alone
is not authentication. The same-user ability to modify a process is outside
this IPC isolation claim; malformed or unrelated registrations still fail.

### Target handle rights

| Handle | Target access | Inspector/supervisor ownership |
| --- | --- | --- |
| Bootstrap mapping | map read | Supervisor writes only before target resume |
| Client mapping | map read/write | Inspector and supervisor read/atomic-observe |
| Reply mapping | map read | Inspector writes its two reply slots |
| Request event | EVENT_MODIFY_STATE | Inspector waits; supervisor observes shared progress independently |
| Install-ready / capture-ack events | SYNCHRONIZE | Inspector signals |
| Emergency file | bounded write to its reserved file | Supervisor created; excluded from stdio aliases |
| Supervisor/inspector process | SYNCHRONIZE and PROCESS_QUERY_LIMITED_INFORMATION | Supervisor binds exact objects; target cannot terminate or modify peers |

Inspector receives target PROCESS_QUERY_INFORMATION, PROCESS_VM_READ and
PROCESS_DUP_HANDLE for optional ring transfer, plus tested thread access
needed by the pinned dump provider, including threads created after launch.
No handle-data dump stream is enabled. All excess inherited handles, especially
job/root-authority handles, are absent from the target list.

### InspectorBootstrapV1: 8192 bytes

The inspector's `--bootstrap-handle` identifies this distinct read-only mapping,
not the target bootstrap. Its handle fields are inspector-local. The supervisor
creates both children suspended and finalizes this mapping before resuming them.

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | magic = 0x31424951 (QIB1) |
| 4 | 2 | major = 1 |
| 6 | 2 | minor = 0 |
| 8 | 4 | encoded_size = 8192 |
| 12 | 4 | profile_id = 1 |
| 16 | 8 | process_instance |
| 24 | 8 | target_process_handle |
| 32 | 8 | client_mapping_handle, map read |
| 40 | 8 | reply_mapping_handle, map read/write |
| 48 | 8 | request_event_handle, SYNCHRONIZE |
| 56 | 8 | install_ready_event_handle, EVENT_MODIFY_STATE |
| 64 | 8 | capture_ack_event_handle, EVENT_MODIFY_STATE |
| 72 | 8 | supervisor_process_handle, SYNCHRONIZE |
| 80 | 8 | bootstrap_ready_event_handle, EVENT_MODIFY_STATE |
| 88 | 8 | artifact_directory_handle, identity/read-attributes only |
| 96 | 1 | expected logging Requirement |
| 97 | 1 | expected capture Requirement |
| 98 | 1 | expected requirement_count, 0..9 |
| 99 | 29 | reserved |
| 128 | 32 | expected target image SHA-256 |
| 160 | 32 | expected DbgHelp binary SHA-256 |
| 192 | 32 | expected DbgCore binary SHA-256 |
| 224 | 32 | reserved |
| 256 | 144 | nine expected 16-byte requirement entries |
| 400 | 3696 | reserved |
| 4096 | 4096 | exact immutable target BootstrapV1 copy |

Only the prefix's handles are dereferenced in the inspector. Target-local
handles in the embedded copy are identity/configuration evidence, never
inspector handles. Expected requirements come from the trusted launch manifest;
the immutable InstallRequest must match them, including disabled/optional/
required policy. The inspector verifies actual target PID/creation/image and
all embedded profile fields before registration; the target cannot weaken its
launcher-declared evidence contract. The artifact-directory handle identifies
the supervisor-reserved instance directory, not authority to clean the root.
Inspector opens only fixed artifact names there and verifies directory identity.

Request event is auto-reset; install-ready, capture-ack and bootstrap-ready
are one-shot manual-reset events. Notifications never authorize reading an
uncommitted payload. Polling immutable publication words also handles coalesced
request hints. Inheritability is cleared in both children before further spawn.

## 3. Client mapping and install request

ClientMappingV1 is 16384 bytes:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | atomic fatal_started: 0 or 1 |
| 4 | 4 | atomic secondary_seen: 0 or 1 |
| 8 | 8 | atomic fatal_tick_ms: GetTickCount64 value, 0 if unavailable |
| 16 | 48 | reserved |
| 64 | 2048 | InstallRequestV1 |
| 2112 | 1984 | reserved |
| 4096 | 8192 | D00 CrashCapsuleV1 |
| 12288 | 4096 | reserved, not C++ scratch storage |

The core's first-fault gate permits one writer. `fatal_started` is published
before potentially blocking work; secondary_seen is a sticky bit, not a
wrapping event counter. Supervisor checks the tick lies within this run's
clock interval; otherwise it uses and records its own first-observation time.

InstallRequestV1:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 8 | publication |
| 8 | 4 | magic = 0x31524951 (QIR1) |
| 12 | 2 | major = 1 |
| 14 | 2 | minor = 0 |
| 16 | 4 | encoded_size = 2048 |
| 20 | 4 | target_pid |
| 24 | 4 | installing_thread_id |
| 28 | 4 | flags: bit 0 ring_present; remaining bits zero |
| 32 | 8 | target_creation_filetime |
| 40 | 8 | process_instance |
| 48 | 8 | launch_nonce.high |
| 56 | 8 | launch_nonce.low |
| 64 | 16 | build_identity (high, low) |
| 80 | 16 | workspace_identity (high, low) |
| 96 | 1 | logging Requirement |
| 97 | 1 | capture Requirement |
| 98 | 1 | requirement_count, 0..9 |
| 99 | 1 | reserved |
| 100 | 4 | acknowledgement_timeout_ms |
| 104 | 4 | supervisor_deadline_ms |
| 108 | 2 | process_name_size |
| 110 | 2 | artifact_root_size |
| 112 | 8 | max_artifact_bytes |
| 120 | 8 | max_total_bytes |
| 128 | 4 | max_artifact_count |
| 132 | 4 | max_pending_captures |
| 136 | 8 | target-local ring_mapping_handle; 0 when absent |
| 144 | 8 | ring_total_bytes; 0 when absent |
| 152 | 4 | ring_slot_count; 0 when absent |
| 156 | 4 | ring_slot_stride; 0 when absent |
| 160 | 4 | ring_payload_capacity; 0 when absent |
| 164 | 28 | reserved |
| 192 | 144 | nine 16-byte requirement entries |
| 336 | 128 | process_name_utf8 |
| 464 | 1024 | artifact_root_utf8 |
| 1488 | 560 | reserved |

Requirement entry: u16 class, u8 context_origin, u8 context_quality, u32
evidence mask, eight zero bytes. Use D00 enum values, sorted by class; unused
entries are zero. Enabled/required combinations and full-mask comparisons
follow D00. Ring-present requires genuine I1 registration, correct mapping
identity and D00 geometry; ring-absent plus required ring_tail is rejected.
The target retains the mapping handle while the inspector duplicates it;
the core/mapping lifetime then lasts through process exit.

## 4. Broker reply mapping and typed failures

ReplyMappingV1 is 4096 bytes: InstallReplyV1 at 0 (256 bytes), CaptureAckV1 at
256 (256 bytes), remaining 3584 bytes zero. Each slot has this common header:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 8 | publication |
| 8 | 4 | magic: 0x31504951 (QIP1) or 0x314B4151 (QAK1) |
| 12 | 2 | major = 1 |
| 14 | 2 | minor = 0 |
| 16 | 4 | encoded_size = 256 |
| 20 | 4 | result enum |
| 24 | 8 | process_instance |
| 32 | 8 | target_creation_filetime |
| 40 | 4 | target_pid |
| 44 | 4 | faulting_thread_id for Ack, zero for InstallReply |
| 48 | 8 | launch_nonce.high |
| 56 | 8 | launch_nonce.low |
| 64 | 160 | typed payload below |
| 224 | 32 | SHA-256 of committed request/capsule bytes, respectively |

InstallReply results: 1 ready, 2 degraded, 3 refused. Payload at 64 is
callback_class_mask u64, native_class_mask u64, external_class_mask u64,
CrashInstallIssue u32, WireIssue u32, emergency_sink_ready u8,
inspector_registered u8, then 126 zero bytes. Target normal-startup code
verifies the request digest before accepting readiness. Actual callback
installation is still the target's later commit step; ready is not a claim
that those callbacks are already installed.

CaptureAck results: 1 target_dependent_closed, 2 partial, 3 failed,
4 deadline. Payload at 64 is artifact_flags u32, WireIssue u32,
dump_bytes u64, ring_bytes u64, context_encoding u16, context_quality u8,
context_origin u8, failure_class u16, reserved u16, capture_elapsed_ms u32,
then 124 zero bytes. Artifact flags are D00 evidence bits for already closed
artifacts; manifest is clear here. Target validates identity, thread, result
and required target-dependent evidence before interpreting successful capture;
it need not hash data on the fatal path. The read-only authenticated reply,
one-shot instance and publication bind it; offline verification checks digest.
Any terminal acknowledgement permits the nonrecovering target to exit, but
only result 1 is successful target-dependent capture. None is final completion.

WireIssue codes are frozen:

| Value | Name | Meaning |
| ---: | --- | --- |
| 0 | none | No issue |
| 1 | invalid_bootstrap | Token/handle/bootstrap shape invalid |
| 2 | identity_mismatch | Instance/build/process/nonce mismatch |
| 3 | unsupported_requirement | Named evidence/context requirement unavailable |
| 4 | handler_conflict | Target reports incompatible callback ownership |
| 5 | storage_unavailable | Root/open/write/close/flush unavailable |
| 6 | quota_exhausted | Count/byte/pending credit unavailable |
| 7 | unsupported_context | Platform/encoding/groups unsupported |
| 8 | inspector_lost | Bound inspector died |
| 9 | supervisor_lost | Bound supervisor died |
| 10 | capture_failed | Provider/structural validation failure |
| 11 | target_exited | Target disappeared before required capture closed |
| 12 | deadline_exceeded | Qualified external deadline reached |
| 13 | malformed_wire | Length/reserved/version/value validation failure |
| 14 | root_busy | Another supervisor owns the artifact root |
| 15 | publication_failed | Final manifest/hash/rename failed after capture |
| 16 | unsupported_provider | Provider tuple or needed I/O capability unavailable |
| 17 | rate_limited | Root's capture-attempt rate allowance exhausted |

The final manifest records native Win32/HRESULT values separately from these
stable codes. Failure of the result channel itself is reported by supervisor
exit/driver observation, never by a fabricated valid reply.

Startup mapping to D00 is fixed: malformed config/enum/limits is invalid_config;
actual allocation failure is resource_exhausted; an unqualified platform is
unsupported_platform for required capture. Otherwise an unmet required crash
profile is required_crash_unavailable. Optional failures retain a degraded host
and available capabilities. WireIssue storage/quota/rate/publication failures
map to artifact_storage_failed; unsupported context/class/provider maps to
unsupported_fault_class; handler conflict maps to handler_conflict; missing,
dead or invalidly bound peers map to inspector_unavailable. Emergency-file
failure additionally sets emergency_sink_failed. Bits may combine; the private
receipt retains the more precise WireIssue even where public issue bits coalesce.

## 5. Context encoding 1: win-amd64-core-v1

D00 capsule uses platform=1, architecture=1, context_encoding=1 and
context_size=512. This schema preserves original control/integer registers
and optional segment group; it does not claim AVX/XSTATE, debug registers or
complete floating-point state. Native quality refers to a genuine supplied
exception context with these declared groups, not to complete machine state.
Unsupported larger contexts require a new encoding/design, not a memcpy of
an OS struct or silent truncation.

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | magic = 0x31435751 (QWC1) |
| 4 | 2 | major = 1 |
| 6 | 2 | header_size = 64 |
| 8 | 4 | encoded_size = 512 |
| 12 | 4 | groups: control=1, integer=2, segments=4 |
| 16 | 2 | exception_count: 0 callback, 1 native |
| 18 | 2 | reserved |
| 20 | 4 | source Windows ContextFlags, metadata only |
| 24 | 40 | reserved |
| 64 | 136 | Rax,Rcx,Rdx,Rbx,Rsp,Rbp,Rsi,Rdi,R8,R9,R10,R11,R12,R13,R14,R15,Rip; seventeen u64 values |
| 200 | 4 | EFlags |
| 204 | 12 | SegCs,SegSs,SegDs,SegEs,SegFs,SegGs; six u16 values |
| 216 | 4 | primary exception code, zero for callback |
| 220 | 4 | primary exception flags, zero for callback |
| 224 | 8 | primary exception address, zero for callback |
| 232 | 4 | NumberParameters, 0..15 |
| 236 | 4 | flags: bit 0 nested_record_omitted; other bits zero |
| 240 | 120 | fifteen u64 exception parameter values; unused entries zero |
| 360 | 152 | reserved |

Control and integer groups are mandatory for callback_context/native_context.
SegCs/SegSs travel with control; other segment values are zero when the segment
group is absent. Derive groups from the captured Windows ContextFlags; do not
infer validity from nonzero register values. For native capture, capsule
exception code equals the primary record code, origin=native_exception,
quality=native_context and exception_count=1. Copy only the primary native
record; a nested-record pointer is represented by nested_record_omitted=1,
never followed. Register/fault/parameter address values are numeric evidence,
not pointers the decoder may dereference.

For callback capture, origin=callback_origin, quality=callback_context and
exception_count=0; all primary-exception fields/parameters/flags are zero.
The inspector builds a synthetic EXCEPTION_RECORD with code
`0xE0510000 | failure_class`, address=RIP and zero parameters, explicitly labeled
synthetic_callback in the manifest. It builds a local CONTEXT with exactly the
encoded valid groups, wraps both in local EXCEPTION_POINTERS, and supplies
the capsule thread ID with ClientPointers=FALSE. For native capture it instead
reconstructs the primary original record, with its link set null.

If native inputs do not safely supply required groups or parameter bounds,
publish reason-only context_size=0/context_encoding=0 and record the missing
native requirement. Do not invent original registers with RtlCaptureContext.
The inspector acknowledges partial/failed when required context cannot be met.

## 6. Ring snapshot artifact and independent emergency record

The inspector uses D00's slot ownership, not a live ReadProcessMemory seqlock.
SnapshotV1 is a closed binary file with this header followed by records:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | magic = 0x31535251 (QRS1) |
| 4 | 2 | major = 1 |
| 6 | 2 | minor = 0 |
| 8 | 8 | total_size |
| 16 | 8 | process_instance |
| 24 | 8 | upper_reserved_watermark |
| 32 | 4 | slots_scanned |
| 36 | 4 | copied_records |
| 40 | 4 | busy_slots |
| 44 | 4 | malformed_slots |
| 48 | 4 | empty_slots |
| 52 | 4 | after_watermark_slots |
| 56 | 4 | flags: bit 0 accounting_degraded; bit 1 history_has_gaps |
| 60 | 4 | reserved |

Each record is u64 sequence, u32 packet_size, u32 zero, then EventPacketV1
bytes, without padding; descending sequence, no duplicates. Validate checked
file extents and correct packet/process identity. The five slot category counts
sum to slots_scanned. Missing generations are not necessarily computable
exactly after overwrite; history_has_gaps is honest loss reporting, not an
invented exact missing count. Snapshot maximum is `64 + slot_count *
(16 + payload_capacity)` and must be reserved before reading. No ring produces
no ring file, not a fake empty-ring success.

The independent emergency file is preopened/preallocated within the artifact
reservation. The fatal path writes at most one 64-byte record at offset zero:
magic QER1 (u32 0x31524551), major u16=1, size u16=64, instance u64,
PID u32, TID u32, failure_class u16, origin u8, quality u8, native_code u32,
synthetic_exit u32, flags u32=0, fatal_tick_ms u64, then sixteen zero bytes.
No text scan, stderr handle, heap or JSON is required. A partial/failed write
is retained as partial evidence; do not classify a non-64-byte record as valid.
The supervisor/inspector may later truncate unused preallocation on healthy
cleanup; the target never rotates or flushes this file on the fatal path.

## 7. Artifact transaction, quotas and manifest

An admitted target owns one pending artifact set before launch. Reservation
counts against both max_pending_captures and max_artifact_count, and reserves
max_artifact_bytes against max_total_bytes until sealed/removed. This keeps a
crash storm from creating unbounded pending files. A clean exit removes its
empty pending set; no crash retry is implied by admission of the next manifest
entry. Existing sealed/partial sets are charged by actual logical file length;
allocation slack is additionally reported by the storage worker.

Root has an exclusive opened lease; supervisor serializes credit decisions in
memory. Root enumeration/open/cleanup occurs only in its storage worker with
a deadline. Use opened-directory/file identities and reparse-point rejection
for the qualified private artifact root; never follow target-supplied file
names. Only fixed filenames below an instance-named directory are used.

Artifact filenames: `capsule.bin`, `capture.dmp`, optional `ring.bin`,
`emergency.bin`, `capture-ack.bin`, `manifest.json`; active files use `.partial`
until individually closed. CaptureAck success requires target-dependent files
closed and minimally validated. Final hashes and manifest publication occur
afterward. Write manifest to a temporary file, flush/close, then rename within
the same directory; never expose a half-written file as the final manifest.
Publish no external upload by default.

Byte accounting reserves 2 MiB per artifact for ring/metadata/emergency and
uses the remainder as the maximum minidump extent in profile 1. A ring snapshot
is limited to 1 MiB; one that exceeds this limit fails preflight, not mid-capture
allocation. Manifest and offline verification receipt are each at most 64 KiB;
quota includes both. Manifest hashes exclude manifest itself and any later
verification receipt to avoid circular identity. Verification lives under
the run's bounded `receipts/` area and binds the sealed manifest hash.

Retention removes the oldest sealed/partial unleased set by supervisor-assigned
sequence until the next reservation fits. It never deletes a live/leased set
or another root. If limits cannot be satisfied, admission returns quota_exhausted.
After a crash of the supervisor, the next root owner enumerates orphan partials
under the same limits; it does not reset accounting to zero. If cleanup/storage
is stuck, refuse admission and preserve the failure receipt instead of growing.

The inherited I0 storm ceiling is ten capture attempts per root per rolling
hour. Reserve a rate credit durably before resuming each armed target; refund
only after a verified clean exit with no fatal progress. Failed, timed-out or
unobserved attempts consume the credit. The bounded root ledger stores the last
ten charged admission UTC FILETIMEs plus live-instance identities, survives
supervisor restart and charges its own bytes against the aggregate limit.
Backward clock movement or an unreadable ledger refuses new admission until
the window can be established; retention cannot erase rate history. The storage
worker writes the ledger atomically before launch, never on the fatal path.
Use distinct private roots for unrelated qualification cases; the storm test
deliberately uses one root. Rate refusal is `rate_limited`, not an automatic retry.

Ledger file `rate-ledger.json` is <=64 KiB, schema `qiven.diag.rate-ledger/1`,
with exactly `schema`, `root_volume_serial` (u64 hex), `root_file_id` (128-bit
hex), `last_seen_utc_filetime` (u64 hex) and `entries` (<=10). Each entry has
`process_instance` (u64 hex), `admitted_utc_filetime` (u64 hex) and `state`
(reserved or charged). Unresolved reservations after restart become charged.
Unknown/corrupt ledgers refuse admission, and only verified clean instances
remove their reserved entry. Time expiration removes entries at least one hour
old only after the clock check; root identity must match the opened directory.

Manifest schema `qiven.diag.crash-manifest/1` has these required keys; optional
facts are explicit null, not fabricated values. Unknown keys/versions are
rejected by the v1 verification tool.

~~~json
{
  "schema": "qiven.diag.crash-manifest/1",
  "profile": "win-x64-minimal-v1",
  "design_origin_sha": "<approved design>",
  "foundation_sha": "<candidate>",
  "process": {"pid": 1, "creation_filetime": "<u64 hex>", "instance": "<u64 hex>", "launch_nonce": "<128-bit hex>", "image_sha256": "<hex>", "build_identity": "<128-bit hex>", "workspace_identity": "<128-bit hex>"},
  "failure": {"class": 0, "origin": 0, "quality": 0, "native_code": 0, "synthetic_exit_code": 0, "observed_exit_code": null, "thread_id": null, "reason": "", "context_encoding": 0, "context_groups": 0, "nested_record_omitted": false, "dump_exception_kind": "none"},
  "capture": {"state": "failed", "wire_issue": 0, "platform_error": null, "ack_result": null, "ack_elapsed_ms": null, "supervisor_elapsed_ms": 0, "deadline_basis": "supervisor_observation", "observation_delay_ms": null},
  "evidence_bits": 0,
  "ring": {"registered": false, "watermark": null, "slots_scanned": 0, "copied": 0, "busy": 0, "malformed": 0, "empty": 0, "after_watermark": 0, "history_has_gaps": false, "accounting_exact": true},
  "artifacts": [],
  "provider": {"name": "windows-dbghelp", "version": "<pinned>", "binary_sha256": "<hex>", "dbgcore_sha256": "<hex>", "dump_flags": "MiniDumpNormal|MiniDumpWithThreadInfo|MiniDumpWithUnloadedModules"},
  "durability": "closed_validated",
  "missing_requirements": [],
  "symbol_verification": "pending",
  "privacy_profile": "local-restricted-stacks-v1"
}
~~~

Artifact entries have exactly `name`, `size_bytes`, `sha256`, `state` (closed,
partial or missing). Required-artifact loss gives capture.state partial or
failed; completed requires every profile requirement plus a sealed final
manifest. Valid states: unavailable, emergency_only, capture_in_progress,
completed, partial, failed (D00 meanings). Missing requirements are objects
with `class`, `context_origin`, `context_quality`, `evidence_mask`, `wire_issue`.
Failure enums are D00 numbers. The driver records `expected_fixture_class`
outside this manifest; an expected test scenario never fabricates the observed
class. dump_exception_kind is none, synthetic_callback or original_native.
Final evidence_bits includes manifest only in the final published record.

The default grade is closed_validated. A successful explicit OS flush may
record os_flush_completed; it still makes no universal power-loss guarantee.
File hashing/rename/flush failure after successful ack leaves a partial
observer/driver receipt with publication_failed, not a fake completed manifest.

## 8. Launcher, profile and verifier input contracts

All commands use structured UTF-8 JSON files, not shell command strings:

~~~text
qiven-diag-supervisor --launch-manifest <absolute-json-path> --result <absolute-json-path>
qiven-diag-inspector --bootstrap-handle <16-hex> --instance <16-hex>
qiven-diag-verify --manifest <absolute-path> --symbol-manifest <absolute-json-path> --result <absolute-path>
~~~

Inspector CLI is supervisor-only and validates inherited capabilities; the
numbers themselves confer no access. Supervisor command exits 0 only when
all declared fixture outcomes and its own containment succeeded, 2 for input
errors, 3 for setup/containment failure, 4 for an evidence/test failure and 5
for an external deadline. Expected deliberate target death is not supervisor
failure if the exact declared evidence passed. Native exit codes are recorded
in the result without being confused with supervisor status.

Launch manifest schema `qiven.diag.launch/1` contains exactly: schema,
artifact_root, inspector_image, inspector_sha256, profile, build_identity,
workspace_identity, targets. `profile` contains name=win-x64-minimal-v1,
max_artifact_bytes=67108864, max_artifact_count=8, max_total_bytes=536870912,
max_pending_captures=2, acknowledgement_timeout_ms=5000,
supervisor_deadline_ms=30000, install_reply_timeout_ms=50,
attempt_deadline_ms=40000, bootstrap_readiness_timeout_ms=5000,
modal_profile=interactive or noninteractive, and
expected_provider_binary_sha256/expected_dbgcore_sha256, plus
max_capture_attempts_per_hour=10. Root ledger and run receipts together reserve
1 MiB within max_total_bytes; artifact admission deducts this overhead first.

Each of at most 64 target entries has: case_id (unique bounded ASCII ID),
absolute image, image_sha256, argv (at most 32 arguments, total UTF-8 <=8192
bytes), process_name, expected_exit_kind (success, callback, native, observer
or timeout), expected_failure_class, required_classes (D00 fields), and
symbol_manifest, logging_requirement and capture_requirement (D00 numbers).
Argument encoding follows the qualified Windows CRT command
line convention and is round-trip tested; arguments are never passed to a
shell. Supervisor runs at most two armed targets concurrently and each entry
once. Unexpected failure stops further admissions. A predeclared negative case
whose exact expected outcome passes may be followed by the next case; it is
never retried automatically. Case names alone cannot waive an evidence requirement.

The whole input file is <=64 KiB. Paths/IDs/enum/number limits are validated
before launching. No free-form test code or commands are interpreted from
artifacts. A target's D00 config uses the same name/identities/root/storage
limits and required classes, with logging disabled for core fixtures.
Capture-disabled cases use plain contained observer launch: no bootstrap,
inspector or crash reservation is created. Their config obeys D00's disabled
rules; they cannot obtain successful capture by inheriting unused resources.

The supervisor's stdout is a dedicated parent-driver result pipe, independent
of every target's stdio. After containment is complete, a result worker writes
one u32 little-endian byte length plus that many UTF-8 JSON bytes (<=64 KiB),
then closes; no ordinary console text goes into that pipe. The driver drains
it concurrently under its own deadline. `--result` asks a storage worker to
publish the same JSON as a file. A blocked/failed pipe or file never blocks the
watchdog event loop; an incomplete frame is invalid and the driver records
supervisor exit/timeout with artifact/result-channel loss. Direct invocation
without a driver can retain the result file and exit code; it cannot promise
the unavailable independent fallback.

Result schema `qiven.diag.supervisor-result/1` contains exactly `schema`,
`run_id` (128-bit hex), `foundation_sha`, `launch_manifest_sha256`, `exit_code`,
`containment_complete` (bool), `artifact_storage_available` (bool), and `cases`
(<=64). Each case has `case_id`, `process_instance` (hex or null before launch),
`expected_exit_kind`, `observed_exit_code` (u32 or null), `observed_class`
(D00 number, 0 if unknown), `verdict` (pass/fail/blocked), `wire_issue`,
`evidence_bits`, `manifest_path` (normalized absolute path or null),
`manifest_sha256` (hex or null), and `elapsed_ms`. Expected values derive from
the launch input; observed values require actual evidence. This compact result
references artifacts; it does not inline dumps or invent missing manifests.

`symbol_manifest` names an absolute UTF-8 JSON file, <=1 MiB, schema
`qiven.diag.symbols/1`, with exactly `schema` and `modules` (<=1024). Each module
has absolute `image`, `image_sha256`, `pe_timestamp` (u32), `pe_size` (u32),
absolute `pdb`, `pdb_sha256`, `pdb_guid` (32 lowercase hex), `pdb_age` (u32),
and `required_frames` (<=32). Each frame has `role` (fault_instruction or
callback_frame), `symbol` (UTF-8 <=512 bytes), and `rva` (u32 or null). Native
fault probes require the expected instruction RVA; callback probes require
the named noinline fixture frame plus the labeled callback chain. No symbol
identity or RVA is accepted solely because it came from the dump being tested.

Verifier exit 0 means structural integrity, expected context and exact required
module symbols passed; 2 invalid input; 4 evidence/symbol mismatch. It validates
file sizes/hashes before parsing dump RVAs, matches PE image and RSDS PDB
GUID+age for host and faulting DLL, and resolves the native fault instruction
or labeled callback chain. It never accepts a same-named PDB alone or downloads
symbols implicitly. Its bounded JSON result records manifest SHA-256, verifier
build, matched image/PDB identities, expected/observed frame and mismatch reason.
Limits: 1024 dump modules, 4096 threads, 512 frames per requested walk and the
profile's dump-size cap; exceeding a limit is explicit partial/unsupported data.

Verifier result schema `qiven.diag.verify-result/1` contains exactly `schema`,
`manifest_sha256`, `symbol_manifest_sha256`, `verifier_sha`, `exit_code`, and
`modules`. Each module result has `image_sha256`, `pdb_sha256`, `pdb_guid`,
`pdb_age`, `identity_matches` (bool), `frames` and `reason` (UTF-8 <=256 bytes).
Frame results have `role`, `expected_symbol`, `observed_symbol` (string or null),
`expected_rva`, `observed_rva` (u32 or null), and `matches` (bool). Result size is
<=64 KiB; excessive requested output is an explicit input/evidence failure,
not truncated success. Optional unknown identities are null. The module/frame
order follows the trusted symbol input, so a missing module cannot disappear
from the result. All other JSON rules from section 1 apply.

## 9. Literal fixtures and decoder tests

W0 implementation includes independently authored literal fixtures for each
table: one valid request/reply/native/callback/empty-ring record, plus wrong
version/extent, oversized length, reserved bit, duplicate class, unknown context,
stale instance, missing group, excessive exception parameters and publication
0/1/4. Decoder tests must not construct all their expected bytes by invoking
the encoder under test. Endian/offset assertions, fuzzed bytes and truncation
at every fixed boundary must return typed failure without out-of-bounds access.

The Windows native reconstruction test compares every specified register,
segment, exception code/address/parameter and origin against an independently
constructed source structure. The actual Release dump probe additionally proves
that the debugger resolves the intended original instruction/callback chain.
A codec round-trip or nonempty minidump alone cannot pass that gate.

The read-only publication adapter above is a design inference restricted to
the pinned Windows-x64/MSVC tuple from Microsoft's
[aligned atomic-access guarantees](https://learn.microsoft.com/en-us/windows/win32/sync/interlocked-variable-access),
[/volatile:ms acquire semantics](https://learn.microsoft.com/en-us/cpp/build/reference/volatile-volatile-keyword-interpretation)
and [MemoryBarrier contract](https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-memorybarrier).
It must pass the B00/B03 rights and publication probes; an adapter failure does
not authorize granting the target write access to broker replies.

Codex (model not-introspectable; reasoning not-introspectable)
