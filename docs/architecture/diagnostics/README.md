# Foundation I2: design entry, ownership and acceptance handoff

> Document ID: D10. Proposal; implementation is not yet accepted.
> Created: 2026-10-01 UTC. Extends PR #9 at `c03310afb2cf54f0656749446f3906dbf56a53e5`.
> This revision designs I2 only. It does not mark the remaining PR #6 program complete.

## 1. Decision and reading entry

Foundation owns the operational diagnostics design beside its implementation.
qiven-docs is the review/acceptance record; qiven-context records decisions,
work state and exact references. An ADR is not the storage location for the
headers, wire schemas, callback algorithms or implementation task bodies.

The implementation entry is this document, followed by the current batch's
named sections. The implementer implements and measures the approved contract.
Changes to semantics, public declarations, wire versions, provider selection
or accepted thresholds return through CONTRACT-CONFLICT; passing a compiler
or a test is not authority to change those decisions.

Read [the common contract](contract-v1.md),
[capture design](i2-capture.md),
[wire/artifact contract](i2-wire.md) and
[execution/acceptance batches](i2-execution.md)
by the batch's named sections; no session needs to reread the whole set.

| ID | PR9 document | Operational path in qiven-foundation |
| --- | --- | --- |
| D00 | `proposal/2026-09-29/00-foundation-diagnostics-contract-v1.md` | `docs/architecture/diagnostics/contract-v1.md` |
| D10 | `proposal/2026-10-01/10-foundation-i2-entry-and-landing.md` | `docs/architecture/diagnostics/README.md` |
| D11 | `proposal/2026-10-01/11-foundation-i2-capture-design.md` | `docs/architecture/diagnostics/i2-capture.md` |
| D12 | `proposal/2026-10-01/12-foundation-i2-wire-and-artifacts.md` | `docs/architecture/diagnostics/i2-wire.md` |
| D13 | `proposal/2026-10-01/13-foundation-i2-execution-and-acceptance.md` | `docs/architecture/diagnostics/i2-execution.md` |

D00 owns the four public source headers and common lifetime/ring/capsule law.
D11 owns the I2 process roles, internal seams, callback chain and provider gate.
D12 completes the Windows-x64 I2 portion of W0: bootstrap, registration,
context encoding, acknowledgement and artifact/receipt schemas. D13 owns the
ordered implementation batches, probes, thresholds and exit labels.
The I1 event/JSONL portion of W0 is not designed again here.

## 2. Authority and PR6 coverage

Accepted PR6 at docs `2dc2f0f83668892b06959384dd4e2efdc009b3e2` and Foundation
F0 at `032a6152efa18407d7b8d29d3203a5def179385a` remain the architectural
baseline. PR9's accepted refinements govern the specified implementation
surface. Historical accepted documents remain immutable review records.

| PR6 work | Coverage after accepting this revision | Remaining condition |
| --- | --- | --- |
| F0 boundary | Existing Foundation baseline retained | Reconcile an independently changed main before applying the design |
| I0 for I2 | D11 provider comparison and D13 fixed evidence/threshold plan | Execute the comparison; do not claim a provider won before measurement |
| I1 logger | D00 contract only; no new I1 detailed execution design this turn | Its implementation and invalidated workload measurements remain pending |
| I2 crash client/inspector | D11-D13 are the detailed execution contract | Deliver and qualify the named profile and integration rows |
| F1 packaging | D00 common rule; D13 tests crash-only static/shared packaging | Whole-library/real logger qualification remains tracked separately |
| I3 Runtime/Devkit integration | Exact handoff requirements only | Real host migration, policy enforcement and deployment remain I3 |
| F2 independent ABI | Unchanged, unlanded | Real independently versioned consumer and dedicated ABI design |
| F3 platform claims | Windows-x64 evidence specified here | Other OS/architecture capture requires separate designs/probes |

The first executable profile is Windows x64, one qualified MSVC toolchain,
matching `/MD` Release or `/MDd` Debug participants, and a prestarted external
inspector. ARM64, Linux and macOS expose truthful unavailable capture until
their own adapters are qualified. Portable headers/codecs continue compiling.
This is a staged qualification boundary, not a Windows dependency in the public API.

## 3. What acceptance does, in order

Acceptance authorizes the following handoff; this PR does not perform it.
The implementer executes the owner-approved acceptance transaction, rather
than treating its own successful implementation as owner acceptance.

1. Record the owner-approved PR9 head and review verdict. Merge only that
   accepted revision into qiven-docs main. Apply its README's dated migration
   to `accepted/<actual-UTC-acceptance-date>/` in the follow-up commit.
   All five filenames are distinct, so the original proposal dates can
   converge on the same acceptance directory without collision.
2. Prepare the Foundation work branch from current main under that repository's
   applicable conventions. If main advanced beyond the reviewed F0, compare
   diagnostics/contracts/CMake changes; preserve unrelated work and surface
   semantic conflicts before implementation. Do not replace the branch with v48.
3. Land the five documents at the exact operational paths above, before C0/I2
   code. Link the new entry from Foundation README and `docs/architecture/foundation.md`.
   Update the capability inventory as `admitted-not-yet-landed`, not delivered.
4. Create `docs/architecture/diagnostics/contract-origin.json` with the schema
   below. This records the accepted source revision and each operational copy.
   Update relative Markdown links using the complete five-path map; no prose,
   code fence, numeric table or requirement may change during transfer.
5. Extract D00's four exact headers and generated-export recipe at C0. Record
   source-header digests and the approved source document/commit separately
   from platform-generated export-header bytes. D11's internal seam is also
   extracted from its single fenced definition and protected against silent drift.
6. In qiven-context, record only the accepted design reference, target
   Foundation commit, current batch, unresolved blockers and exit evidence.
   Existing governance decides whether a short ADR amendment is necessary.
   Such a record points to Foundation's design; it contains no duplicate
   technical specification. A context schema requirement is satisfied by a
   reference-bearing decision record, not by embedding these documents.
7. Start D13 batch B00, and advance only through its explicit dependency exits.
   Record execution results in Foundation's implementation receipt paths.
   Push implementation branches through the existing repository review flow;
   updating qiven-docs design is not permission to self-merge product code.

### Transfer receipt schema

The JSON shown is a schema/example, not a request to add JSON to qiven-docs.
Repository documents here remain Markdown only.

~~~json
{
  "schema": "qiven.diag.contract-origin/1",
  "source_pr": "https://github.com/JasonHuang3D/qiven-docs/pull/9",
  "approved_head_sha": "<40-hex approved head>",
  "accepted_docs_commit": "<40-hex after acceptance migration>",
  "documents": [
    {
      "id": "D00",
      "source_path": "accepted/<UTC-date>/00-foundation-diagnostics-contract-v1.md",
      "source_blob_sha": "<git blob id>",
      "source_sha256": "<64-hex>",
      "target_path": "docs/architecture/diagnostics/contract-v1.md",
      "target_sha256": "<64-hex>"
    }
  ],
  "link_rewrite_version": 1,
  "source_header_digests": {},
  "private_seam_digest": "<filled at B00>"
}
~~~

`documents` contains all five rows, sorted by ID. Hash UTF-8 file bytes with
LF endings. The transfer checker rewrites only relative link destinations
that resolve to one of those five source documents; it uses their exact
target paths and preserves fragments. Markdown inline code and code fences
are never rewritten. It compares the resulting complete bytes to each target.
Absolute evidence URLs remain historical pins. Acceptance-migration link
rewrites are checked against the same map. An unresolved relative target is
a transfer failure, not permission to edit nearby prose.

The receipt is initially created after transfer and filled with extracted
header/seam hashes at B00. Its accepted baseline is read from the approved
docs commit/review receipt independently of the implementation branch;
editing a hash beside changed code cannot approve a contract change.

## 4. Operational authority after transfer

Foundation's landed design is the implementation entry for future sessions.
The accepted qiven-docs copy is its immutable provenance; it is not a second
live document that GLM must continually keep synchronized by memory.
Future design revisions follow a review/acceptance transaction and update
Foundation's document plus origin record together. The superseded snapshot
in accepted qiven-docs stays historical. No silent direct edit to an accepted
design is justified by an implementation failure.

For any implementation receipt, record both the design-origin revision and
the exact Foundation candidate. If those differ from the qualified pair,
the receipt is historical evidence, not current qualification.

## 5. Independence and usable exits

The I2 implementation uses the same process core and public
`install_process_diagnostics()` as D00. It can run with `logging = disabled`:
no normal queue, writer or crash ring is created; Emitter is unavailable,
while required crash capture can be ready. B00-B05 implement this useful
crash-only profile without first building a logger.

Two receipt labels prevent overclaiming:

- `I2_CORE_QUALIFIED`: the crash-only profile, callback/native context,
  independent emergency evidence, inspector, supervisor, artifact limits,
  symbols and static/shared crash package passed. This is usable for a host
  that selects this profile. It does not certify ring/writer integration.
- `I2_INTEGRATION_QUALIFIED`: the exact later I1 candidate passed the real
  stalled-writer, live ring and module-integration rows in B06. Combined with
  the core receipt, this closes the I2 portion of PR6 for that platform/profile.

These labels do not claim I3 Runtime deployment, F2 ABI stability, F3 other
platforms or the original installed Desktop H1 verdict. B06 may wait for I1
without holding back a correctly scoped core delivery.

## 6. Change authority and stop conditions

The implementer may choose local loop organization, private helper names
below the fixed seams and optimizations within the byte/work budgets.
It must not add a second process core, substitute an in-process dump writer,
weaken evidence grades, widen a threshold after observing failure, improvise
an IPC field, or convert a skipped integration row into a pass.

A conflict report identifies document ID/section, exact candidate/probe,
old and proposed observable behavior, and the smallest discriminating test.
Stop only the affected slice; completed unrelated work remains valid.
Provider evidence that defeats the nominated candidate is such a design
decision boundary, not a license for an implementation agent to pick a new
architecture while repairing tests.

Codex (model not-introspectable; reasoning not-introspectable)
