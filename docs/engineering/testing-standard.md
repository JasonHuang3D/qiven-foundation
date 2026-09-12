# Qiven Foundation Testing Standard

Tests exist to validate semantics and protect architectural contracts.

The goal is not maximum test count. The goal is strong coverage of the ways a primitive could be subtly wrong.

## 1. Tests are part of the feature

A feature is not implementation-complete when production code compiles.

Tests should be designed while reasoning about the contract, not appended afterward as a checkbox.

For each feature identify:

- primary success behavior;
- boundary states;
- failure states;
- ownership/lifetime transitions;
- arithmetic hazards;
- platform-sensitive assumptions;
- invariants that a plausible incorrect implementation could violate.

## 2. Test the contract, not implementation trivia

Prefer tests that remain valid across reasonable internal refactors.

Do not overfit tests to:

- private helper structure;
- incidental call order;
- exact internal representation;
- implementation details not promised by the contract.

It is appropriate to inspect backend calls when allocator provenance, resource release, or another interaction is itself part of
the contract.

## 3. Resource-owning primitives

When applicable, test:

- successful acquisition;
- destruction releases exactly the intended resource;
- allocation/acquisition failure;
- valid zero-size or empty success;
- moved-from state;
- move construction;
- move assignment;
- previous destination resource cleanup;
- preservation of originating allocator/backend;
- destruction after move;
- self-move if the implementation deliberately supports or guards it.

Do not assume one happy-path allocation test proves ownership correctness.

## 4. Integer and range primitives

When applicable, test:

- zero;
- one;
- typical values;
- minimum/maximum representable values;
- exact boundary success;
- one-past-boundary failure;
- overflow;
- underflow;
- narrowing;
- sign changes;
- empty ranges;
- full ranges;
- offset + length interactions.

Prefer explicit values that expose the contract.

## 5. Alignment primitives

When applicable, test:

- smallest valid alignment;
- typical power-of-two alignments;
- already-aligned values;
- values requiring adjustment;
- boundary addresses/sizes;
- invalid alignment behavior according to the contract;
- overflow in align-up calculations if relevant.

## 6. Parsing and byte access

When applicable, test:

- exact-sized input;
- truncated input;
- empty input;
- sequential reads;
- boundary reads;
- cursor/offset state after success;
- cursor/offset state after failure;
- endian correctness;
- no out-of-bounds access.

Failure should not silently corrupt parser state unless that behavior is explicitly part of the contract.

## 7. Contracts and assertions

Do not write tests that depend on undefined behavior merely to prove an assertion exists.

When contract behavior differs between assert-enabled and assert-disabled configurations, make the distinction explicit and
consistent with repository policy.

Do not weaken a contract to make a test easier to write.

## 8. Header checks

New or materially changed public headers must remain self-contained.

When adding a public header, update the existing header-check target according to repository convention.

A header compiling only because another test included prerequisites first is not sufficient.

## 9. Debug and Release local validation

For ordinary source features, jason-worker must validate both configurations using the repository's Visual Studio 2022 CMake
presets.

Configure/generate as needed:

```cmd
tools\gen-vs2022-x64.cmd
```

Debug:

```cmd
cmake --build --preset vs2022-x64-debug --parallel
ctest --preset vs2022-x64-debug
```

Release:

```cmd
cmake --build --preset vs2022-x64-release --parallel
ctest --preset vs2022-x64-release
```

Use `--output-on-failure` if invoking CTest outside a preset or when additional diagnostic output is needed.

If the repository later changes its authoritative local workflow, update this document rather than maintaining hidden
alternative commands.

## 10. Formatting validation

The repository formatting scripts intentionally operate on files visible through `git ls-files`. When a feature creates
new C/C++ files, make only those files visible before formatting:

```cmd
git add -N -- <each new C/C++ file created by this feature>
tools\format.cmd
tools\format-check.cmd
```

Do not use a broad `git add -N .` to discover new files.

Formatting is part of local validation.

Inspect the diff afterward because a formatter can legitimately change more text than expected.

## 11. Documentation-only changes

A documentation-only feature does not automatically require full Debug/Release compilation unless:

- the feature specification requires it;
- build commands or build configuration documentation changed;
- documentation is generated/validated by a programmatic check;
- another applicable `AGENTS.md` requires it.

Still inspect the diff and verify links/paths/commands against the repository.

## 12. Failed validation

When a required local build or test fails:

1. diagnose the failure;
2. fix it if the fix is within the current feature scope;
3. rerun the failed validation;
4. rerun any validation whose result may have been invalidated by the fix.

Do not commit a feature as complete with known required local test failures.

If the failure requires architectural scope expansion or unrelated changes, stop with `CTO REVIEW REQUIRED` or
`ENVIRONMENT_BLOCKED` as appropriate.

## 13. Never weaken the detector

Do not obtain a passing result by:

- disabling a test;
- commenting out a test;
- reducing assertions;
- suppressing a sanitizer;
- lowering warning levels;
- changing compiler flags;
- adding unsafe casts solely to silence diagnostics;
- excluding a failing target;
- reducing test scope without an approved reason.

If a detector is actually wrong, escalate with evidence.

## 14. Regression fixes

A bug fix should normally add a regression test that fails for the defective behavior and passes for the corrected behavior.

If a regression test is impractical, record why in the handoff.

## 15. Cross-platform limits of local validation

Local VS2022 validation proves only the Windows configuration tested.

It does not prove:

- GCC compatibility;
- Clang compatibility;
- AppleClang compatibility;
- Linux APIs;
- macOS APIs;
- x64/arm64 differences;
- sanitizer cleanliness;
- no-exception/no-RTTI configuration.

Do not claim those are verified locally.

They remain responsibilities of later GitHub CI and CTO review.

## 16. Future incremental testing

The repository may later introduce CTest labels, affected-target analysis, caching, sharding, or other incremental CI
mechanisms.

Until such mechanisms are explicitly part of the repository contract, do not invent per-feature shortcuts that skip existing
required local tests.

Optimization of test latency must preserve confidence, not merely reduce elapsed time.
