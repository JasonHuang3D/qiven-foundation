# Qiven Foundation Architecture

This document is the current architectural contract of Qiven Foundation
(replaced 2026-09-26 per the accepted qiven-docs PR4 audit; the bootstrap
original with its amendments is preserved verbatim at
`docs/legacy/architecture/foundation-bootstrap-and-amendments-2026-09-25.md`;
targeted amendment 2026-09-29 per the accepted qiven-docs PR6 companion
and ADR-0059 — the Section 3 dependency default, the Section 9
representation appendix, the Section 10 ABI policy and the Sections
11/13 platform/build clauses; the pre-amendment text at commit
`5e3e018` is preserved by git history).
Accepted Context decisions it executes: ADR-0024 (semantic ownership over
consumer count), ADR-0008 (explicit ownership/failure/allocation),
ADR-0009 (separate representation boundaries), ADR-0017 (semantic
layering), ADR-0059 (C++ diagnostics infrastructure program and this
boundary amendment).

## 1. Mission

Qiven Foundation provides low-level, reusable C++ building blocks for the
Qiven ecosystem: code that must remain portable, predictable, testable,
explicit about cost and failure, and suitable for performance-sensitive
systems.

Foundation is not a miscellaneous `utils` repository, not a home for product
or domain logic, not a wrapper around every OS/standard-library facility,
not a place to hide expensive work behind convenient APIs, and not a
compatibility layer for preserving bad historical abstractions.

## 2. Admission: semantic owner and first real consumer

Admission is decided by semantic ownership, not by consumer count or
minimal-primitive-count as a proxy for discipline (ADR-0024; ADR-0007's
wait-for-broad-demand threshold is superseded).

1. **Admission by semantic owner and first real consumer.** Before a
   dependent implementation, list its invariants, owner, lifetime, failure
   channel, bounds, concurrency, platform cases, and observable resource
   cost. Place a stable, product-independent semantic operation in
   Foundation if Foundation owns it — even with one consumer, and even if
   it composes multiple primitives, owns memory, or wraps an OS operation.
   Product policy, cognition-specific admission, transaction policy and
   process supervision remain above Foundation; an unknown contract calls
   for a bounded disposable probe with an explicit falsifier.
2. **Complete the admitted operation.** A lower API that exposes only
   `ByteCursor`/`ByteWriter` primitives is complete for borrowed bounded
   cursors, not for an owning bounded growing builder (`byte_builder`,
   admitted via `docs/architecture/byte-builder-admission.md` and landed
   2026-09-28 with the RR-0 batch). Likewise a
   lexical path parser is not proof of filesystem authorization. For each
   admitted consumer, either implement the owned/bounded composition at
   its lower semantic owner with explicit cost/failure/limit, or record
   why the needed semantics belong to a named higher layer. A general ban
   on "high-level encapsulation" is rejected; no abstract maximum
   abstraction height governs admission. Hidden expensive work is still
   prohibited: allocation, filesystem I/O, locking and failure stay
   observable in the API.
3. **Public-contract and consumer proof.** Publish an operation contract
   and exact source baseline before the first dependent change. Lower unit
   tests cover malicious/boundary input and representation/lifetime cases;
   consumer integration tests compile/link the exact lower revision and
   exercise the real call path, including a negative test against the
   previous defect; Windows-native alias/reparse/path cases are tested on
   Windows when relevant. Receipts record the **same candidate SHAs** and
   show which hand-coded upper path was removed. A Foundation-only green
   test does not close an upper-layer boundary.
4. **Invariant-preserving boundaries.** Downward dependencies only;
   explicit lifetime/allocator provenance; checked arithmetic; deliberate
   platform isolation; no-exception consumers remain supportable. A public
   C++ type cannot silently authorize a path, define a wire frame, or make
   another process obey it. If a security or transaction policy spans two
   frames or layers, the policy's owning layer publishes the whole
   operation and Foundation supplies only semantically lower pieces.
5. **Discovery and enforcement.** The active README, AGENTS entry and
   capability inventory point to this document and the accepted Context
   decisions. Machine-readable capability rows distinguish `landed`,
   `admitted-not-yet-landed` and `historical`, and name the header,
   contract, consumer and pinned proof; a design proposal is never
   presented as a delivered header.

## 3. Dependency law

Dependencies point downward. Public Foundation code may depend on: the C++
standard library; lower-level Foundation components; operating-system
primitives behind explicit platform boundaries. Foundation must not depend
on higher-level Qiven repositories. Circular dependencies between
Foundation modules are forbidden.

Third-party dependencies are admitted by recorded evidence, with no
directional default. Foundation owns stable, product-independent
semantics. A private implementation may use a pinned third-party
component, a platform facility or Qiven code after an I0-style
comparison of required coverage, correctness, latency and worst-case
cost, security/maintenance history, compiler/platform support,
deployment/offline cost, provenance and license/NOTICE obligations.
Dependency admission is a recorded decision for the exact consumer and
build closure, with a provider-independent public contract, tests
against plausible failures and a revisit trigger. Neither using a mature
library nor rewriting it is automatically preferred. Public headers and
independently versioned ABI tables may not expose provider-specific
types. This changes a default preference, not the ownership test in
Section 2 or an accepted Context representation rule; the governing
acceptance transaction is ADR-0059 (qiven-docs PR6, accepted 2026-09-29).

## 4. Namespace law

The C++ root namespace is `qiven::`. Foundation does not introduce a
`qiven::foundation` namespace; public symbols live in the shortest
namespace that communicates their real semantic domain. `detail`
namespaces are implementation details, not API contracts. The CMake target
is `qiven::foundation`.

## 5. Public and private boundaries

Public headers live under `include/qiven/`; implementation and private
headers under `src/`. A public header must be self-contained: including it
in an otherwise empty translation unit must compile.

## 6. Language and toolchain baseline

C++20. Buildable with mainstream MSVC, Clang, and GCC; exact minimum
versions are a CI contract recorded only when verified. Compiler extensions
are not part of the portable API unless isolated behind an explicit layer.

## 7. Error handling and arithmetic

Low-level public APIs make failure visible in their signatures. Exceptions
are not required for ordinary control flow; no-exception consumers remain
supportable. The common vocabulary is `qiven::Result<T, Reason>`
(`result.hpp`): holds exactly one of a success value or a typed reason,
defaults the reason to `qiven::Error`, allows domain substitution,
`[[nodiscard]]`, no allocation/throwing in its own operations, misuse of
`value()`/`reason()` is a programming error (`QIVEN_ASSERT`); copy
operations exist only while both alternatives are copy-constructible.
(The design history of the `Result<void>` specialization is preserved at
`docs/legacy/design/result-void.md`.)

Arithmetic that derives byte counts, capacities, offsets, or externally
controlled lengths must not silently wrap: checked arithmetic reports
overflow/underflow explicitly; saturating or intentionally wrapping
arithmetic uses distinct APIs. Integer conversions that may be
unrepresentable use `checked_integer_cast`; an unchecked cast requires a
prior proof or an explicit lossy contract.

## 8. Memory and ownership

Ownership must be visible. Primitive APIs avoid hidden dynamic allocation
the caller cannot reason about. The allocator boundary is
`qiven::memory::AllocatorRef` (non-owning, type-erased; backend outlives
every reference). Raw allocations always carry size and non-zero
power-of-two alignment; failure returns `nullptr`; zero-size canonicalizes
to `nullptr`. `qiven::memory::Layout` is a validated size+alignment value.
Deallocation of `nullptr` is a no-op; non-null pointers return to the same
backend with the same size/alignment. No mutable process-global default
allocator. `SystemAllocator` is one hosted backend; `LinearArena` is a
non-owning fixed-capacity bump allocator (reset is bulk reclamation; not
thread-safe); `OwnedAllocation` is a move-only raw-storage owner;
`OwnedObject<T>`/`OwnedArray<T>` own live objects with non-throwing
construction/destruction and `std::nullopt` failure representation.

## 9. Representation and boundary law

An in-memory C++ representation is local unless an explicit boundary
contract says otherwise. Raw pointers, function pointers and process
addresses are process-local capabilities. Cross-process communication
requires an explicit transferable representation; shared-memory structures
must not depend on absolute process-local pointers; network and persistent
formats define their own widths, byte order, compatibility and versioning.
Endian codecs explicitly convert between fixed-width values and bytes,
host-endian-independent, non-throwing, allocation-free. A Foundation
source-level API is not automatically a module ABI, IPC representation,
wire format, or persistent format (ADR-0009). `std::span` is the
non-owning bounded view vocabulary. `qiven::ByteCursor` consumes immutable
bytes non-owningly; `qiven::ByteWriter` reserves bounded mutable ranges
non-owningly; `qiven::ByteBuilder` is the owning growing builder
(`byte_builder.hpp`, landed 2026-09-28; admission record at
`docs/architecture/byte-builder-admission.md`).

A C++ type, including `Result<T>`, `std::span`, `AllocatorRef` and their
template/function-pointer implementation details, is a source-level
vocabulary, not a stable independently versioned binary layout. Moving a
function into a DLL/.so/.dylib does not turn these types into a portable
module ABI. Process-global services are owned by one host instance even
when its clients reside in several loaded images. Shared memory, module
calls, persistent artifacts and wire/IPC messages each declare separate
lifetime, width, alignment, version and validation rules.

## 10. ABI policy

Foundation defines three intentionally different contracts (the table is
the summary; the clauses below are the law):

| Boundary | Intended consumer | Compatibility promise |
| --- | --- | --- |
| C++ source API | Rebuilt Qiven components, static or lockstep shared linkage | C++20 source contract; binary participants use one verified toolchain/runtime/build tuple and are released together. |
| Independently versioned in-process module ABI | A DLL/.so/.dylib or plugin that may be built or updated separately | Narrow, versioned C-callable function table with opaque handles and explicit ownership; per-platform binary and architecture compatibility, never one binary for all OSes. |
| Cross-process/crash artifact format | Inspector, supervisor and forensic tools | Independently versioned, pointer-free representation with validation; it is neither the C++ object layout nor the in-process C ABI. |

Foundation supports static linking and a lockstep shared-library build
from a common source contract. For the shared build, explicitly export
only intended symbols, hide private symbols where supported, publish a
relocatable CMake package and record the required compiler, C++
standard library, CRT, architecture, build configuration and dependency
closure. A lockstep C++ shared consumer is rebuilt and released against
the same qualified tuple; there is no general promise that arbitrary
C++ classes or templates remain binary-compatible across independent
upgrades.

Where independently built or upgraded modules are required, define a
separate, small C-callable ABI at the semantic owner's boundary. Use
fixed-width scalar fields, explicit calling convention and
packing/alignment contract, sized/versioned structs, feature/version
negotiation, opaque handles or borrowed buffers, bounded lengths and
explicit status codes. Never pass STL types, `Result<T>`, exceptions,
RTTI-dependent objects, C++ allocators, ownership-bearing file/CRT
objects or an unversioned C++ vtable across that boundary. Pointers are
valid only inside the current process and for the documented
call/handle lifetime. Allocate and release across a boundary through
the same owner or caller-provided storage; all callbacks and
module-owned objects are quiesced before unloading their code.

A process-wide diagnostic installer and fatal handler are owned by one
host service. Loaded modules obtain a capability handle or function
table and emit records through that owner; they do not each install
CRT/signal/Mach handlers, start an independent writer, or instantiate a
private crash ring. The crash service itself stays loaded for the
process lifetime once handlers may reference its code; hot reload is
not implied by shared linkage. Multiple static copies of the process
service in one process are a configuration error.

The first independently versioned ABI slice is the diagnostics client,
once a real module consumer and compatibility tests exist. Other
Foundation operations retain their C++ source API unless a separately
justified module boundary needs them. Building Foundation as a shared
library does not by itself give the C++ API an independently stable
ABI; conversely, supporting an independent plugin does not require
turning every Foundation template or value type into a C facade.

The C-callable API may be implemented as an exported
version-negotiating entrypoint returning a sized function table. The
first diagnostics slice could take this **illustrative** shape — not a
landed contract; the exact names, representation, maximum length,
thread-safety and registration/unload mechanics are fixed by the first
implementation contract — with `QIVEN_CALL` and `QIVEN_EXPORT` defined
per platform:

~~~c
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t size;
    uint32_t abi_major;
    uint32_t abi_minor;
    void* host_context;
    int32_t (QIVEN_CALL *emit)(void* host_context,
                               const uint8_t* bounded_event,
                               uint32_t byte_count);
} QivenDiagApiV1;

QIVEN_EXPORT int32_t QIVEN_CALL qiven_diag_query_api(
    uint32_t requested_major, uint32_t caller_size, QivenDiagApiV1* out);

#ifdef __cplusplus
}
#endif
~~~

The payload is a bounded, versioned borrowed view valid through the
call; an asynchronous host copies or publishes it under the same
backpressure contract. ABI versions are scoped per platform and
architecture, with an explicit major-version refusal and tested
minor-version extension behavior.

## 11. Platform policy

Desktop/server platforms: Windows, Linux, macOS. x86-64 is the baseline;
ARM64 is a first-class direction whose support becomes a contract only
when actually built and tested by dispatched CI runs. The CI workflow is
**explicit-dispatch** (`workflow_dispatch`): a dispatched run verifies
Windows MSVC, Ubuntu GCC/Clang, and macOS x64/ARM64 configurations in
Debug and Release, plus an Ubuntu/Clang no-exceptions/no-RTTI contract
build under ASan/UBSan. These describe what a dispatched run verifies —
they are not a continuous-verification claim. Hosted-runner compiler
versions are not minimum-version promises. Platform-specific code is
isolated from portable code.

Portable semantics do not imply identical fault hooks, dump formats or
fault-class coverage. Logging vocabulary, bounded event transport,
install-health semantics and artifact manifest are common; Windows
UCRT/SEH, Linux POSIX signal/core-dump facilities and macOS platform
exception/report facilities are separately qualified backends. A Linux
fatal signal handler obeys its async-signal-safety contract; Windows
handler assumptions cannot be copied into it. Each OS/architecture
advertises actual capture capabilities and grades in a versioned
capability matrix. No platform is labeled crash-covered merely because
the common headers compile or a Windows probe passes.

## 12. Testing law

Every public component requires tests for its contract, including boundary
and failure cases. A bug fix normally adds a regression test (for
defect-bearing contracts: old-fail/new-pass discrimination per the Devkit
testing standard). Sanitizers, static analysis and platform validation
belong in the pipeline, not the runtime dependencies.

## 13. Development environment

CMake is the build-system source of truth; IDE project files are outputs.
Visual Studio 2022 is a first-class Windows environment; shared
configuration lives in `CMakePresets.json` and convenience tooling
delegates to presets. Dependency resolution is workspace-resolved (the
control-repository lock; see the README build section). IDE convenience
must not compromise command-line, CI, or non-Windows builds.

Foundation's CMake source of truth offers explicitly named static and
shared distributions without accidentally placing duplicate
process-global diagnostics instances in one address space. Build the
selected variant with correct platform symbol visibility/import-export
definitions, position-independent code where needed, transitive usage
requirements, a relocatable install/export target and packaged runtime
dependencies. A generated export header is a mechanical aid, not a
declaration of ABI stability. The selected process service, linkage
form and module ABI version are part of the workspace's exact
candidate/build receipt.

## 14. Change rule

Before a new capability enters this repository, ask (per §2):

1. Is Foundation the natural semantic owner (ADR-0024), independent of
   consumer count?
2. Can its ownership, failure behavior, bounds, concurrency and observable
   cost be made explicit?
3. Does its dependency set remain one every higher layer can inherit?
4. Is the contract more stable than the use case that motivated it?
5. Would the capability carry higher-level policy that belongs in a layer
   above?

"Broadly useful" and convenience alone are not admission reasons, and
**they are not vetoes either**: one real consumer with clear semantic
ownership suffices.
