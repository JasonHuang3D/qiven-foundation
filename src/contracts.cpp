#include <qiven/contracts.hpp>
#include <qiven/platform.hpp>

#include <cstddef>
#include <cstdio>
#include <cstdlib>

#if QIVEN_PLATFORM_WINDOWS
#    ifndef VC_EXTRALEAN
#        define VC_EXTRALEAN
#    endif
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    ifndef STRICT
#        define STRICT
#    endif
#    include <Windows.h>
#else
#    include <unistd.h>
#endif

namespace qiven::detail
{
namespace
{
constexpr std::size_t message_capacity = 2048;

std::size_t message_size(int result) noexcept
{
    if (result <= 0)
        return 0;

    const auto size = static_cast<std::size_t>(result);
    return size < message_capacity ? size : message_capacity - 1;
}

void write_stderr(const char* message, std::size_t size) noexcept
{
    if (size == 0)
        return;

#if QIVEN_PLATFORM_WINDOWS
    const HANDLE handle = ::GetStdHandle(STD_ERROR_HANDLE);
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE)
        return;

    DWORD written = 0;
    ::WriteFile(handle, message, static_cast<DWORD>(size), &written, nullptr);
#else
    static_cast<void>(::write(STDERR_FILENO, message, size));
#endif
}

void write_debugger(const char* message) noexcept
{
#if QIVEN_PLATFORM_WINDOWS
    ::OutputDebugStringA(message);
#else
    static_cast<void>(message);
#endif
}

void break_if_debugger_attached() noexcept
{
#if QIVEN_PLATFORM_WINDOWS
    if (::IsDebuggerPresent() != FALSE)
        ::DebugBreak();
#endif
}
} // namespace

[[noreturn]] void contract_fail(const char* kind, const char* expression, std::source_location location) noexcept
{
    char message[message_capacity]{};

    const int result = expression != nullptr
                           ? std::snprintf(
                                 message,
                                 sizeof(message),
                                 "Qiven contract violation: %s\nExpression: %s\n%s:%u\n%s\n",
                                 kind,
                                 expression,
                                 location.file_name(),
                                 static_cast<unsigned>(location.line()),
                                 location.function_name())
                           : std::snprintf(
                                 message,
                                 sizeof(message),
                                 "Qiven contract violation: %s\n%s:%u\n%s\n",
                                 kind,
                                 location.file_name(),
                                 static_cast<unsigned>(location.line()),
                                 location.function_name());

    write_stderr(message, message_size(result));
    write_debugger(message);
    break_if_debugger_attached();
    std::abort();
}
} // namespace qiven::detail
