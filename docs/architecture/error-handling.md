# Error Handling and Memory Observation

Status: design for Foundation (docs first, implementation follows).

## Problem

Foundation currently has two contract primitives (`QIVEN_ASSERT` for
programming errors, `QIVEN_VERIFY` for invariant violations — both terminate)
and `std::optional` returns for arithmetic failures. This covers the "caller
bug" and "can I produce a value?" cases but provides no infrastructure for:

1. **Recoverable runtime failures that carry information** — "the file was
   not found" is not a caller bug and not an empty optional; it is a typed
   failure with a reason the caller needs.
2. **Memory usage observation** — how much memory is allocated, by which
   subsystem, and are there leaks at shutdown.

## Design principles

From `foundation.md` and the engineering philosophy:
- the smallest abstraction that removes proven friction;
- no exceptions in non-throwing contracts;
- no hidden allocation in the failure path;
- predictable cost: checking success must be a single branch;
- `noexcept` in the hot path; allocation only for the diagnostic message.

## Component 1: Error

A lightweight, non-throwing error type for recoverable runtime failures.
Analogous to `absl::Status` or `std::error_code` but Foundation-sized:
no `std::error_category` vtable machinery, no internationalization.

```cpp
enum class error_category : u8 {
    none              = 0,  // not an error
    invalid_argument  = 1,
    not_found         = 2,
    permission_denied = 3,
    resource_exhausted= 4,
    timeout           = 5,
    unavailable       = 6,
    internal          = 7,
};

struct Error {
    u32         code;       // domain-specific detail within the category
    error_category category;
    std::string message;    // empty for zero-allocation construction

    [[nodiscard]] bool ok() const noexcept;
    explicit operator bool() const noexcept;  // true = error present
    static const Error ok_value;              // canonical non-error
    static Error make(error_category cat, u32 code, std::string msg = {});
};
```

Rules:
- checking `ok()` is a single branch on `category != none`;
- the `message` string is only populated by the error producer when it has
  diagnostic context; the hot path allocates nothing;
- `Error` is trivially copyable when `message` is empty (SBO applies);
- NOT for programming errors — those are `QIVEN_ASSERT`.

## Component 2: Memory Observer

A pluggable, thread-safe observation layer that sits alongside the existing
allocator infrastructure. It does NOT replace allocators — it observes them.

```cpp
struct MemoryStats {
    usize total_allocations;   // monotonic count
    usize total_bytes_allocated; // monotonic
    usize current_usage;       // allocated - freed
    usize peak_usage;          // high-water mark
    usize allocation_failures; // try_allocate returned nullptr
};

class AllocationObserver {
    // thread-safe (atomic counters)
    // global singleton per process
    // hooks: on_allocate(size, ptr), on_deallocate(size, ptr), on_allocate_fail(size)
    // snapshot() -> MemoryStats
    // reset() for testing
};

// RAII guard for a scoped observation window
class ScopedMemoryWatch {
    // captures a snapshot at entry, reports delta at destruction
};
```

Integration points:
- `LinearArena::try_allocate` / `deallocate` call the observer hooks (zero
  cost when observation is disabled);
- `SystemAllocator::try_allocate` / `deallocate` ditto;
- `AllocatorRef` passes through to the underlying allocator (observation is
  in the concrete allocators, not the type-erased reference).

A compile-time flag `QIVEN_ENABLE_MEMORY_OBSERVER` (default: off in Release,
on in Debug) controls whether the hooks are active. When disabled, the
observer calls compile to zero-cost no-ops.

## Component 3: Activation of F2 (Result vocabulary)

The distillation doc's F2 trigger fires: two consumers exist (`DeserializeResult`,
`StoreReceipt`) and the owner has directed error handling infrastructure.
The `Error` type above is the foundation. A full `Result<T>` monad is
deferred until K1 (where the kernel's commit path needs it), but the `Error`
type is the prerequisite and lands now.

## Rejected alternatives

- **Exceptions** — Foundation is a no-exceptions library by engineering law.
- **`std::expected`** — C++23; Foundation is C++20. A full `Result<T>` would
  be a second implementation of the same concept, deferred to K1.
- **Global operator new/delete hooks** — invasive, ordering-dependent, and
  violates "no hidden work behind convenient APIs". Observation is at the
  allocator layer, not the global operator.
- **Callbacks/vtables for the observer** — unnecessary indirection; atomic
  counters are sufficient for the "how much / any leaks" question.

## Sequencing

1. This document.
2. `include/qiven/error.hpp` + `src/error.cpp` + `tests/error.cpp`.
3. `include/qiven/memory/observer.hpp` + `src/memory/observer.cpp` +
   `tests/memory_observer.cpp`.
4. Wire observation into `LinearArena` and `SystemAllocator`.
5. Draft consumption: `DeserializeError` and `StoreReceipt` can migrate to
   `Error` when the draft next touches those files (not forced).
