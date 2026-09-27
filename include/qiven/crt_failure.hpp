// ============================================================================
// include/qiven/crt_failure.hpp -- headless CRT failure behavior for
// executables invoked by agents, operators and batch entrypoints.
//
// Law (operating contract, timeout/hang classification rule 4; governing
// incident MEM-20260919T153758Z-D6B4E7, recurrence 2026-09-27 MVP-4 H1 kit):
// Qiven executables must never block on OS modal UI in automated contexts.
// CRT fault paths (invalid parameter, assert, abort) terminate the process
// deterministically with an evidence line and a distinct exit code; they
// never pause awaiting interaction and never pop a dialog.
//
// install_headless_crt_failure_behavior() installs, process-wide:
//   - an invalid-parameter handler: the release-UCRT _invoke_watson
//     fail-fast (0xC0000409, zero output, no evidence) and the Debug-CRT
//     "Debug Assertion Failed" modal both become a typed termination;
//   - abort-behavior without the fault-report (Watson) path;
//   - Debug-CRT report modes without _CRTDBG_MODE_WDW (no MessageBox).
//
// The handler writes one bounded evidence line to the standard error
// HANDLE through WriteFile -- never through stdio, because the fault being
// reported may be inside the stdio machinery itself -- and terminates the
// process with crt_failure_exit_code.
// ============================================================================

#pragma once

namespace qiven
{

// Exit code of a process terminated by the installed CRT failure behavior.
// 3143 = 0xC47; deliberately outside the 0/1/2 usage band Qiven entrypoints
// already use, so callers can classify the death mechanically.
inline constexpr int crt_failure_exit_code = 3143;

// Installs the headless CRT failure behavior (see file header). Call once,
// as early in main() as possible -- before any stdio work whose corruption
// would need to be reported. Idempotent; no failure mode (the underlying
// setters do not fail). On non-Windows platforms this is a no-op: there is
// no modal CRT surface to suppress there.
void install_headless_crt_failure_behavior();

} // namespace qiven
