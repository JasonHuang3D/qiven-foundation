// ============================================================================
// src/crt_failure.cpp -- see include/qiven/crt_failure.hpp.
// ============================================================================

#include <qiven/crt_failure.hpp>

#include <qiven/platform.hpp>

#if QIVEN_PLATFORM_WINDOWS

    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>

    #include <crtdbg.h>
    #include <cstdint>
    #include <cstdio>
    #include <cstdlib>
    #include <cstring>
    #include <cwchar>

namespace qiven
{
namespace
{

// Re-entrancy guard: a fault raised while handling a fault terminates
// immediately without another evidence attempt.
volatile LONG g_entered_fault_handler = 0;

void write_all(HANDLE handle, const char* text) noexcept
{
    const auto length = static_cast<DWORD>(std::strlen(text));
    DWORD written     = 0;
    // Best effort: a failing evidence write must not mask the termination.
    WriteFile(handle, text, length, &written, nullptr);
}

void terminate_with_evidence(const char* evidence) noexcept
{
    if (InterlockedExchange(&g_entered_fault_handler, 1) != 0)
    {
        TerminateProcess(GetCurrentProcess(), crt_failure_exit_code);
    }
    const HANDLE error_stream = GetStdHandle(STD_ERROR_HANDLE);
    if (error_stream != INVALID_HANDLE_VALUE && error_stream != nullptr)
    {
        write_all(error_stream, evidence);
        char tail[96];
        std::snprintf(tail, sizeof(tail),
                      "[qiven-crt] terminating with exit code %d "
                      "(headless CRT failure behavior)\n",
                      crt_failure_exit_code);
        write_all(error_stream, tail);
    }
    // No CRT exit path: the fault may be inside the CRT itself.
    TerminateProcess(GetCurrentProcess(), crt_failure_exit_code);
}

void qiven_invalid_parameter(const wchar_t* expression, const wchar_t* function,
                             const wchar_t* file, unsigned int line,
                             std::uintptr_t /*reserved*/) noexcept
{
    // Formatting to a memory buffer only -- no stdio stream or lowio call
    // from inside the fault handler (the reported fault is often in stdio).
    char evidence[1024];
    if (expression != nullptr && file != nullptr)
    {
        std::snprintf(evidence, sizeof(evidence),
                      "[qiven-crt] invalid parameter: expression '%ls', "
                      "function '%ls', %ls:%u\n",
                      expression, (function != nullptr ? function : L"?"),
                      file, line);
    }
    else
    {
        std::snprintf(evidence, sizeof(evidence),
                      "[qiven-crt] invalid parameter (no expression "
                      "available -- release CRT fault)\n");
    }
    terminate_with_evidence(evidence);
}

} // namespace

void install_headless_crt_failure_behavior()
{
    // Takes precedence over both the release _invoke_watson fail-fast and
    // the Debug "Debug Assertion Failed" modal (the handler replaces the
    // whole assert-and-terminate sequence in either CRT flavor).
    _set_invalid_parameter_handler(qiven_invalid_parameter);

    // abort(): keep the stderr text, drop the Watson fault report (its
    // dialog and dump path are both unwanted in headless execution).
    _set_abort_behavior(_WRITE_ABORT_MSG, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

    #if defined(_DEBUG)
    // Reports that do not route through the invalid-parameter handler
    // (explicit _ASSERTE/_RPTn sites): debugger + stderr output only --
    // never the modal "Debug Assertion Failed" report path.
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG | _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG | _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
    #else
        // Release: no report-mode surface exists; the handler above is the law.
    #endif
}

} // namespace qiven

#else // !QIVEN_PLATFORM_WINDOWS

namespace qiven
{

void install_headless_crt_failure_behavior()
{
    // No modal CRT surface on this platform; nothing to install.
}

} // namespace qiven

#endif // QIVEN_PLATFORM_WINDOWS
