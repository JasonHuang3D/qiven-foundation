# Qiven Foundation Architecture

This document is the current architectural contract of Qiven Foundation
(replaced 2026-09-26 per the accepted qiven-docs PR4 audit; the bootstrap
original with its amendments is preserved verbatim at
`docs/legacy/architecture/foundation-bootstrap-and-amendments-2026-09-25.md`).
Accepted Context decisions it executes: ADR-0024 (semantic ownership over
consumer count), ADR-0008 (explicit ownership/failure/allocation),
ADR-0009 (separate representation boundaries), ADR-0017 (semantic layering).

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
   cursors, not for an owning bounded growing builder (the admitted,
   unlanded `byte_builder` gap in the capability inventory). Likewise a
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
on higher-level Qiven repositories. Third-party dependencies require a
specific architectural justification; the default is none. Circular
dependencies between Foundation modules are forbidden.

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
non-owningly (the owning growing builder is the admitted `byte_builder`
gap, not yet landed).

## 10. ABI policy

Before 1.0, no stable C++ binary ABI is promised. Binary compatibility
boundaries are introduced intentionally where a real distribution or
plugin requirement exists.

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
