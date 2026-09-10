# Qiven Foundation Architecture

This document defines the initial architectural contract of Qiven Foundation. It is deliberately stricter than the current
amount of code: the point is to constrain future growth before convenience turns into accidental architecture.

## 1. Mission

Qiven Foundation provides low-level, reusable C++ building blocks for the Qiven ecosystem.

A component belongs here only when it is broadly useful below product or domain layers and can be implemented without
depending on those layers.

Foundation should optimize for:

- predictable cost;
- explicit ownership;
- portability;
- testability;
- low dependency weight;
- clear failure behavior;
- long-term maintainability.

## 2. Non-goals

Foundation is not:

- a miscellaneous `utils` repository;
- a home for product or domain logic;
- a wrapper around every operating-system or standard-library facility;
- a place to hide expensive work behind convenient APIs;
- a compatibility layer for preserving bad historical abstractions.

## 3. Dependency law

Dependencies point downward.

Public Foundation code may depend on:

1. the C++ standard library;
2. lower-level Qiven Foundation components;
3. operating-system primitives behind explicit platform boundaries.

Foundation must not depend on higher-level Qiven repositories such as runtime, geometry, graphics, CAD, networking, or
applications.

Third-party dependencies are not forbidden, but adding one to Foundation requires a specific architectural justification.
The default is no third-party dependency.

Circular dependencies between Foundation modules are forbidden.

## 4. Namespace law

The C++ root namespace is:

```cpp
qiven::
```

Foundation does not introduce a `qiven::foundation` namespace. Public symbols should live in the shortest namespace that
communicates their real semantic domain.

Examples:

```cpp
qiven::Span<T>
qiven::memory::Arena
qiven::sync::Mutex
```

`detail` namespaces are implementation details and are not API contracts.

The CMake target namespace is separate from the C++ namespace. The Foundation target is:

```cmake
qiven::foundation
```

## 5. Public and private boundaries

Public headers live under:

```text
include/qiven/
```

Implementation files and private headers live under:

```text
src/
```

A public header must be self-contained: including it in an otherwise empty translation unit must compile.

Public headers must not require consumers to know private directory layout.

## 6. Language and toolchain baseline

The initial language baseline is C++20.

Qiven should remain buildable with mainstream MSVC, Clang, and GCC toolchains. Exact minimum compiler versions are a CI
contract and should be recorded only when CI continuously verifies them.

Compiler extensions are not part of the portable API unless isolated behind an explicit compiler or platform layer.

## 7. Error handling

Low-level public APIs should make failure visible in their signatures.

Exceptions must not be required for ordinary Foundation control flow. Foundation should remain capable of supporting
no-exception consumers.

A concrete result/status abstraction will be designed before APIs need one rather than invented ad hoc by each module.

## 8. Memory and ownership

Ownership must be visible.

Primitive APIs should avoid hidden dynamic allocation where the caller cannot reason about its cost or lifetime. Components
that own dynamic storage should expose enough semantics for allocation strategy and lifetime to be understood.

The allocator boundary is `qiven::memory::AllocatorRef`, a non-owning type-erased reference to an allocator backend. The
backend object must outlive every `AllocatorRef` that refers to it. The reference itself owns no memory and performs no
allocation while dispatching.

Raw allocation requests always carry both size and alignment. Alignment is a non-zero power of two. Allocation failure is
reported by returning `nullptr`; exceptions and process-global out-of-memory handlers are not part of the primitive contract.
A zero-size allocation is canonicalized to `nullptr` without calling the backend.

Deallocation of `nullptr` is a no-op. A non-null pointer must be returned to the same allocator backend with the same size
and alignment used for the successful allocation request.

Foundation does not provide a mutable process-global default allocator. Concrete allocators define their own ownership,
lifetime, and thread-safety semantics.

## 9. RTTI and runtime type machinery

Foundation APIs must not require RTTI for their core semantics.

Type erasure or runtime type identification, when genuinely needed, should be explicit and local rather than an ambient
dependency of the entire library.

## 10. ABI policy

Before 1.0, Qiven Foundation does not promise a stable C++ binary ABI.

The priority is a clean source-level architecture. Binary compatibility boundaries should be introduced intentionally where
a real distribution or plugin requirement exists.

## 11. Platform policy

The intended desktop/server platforms are:

- Windows;
- Linux;
- macOS.

x86-64 is the initial architecture baseline. ARM64 is a first-class target direction, but support becomes a contract only
when it is continuously built and tested.

Platform-specific code should be isolated so that portable code does not accumulate preprocessor branches for unrelated
operating systems.

## 12. Testing law

Every public component requires tests for its contract, including boundary and failure cases.

A bug fix should normally add a regression test.

Tests may use more expensive diagnostics than production code. Sanitizers, static analysis, and platform-specific validation
belong in the engineering pipeline even when they are not runtime dependencies.

## 13. Development environment

CMake is the build-system source of truth. IDE-specific generated project files are outputs, not hand-maintained project
configuration.

Visual Studio 2022 is a first-class Windows development environment. Qiven should provide a comfortable generate, build,
test, and debug path without requiring developers to remember generator-specific command lines. Shared configuration belongs
in `CMakePresets.json`; convenience scripts should delegate to those presets instead of duplicating configuration.

IDE convenience must not compromise command-line, CI, or non-Windows builds.

## 14. Change rule

Convenience is not sufficient justification for adding a primitive to Foundation.

Before a new subsystem enters this repository, ask:

1. Is it genuinely foundational?
2. Can its ownership and failure behavior be made explicit?
3. Does it introduce a dependency that every higher layer will inherit?
4. Is the abstraction more stable than the use case that motivated it?
5. Would this be healthier in a higher-level repository?

The repository should stay smaller than the set of things Qiven can build.
