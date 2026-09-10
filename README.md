# Qiven Foundation

Qiven Foundation is the low-level C++ foundation of the Qiven ecosystem.

Its job is not to become a grab bag of utilities. It provides a small, disciplined base for code that must remain portable,
predictable, testable, and suitable for performance-sensitive systems.

## Status

Qiven Foundation is in its bootstrap phase. Public APIs are not stable yet.

The initial language baseline is **C++20**. The project is designed to support Windows, Linux, and macOS, with concrete
compiler and architecture support locked down by CI as the implementation grows.

## Build

### Visual Studio 2022 on Windows

Visual Studio is a first-class development environment for Qiven Foundation. The Windows configuration is defined once in
`CMakePresets.json`.

Windows development uses the pinned tools from `qiven-toolchain-win`. By default, the toolchain repository is expected next
to `qiven-foundation`. Set `QIVEN_TOOLCHAIN_ROOT` when using a different workspace layout.

The Windows host still needs Git and Visual Studio 2022 with the C++ desktop development workload. CMake and clang-format
come from `qiven-toolchain-win` rather than the host `PATH`.

Generate the Visual Studio solution with:

```text
tools\gen-vs2022-x64.cmd
```

It generates `build/vs2022-x64/qiven-foundation.sln` without launching Visual Studio. When tests are enabled,
`qiven-foundation-smoke` is configured as the Visual Studio startup project so `F5` starts an executable rather than the
CMake `ALL_BUILD` target.

Format tracked C/C++ sources with:

```text
tools\format.cmd
```

Check formatting without modifying files with:

```text
tools\format-check.cmd
```

Visual Studio should use `qiven-toolchain-win\bin\clang-format.exe` as its custom clang-format executable so IDE formatting
and command-line formatting use the same version.

### Portable command line

For other generators and platforms:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

When consumed from CMake, the project exposes:

```cmake
qiven::foundation
```

The C++ root namespace is:

```cpp
qiven::
```

There is intentionally no `qiven::foundation` C++ namespace. Foundation is the base layer of Qiven, not an extra namespace
level that every caller should carry.

## Architecture

The architectural rules for this repository live in
[`docs/architecture/foundation.md`](docs/architecture/foundation.md).

## License

This repository does not currently include an open-source license. No license should be inferred from the repository being
public.
