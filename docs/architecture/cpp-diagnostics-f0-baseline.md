# C++ Diagnostics F0 Baseline — Inventory, Selections and Matrices

> Status: F0 deliverable of the C++ diagnostics infrastructure program
> (qiven-docs PR6, accepted 2026-09-29; adopted as program law by
> qiven-context ADR-0059, accepted 2026-09-29). This document records
> the pre-implementation inventory and boundary selections the amended
> architecture requires (PR6 doc 02 section 4 gate F0). It claims no
> implementation: I1/I2/F1-F3 deliverables are unlanded.
>
> Baseline commit: qiven-foundation `5e3e018` (== main at F0 drafting;
> local == remote verified). Workspace: generation
> `sha256:e323ed911a9fc668ffcfc1f2a4f52a2c6c87fff1cefdbcf006e15b15a3d2ff58`
> at F0 start (advanced to `sha256:6dc3171e…` by the ADR-0059
> publication in the same window).

## 1. Scope and authority

F0 lands the targeted owning-repository architecture amendment (applied
to `foundation.md` in this transaction) plus this baseline: the
current source API / export-symbol / process-global-state inventory,
the static/shared/independent-module selection per real consumer, the
pinned toolchain tuple, and the declared platform and ABI acceptance
matrices (PR6 doc 02 section 4.1). Mechanism/provider selection is NOT
made here: I0's predeclared comparison decides mechanisms and fault
classes (PR6 doc 00 section 3); this document declares the candidate
classes and the decision vehicle only.

## 2. Source API inventory (public surface at the baseline)

All public headers live under `include/qiven/` (26 headers; every one
self-contained per foundation.md §5). Semantic groups:

- **Vocabulary/platform**: `types.hpp`, `platform.hpp`, `compiler.hpp`,
  `config.hpp`, `architecture.hpp`, `alignment.hpp`.
- **Contracts/diagnostics-adjacent**: `contracts.hpp` (QIVEN_ASSERT
  contract-failure channel), `crt_failure.hpp` (the 2026-09-27 incident
  primitive — see §4; becomes a compatibility wrapper during I1/I2
  migration per PR6 doc 01 §13).
- **Byte representation**: `byte_cursor.hpp`, `byte_writer.hpp`,
  `byte_builder.hpp` (landed 2026-09-28), `endian.hpp`,
  `checked_span.hpp`.
- **Arithmetic**: `checked_arithmetic.hpp`, `checked_integer_cast.hpp`.
- **Errors/results**: `error.hpp`, `result.hpp` (Result<T,Reason>).
- **Hashing**: `hashing.hpp` (fnv1a64), `hashing_sha256.hpp`.
- **Memory**: `memory/allocator.hpp` (AllocatorRef), `memory/layout.hpp`,
  `memory/linear_arena.hpp`, `memory/owned_allocation.hpp`,
  `memory/owned_array.hpp`, `memory/owned_object.hpp`,
  `memory/observer.hpp`, `memory/system_allocator.hpp`.

Implementation translation units (5): `src/contracts.cpp`,
`src/crt_failure.cpp`, `src/memory/linear_arena.cpp`,
`src/memory/owned_allocation.cpp`, `src/memory/system_allocator.cpp`.

Representation-boundary facts the amended §9/§10 now state explicitly:
`Result<T>`, `std::span`, `AllocatorRef` are source-level vocabulary,
not stable binary layouts; no STL type, exception, C++ allocator or
unversioned vtable crosses a future module ABI; the cross-process
crash-artifact format is a separate versioned pointer-free contract
(I2).

**Typed-path dependency declaration (program law, PR6 doc 01 §2.1
review F2)**: the diagnostics program's new path-bearing surfaces
(crash-artifact directories, registration destination roots, log file
identity, retention/cleanup paths) declare themselves consumers of a
typed path primitive when it lands; until then every new raw
path-string comparison site the program introduces is recorded in the
I0 census and swept at the primitive's landing. Foundation's current
surface introduces no filesystem-path API (no path header exists at
this baseline).

## 3. Export-symbol and linkage baseline

- CMake declares exactly one library target:
  `add_library(qiven-foundation STATIC)` + alias `qiven::foundation`
  (CMakeLists.txt:16-17). There is no shared variant, no
  install/export packaging, no visibility-control machinery, and no
  generated export header at this baseline.
- Because the only distribution is static, all symbols are internal to
  each consumer's link; there is no exported-symbol surface to
  inventory beyond the header API above. The F1 shared build (with I1)
  introduces explicit export/hide, relocatable packaging and the
  external installed-package consumer probes (PR6 doc 02 section 4
  F1).
- Real consumers today (workspace lock edges, all first-party-source,
  all lockstep): qiven-runtime, qiven-context-draft, qiven-math — each
  resolves Foundation at its locked node in the WorkspaceGeneration and
  rebuilds against it. No third-party or out-of-tree consumer exists.

## 4. Process-global state census (complete at the baseline)

Exactly ONE process-global state site exists in Foundation today:

- `src/crt_failure.cpp:29` — `volatile LONG g_entered_fault_handler`
  (re-entrancy guard), plus the process-wide CRT handler state
  `install_headless_crt_failure_behavior()` installs on Windows:
  `_set_invalid_parameter_handler`, `_set_abort_behavior`, and the
  Debug-only `_CrtSetReportMode`/`_CrtSetReportFile` trio. On
  non-Windows the installer is a no-op.
- No other mutable globals, statics or thread-locals exist in `src/`
  (git-grep verified at the baseline; `src/contracts.cpp` and the
  memory backends are stateless per-object; foundation.md §8's "no
  mutable process-global default allocator" holds — the allocator is
  an explicitly passed `AllocatorRef`).

The current incident primitive's documented weaknesses (PR6 doc 01 §1)
are the migration obligations: CRT `snprintf`/`strlen` on the fault
path, `GetStdHandle(STD_ERROR_HANDLE)` dependence (the possibly-aliased
stderr class), and fixed-code `TerminateProcess` termination with no
fault-context capture. The I0 census extends this Foundation-side
census to every governed executable (Runtime-side handlers,
entrypoints, stdio redirection and loader-order facts).

## 5. Distribution/module selection per real consumer

| Consumer class | Real consumer today | Selected contract | Gate |
| --- | --- | --- | --- |
| Process host (installs the one process-global diagnostics service) | qiven-runtime hosts (RuntimeHost, hook exe, runtimectl — I0 census enumerates) | C++ source API, static linkage (current); the host owns the single installer once I1/I2 land | I3; Devkit declaration |
| Lockstep shared client | none yet (all consumers static today) | C++ source API; lockstep shared distribution delivered at F1 with I1 | F1 |
| Independently versioned module | none (no real out-of-tree/independently built consumer exists) | Admitted, unlanded: narrow C-callable ABI only when a real module consumer exists | F2 |
| Cross-process artifact consumer (inspector/supervisor) | none yet | Versioned pointer-free artifact format | I2 |

Selection rationale: every real consumer today is a lockstep
first-party static consumer resolved through the workspace lock, so the
current static-only distribution remains the delivered baseline; the
amended §10/§13 clauses make the F1 shared distribution and the F2
module ABI deliberately gated deliveries rather than implied by the
amendment. No existing header requires refactor before admitting the
diagnostics boundary (PR6 doc 02 section 4: "The current Foundation
static library is a valid F0 baseline").

## 6. Toolchain pin (qualified tuple baseline)

- **Build tools**: the workspace lock's `qiven-toolchain-win` node
  pinned at commit `9f6ce2a9…` (pinned executables: CMake and
  clang-format, per `toolchain.json` inside that revision;
  repository-manifest declaration, identity-checked at consumption —
  WR-5 law).
- **Compilers**: exact minimum versions are a verified-CI contract only
  (foundation.md §6). The diagnostics program's first Release fault
  proofs run on the Windows/MSVC first-class environment (VS 2022;
  exact toolchain version resolves live at build time and is recorded
  in each program receipt per PR6 doc 00 §7); non-Windows claims wait
  for their own F3 probes.
- **Standard**: C++20 (foundation.md §6). The diagnostics program must
  not break the existing no-exceptions/no-RTTI contract build class
  (§11 CI dispatch description).

**Providers**: none pinned at F0. The I0 comparison (PR6 doc 00 §3)
studies pinned Quill/spdlog (async logging), Crashpad /
DbgHelp-MiniDumpWriteDump / WER (crash capture) against the smallest
Qiven implementations, on predeclared budgets; selections are recorded
decisions with rationale and revisit triggers, presented to the owner
at the I0 boundary. The amended §3 dependency law is the admissibility
gate for any pinned provider.

## 7. Declared acceptance matrices (declarations, not claims)

### 7.1 Platform matrix (per PR6 doc 02 §2.4 + F3)

| OS/arch | Status at F0 | Claim requires |
| --- | --- | --- |
| Windows x64 (MSVC, Debug+Release) | First Release fault-proof target (the original H1 regression class) | I2 UCRT + access-violation classes at their evidence grades; F3 matrix for the coverage claim |
| Linux (Ubuntu GCC/Clang) | Admitted, uncovered | Own F3 fault/symbol probes (ELF build ID ↔ debug file); async-signal-safe client law |
| macOS (x64/ARM64) | Admitted, uncovered | Own F3 fault/symbol probes (Mach-O UUID ↔ dSYM); platform report-mechanism qualification |
| Any OS: no-exceptions/no-RTTI contract build | Standing CI dispatch class | Diagnostics changes keep it green |

No platform is labeled crash-covered by header compilation or by a
Windows probe passing (amended §11).

### 7.2 ABI/distribution matrix

| Contract | Status at F0 | Delivery gate |
| --- | --- | --- |
| C++ source API (static, lockstep) | Delivered (sole distribution today) | Standing |
| Lockstep shared distribution (export/hide, relocatable package, external consumer) | Unlanded | F1 (with I1) |
| Independently versioned C-callable module ABI | Admitted, unlanded (no real consumer) | F2 |
| Cross-process crash-artifact format | Unlanded | I2 |

## 8. F0 exit and falsifiers

F0 is complete when: the amended architecture texts land at this
repository (this transaction), this inventory exists and is accurate at
the exact head, the selections above are recorded, and the matrices
declare (not claim) their coverage. The amendment is NOT complete
because a shared library links — PR6 doc 02 §5's falsifiers bind the
later gates: external installed-package consumer, loader/symbol-export
inventory, static/shared measurements, module ownership/unload tests,
no duplicate process service, same-revision Runtime/Devkit receipts,
and (for an independent ABI claim) a real module consumer with
compatibility/refusal tests. A failing platform/provider stays visibly
uncovered.

## 9. Provenance

- qiven-docs PR6 (accepted 2026-09-29; `accepted/2026-09-29/` at docs
  main `2dc2f0f`; review revisions F1-F4 at `04079d5`; merge
  `97f838c`; signed record issuecomment-5877399900).
- qiven-context ADR-0059 (accepted 2026-09-29, owner popup; context
  main `d5a8f2b`).
- Baseline facts verified in-tree at qiven-foundation `5e3e018`
  (CMakeLists, header/TU census, crt_failure.cpp, capability-surface);
  workspace lock receipt `workspace.lock.json` at control `9e03162`.
- F0 transaction authored by the v47 session (designation
  jason-extended-cognition, long-running mode; review loop
  qiven-fresh-review K=3/SL=3 before owner adjudication).
