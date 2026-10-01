# Foundation I2: ordered implementation and qualification

> Document ID: D13. Executable work contract for D10-D12, not a test receipt.
> Public declarations remain D00. Only I2 is designed in this revision.

## 1. Start state and dependencies

Start from the current Foundation branch after D10's accepted-document transfer.
The reviewed F0 is `032a6152efa18407d7b8d29d3203a5def179385a`. At that revision,
`src/crt_failure.cpp` is a headless compatibility helper; `src/contracts.cpp`
still formats to stderr and can break into a debugger before terminating.
Neither implementation is the full I2 client. Preserve their public entrypoints
while replacing their internals with the single process core.

Use the repository's existing workspace-bootstrap/gate/preset entry from its
README. Do not recreate retired generation scripts or bypass control-lock
resolution. Record the actual toolchain/SDK/CRT/compiler/linker flags, OS build,
provider binaries and shared/static configuration before a native run.

| Batch | Depends on | Concrete output and exit |
| --- | --- | --- |
| B00 — contract and codec foundation | Accepted transfer | C0 exact headers/export support; D11 private seam; common core ownership skeleton; D12 codecs and independent byte fixtures; install/requirements tests without pretending capture is ready |
| B01 — S0 and incident oracle | B00 | External supervisor containment/deadline/modal detector; forced driver failure cleanup; minimal historical regression fixture with frozen old-fail oracle |
| B02 — E1 provider pilot | B01 | Minimal UCRT/native client and external inspector for the A/B below; nominated mechanism meets gates or a CONTRACT-CONFLICT receipt stops its dependent work |
| B03 — IPC and artifacts | B02 | Full launch/rights/identity binding, provider I/O quotas, acknowledgement/finalization, independent emergency evidence, rate/retention and verifier |
| B04 — callback and ownership completion | B03 | Complete callback table, contracts/legacy adapters, required/optional/disabled outcomes, reentrancy and CRT-scope qualification |
| B05 — core qualification | B04 | All core rows below on the exact Release and Debug/static/shared tuples; installed crash-only consumers; `I2_CORE_QUALIFIED` or explicit blockers |
| B06 — real I1 integration | B05 plus independently qualified I1 ring/writer candidate | Real ring, stalled writer and host/module integration rows; `I2_INTEGRATION_QUALIFIED` bound to both candidates |

B00 does not wait for I1-A. The process core is a shared prerequisite owned by
whichever slice lands it first. Later I1 attaches to that core/seam; it cannot
introduce another singleton or copy the legacy callback stack. B02 is a small
proof candidate, then extended through B03-B04 on the same branch. It is not
permission to distribute an unqualified pilot as the complete service.

Each batch may use several focused commits. Its exit receipt identifies the
exact tested commit, rather than declaring an entire evolving branch green.
No new Skill, general logger implementation, Runtime migration or F2 ABI work
is a prerequisite to B00-B05.

## 2. B00: compile and wire gates

Extract D00's four public headers and D11's one private header without editing
their declarations. Implement the common ownership/install shell needed by
crash-only hosts. Keep disabled/unimplemented services truthful and compile
the unsupported-platform adapter. Update capability-surface rows only when
their named declarations and implementation actually exist.

Build each installed public header alone with C++20, no required PCH, and the
repository's no-exception/no-RTTI configuration. Generate/install export support
for both static and shared consumers. Compare header/seam hashes against the
independent accepted origin; hashes regenerated beside edited headers are not
acceptance. Use actual link/load tests in B05, not syntax checks as their proxy.

Implement bounded scalar codecs from D12 before native capture code consumes
them. Test literal valid and invalid bytes, all fixed-boundary truncations,
checked arithmetic, unknown enums/versions, zeroing, UTF-8 and process binding.
Pin MSVC `/volatile:ms` only for the Windows read-only publication adapter;
inspect its aligned load/barrier instructions. Test actual read-only reply
mapping permissions, not a writable stand-in. Ring Interlocked claims use
writable views. A codec test never invokes a native crash.

Private bootstrap/provider limits, callback tables and reasons have one
implementation definition each. Add generated schema/offset assertions but
keep independently written byte fixtures as the interoperability oracle.

## 3. B01: containment and the old-fail discriminator

No deliberate crash is launched before S0 passes. First prove: a sleeping
child, a hung inspector, a descendant that attempts breakaway, a deliberately
created modal fixture, supervisor death, and a test assertion while worker
threads are active. Every child is contained; no unrelated process is killed.
On assertion failure, signal stop before joining workers, preserve the original
failure and return within the outer deadline. Do not let a joinable-thread
destructor or blocking join hide the oracle.

Use an interactive desktop run to validate actual modal attribution. A
noninteractive run reports that distinct profile; it cannot certify an
interactive window scan. Both profiles still have an independent deadline.
No automatic restart/rerun follows an unexpected native failure. Predeclared
sample repetitions stop at the first unexpected failure, retain the artifact
and require an explained changed candidate before continuation.

The original Release fixture reproduces the mechanism recorded in pinned
Runtime `047cca6e4deb190920190c7bcf94e9ad7f527fdc`:

1. Parent opens one inheritable log file handle, uses that SAME handle for
   child's stdout and stderr, and passes the same path as its redirect target.
2. Frozen failing child performs the historical double `freopen` redirect and
   the first `printf`. Preserve checked-call results and actual UCRT outcome;
   if the pinned tuple does not reproduce the old failure, the regression row
   is blocked, not replaced with an unrelated fabricated crash.
3. The new-capture child keeps that deliberately faulty redirect but installs
   the new crash-only core first. It must preserve the original UCRT callback
   classification, exit 3143, independent capsule/dump and matching Release
   callback stack even when the log remains empty.
4. A separate corrected `_open` + `_dup2` child proves successful normal
   execution. Diagnostics capture is not claimed to repair the redirect bug.

Archive fixture source, compiler/CRT, launch setup and old/new binaries with
hashes. Distinguish the pre-helper exit `0xC0000409` from the F0 helper's 3143
without durable evidence. Never infer a specific native cause from that shared
Windows status alone. A deterministic UCRT invalid-argument fixture also runs
for C01/C02, but does not replace this incident-specific old-fail/new-evidence
discriminator. Runtime's real host lifecycle and installed Desktop H1 remain
later integration verdicts; the fixture does not mark them complete.

The existing Foundation `tests/crt_failure.cpp` only checks handler installation
and Debug report mode readback. Migrate incidental FILE/DEBUG mode assertions
to the new owned non-modal policy plus real contained callback probes. Retain
the no-window requirement; neither a non-null handler nor a zero-byte log is
sufficient evidence of capture.

## 4. B02: provider comparison and decision rule

Retain I0's Crashpad source pin
`ce308a86daa85e65df219ff5fb385095b0a1c467`. Resolve its full dependency/build
closure and licenses before measurement. Any necessary source-pin change is
recorded before the run, with reason and invalidated prior evidence. Compare
the nominated Qiven client/DbgHelp inspector against a minimal Crashpad adapter
on the same Release executable, CRT, fault sites, symbol files, storage and
prestarted-handler policy. Only provider adapter/build linkage may differ.

Required cases: deterministic UCRT callback, original aliased-stderr fixture,
unhandled native access violation, then inspector/handler death and unusable
destination. Measure 20 predeclared attempts per healthy fault case, with
isolated roots to respect the production rate ceiling; stop on the first
unexpected failure. Negative cases run once per failure mode. Run the Debug
callback/no-modal case separately. Record every sample, including cold first
sample; no warmup exclusion for the 5 ms one-shot install gate.

Both candidates must meet the same semantic contract: callback vs native
origin, exact symbol match, independent evidence, no modal, deadlines and
truthful degradation. A provider that cannot produce one required grade fails
that row. Direct fail-fast/WER results are a separate external profile, not
credited to either ordinary callback layer.

Before running, record cost fields: target private committed bytes, mapped
bytes, added target threads, install max/p50/p95, capture-ack max/p50/p95,
inspector/supervisor peak memory and CPU, total deployed bytes, dependency/
license files, custom adapter source lines excluding tests/generated/vendor
code, and number of lifecycle/restart/upgrade components operated by the host.
Report the Qiven supervisor cost on both legs where it is common. Crashpad's
own handler/database overhead is not hidden outside its leg. Count target
pause from actual fault to ack; a later manifest timestamp is not that metric.

Decision: retain the nominated small Qiven implementation if all hard gates
pass and Crashpad does not dominate it. Dominance means meeting every evidence
and degradation gate, being no worse in every recorded numerical cost, and
strictly better in at least one, with no additional mandatory lifecycle,
license or deployment obligation. A Qiven hard-gate failure, Crashpad dominance
or unresolved comparison uncertainty returns a design-decision receipt before
B03. Tradeoffs remain visible in the receipt; GLM cannot quietly choose a new
provider or loosen a budget after measuring. If Crashpad fails, retain its
negative evidence instead of claiming an unmeasured general inferiority.

## 5. Frozen profile, evidence and budgets

The following are inherited from the pinned I0 census or explicit stricter
profile choices made by this design; none is a measured result.

| Axis | Required threshold and measurement |
| --- | --- |
| Install | Every healthy one-shot sample <=5 ms from entry to return of full install, including mapping, registration and handler commit; prestarted supervisor/inspector startup is measured separately |
| Registration failure bound | <=50 ms reply wait; this bounds a failed/degraded attempt and does not relax the 5 ms healthy gate |
| Capture acknowledgement | Every healthy sample <=5,000 ms from first-fault tick to closed/validated target-dependent artifacts and published ack |
| External bound | 30,000 ms from valid fatal tick or explicit first-observation basis; record scheduling/detection delay; outer contained attempt 40 s |
| Artifact extent | <=64 MiB per complete set, including partial files; 2 MiB reserved for metadata/ring/emergency, so dump extent <=62 MiB; ring snapshot <=1 MiB |
| Retention | <=512 MiB aggregate including root ledger/receipts; at most 8 artifact sets and 2 armed captures in this stricter profile; inherits the I0 ceiling of <=100 crash directories |
| Storm | <=10 capture attempts per root per rolling hour; persistent rate history, no unbounded pending queue or automatic retry |
| Fatal target work | No dynamic allocation, normal logging, application lock, CRT formatting or symbolization; fixed capsule/context/emergency buffers; no target helper thread |
| Core storage | Declare exact fixed allocation/mapping inventory in B00; prove no per-fault growth or escaping unbounded queue; measure target and external-worker memory separately |
| Idle process cost | Record added threads, private/mapped bytes, helper CPU and deployment bytes; no fabricated inherited absolute threshold where I0 specified comparison only |
| Degradation | Every negative case has a typed bounded outcome; no hang, modal, invented success, wrong-instance artifact or unknown storage growth |

The smaller directory/set/dump limits refine the named I2 reference profile;
they do not alter D00 public defaults or silently loosen I0. Other qualified
profiles require an explicit predeclared limits/evidence receipt. I1 throughput,
writer CPU and emit latency formulas remain I1 gates, not prerequisites for a
crash-only receipt.

Default core positive fixtures use logging=disabled and capture=required.
Their required class is the actual fixture class, exact origin/quality below,
with `reason | appropriate_context | external_dump | supervisor_exit | manifest`.
No `ring_tail` is requested. A combined host smoke profile requires all six
callback/native classes together; sorted unique requirements fit D12's nine
entries. Negative/bypass fixtures explicitly declare their weaker expected
outcome before launch and never report required-profile success.

| Class | Qualified origin/quality | Callback capability mask / native mask |
| --- | --- | --- |
| ucrt_invalid_parameter, abort_call, cpp_terminate, pure_virtual_call, contract_failure | callback_origin / callback_context | Bits 1,2,3,4,6 / none |
| unhandled_native_exception (access violation probe) | native_exception / native_context | none / bit 5 |
| direct_fail_fast, stack_cookie_failure, pre_main_or_loader_failure, unqualified stack overflow | external_observer / observer_only when only exit observed | No callback/native capability; no external_dump without separately qualified external provider |

Those masks are a qualification target, not a declaration that the current F0
already supplies them. Observed unknown causes stay unknown. Callback_context
or native_context requires valid D12 groups; quality is not an integer rank.
Default external_class_mask is zero for dump coverage in this profile; bypass
observation does not pretend to cover those faults in-process. A user request
for unqualified external evidence fails required installation truthfully.

## 6. Gate-to-batch map

All D00 C01-C45 IDs remain normative. This table supplies concrete oracles
and the earliest final qualification batch; pilot results alone do not close B05.

| Gate | Final batch | Probe/oracle |
| --- | --- | --- |
| C01 | B05 | Release UCRT callback; independent completed set and exact callback symbols |
| C02 | B05 | Debug report returns handled/0 before invalid-parameter dispatch; actual fatal class remains UCRT, exit 3143, zero modal |
| C03 | B05 | Real abort; handler never recursively calls abort; constant reason/origin |
| C04 | B05 | EH-enabled fixture throws uncaught; Foundation itself still builds without exceptions; callback stack identifies terminating thread |
| C05 | B05 | Contained purecall fixture confirmed to enter the real CRT hook; direct helper invocation is not a substitute |
| C06 | B05 | Native AV in a noinline host site and loaded client DLL; original instruction/address/context resolves |
| C07 | B05 | Closed std handles, one aliased handle and duplicated handles; independent emergency/dump evidence |
| C08 | B06 | Actual I1 writer deliberately stalled before fatal; inspector never requires its lock/progress |
| C09 | B05 | Allocation hooks fail after install; fatal-call audit plus live callback probe shows no allocation request |
| C10 | B05 | Test-only fault at handler entry/publication; one guard and bounded direct termination |
| C11 | B05 | Inspector absent before install; optional degraded, required invalid host |
| C12 | B05 | Kill inspector after registration; bounded health change and partial/failed receipt |
| C13 | B05 | Root/open/write denied separately; correct storage issue and no completed claim |
| C14 | B05 | Constrained volume plus injected short write; partial files accounted and no durable success |
| C15 | B05 | Kill target before/after target-dependent close; only the latter may preserve a successful ack |
| C16 | B05 | Direct fail-fast bypass; observer floor and no callback claim |
| C17 | B05 | Pre-main fixture before install; no invented capsule/context |
| C18 | B05 | Same-named wrong PDB/PE, wrong GUID/age/hash; verifier rejects |
| C19 | B05 | Release optimized host and DLL matching PDB; fault instruction/callback chain resolves |
| C20 | B05 | Old PID/creation/nonce or another target handle; registration rejected |
| C21 | B03/B05 | Every fixed-boundary malformed/oversized capsule and unknown version; typed rejection |
| C22 | B03/B05 | Kill/stall writer at publication 0/1; decoder never calls provider as if committed |
| C23 | B06 | Real I1 overwrite, delayed old producer, dead writer/reader claim and post-watermark reservation; stable unique bounded snapshot with honest gaps |
| C24 | B05 | Two targets fault together; separate inspectors/identities, shared quota owner, no crossed artifacts |
| C25 | B05 | Repeated full install incl. after shutdown; no second service/restart |
| C26 | B05 | Loaded client tries host install; D00 ownership gate rejects it |
| C27 | B05 | Legacy first, then full install; same core upgraded and exit behavior retained |
| C28 | B05 | Full first, then legacy call; no handler replacement or second callback stack |
| C29 | B05/B06 | Installed static and shared crash-only hosts pass B05; later real emitter/module integration at B06 |
| C30 | B05 | One-root storm, ten rate credits, eleventh refusal; restart/orphans/clock rollback cannot reset bound |
| C31 | B05 | All healthy ack samples meet 5 s; target alive through target-dependent close |
| C32 | B05 | Hung dump, emergency write, final hash and storage worker independently; external cutoff/classification |
| C33 | B05 | Cold and repeated fresh-process installs, every sample <=5 ms; full tuple recorded |
| C34 | B05 | Link/import/disassembly and source call-graph audit of every fatal entry, including legacy/contracts, plus allocator tripwire |
| C35 | B05 | Every native callback fixture and standalone narrow/wide report-policy probe under modal detector; interactive and noninteractive results distinct |
| C36 | B05 | Thread-local invalid handler and later handler override interference; uncovered scope explicit, no false required startup guarantee |
| C37 | B02/B05 | Same-case pinned Crashpad/Qiven A/B, frozen decision rule; final candidate retains proved behavior |
| C38 | B05 | Ordinary profile leaves WER policy unchanged; external WER dump support is separately qualified or explicitly unsupported |
| C39 | B05 | Barrier-synchronized fatal threads and recursive fault while copying; immutable primary capsule, partial shortened result |
| C40 | B03/B05 | Wrong rights, spoofed/bootstrap/reply handles, nonce, root identity and stale mapping; target cannot write broker replies |
| C41 | B05 | Ack succeeds then hash/manifest rename fails; ack retained, final state partial with publication_failed |
| C42 | B05 | Zero/below-minimum quotas, concurrent reservations, oversized dump seek, partial-file and receipt accounting; refusal without growth |
| C43 | B01/B05 | Historical stderr regression plus Foundation contract failure at exit 3; original failing expression's site recovered from symbols |
| C44 | B00/B05 | Independent context literals and native reconstruction; missing groups/oversized unsupported encoding never gain native quality |
| C45 | B05 | Disabled/optional/required permutations, every class/artifact bit, invalid combination and linked-CRT scope |

B06 additionally runs applicable D00 L/H rows that protect the shared core,
ring and package. A synthetic standalone ring is useful for decoder tests but
cannot close C08/C23 or claim actual logger integration. A real loaded DLL can
prove crash-only ownership/context at B05; emitter behavior still waits for I1.

## 7. B03-B05 implementation details that remain gates

Use installed-package consumers outside Foundation's source build. Link a
static host separately, then a shared host plus a loaded client DLL using the
same qualified CRT tuple. Inspect import/export tables, PDB inventory and
deployed provider paths. Prove one service, module unload after caller quiescence,
service image pinning through host move/drop/shutdown, and safe legacy calls
before/after install. A second static service copy in a module is a rejected
packaging arrangement, not a successful one-core test.

The fatal-call audit includes compiler-generated helpers/imports and any
stack-probe/security checks on the actual optimized build; a handwritten
no-allocation claim is insufficient. Narrow/wide CRT report hooks cover the
configured shared CRT and test error/assert paths without relying on debugger
presence. Raw CRT reports are non-modal policy probes, while Foundation contract
failures are fatal; do not misclassify Debug's preliminary invalid-parameter
report as a contract fatality. Direct helper calls test internals only. If a platform/CRT bypasses
an entry, classify that path honestly instead of claiming a handler was called.

Required handler scope is host-owned: existing incompatible handlers fail
required install; later/thread-local foreign replacement invalidates that
coverage. The host policy forbids such replacement in the qualified tuple.
No bounded health() call claims to enumerate every future thread's CRT state.
Add a mechanical audit of governed handler setter sites and runtime interference
probes; a foreign CRT copy requires separate coordination and qualification.

Verify local restricted artifact ACLs and that no uploader/network symbol
fetch exists. Seed unrelated heap buffers with a known secret and record
whether the selected dump includes them; do not enable full-memory streams.
Stack/nearby committed memory can still contain application data. The result
must state actual exposure and the local access/retention policy, not claim
perfect redaction because a seed happened to be absent.

WER qualification, when needed, is a distinct opt-in test profile on an owned
test environment with exact OS/settings/capture receipt and restoration record.
The ordinary I2 executable never edits global WER settings. Without that
separate run C38 passes only the unsupported/observer boundary, and its dump
support remains blocked; it is not a WER success receipt.

## 8. Receipts, completion and implementation handoff

Keep reviewable receipt summaries under
`docs/architecture/diagnostics/receipts/<candidate-sha>/<batch>.md`; machine
results/fixtures live under `tests/diag/crash/` or the bounded artifact root.
Do not commit large dumps/PDBs or machine-private paths/secrets into docs.
An artifact inventory records hashes, sizes, qualified access location and
retention so another reviewer can verify the exact run.

Each batch receipt includes: approved design head and transfer hash; exact
Foundation candidate; toolchain/OS/CRT/provider/build tuple; profile and input
hashes; prerequisite receipt hashes; commands; per-gate expected and observed
outcome; all timing samples/maxima; failure/cleanup state; artifact/dump/PE/PDB
identities; and explicit blockers. States are `pass`, `fail`, `blocked` or
`outside_profile` with reason. Missing platform, tools or evidence is blocked,
never pass. Narrowing the profile cannot remove a core-required gate silently.

`I2_CORE_QUALIFIED` requires B00-B05 and every core row above. C08/C23 and the
real-I1 part of C29 are marked `blocked_by_I1`, not failed core or passed overall
I2. `I2_INTEGRATION_QUALIFIED` requires B06 on the exact real-I1 candidate plus
its compatible core receipt. If either shared contract/core changes, rerun the
affected evidence; historical receipts remain identifiable.

The handoff to I3 names the installed Foundation package, approved design,
supervisor/inspector/verifier binaries, deployment/launch template, supported
CRT/platform tuple, required startup configuration, actual evidence coverage,
known bypasses and exact receipt paths. Runtime then wires real host ownership,
packaging and entrypoints; Devkit owns workspace-wide enforcement. Their
integration commits and original installed Desktop H1 remain independent
verdicts. Context records those refs and progress only, as D10 requires.

Do not advance capability inventory to landed/qualified from this document
alone. This PR supplies design authority; the implementation agent supplies
code and evidence, and existing owner/repository acceptance rules still apply.

## 9. Pinned evidence used for this design

- [Foundation F0 tree](https://github.com/JasonHuang3D/qiven-foundation/tree/032a6152efa18407d7b8d29d3203a5def179385a): README, CMake, contracts and CRT helper/test inspected selectively.
- [Runtime I0 census](https://github.com/JasonHuang3D/qiven-runtime/blob/047cca6e4deb190920190c7bcf94e9ad7f527fdc/docs/design/cpp-diagnostics-i0-census.md): sections 3-4 and 7-9 supply incident/provider pins, accepted budgets and evidence boundaries.
- [Runtime redirect source](https://github.com/JasonHuang3D/qiven-runtime/blob/047cca6e4deb190920190c7bcf94e9ad7f527fdc/apps/runtime_host_main.cpp): incident comments and corrected `_open`/`_dup2` path.
- [Runtime lifecycle regression](https://github.com/JasonHuang3D/qiven-runtime/blob/047cca6e4deb190920190c7bcf94e9ad7f527fdc/tests/host_server_lifecycle.cpp): ALIASED-STDIO fixture launch shape; retained as provenance, not copied as a claim that I2 is already implemented.

Codex (model not-introspectable; reasoning not-introspectable)
