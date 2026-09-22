#pragma once

// ============================================================================
// result.hpp — the common recoverable-failure vocabulary
//
// Result<T, Reason> is the standard way a recoverable runtime failure crosses
// a Qiven API: it holds exactly one of a success value or a typed reason. The
// default reason is the Error type from error.hpp; a domain layer may
// substitute its own typed reason (an enum or a small struct) to carry
// domain-specific failure vocabulary without inventing a local result shape
// (layer contract §3.2; distillation F2, activated by OBL-D3F7B2).
//
// Contracts:
// - [[nodiscard]] at the type level: an ignored Result is a lost failure.
// - No Result operation allocates. A reason type may choose to allocate for
//   diagnostics under its own contract (Error's message does); the failure
//   path itself stays allocation-free with an empty message.
// - Result itself neither throws nor catches; copying may propagate an
//   exception thrown by T's or Reason's copy constructor.
// - Misuse — value() on a failure, reason() on a success — is a caller
//   programming error and trips QIVEN_ASSERT, not a recoverable channel
//   (docs/architecture/error-handling.md: assertions are for programming
//   errors, this type is for recoverable failures).
// ============================================================================

#include <qiven/contracts.hpp>
#include <qiven/error.hpp>
#include <qiven/types.hpp>

#include <new>
#include <type_traits>
#include <utility>

namespace qiven
{
template <typename T, typename Reason = Error>
class [[nodiscard]] Result
{
    static_assert(!std::is_reference_v<T>, "Result value type must be an object type");
    static_assert(!std::is_reference_v<Reason>, "Result reason type must be an object type");

public:
    using value_type  = T;
    using reason_type = Reason;

    // Implicit from a success value so `return value;` works in APIs that
    // produce Result. A failure is always created explicitly via fail().
    Result(T value) noexcept(std::is_nothrow_move_constructible_v<T>) :
    m_has_value(true)
    {
        ::new (static_cast<void*>(std::addressof(m_storage.m_value))) T(std::move(value));
    }

    [[nodiscard]] static Result fail(Reason reason) noexcept(std::is_nothrow_move_constructible_v<Reason>)
    {
        Result out;
        out.m_has_value = false;
        ::new (static_cast<void*>(std::addressof(out.m_storage.m_reason))) Reason(std::move(reason));
        return out;
    }

    Result(const Result& other)
        requires std::is_copy_constructible_v<T> && std::is_copy_constructible_v<Reason>
    :
    m_has_value(other.m_has_value)
    {
        if (m_has_value)
        {
            ::new (static_cast<void*>(std::addressof(m_storage.m_value))) T(other.m_storage.m_value);
        }
        else
        {
            ::new (static_cast<void*>(std::addressof(m_storage.m_reason))) Reason(other.m_storage.m_reason);
        }
    }

    Result(Result&& other) noexcept
        requires std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_constructible_v<Reason>
    :
    m_has_value(other.m_has_value)
    {
        if (m_has_value)
        {
            ::new (static_cast<void*>(std::addressof(m_storage.m_value))) T(std::move(other.m_storage.m_value));
        }
        else
        {
            ::new (static_cast<void*>(std::addressof(m_storage.m_reason))) Reason(std::move(other.m_storage.m_reason));
        }
    }

    Result& operator=(const Result& other)
        requires std::is_copy_constructible_v<T> && std::is_copy_constructible_v<Reason>
    {
        if (this != &other)
        {
            destroy_active();
            m_has_value = other.m_has_value;
            if (m_has_value)
            {
                ::new (static_cast<void*>(std::addressof(m_storage.m_value))) T(other.m_storage.m_value);
            }
            else
            {
                ::new (static_cast<void*>(std::addressof(m_storage.m_reason))) Reason(other.m_storage.m_reason);
            }
        }
        return *this;
    }

    Result& operator=(Result&& other) noexcept
        requires std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_constructible_v<Reason>
    {
        if (this != &other)
        {
            destroy_active();
            m_has_value = other.m_has_value;
            if (m_has_value)
            {
                ::new (static_cast<void*>(std::addressof(m_storage.m_value))) T(std::move(other.m_storage.m_value));
            }
            else
            {
                ::new (static_cast<void*>(std::addressof(m_storage.m_reason))) Reason(std::move(other.m_storage.m_reason));
            }
        }
        return *this;
    }

    ~Result()
    {
        destroy_active();
    }

    [[nodiscard]] bool is_ok() const noexcept
    {
        return m_has_value;
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return m_has_value;
    }

    [[nodiscard]] T& value() & noexcept
    {
        QIVEN_ASSERT(m_has_value);
        return m_storage.m_value;
    }

    [[nodiscard]] const T& value() const& noexcept
    {
        QIVEN_ASSERT(m_has_value);
        return m_storage.m_value;
    }

    [[nodiscard]] T&& value() && noexcept
    {
        QIVEN_ASSERT(m_has_value);
        return static_cast<T&&>(m_storage.m_value);
    }

    [[nodiscard]] Reason& reason() & noexcept
    {
        QIVEN_ASSERT(!m_has_value);
        return m_storage.m_reason;
    }

    [[nodiscard]] const Reason& reason() const& noexcept
    {
        QIVEN_ASSERT(!m_has_value);
        return m_storage.m_reason;
    }

    [[nodiscard]] Reason&& reason() && noexcept
    {
        QIVEN_ASSERT(!m_has_value);
        return static_cast<Reason&&>(m_storage.m_reason);
    }

private:
    Result() noexcept = default; // used by fail()

    union Storage
    {
        Storage() noexcept
        {
        }
        ~Storage()
        {
        }
        char m_unused;
        T m_value;
        Reason m_reason;
    };

    void destroy_active() noexcept
    {
        if (m_has_value)
        {
            m_storage.m_value.~T();
        }
        else
        {
            m_storage.m_reason.~Reason();
        }
    }

    Storage m_storage;
    bool m_has_value;
};

// Void specialization: the success state carries no value, so it is
// constructed via ok() and the value() accessors do not exist. Everything
// else (nodiscard, fail()-only failures, no-throw contract, misuse trips
// QIVEN_ASSERT) matches the primary template. Required by value-less
// mutation APIs (for example qiven-runtime's IRuntimeJournalPort::append,
// MVP-0): the primary template cannot express a void success.
template <typename Reason>
class [[nodiscard]] Result<void, Reason>
{
    static_assert(!std::is_reference_v<Reason>, "Result reason type must be an object type");

public:
    using value_type  = void;
    using reason_type = Reason;

    [[nodiscard]] static Result ok() noexcept
    {
        Result out;
        out.m_has_value = true;
        return out;
    }

    [[nodiscard]] static Result fail(Reason reason) noexcept(std::is_nothrow_move_constructible_v<Reason>)
    {
        Result out;
        out.m_has_value = false;
        ::new (static_cast<void*>(std::addressof(out.m_reason))) Reason(std::move(reason));
        return out;
    }

    Result(const Result& other) noexcept(std::is_nothrow_copy_constructible_v<Reason>)
    :
    m_has_value(other.m_has_value)
    {
        if (!m_has_value)
        {
            ::new (static_cast<void*>(std::addressof(m_reason))) Reason(other.m_reason);
        }
    }

    Result(Result&& other) noexcept(std::is_nothrow_move_constructible_v<Reason>)
    :
    m_has_value(other.m_has_value)
    {
        if (!m_has_value)
        {
            ::new (static_cast<void*>(std::addressof(m_reason))) Reason(std::move(other.m_reason));
        }
    }

    Result& operator=(const Result& other) noexcept(std::is_nothrow_copy_assignable_v<Reason>)
    {
        if (this != &other)
        {
            destroy_active();
            m_has_value = other.m_has_value;
            if (!m_has_value)
            {
                ::new (static_cast<void*>(std::addressof(m_reason))) Reason(other.m_reason);
            }
        }
        return *this;
    }

    Result& operator=(Result&& other) noexcept(std::is_nothrow_move_assignable_v<Reason>)
    {
        if (this != &other)
        {
            destroy_active();
            m_has_value = other.m_has_value;
            if (!m_has_value)
            {
                ::new (static_cast<void*>(std::addressof(m_reason))) Reason(std::move(other.m_reason));
            }
        }
        return *this;
    }

    ~Result()
    {
        destroy_active();
    }

    [[nodiscard]] bool is_ok() const noexcept
    {
        return m_has_value;
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return m_has_value;
    }

    [[nodiscard]] Reason& reason() & noexcept
    {
        QIVEN_ASSERT(!m_has_value);
        return m_reason;
    }

    [[nodiscard]] const Reason& reason() const& noexcept
    {
        QIVEN_ASSERT(!m_has_value);
        return m_reason;
    }

    [[nodiscard]] Reason&& reason() && noexcept
    {
        QIVEN_ASSERT(!m_has_value);
        return static_cast<Reason&&>(m_reason);
    }

private:
    Result() noexcept = default; // used by ok()

    void destroy_active() noexcept
    {
        if (!m_has_value)
        {
            m_reason.~Reason();
        }
    }

    union
    {
        char m_unused;
        Reason m_reason;
    };
    bool m_has_value = false;
};
} // namespace qiven
