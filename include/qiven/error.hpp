#pragma once

// ============================================================================
// error.hpp — a lightweight, non-throwing error type for recoverable failures
//
// This is NOT for programming errors (use QIVEN_ASSERT) or invariant
// violations (use QIVEN_VERIFY). It is for recoverable runtime failures
// where the caller needs to know WHY the operation failed and decide what
// to do next.
//
// Checking success is a single branch. The message string is only populated
// by the producer when it has diagnostic context; the happy path allocates
// nothing.
// ============================================================================

#include <qiven/types.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace qiven
{
enum class error_category : u8
{
    none               = 0,
    invalid_argument   = 1,
    not_found          = 2,
    permission_denied  = 3,
    resource_exhausted = 4,
    timeout            = 5,
    unavailable        = 6,
    internal           = 7,
};

[[nodiscard]] constexpr std::string_view to_string(error_category cat) noexcept
{
    switch (cat)
    {
    case error_category::none: return "none";
    case error_category::invalid_argument: return "invalid-argument";
    case error_category::not_found: return "not-found";
    case error_category::permission_denied: return "permission-denied";
    case error_category::resource_exhausted: return "resource-exhausted";
    case error_category::timeout: return "timeout";
    case error_category::unavailable: return "unavailable";
    case error_category::internal: return "internal";
    }
    return "unknown";
}

struct Error
{
    u32 code                = 0;
    error_category category = error_category::none;
    std::string message;

    [[nodiscard]] constexpr bool ok() const noexcept
    {
        return category == error_category::none;
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return category != error_category::none;
    }

    [[nodiscard]] bool operator==(const Error& other) const noexcept
    {
        return code == other.code && category == other.category;
    }

    [[nodiscard]] static Error ok_value() noexcept
    {
        return Error {};
    }

    [[nodiscard]] static Error make(error_category cat, u32 code = 0, std::string message = {})
    {
        return Error { code, cat, std::move(message) };
    }
};

} // namespace qiven
