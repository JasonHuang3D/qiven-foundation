# jason-worker Work Protocol

This document defines the mechanical workflow for Work-mode implementation.

The objective is high implementation throughput without granting Work mode release authority.

## 1. Roles

`jason-brother`:

- CTO;
- architect;
- feature planner;
- reviewer;
- GitHub CI reviewer;
- merge/release gate.

`jason-worker`:

- implementation engineer;
- local code/test/build executor;
- local feature-branch author;
- structured handoff provider.

jason-worker must not call itself jason-brother or use the `jason-brother/*` branch namespace.

## 2. Remote boundary

Unless the current user instruction explicitly overrides this protocol, jason-worker performs local development only.

Do not:

- push;
- fetch solely to publish work;
- create PRs;
- merge to `main`;
- delete remote branches;
- force-push;
- rewrite remote history.

Reading remote state when needed to understand the base is acceptable, but local `main` should normally already be prepared by
the user before the batch begins.

The normal handoff leaves completed local branches ready for the user to push later in Chat mode.

## 3. Git identity

Do not change:

```text
user.name
user.email
```

Do not modify global or repository Git identity configuration.

Commits use the developer's existing configured identity.

Branch namespace identifies Work-mode origin.

## 4. Branch naming

Each feature branch is:

```text
jason-worker/<feature-name>
```

Use short lowercase kebab-case feature names.

Examples:

```text
jason-worker/owned-allocation
jason-worker/checked-offset
jason-worker/small-buffer
```

The CTO feature specification may provide the exact branch name. If so, use it.

## 5. Serial stacked branches

A Work batch is normally serial, not parallel.

Example:

```text
main
  \
   jason-worker/feature-a
       \
        jason-worker/feature-b
            \
             jason-worker/feature-c
```

Feature B is created from completed Feature A when the authorized queue says B depends on A.

Feature C is created from B, and so on.

Do not create all branches from `main` when later features depend on earlier work.

Do not merge A into B; B naturally starts from A's commit.

## 6. Completed layers are frozen

After Feature A is committed and work begins on Feature B, treat A as frozen for the remainder of the batch.

If B reveals a defect in A:

### Minor, unambiguous implementation issue

If the CTO feature specification explicitly authorizes stack repair, repair using the instructed method.

Otherwise stop and report it.

### Semantic or architectural issue

Always stop.

Do not amend A, rebase the stack, or rewrite several completed commits autonomously.

Reason: later Chat/GitHub review must be able to understand exactly what Work produced.

## 7. Batch authorization

jason-worker may implement only features listed in the authorized CTO queue.

Example:

```text
AUTHORIZED FEATURE QUEUE
1. A
2. B
3. C
STOP AFTER C
```

After C, stop even if there is available time or usage.

Never infer Feature D.

Never convert a deferred opportunity into a new branch.

## 8. Batch size

Default batch size is approximately 3 ordinary features.

A CTO may authorize up to 5 low-risk features in one batch.

Architecturally risky work may intentionally be a batch of one.

Feature count is a safety boundary, not a productivity target.

## 9. Usage budget

Do not intentionally consume the entire Work allowance.

When a reliable usage indicator is available, roughly one third of the available usage window is the normal soft budget for a
batch unless the CTO specification defines a different limit.

A lower CTO-specified limit takes precedence.

Before beginning another feature, consider whether enough capacity remains to:

- understand the feature;
- implement it;
- run its required validation profile;
- repair normal defects;
- format;
- inspect the diff;
- commit;
- leave enough reserve for final batch validation and handoff.

If not, do not begin it.

When the soft budget is reached, finish the current coherent checkpoint and stop with:

`USAGE_BUDGET_SOFT_STOP`

If exact usage is not visible, do not fabricate a percentage. Rely on the finite feature queue and conservative batch size.

## 10. Architectural barriers

A feature is a likely architectural barrier when it materially changes:

- platform detection;
- compiler abstraction;
- public core types;
- allocator protocols;
- ownership contracts broadly depended on;
- exception policy;
- RTTI policy;
- ABI;
- CMake architecture;
- compiler flags;
- sanitizers;
- CI configuration.

Unless the CTO explicitly states that stacking may continue after such a feature, complete that feature locally and stop the
batch.

The next step is Chat-mode push, GitHub CI, CTO review, and merge.

## 11. Starting a batch

Before creating or modifying any feature branch:

```cmd
git status
git branch --show-current
git log -1 --oneline
tools\format-check.cmd
git diff --check
```

Verify:

- the working tree is clean;
- the expected base branch is checked out;
- the expected base commit is present;
- the repository formatting baseline passes;
- no existing local diff is present.

Read all mandatory engineering documents from `AGENTS.md` once for the batch.

Read the entire authorized feature queue before implementing Feature A so dependency order is understood.

If `tools\format-check.cmd` fails on the clean authorized base before feature work begins, do not create a feature branch and do
not repair unrelated files autonomously.

Stop with:

`BASELINE_VALIDATION_BLOCKED`

Report the failing command and affected files.

Do not classify a pre-existing formatting or validation defect as an architectural contradiction merely because it blocks the
batch.

Do not modify unrelated existing local work.

If the working tree contains user changes you did not create, stop rather than cleaning or overwriting them.

## 12. Starting a feature

For each feature:

1. verify current branch/commit matches the specified base;
2. create the specified branch;
3. inspect relevant production code, tests, CMake registration, and architecture;
4. restate the feature's required semantics internally before editing;
5. identify stop conditions;
6. implement only the feature.

Do not begin by writing code before understanding existing contracts.

## 13. During implementation

Maintain a narrow diff.

When discovering an adjacent improvement:

- do not implement it;
- record it in handoff notes if useful.

When discovering a blocking ambiguity:

- stop if it affects public semantics or architecture;
- otherwise choose the most conservative local implementation consistent with existing contracts and record the assumption.

## 14. Required local validation

For every ordinary C++ feature, first make newly created C/C++ files visible to the tracked-file formatting scripts without
staging their contents:

```cmd
git add -N -- <each new C/C++ file created by this feature>
```

Name only the files created by the current feature. Do not use a broad `git add -N .`.

Then always run:

```cmd
tools\format.cmd
tools\format-check.cmd
git diff --check
```

### FULL validation

`FULL` is the default profile.

Run:

```cmd
tools\gen-vs2022-x64.cmd
cmake --build --preset vs2022-x64-debug --parallel
ctest --preset vs2022-x64-debug
cmake --build --preset vs2022-x64-release --parallel
ctest --preset vs2022-x64-release
```

### FOCUSED validation

`FOCUSED` may be used only when the CTO feature specification explicitly selects it and provides exact build targets, tests,
selectors, or commands.

Run the authorized focused validation in both Debug and Release configurations.

Do not infer that a target or test is irrelevant merely because it appears unrelated.

If the CTO specification says `FOCUSED` but does not define a sufficiently precise validation scope, fall back to `FULL`
rather than guessing.

Architectural barriers and cross-cutting infrastructure work use `FULL` unless explicitly overridden.

### Batch-final validation

Before normal completion of the authorized queue, the final stack tip must pass:

```cmd
tools\format-check.cmd
cmake --build --preset vs2022-x64-debug --parallel
ctest --preset vs2022-x64-debug
cmake --build --preset vs2022-x64-release --parallel
ctest --preset vs2022-x64-release
```

This batch-final full validation supplements feature-level focused validation; it does not replace semantic feature tests.

If the batch stops early for an unrelated blocker before normal completion, report whether batch-final validation was run
rather than pretending it passed.

If only documentation changes, follow the documentation-only rule in `testing-standard.md`.

## 15. Diff inspection

Before commit:

```cmd
git status
git diff --check
git diff
```

Inspect every changed file.

Look specifically for:

- unrelated formatting;
- temporary output;
- debug code;
- abandoned experiments;
- generated artifacts;
- local machine paths;
- accidental CMake changes;
- unrelated source edits.

Do not rely on tests to detect scope pollution.

## 16. Commit

A feature should normally end as one coherent commit relative to its approved parent.

Commit message style should describe the semantic change.

Examples:

```text
feat(memory): add OwnedAllocation
feat: add checked integer cast
fix(memory): preserve allocator provenance on move
```

Do not alter Git identity.

Do not amend a frozen earlier feature.

After commit:

```cmd
git status
git log -1 --oneline
```

The working tree should be clean.

## 17. Proceeding to the next feature

Continue only if all are true:

- current feature is committed;
- the feature's required validation profile passed;
- working tree is clean;
- no architectural review is pending;
- next feature is explicitly in the authorized queue;
- the current feature is not an unapproved barrier;
- usage budget is sufficient for a coherent next feature.

Otherwise stop.

## 18. Failure handling

### Baseline validation failure

If the clean authorized base fails repository validation before feature implementation begins:

`BASELINE_VALIDATION_BLOCKED`

Do not repair unrelated baseline files unless the CTO explicitly authorizes a maintenance scope.

### Local implementation/test failure inside scope

Diagnose, fix, rerun validation, continue.

### Failure requires scope expansion

Stop:

`CTO REVIEW REQUIRED`

### Toolchain/environment failure

Do not mutate the host globally to repair it.

Stop:

`ENVIRONMENT_BLOCKED`

Include the exact failing command and relevant error.

### Architectural contradiction

Stop immediately:

`ARCHITECTURAL_REVIEW_REQUIRED`

Do not “finish the rest of the batch first.”

## 19. Handoff format

Do not emit the complete handoff after each successful feature while the batch is still continuing.

For a mid-batch blocker, return only:

```text
JASON-WORKER BLOCKER

Type:
Branch:
Commit/worktree state:
Condition or failing command:
Evidence:
User/CTO action required:
```

The complete handoff below is reserved for actual batch termination.

At the end of the batch, return a report in this structure:

```text
JASON-WORKER HANDOFF

Base:
<base branch and commit>

Feature 1:
  Name:
  Branch:
  Base:
  Commit:
  Summary:
  Files:
  Tests:
  Validation profile:
  Format:
  Debug validation:
  Release validation:
  Cross-platform status: NOT VERIFIED LOCALLY
  Assumptions:
  Concerns:
  Deferred notes:
  CTO review required: yes/no

Feature 2:
  ...

Batch-final validation:
<FULL PASS | NOT RUN | WAIVED BY CTO>

Batch stop reason:
<AUTHORIZED_QUEUE_COMPLETED | ARCHITECTURAL_REVIEW_REQUIRED | BASELINE_VALIDATION_BLOCKED |
 LOCAL_TEST_FAILURE | ENVIRONMENT_BLOCKED | USAGE_BUDGET_SOFT_STOP>

Local branches created:
- ...

Remote operations performed:
NONE

Final working tree:
CLEAN
```

Do not claim `NONE`, `PASS`, or `CLEAN` unless verified.

## 20. Chat-mode consumption

After Work stops, the user returns to Chat mode.

The expected review flow is feature-by-feature:

1. user pushes the first local `jason-worker/*` branch;
2. GitHub CI runs according to repository policy;
3. jason-brother inspects commit, diff, architecture, tests, and CI;
4. user merges after approval;
5. user pushes the next stacked branch;
6. repeat.

Work mode does not pre-approve later branches merely because their local tests passed.

If an early branch needs correction during Chat review, later stacked branches may need restacking. That repair is a new
explicit task, not something jason-worker should have predicted by rewriting history in advance.
