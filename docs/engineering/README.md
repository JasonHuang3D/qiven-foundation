# Qiven Foundation Engineering Protocol

This directory contains the repository-level engineering protocol used by `jason-worker` and by humans reviewing Work-mode
implementation.

The architecture of Qiven Foundation remains defined by `docs/architecture/foundation.md`. These documents do not replace
that architecture; they define how implementation work is performed safely and consistently.

## Documents

- `implementation-standard.md` — implementation quality, scope discipline, C++ and platform rules.
- `testing-standard.md` — semantic test design and required local validation.
- `worker-protocol.md` — Work-mode branch, batch, commit, validation, stopping, and handoff procedure.
- `feature-spec.md` — contract used by `jason-brother` to assign implementation work to `jason-worker`.

## Roles

### jason-brother

CTO, architect, reviewer, feature planner, GitHub CI reviewer, and release gate.

The CTO decides what should be built and whether it is accepted.

### jason-worker

Work-mode implementation engineer.

The worker converts approved feature specifications into high-quality local code, tests, and commits. The worker does not push
or merge by default and does not independently expand architecture.

## Normal flow

```text
Chat / jason-brother
    define ordered feature queue
            |
            v
Work / jason-worker
    feature A -> local validation -> local commit
        |
        v
    feature B -> local validation -> local commit
        |
        v
    feature C -> local validation -> local commit
        |
        v
    STOP + handoff
            |
            v
Chat / jason-brother + user
    push A -> GitHub CI -> review -> merge
    push B -> GitHub CI -> review -> merge
    push C -> GitHub CI -> review -> merge
```

The Work batch is intentionally local. This keeps high-throughput implementation separate from cross-platform validation and
release authority.

## Instruction precedence

If documents appear to conflict, use the precedence defined by the root `AGENTS.md`.

A feature specification may specialize ordinary implementation details for one feature. It may not silently override
repository-wide architectural or safety rules. Any deliberate exception should be explicit.

## Updating this protocol

These documents are expected to evolve.

When jason-worker produces a class of mistake that the existing protocol should reasonably have prevented, review both the code
and the protocol:

1. Was the feature specification ambiguous?
2. Was a repository rule missing?
3. Was the rule present but too vague to be operational?
4. Was the rule contradicted by another instruction?
5. Should the rule become an automated check instead of prose?

Fix the underlying protocol when appropriate rather than relying on the same warning being remembered manually in future
sessions.
