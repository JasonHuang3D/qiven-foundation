#pragma once

#include <source_location>

#ifndef QIVEN_ENABLE_ASSERTS
    #if defined(NDEBUG)
        #define QIVEN_ENABLE_ASSERTS 0
    #else
        #define QIVEN_ENABLE_ASSERTS 1
    #endif
#endif

#if QIVEN_ENABLE_ASSERTS != 0 && QIVEN_ENABLE_ASSERTS != 1
    #error "Qiven Foundation: QIVEN_ENABLE_ASSERTS must be 0 or 1"
#endif

namespace qiven::detail
{
[[noreturn]] void contract_fail(
    const char* kind,
    const char* expression,
    std::source_location location = std::source_location::current()) noexcept;
} // namespace qiven::detail

#if QIVEN_ENABLE_ASSERTS
    #define QIVEN_ASSERT(expression) \
        (static_cast<bool>(expression) ? static_cast<void>(0) : ::qiven::detail::contract_fail("assert", #expression))
#else
    #define QIVEN_ASSERT(expression) static_cast<void>(0)
#endif

#define QIVEN_VERIFY(expression) \
    (static_cast<bool>(expression) ? static_cast<void>(0) : ::qiven::detail::contract_fail("verify", #expression))

#define QIVEN_UNREACHABLE() ::qiven::detail::contract_fail("unreachable", nullptr)
