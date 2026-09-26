# Qiven Foundation

Qiven Foundation is the low-level C++ foundation of the Qiven ecosystem.

Its job is not to become a grab bag of utilities. It provides a small,
disciplined base for code that must remain portable, predictable,
testable, explicit about cost and failure, and suitable for
performance-sensitive systems. Admission is by semantic ownership and
first real consumer (ADR-0024), not by consumer count.

## Status

Phase I landed (platform/compiler/config/contracts primitives, checked
arithmetic/span/bounded byte views, allocator/memory model); adopted into
the Devkit managed lifecycle; the workspace-resolved dependency provider
for qiven-math / qiven-context-draft / qiven-runtime (WR-3 Profile E).
Public APIs are not stable yet (no pre-1.0 C++ ABI promise). The admitted
`byte_builder` gap (owning growing bounded byte accumulator) is recorded
in the capability inventory as `admitted-not-yet-landed` and lands with
the RR-0 implementation batch.

The initial language baseline is **C++20**, designed for Windows, Linux
and macOS (see the architecture document's platform policy for the
explicit-dispatch CI claim scope).

## Build

### Visual Studio 2022 on Windows

Windows development uses the pinned tools from `qiven-toolchain-win`
(expected next to this repository by default; `QIVEN_TOOLCHAIN_ROOT`
overrides). Since the WR-3 cutover, configure routes through the
**workspace bootstrap** — the control lock (`qiven-workspace`) supplies
the adapter resolution file to CMake, and moving Foundation to a
compatible revision is a control-lock transaction with no consumer
re-pin commits. The repository gate wraps the whole path:

```text
tools\qiven.cmd gate
```

Generate/build/test directly via the CMake presets (the configuration is
defined once in `CMakePresets.json`):

```text
python ..\qiven-workspace\bootstrap\qiven-bootstrap.py gate-configure --devkit ..\qiven-devkit --repo qiven-foundation --repo-root . --preset vs2022-x64 --cmake cmake
cmake --build --preset vs2022-x64-debug
ctest --preset vs2022-x64-debug
```

(The earlier `tools\gen-vs2022-x64.cmd` instruction is retired: no such
script exists in this tree; the preset path above is the actual entry.)

Format tracked C/C++ sources with the repository tool (delegates to the
toolchain's clang-format):

```text
python tools\format_sources.py --fix
python tools\format_sources.py --check
```

Visual Studio should use `qiven-toolchain-win\bin\clang-format.exe` as its
custom clang-format executable so IDE and command-line formatting agree.
(The earlier `tools\format.cmd` / `tools\format-check.cmd` spellings are
retired: those scripts do not exist in this tree.)

### Portable command line

For other generators and platforms (configure still needs the workspace
adapter file):

```bash
python ../qiven-workspace/bootstrap/qiven-bootstrap.py gate-configure --devkit ../qiven-devkit --repo qiven-foundation --repo-root . --preset <preset> --cmake cmake
cmake --build --preset <preset>-debug
ctest --preset <preset>-debug
```

When consumed from CMake, the project exposes `qiven::foundation`; the C++
root namespace is `qiven::` (intentionally no `qiven::foundation`
namespace).

## Architecture and entry points

- Current architecture contract: [`docs/architecture/foundation.md`](docs/architecture/foundation.md)
- Capability inventory (landed / admitted-not-yet-landed rows with headers
  and contracts): [`docs/architecture/capability-surface.yaml`](docs/architecture/capability-surface.yaml)
- Engineering conventions and standards: canonical in the Devkit
  (`JasonHuang3D/qiven-devkit`, ADR-0046; `docs/conventions/README.md` and
  `docs/engineering/README.md` there).
- Historical architecture/design documents (bootstrap architecture, the
  cross-repo C++ architecture snapshot, error-handling design, the
  context-draft distillation, the result-void design) are preserved under
  [`docs/legacy/`](docs/legacy/) as labeled history — not current law.

## License

This repository does not currently include an open-source license. No license should be inferred from the repository being
public.
