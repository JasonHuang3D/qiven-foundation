# Qiven Foundation Implementation Standard

This document defines implementation quality expected from `jason-worker`.

Read it together with the root `AGENTS.md` and `docs/architecture/foundation.md`.

## 1. Engineering objective

Implementation should optimize for correctness and durable simplicity, not for maximum code production.

Foundation code should make important properties visible:

- who owns a resource;
- how long it lives;
- how failure is represented;
- what work an operation performs;
- whether allocation occurs;
- whether synchronization occurs;
- which platform boundary is crossed;
- which invariants callers must satisfy.

Do not hide meaningful cost or failure behind convenience APIs.

## 2. Scope discipline

The CTO feature specification defines the permitted change surface.

Before editing, identify:

- required new behavior;
- permitted public contract changes;
- expected files or modules;
- explicit out-of-scope work;
- stop conditions.

While implementing, distinguish:

- work required for correctness;
- work required for tests/build registration;
- unrelated improvement opportunities.

Only the first two belong in the feature branch.

If an unrelated defect blocks the feature, report it. Do not silently broaden scope.

## 3. Minimal sufficient abstraction

Prefer the smallest abstraction that fully expresses the required semantics.

Do not build generalized infrastructure for possible future needs.

Avoid speculative:

- registries;
- plugin systems;
- generic factories;
- reflection systems;
- serialization layers;
- custom containers;
- error frameworks;
- portability wrappers;
- traits frameworks;
- allocator hierarchies.

A future extension point should exist only when the current architecture genuinely requires it.

## 4. API quality

Public APIs should be:

- small;
- explicit;
- difficult to misuse accidentally;
- clear about ownership and lifetime;
- clear about failure;
- predictable in cost;
- portable across supported targets unless intentionally platform-specific.

Do not add convenience overloads without a concrete need.

Avoid Boolean parameters when they obscure meaning.

Avoid public implementation details.

If naming or API shape materially changes the contract and the feature spec did not decide it, request CTO review.

## 5. Ownership and lifetime

For every pointer, reference, span, allocator handle, or resource-owning object, know whether it is:

- owning;
- borrowing;
- nullable;
- empty-but-valid;
- lifetime-bound to another object;
- required to outlive another object.

Move-only ownership types require deliberate handling of:

- moved-from state;
- destination cleanup before move assignment;
- allocator/backend provenance;
- self-move when relevant;
- destruction after move;
- zero-size/empty success.

Do not rely on implicit conventions that are not expressed by type or documented contract.

## 6. Raw storage versus objects

Raw allocation and C++ object lifetime are separate responsibilities.

An allocator or raw storage owner must not silently construct or destroy typed objects unless its contract explicitly owns object
lifetime.

When typed construction is later required, design that feature separately rather than smuggling object lifetime into a raw
storage abstraction.

## 7. Integer and size safety

Code that handles byte sizes, offsets, counts, alignments, ranges, or address arithmetic must reason about:

- overflow;
- underflow;
- narrowing;
- signed/unsigned conversion;
- invalid alignment;
- addition/multiplication overflow;
- pointer-sized versus fixed-width integers.

Prefer existing checked primitives in the repository where appropriate.

Do not use a cast solely to silence a compiler warning.

If arithmetic failure semantics are not defined, do not invent them locally.

## 8. Error and contract handling

Use repository contract mechanisms consistently.

Do not conflate:

- caller programming errors;
- internal invariant violations;
- recoverable runtime failures;
- environmental failures.

Assertions are not substitutes for recoverable error channels.

Do not add exceptions to APIs whose semantics are non-throwing.

Do not convert an error to a default value merely to simplify callers.

## 9. `noexcept`

Apply `noexcept` when it is a real semantic guarantee.

Do not add it decoratively.

For resource cleanup and move operations, reason about whether the entire implementation can actually uphold the guarantee.

Do not call potentially throwing operations from a `noexcept` function unless termination is intentionally part of the contract.

## 10. Standard library use

Use the standard library where it provides the required semantics without unacceptable cost or dependency impact.

Do not reimplement standard functionality for style.

Conversely, do not choose a high-level STL abstraction if it introduces hidden allocation, ownership, exception, RTTI, or
runtime cost contrary to the feature's contract.

## 11. Templates and generic code

Generic programming must earn its complexity.

Before adding a template/concept/type-erased abstraction, verify that:

- more than one real use case exists or the public contract intrinsically requires genericity;
- error messages and compile cost remain reasonable;
- semantics remain understandable;
- the abstraction does not expose implementation machinery unnecessarily.

A single feature should not create a framework for hypothetical future types.

## 12. Platform boundaries

Portable code should remain platform-neutral.

Platform-specific branches should be narrow and explicit.

Prefer the least invasive native dependency. For example, do not include `windows.h` when a narrower runtime or CRT facility is
sufficient.

When `windows.h` is unavoidable:

- keep it out of public headers;
- isolate it to Windows implementation code;
- use the established defensive macro policy from the repository rather than inventing local variants;
- prevent common macro pollution;
- keep include order deliberate.

Do not assume Linux/macOS equivalents without verifying their documented semantics.

## 13. Public headers

Public headers must:

- include what they directly need;
- avoid accidental transitive dependencies;
- avoid platform header leakage;
- avoid unnecessary implementation detail;
- remain compatible with the repository header-check target.

If a new public header is added, register it consistently in CMake and header checks.

## 14. Includes

Keep include lists minimal but explicit.

Do not remove an include merely because another header currently provides it transitively.

Do not add broad umbrella headers for convenience.

Respect repository formatting/include ordering.

## 15. Comments

Write comments for reasoning that is not obvious from code:

- why a platform workaround exists;
- why an invariant is safe;
- why an apparently simpler approach is incorrect;
- subtle ownership/lifetime constraints;
- non-obvious performance or ABI trade-offs.

Do not write comments that restate syntax.

Avoid long tutorial comments inside implementation files.

If a rule requires a paragraph to understand, architecture or engineering documentation may be a better location.

## 16. Naming

Follow existing repository naming conventions unless the feature specification deliberately changes them.

Names should communicate semantics rather than implementation history.

Avoid abbreviations that save little space and lose meaning.

Do not rename unrelated existing APIs for consistency during another feature.

## 17. Formatting

Repository `.clang-format` is authoritative.

For source changes use:

```cmd
tools\format.cmd
tools\format-check.cmd
```

After formatting, inspect the actual diff. Formatting success does not prove that unrelated files were not changed.

Never modify `.clang-format` as a side effect of implementing another feature.

## 18. Build configuration

CMake is the build-system source of truth.

Do not hand-maintain divergent Visual Studio configuration when CMake should express the setting.

Use existing targets, presets, helper functions, and folder conventions.

Do not restructure CMake for aesthetic reasons while implementing an unrelated feature.

## 19. Performance

Foundation is performance-sensitive, but premature optimization is not permission for opaque code.

When performance materially affects design:

- identify the cost being controlled;
- prefer predictable complexity;
- avoid hidden allocation;
- avoid unnecessary virtual dispatch;
- avoid unnecessary synchronization;
- keep hot-path work explicit.

Do not add micro-optimizations without evidence when they harm clarity or portability.

## 20. Concurrency

Do not make a type thread-safe accidentally or implicitly.

Thread-safety is part of the contract.

If the feature specification does not require synchronization, do not add locks “just in case.”

If correctness requires a new concurrency contract, stop for CTO review.

## 21. Diff quality

Before committing, inspect the full diff and remove:

- temporary diagnostics;
- debug output;
- commented-out experiments;
- unrelated format churn;
- editor-generated changes;
- accidental file renames;
- local paths;
- generated build products;
- speculative TODO implementations.

A feature diff should tell one coherent story.

## 22. Done means understood

Do not consider a feature complete merely because it compiles.

Before handoff, be able to explain:

- the invariant;
- ownership/lifetime behavior;
- failure behavior;
- meaningful edge cases;
- what was tested;
- what Windows-local testing could not prove;
- why the implementation stays inside the architecture.
