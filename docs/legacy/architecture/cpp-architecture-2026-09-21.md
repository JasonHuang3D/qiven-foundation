# Qiven C++ Architecture — Layer Contracts

Status: **accepted design** (owner-directed 2026-09-21; ADR-0039 boundary
execution). Normative for every `qiven-*` C++ repository. ADR-0039 fixes
the three-plane repository topology; this document fixes what each layer
OWES its consumers and what it may demand of its dependencies, plus the
cross-cutting representation, failure, concurrency, and platform laws.
The Runtime-specific type and port design lives in
`qiven-runtime/docs/architecture/runtime-cpp-design.md` and conforms to
this document.

Frozen semantic anchors: `qiven-context-draft` @ `4cbc995` (v4 cognition
semantics), ADR-0038 (Runtime Cognitive Control), ADR-0039 (topology).

---

## 1. Layer contract template

Every layer states, in its `docs/architecture/<layer>.md`:

```text
mission          the one semantic responsibility it owns
public surface   what it exports and the stability promise
dependency law   the exact lower layers it may consume
representation   which of the five boundaries (ADR-0009) it crosses
failure model    typed failure vocabulary; exceptions forbidden across ports
concurrency      threading contract for every exported facility
allocation       allocator discipline for hot paths
platform         cross-platform by default; Windows-only exceptions named
testing law      which semantic failure modes tests must target
```

A layer that cannot state a row does not ship that row's convenience.

## 2. The dependency spine

```text
L0  OS / C++ standard library / pinned mature third-party
L1  qiven-foundation            (base; exists)
L2  qiven-math                  (sibling; exists)
    qiven-units                 (sibling; future)
    qiven-process               (sibling; future, ADR-0039 renaming of
                                 the archived ADR-0025 layer)
    qiven-networking            (sibling; future)
    qiven-context-draft         (sibling; frozen v4 semantic definition
                                 library — Plane B, but consumed as an L2
                                 library by the Runtime)
L3  qiven-geometry              (composite: math + units; future)
    qiven-scene, qiven-compute  (composites; future)
P   product infrastructure      qiven-dcr-win (Win transport), qiven-host
                                (Win execution authority)
B   cognition plane             qiven-runtime (Cognitive Control Runtime;
                                consumes foundation + the frozen draft
                                library; composes with qiven-host through
                                an execution-authority port, never by
                                direct host API coupling)
U   UI / workflow shells        always after headless cores (ADR-0019)
```

Laws:

- Dependencies point strictly DOWNWARD along semantic ownership
  (ADR-0017/0024). Siblings never depend on siblings; a composite (L3)
  may consume several L2s. No layer consumes upward, and no product
  layer is consumed by a shared layer.
- `qiven-context-draft` is consumed by `qiven-runtime` as a **frozen
  semantic library**: the draft's value types ARE the v4 semantics. The
  Runtime never copies or paraphrases them; it pins an exact frozen
  revision (today `4cbc995`) and upgrades by explicit re-pin with
  re-validation. The draft gains no runtime-process responsibilities.
- Product layers (P/U) may consume B only through published,
  cognition-free contracts; the Runtime is not a product utility layer.

## 3. Cross-cutting laws

### 3.1 Representation (ADR-0009 restated as layer duty)

In-memory layout is process-local unless an explicit boundary contract
exists. The five boundaries — module ABI, IPC, wire, persistent,
shared-memory — each get their own transferable form with fixed widths,
endianness decisions, validation, and versioning. Default linkage is
STATIC. A dynamic-library/plugin boundary is created only by an accepted
concrete requirement and is a C ABI (`extern "C"`, opaque handles) —
never native C++ layout, STL types, or exceptions across the edge. C++20
modules are not used (ADR-0039).

### 3.2 Failure model

Shared native layers do not throw across their public surface. Failure
channels are typed:

- absence/presence → `std::optional` / `std::expected`-shaped results;
- recoverable domain failures → the common **result vocabulary**
  (typed outcome enum + reason + detail; `[[nodiscard]]`; no heap on the
  failure path). Its landing home is qiven-foundation; the Runtime's
  K1 work is the first consumer that forces it (OBL-20260913T181224Z-
  9E27A4 / OBL-20260920T205211Z-D3F7B2 trigger). Interface sketch is in
  the Runtime design §12; foundation owns the implementation. Landed as
  `qiven::Result<T, Reason>` (`qiven/result.hpp`) in the RCA-0 batch.
- programming errors / invariant violations → assertions, never
  recoverable channels.

### 3.3 Concurrency

Every exported facility states its threading contract. Shared layers
provide primitives (queues, handles, fences — as needed, demand-driven),
not frameworks. A layer with a single-threaded core says so and keeps
its ingress boundaries explicit (posted-message ingestion), which is how
the Runtime control core stays deterministic (Runtime design §9).

### 3.4 Allocation

Foundation's allocator protocol governs hot paths in shared layers.
Ordinary configuration-and-control code may use standard containers;
anything on a per-action or per-byte hot path states its allocator
discipline in its layer document.

### 3.5 Platform

Cross-platform (Windows / Linux / macOS) is the default for every layer
below products. Windows-only layers (`qiven-dcr-win`, `qiven-host`,
`qiven-toolchain-win`) are named exceptions; they still keep failure,
naming, and representation laws. Platform headers never leak through
public contracts.

### 3.6 Identity and digests

Non-cryptographic identity (correlation ids, fingerprint hashing of
runtime strings) uses `qiven::fnv1a64`. Integrity-grade digests
(decision binding, artifact identity) use SHA-256
(`qiven/hashing_sha256.hpp`, landed in the Runtime's RCA-0 foundation
batch — byte-level content integrity is an intrinsically foundational
primitive, same reasoning as the fnv1a64 distillation, bucket B).

### 3.7 Build, naming, testing

CMake conventions (presets-only entry, toolchain pinning, explicit
file lists, FILE_SET HEADERS + VS source_group, `qiven-<name>` raw
targets with `qiven::<snake>` aliases) are canonical in
`qiven-devkit/docs/conventions/` and are not restated here. Every
layer's tests target semantic failure modes of ITS contract; tests are
named after the obligation or incident that motivates them (scars
compile).

## 4. Per-layer contract summaries

### qiven-foundation (L1)

Mission: low-level vocabulary and primitives (types, contracts, checked
arithmetic/conversions, byte cursor/writer, endian, hashing, memory
protocol, platform boundaries). No domain semantics, no product
knowledge, no process policy. Failure-lean. Existing law:
`docs/architecture/foundation.md` remains normative; the result
vocabulary and SHA-256 join it by the obligations above.

### qiven-math (L2 sibling)

Small, deterministic, external-semantics-free spatial vocabulary
(ADR-0013/0014/0015). Compact scalar-aligned values; no SIMD ABI in
canonical types; coordinate conventions and tolerances live at higher
boundaries.

### qiven-context-draft (L2, frozen semantic library)

Pure value-tree cognition semantics per the frozen v4 baseline:
Snapshot, InvocationPolicy, ActionIntent, requirement derivation and
readiness (`ready_for_judgment`/`ready_for_execution`), PreparationPacket,
serialization (golden-vector-pinned). Consumption law: pin exact
revision; treat as data + pure functions; no runtime I/O
responsibilities are added to the draft.

### qiven-process (future L2 sibling; scope unchanged from archived ADR-0025)

Process generations, owned child trees, concurrent drainage, bounded
stop, truthful exit results; Windows backend via Job Objects. Created
when DCR Batch 001 resumes or when a real consumer (for example the
Runtime's real MechanicalCheck resolver) needs subprocess execution.

### qiven-units / qiven-geometry / qiven-scene / qiven-compute / qiven-networking (future)

Created per ADR-0018 triggers (ADR-0039 §1). Units never depends on
math; geometry consumes both; each ships its layer document before its
first implementation.

### qiven-host (P, Windows)

Machine-local execution authority (ADR-0026): lease, fencing, admission,
quarantine, reconciliation journal. Consumed by the Runtime only through
the execution-authority port boundary (Runtime design §8.5) — the
Runtime never links host APIs directly.

### qiven-dcr-win (P, Windows)

Native DCR supervisor. Consumes foundation now; consumes qiven-process
when that layer exists. Owns DCR policy only.

### qiven-runtime (B — the cognition runtime)

Implements ADR-0038/Component ADL against the frozen draft semantics.
Dependency closure: foundation + draft + (through ports) host and
mechanisms. Its detailed C++ design is the companion document.

### UI / workflow shells (U)

Created per product, only after headless cores are proven (ADR-0019).

## 5. Sequence discipline (anti-apprenticeship gates)

1. A layer's architecture document (this template) is accepted before
   its first implementation commit.
2. First implementations meet philosophy §9 quality bar (ownership,
   lifetimes, recoverable failure channels, bounded resources,
   concurrency contracts, platform boundaries).
3. Repositories are created only by ADR-0018 criteria.

The Runtime RCA program (component ADL §89) is the first execution of
this discipline under the new topology.

## 6. Provenance

Owner direction 2026-09-21 (one-turn unified design with the Runtime
C++ detail design). Implements ADR-0039; inherits ADR-0007/0008/0009/
0013-0020/0024 lineage; cross-checked against the frozen v4 draft
surface (`include/qiven/context/{cognition,runtime,persistence}.hpp`
@ `4cbc995`).
