# ByteBuilder Admission Record

Status: ADMITTED (landed with the RR-0 implementation batch, 2026-09-28).
Decision authority: ADR-0024 (semantic ownership over consumer count);
the admission decision itself is
`qiven-runtime docs/design/rr0-byte-representation.md` §2 (v23, 2026-09-24),
which this record implements. This file is the Foundation-side contract
and landing evidence for the primitive.

## What was admitted

`include/qiven/byte_builder.hpp` — an owning, growing, bounded byte
accumulator with scalar-put composition:

- storage semantics only: append bytes, append fixed-width LE/BE scalars
  (u16/u32/u64), extract the accumulated span — no field order, no
  framing, no prefix-width policy, no versioning (each format owner keeps
  those, per ADR-0009 representation boundaries);
- explicit failure channel (`append_status`: `ok` / `capacity_exceeded` /
  `allocation_failed`), sticky: the first failed append latches, later
  appends are no-ops, `ok()`/`status()` report, `reset()` clears and
  retains capacity — no exceptions, no hidden failure;
- bounded by an explicit `max_capacity`; growth is lazy (nothing allocated
  before the first append) and geometric (double, clamp to max); capacity
  is observable and monotonic non-decreasing between resets;
- ownership via the Foundation allocator model
  (`memory::AllocatorRef` + `memory::OwnedAllocation`); move-only;
  scalar puts compose `endian.hpp` codecs (no second encoder).

## First real consumers

The RR-0 mechanical consolidation in qiven-runtime replaces six local
put/get mechanic definitions (framing, decision, journal, state store,
adapter manifest, identity; plus the scope preimage lambda) with this
primitive plus `endian.hpp`/`ByteCursor`, under byte-compat golden
fixtures. Runtime formats keep their existing representation decisions
(rr0 doc §3); only the mechanics are replaced.

## Rejection boundary

ByteWriter (fixed-capacity, non-owning) remains the tool for stack-buffer
cases; ByteBuilder is the owning/growing complement, not a replacement.
Framing, message structure, field order, length-prefix width,
compatibility and versioning were explicitly NOT admitted and stay with
each Runtime format owner.

## Testing law

`tests/byte_builder.cpp` (registered in `tests/CMakeLists.txt`; header
self-containment in `tests/headers/byte_builder.cpp`): scalar puts
against `endian.hpp` golden vectors, growth data preservation, the
geometric growth law (double, clamp), bounds latching and reset reuse,
the allocation-failure channel via a failing slot allocator, and move
semantics including self-move-assignment.

## Landing note

The rr0 decision document names the admission record at
`docs/design/byte-builder-admission.md`; Foundation's live layout
(`docs/architecture/` only — README §Documentation; devkit
`agent-entry.md`) places it here instead. Same document, corrected path;
recorded as an honest path delta in the RR-0 batch.
