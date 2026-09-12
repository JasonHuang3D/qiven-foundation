# CTO Feature Specification Contract

This document defines how `jason-brother` should assign implementation work to `jason-worker`.

A strong feature specification communicates architectural intent and semantic boundaries while leaving ordinary local
implementation choices to the worker.

## 1. Why the specification exists

The purpose is not to micromanage code.

The purpose is to prevent three failure modes:

1. the worker guesses an architectural contract;
2. the worker expands scope because an adjacent improvement looks useful;
3. the worker cannot tell whether an ambiguity is local or requires CTO review.

A good feature specification says what must be true when the feature is complete, what must not be changed, and when the worker
must stop.

## 2. Required batch header

A Work request should start with:

```text
WORK BATCH
Base branch:
Base commit: <optional when branch state is authoritative>
Maximum features:
Usage policy:
Remote policy: local only; no push/merge/PR
```

Then provide the ordered queue:

```text
AUTHORIZED FEATURE QUEUE
1. <feature A>
2. <feature B>
3. <feature C>

STOP AFTER <feature C>.
```

If a feature is an architectural barrier, say so explicitly.

## 3. Required feature fields

Each feature should define, when relevant:

```text
Feature:
Branch:
Base:
Intent:
Required semantics:
Public API:
Ownership/lifetime:
Failure semantics:
Performance/cost constraints:
Platform constraints:
Expected integration:
Required tests:
Out of scope:
Stop conditions:
Documentation:
Notes:
```

Not every field needs text. Use `N/A` when deliberately irrelevant rather than leaving a contract-critical field accidentally
ambiguous.

## 4. Feature name and branch

Example:

```text
Feature:
OwnedAllocation

Branch:
jason-worker/owned-allocation

Base:
main
```

For stacked work:

```text
Feature:
ByteBuffer

Branch:
jason-worker/byte-buffer

Base:
jason-worker/owned-allocation
```

This dependency must be intentional.

## 5. Intent

Intent should explain why the primitive exists in one short paragraph.

Bad:

```text
Add a buffer class.
```

Better:

```text
Provide a move-only owner for contiguous raw byte storage obtained through AllocatorRef so higher Foundation primitives can
hold allocator-backed bytes without taking responsibility for manual deallocation.
```

Intent helps the worker make sensible private implementation decisions without inventing architecture.

## 6. Required semantics

State externally meaningful behavior.

Example:

```text
Required semantics:
- move-only;
- allocator provenance retained;
- zero-size allocation is valid;
- non-zero allocation failure is explicit;
- destruction returns storage through the originating allocator;
- moved-from object is valid and empty;
- no object construction/destruction semantics.
```

Do not specify trivial line-by-line implementation unless the implementation technique itself is part of the contract.

## 7. Public API

When the public API is architecturally important, specify it or its allowed shape.

If the worker may choose the exact private implementation but not public naming, say so.

If public API shape is deliberately open for worker proposal, mark the feature as requiring CTO review before implementation
of that public contract rather than encouraging the worker to guess.

## 8. Ownership and lifetime

For ownership-related features define:

- what is owned;
- what is borrowed;
- what must outlive what;
- moved-from state;
- cleanup responsibility;
- allocator/backend provenance;
- raw storage versus typed object lifetime.

If these are not yet decided, the feature is not ready for autonomous Work implementation.

## 9. Failure semantics

Define meaningful failure channels.

Examples:

```text
Failure semantics:
- allocation failure returns std::nullopt;
- zero-size success remains distinguishable from allocation failure.
```

or:

```text
Failure semantics:
- invalid caller alignment is a contract violation;
- resource exhaustion is recoverable and returns nullptr.
```

Do not leave the worker to choose between assert, optional, error code, exception, or termination when that choice changes the
contract.

## 10. Performance and cost

State constraints only when meaningful.

Examples:

```text
- no hidden allocation;
- O(1) move;
- no synchronization;
- no virtual dispatch;
```

Avoid demanding “high performance” without defining the relevant cost.

## 11. Platform constraints

State special cross-platform requirements when a feature touches native facilities.

Example:

```text
Platform constraints:
- public API remains platform-neutral;
- Windows implementation must not expose windows.h;
- Linux/macOS behavior must preserve identical allocation semantics.
```

A platform-sensitive feature is a candidate architectural barrier.

## 12. Expected integration

List likely required registration when useful:

```text
Expected integration:
- public header registered in top-level CMake FILE_SET;
- implementation source registered privately;
- header self-check added;
- dedicated test target added.
```

This prevents a correct `.cpp` implementation from being incomplete as a repository feature.

## 13. Required tests

Specify semantic risks, not only filenames.

Example:

```text
Required tests:
- success and destruction;
- failure;
- zero-size;
- move construction;
- move assignment with distinct allocator backends;
- self-move;
- public header self-check.
```

The worker may add another directly relevant test when it exposes a meaningful risk, but should not inflate test count
mechanically.

## 14. Out of scope

This is one of the most important sections.

Example:

```text
Out of scope:
- resizing;
- typed construction;
- small-buffer optimization;
- PMR integration;
- serialization;
- changing AllocatorRef.
```

Out-of-scope work must remain unchanged even if it appears useful.

## 15. Stop conditions

Explicitly list conditions that require escalation.

Example:

```text
Stop conditions:
- AllocatorRef must change;
- new public failure type appears necessary;
- platform behavior cannot be made equivalent;
- third-party dependency appears necessary;
- CMake architecture must change.
```

Repository-wide stop conditions from `AGENTS.md` always apply even if not repeated.

## 16. Documentation

Say whether architecture documentation should change.

Do not force documentation churn for trivial implementation details.

When a public architectural contract is added, it is often appropriate to update `docs/architecture/foundation.md`.

## 17. Architectural barrier marker

Use:

```text
Architectural barrier:
YES
```

when dependent features should not be stacked until GitHub CI and CTO review pass.

Use:

```text
Architectural barrier:
NO
```

only when the CTO has consciously decided local stacking is safe.

If omitted for an obviously cross-cutting feature, jason-worker should conservatively stop after completing it.

## 18. Example complete feature

```text
Feature:
OwnedAllocation

Branch:
jason-worker/owned-allocation

Base:
main

Intent:
Provide a move-only owner of raw allocator storage.

Required semantics:
- retain AllocatorRef, pointer, size, and alignment;
- non-zero allocation failure is explicit;
- zero-size allocation is a valid successful empty state;
- move transfers ownership and empties the source;
- destruction returns storage through the originating allocator;
- raw storage only; no typed object lifetime.

Public API:
Use the CTO-approved OwnedAllocation API.

Ownership/lifetime:
Allocator backend must outlive OwnedAllocation.

Failure semantics:
Non-zero allocation failure returns an empty optional.

Performance/cost constraints:
- no hidden allocation beyond the requested allocation;
- O(1) move;
- no synchronization.

Platform constraints:
Platform-neutral public API.

Expected integration:
- public header;
- source file;
- CMake registration;
- header self-check;
- dedicated tests;
- architecture note.

Required tests:
- success/destruction;
- failure;
- zero-size;
- move construction;
- move assignment across two allocator backends;
- self-move.

Out of scope:
- resizing;
- typed objects;
- changing AllocatorRef;
- PMR.

Stop conditions:
- AllocatorRef contract needs modification;
- ownership cannot be expressed without changing public allocator semantics.

Architectural barrier:
NO
```

## 19. Specification quality check for the CTO

Before handing a batch to Work, ask:

- Are feature dependencies explicit?
- Can the worker identify the correct branch base?
- Are public semantics decided?
- Are ownership and failure semantics decided?
- Is out-of-scope work explicit?
- Are high-risk test cases named?
- Are architectural barriers identified?
- Is the queue small enough to review later?
- Would a senior engineer understand when to stop without asking trivial questions?

If not, improve the specification before spending Work budget.
