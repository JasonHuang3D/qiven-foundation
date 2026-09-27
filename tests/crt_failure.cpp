// ============================================================================
// tests/crt_failure.cpp -- contract of <qiven/crt_failure.hpp>.
//
// Install-surface assertions that hold in BOTH configurations (the handler
// read-back and the Debug report-mode read-back). The full
// terminate-with-evidence path is exercised at the executable level by the
// qiven-runtime host spawn regressions (the 2026-09-27 MVP-4 kit incident
// class); killing this test process from inside itself is not a test.
// ============================================================================

#include <qiven/crt_failure.hpp>
#include <qiven/platform.hpp>

#include <cstdio>

#if QIVEN_PLATFORM_WINDOWS
    #include <cstdlib>
    #if defined(_DEBUG)
        #include <crtdbg.h>
    #endif
#endif

static_assert(qiven::crt_failure_exit_code == 3143);

int main()
{
#if QIVEN_PLATFORM_WINDOWS
    // The fresh test process has no invalid-parameter handler installed.
    if (_get_invalid_parameter_handler() != nullptr)
    {
        return 2;
    }

    qiven::install_headless_crt_failure_behavior();

    if (_get_invalid_parameter_handler() == nullptr)
    {
        return 3;
    }

    // Idempotent: a second install keeps a handler installed.
    qiven::install_headless_crt_failure_behavior();
    if (_get_invalid_parameter_handler() == nullptr)
    {
        return 4;
    }

    #if defined(_DEBUG)
    // Debug assert reports route to the debugger and stderr -- both the
    // FILE and DEBUG bits are set (never the modal report path).
    const int assert_mode = _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_REPORT_MODE);
    if ((assert_mode & _CRTDBG_MODE_DEBUG) == 0 || (assert_mode & _CRTDBG_MODE_FILE) == 0)
    {
        return 5;
    }
    const int error_mode = _CrtSetReportMode(_CRT_ERROR, _CRTDBG_REPORT_MODE);
    if ((error_mode & _CRTDBG_MODE_DEBUG) == 0 || (error_mode & _CRTDBG_MODE_FILE) == 0)
    {
        return 6;
    }
    #endif
#else
    qiven::install_headless_crt_failure_behavior(); // no-op, must link
#endif

    std::printf("[ OK ] crt_failure install surface%s", "\n");
    return 0;
}
