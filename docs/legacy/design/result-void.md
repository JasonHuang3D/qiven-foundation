# Result<void> specialization (batch design)

Status: **small batch design** (design-first standard; E-exceptions do not
apply — this adds a public type shape). Scaled-down sections; N/A marks
what a ~100-line specialization genuinely does not have.

## 1. Basis

`qiven::Result<T, Reason>` (include/qiven/result.hpp) is the canonical
recoverable-failure vocabulary. The primary template cannot express a
successful `Result<void>`: its success channel is the implicit
`Result(T value)` constructor, and its private default constructor exists
only for `fail()`. `qiven-runtime`'s MVP-0-landed
`IRuntimeJournalPort::append` declares `qiven::Result<void>`; MVP-1 is the
first consumer to instantiate it, and a compile probe (2026-09-23,
qiven-runtime session) confirmed the instantiation is ill-formed today.
Per the engineering philosophy (§1 semantic ownership, §2 consumer count
is evidence not prerequisite), the fix belongs here.

## 2. Module map

`include/qiven/result.hpp` gains a `Result<void, Reason>` partial
specialization after the primary template. No other file changes except
`tests/result.cpp` (verification) and this document.

## 3. Contracts

`template <typename Reason> class [[nodiscard]] Result<void, Reason>`:

- `static Result ok() noexcept` — the only success construction;
- `static Result fail(Reason)` — unchanged failure semantics;
- `is_ok()`, `explicit operator bool`, `reason()` (&/&&, const) — identical
  contracts to the primary template;
- no `value()` accessors (there is no value);
- copy/move/destructor follow the primary template's placement-new storage
  discipline; the union holds only the reason alternative and a `char`
  placeholder;
- misuse (`reason()` on a success) trips `QIVEN_ASSERT`, as before.

No primary-template line changes; non-void instantiations are untouched.

## 4. Concurrency and lifecycle

N/A — a value type, no lifecycle beyond its own storage.

## 5. Failure modes

N/A — the type IS the failure channel. One deliberate decision: `ok()` is
`noexcept` unconditionally (no Reason construction happens on success).

## 6. Test spine

`tests/result.cpp` adds a void-specialization block: ok() success, fail()
reason round-trip (code survives), copy/move of a failure, move-assign to
ok(), and a typed-enum reason instantiation. Existing blocks prove the
primary template is unchanged.

## 7. Dependencies

None (header-only, standard library only). No deployment impact.

## 8. Deferrals

None. The specialization is complete; no follow-up mechanism is owed.

## 9. Review record

Self-review 2026-09-23 (v18 session, designation jason-extended-cognition),
performed before implementation:

1. Considered instead constraining the primary template's value
   constructor and adding a public default constructor — rejected: a
   public default would make "forgot to set the reason" silently express
   success for non-void instantiations too. The partial specialization
   isolates the void shape.
2. Considered `std::optional<Reason>` as the storage — rejected: keeps the
   placement-new union shape of the primary template for symmetry and to
   avoid an optional-wrapping-allocation question for exotic reasons.
3. Checked `Result<void>` against [[nodiscard]] discipline: an ignored
   success is harmless, an ignored failure is the lost-failure defect the
   attribute already flags — unchanged.
4. `ok()` naming over `success()`/`empty()`: matches `Error::ok_value()`
   vocabulary already in the codebase.
