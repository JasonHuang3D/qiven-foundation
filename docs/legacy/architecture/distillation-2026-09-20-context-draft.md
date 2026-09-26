# Distillation Review — Engineering Content from qiven-context-draft

Status: proposal (docs first, development follows per the phased plan below).
Provenance: the 2026-09-19/20 unattended v3 phase 1–3 + 2C run on
`JasonHuang3D/qiven-context-draft` (commits through `f078028`/`9d654ed`), the
owner's 2026-09-20 review of that run, and the P-42 incident. This review was
ordered by the project owner with one constraint: strictly follow and extend
the Foundation architecture defined in `foundation.md`.

## 1. Method

Every engineering artifact the draft produced was classified into exactly one
of four buckets:

- **A — already in Foundation**: the draft reimplemented it; the draft migrates
  and the local copy is deleted (philosophy §1: never keep a weaker local copy
  of a capability that exists in a lower layer).
- **B — new Foundation candidate**: low-level, domain-free, broadly useful
  below product layers, implementable without depending on them.
- **C — stays in the draft**: domain semantics of project cognition.
- **D — process knowledge**: recorded in canonical memory and the draft's pit
  map, not as code.

## 2. Inventory and classification

| Draft artifact | Bucket | Rationale |
| --- | --- | --- |
| `Reader` (bounds-checked TLV reads: u8/u32/u64/i64/str/enum/count-capped) | A (+ gap) | `qiven::ByteCursor` + `qiven::endian` already provide span-based bounded reads; the draft duplicated them. Gaps found: count-capped reads and enum-range reads are draft compositions, not Foundation gaps. |
| `putU8/putU32/putU64/putI64/putStr` writers | A | `qiven::ByteWriter::reserve` + `qiven::endian::encode_le_*` cover the same ground. |
| `DeserializeError` taxonomy (Truncated/BadVersion/BadEnum/ResourceAbuse/DigestMismatch) | C | Domain-typed failure vocabulary of the cognition wire format. Foundation stays failure-lean (`std::optional`/Status); the domain taxonomy lives above it. |
| `fnv1a64(text, bytes)` + `hex64` | B | Deterministic, allocation-free, byte-level non-cryptographic checksum — a textbook low-level primitive. Consumed by the draft today; K1 golden vectors will consume it tomorrow. |
| `DeserializeResult{ok, snapshot, error}` / `StoreReceipt{Committed, CompareFailed, OutcomeUnknown}` | B (deferred) | Two concrete consumers of a common result vocabulary now exist — this is the evidence the deferred obligation OBL-20260913T181224Z-9E27A4 asked for. Proposed as F2 below, with a defined trigger, not implemented speculatively. |
| `PolicyTable`, `Verdict`, `ExecutionGrant`, `Conflict`, `ViewSpec`, `ContextBundle`, quarantine machine | C | Cognition-domain semantics. They must NOT sink into Foundation (architecture §2: not a home for domain logic). |
| Session economics, `ReviewRecord`, `Qualification` | C | Process/domain layer. |
| "Scars compile" methodology, adversarial semantic tests, overnight mode | D | Process knowledge; already recorded in the canonical repository and the draft's docs. |

## 3. Finding A (mandatory): the draft duplicated Foundation byte-IO

The draft's `Reader` and `put*` helpers reimplemented `qiven::ByteCursor`,
`qiven::ByteWriter` and `qiven::endian` with a weaker failure model. Root
cause: the draft's preflight (its AGENTS.md §2) surveys the draft's own
contracts but did not survey Foundation's public surface before writing
byte-level code.

**Action (draft side, after F1 lands):** migrate the READ path onto
`qiven::ByteCursor` + `qiven::endian` and delete the duplicated bounds
machinery; keep the domain-typed `DeserializeError` taxonomy as a composition
over Foundation's optional-based reads. Acceptance: the serialization golden
vectors are byte-IDENTICAL before and after (the wire format does not change),
and the full pit suite stays green in Debug and Release.

**Migration outcome (2026-09-20, same day):** the read path migrated; the
golden digest is byte-identical (`snap-ae811bad8dada212`); the full pit suite
is green. The WRITE path keeps the draft's small growing-buffer put* helpers
with a recorded rationale: `qiven::ByteWriter` targets FIXED-capacity spans,
while the draft serializer writes an unbounded growing buffer - composing the
writer strategy is a serialization-strategy decision the kernel's K1 batch
owns. `fnv1a64`/hex now come from Foundation hashing (F1).

**Action (process, draft side):** record pit P-49 "draft reimplemented a
lower-layer capability" — the preflight must include a Foundation-surface
survey step.

## 4. Candidate F1: non-cryptographic 64-bit hashing and hex encoding

**What.** `qiven::hashing.hpp`:

```cpp
namespace qiven {
// FNV-1a 64: deterministic, allocation-free, NOT cryptographic.
[[nodiscard]] constexpr u64 fnv1a64(const std::byte* data, usize size, u64 seed = 1469598103934665603ULL) noexcept;
[[nodiscard]] inline std::string to_hex_u64(u64 value); // lowercase, fixed width
}
```

**Why Foundation.** Byte-level content identity is consumed below domain
layers (the draft's snapshot/delta digests, future K1 golden vectors, devkit
tooling). It is allocation-free, has predictable cost, and carries no
platform surface.

**Explicit boundary.** FNV-1a is NOT cryptographic. Content IDs derived from
it are integrity and identity tokens for draft/tooling use; the production
engine's SHA-256 requirement (ADR-0033) stays at the kernel layer, backed by
a dedicated primitive when it exists.

**Fit.** C++20, `constexpr` where the span allows, noexcept, no exceptions, no
allocation (hex writes into a caller buffer or returns `std::string` only at
the convenience wrapper).

**Acceptance.** Foundation unit tests: known FNV-1a vectors, seed variance,
hex width/charset; then the draft consumes it and its golden vectors are
byte-identical.

## 5. Candidate F2 (deferred): the common result/status vocabulary

OBL-20260913T181224Z-9E27A4 deferred a shared result/status abstraction until
Foundation APIs genuinely need structured recoverable status. That evidence
now exists: the draft produced two shapes (`DeserializeResult`,
`StoreReceipt::Kind`) with the same anatomy (outcome + typed reason + detail).

**Decision now:** record the evidence and the shape constraint (typed outcome
enum, no exceptions, no heap in the failure path, `[[nodiscard]]`), and define
the trigger: land the vocabulary when the first Foundation public API needs a
recoverable failure channel, or at K1 — whichever comes first. Do not
implement it speculatively inside this distillation.

## 6. Rejected distillations

Cognition semantics (policy tables, verdicts, grants, conflicts, view specs,
bundles, session economics) stay in the draft: they are the domain the draft
exists to specify. Sinking any of them would convert Foundation into the
"miscellaneous utilities" home its architecture forbids.

## 7. Sequencing

1. This document (accepted as part of the overnight batch review).
2. F1 lands in Foundation (header + tests + gate).
3. The draft migrates onto Foundation byte-IO and `fnv1a64`; golden vectors
   prove byte identity; pit P-49 recorded.
4. F2 activates per its trigger.

## 8. Provenance

Authored 2026-09-20 by jason-extended-cognition (GLM-5.3-Flash, reasoning max,
disclosed) during an owner-authorized overnight window, from the owner's
direction to distill draft engineering into Foundation strictly within
Foundation's architecture. Local commits only; batch H2 at window close
(operating contract, "Overnight unattended mode").
