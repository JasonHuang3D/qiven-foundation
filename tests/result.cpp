#include <qiven/contracts.hpp>
#include <qiven/error.hpp>
#include <qiven/result.hpp>

#include <cstdio>
#include <memory>
#include <string>
#include <utility>

using namespace qiven;

namespace
{
// Draft-shaped typed reason (the distillation F2 anatomy: a typed outcome
// enum used as the failure vocabulary of a store-like API).
enum class StoreFailure : u8
{
    CompareFailed,
    OutcomeUnknown,
};

struct PinError
{
    error_category category = error_category::none;
    u32 code                = 0;
};

// Destructor-counting value type: proves the union destroys exactly the
// active alternative across copies, moves and assignments.
struct Counted
{
    static inline int alive = 0;

    int payload = 0;

    explicit Counted(int v) :
    payload(v)
    {
        ++alive;
    }
    Counted(const Counted& other) :
    payload(other.payload)
    {
        ++alive;
    }
    Counted(Counted&& other) noexcept :
    payload(other.payload)
    {
        ++alive;
    }
    Counted& operator=(const Counted& other) = default;
    Counted& operator=(Counted&& other)      = default;
    ~Counted()
    {
        --alive;
    }
};
} // namespace

int main()
{
    // success path: value round-trip and queries
    {
        Result<int> r(42);
        QIVEN_VERIFY(r.is_ok());
        QIVEN_VERIFY(static_cast<bool>(r));
        QIVEN_VERIFY(r.value() == 42);

        const Result<std::string> s(std::string("pinned"));
        QIVEN_VERIFY(s.value() == "pinned");
    }

    // failure path: the full Error reason survives (category, code, message)
    {
        const Result<int> r = Result<int>::fail(Error::make(error_category::not_found, 404, "missing"));
        QIVEN_VERIFY(!r.is_ok());
        QIVEN_VERIFY(!static_cast<bool>(r));
        QIVEN_VERIFY(r.reason().category == error_category::not_found);
        QIVEN_VERIFY(r.reason().code == 404);
        QIVEN_VERIFY(r.reason().message == "missing");
    }

    // typed enum reason: the draft store-receipt vocabulary as a Result shape
    {
        const Result<u64, StoreFailure> committed(7u);
        const Result<u64, StoreFailure> unknown = Result<u64, StoreFailure>::fail(StoreFailure::OutcomeUnknown);
        QIVEN_VERIFY(committed.is_ok() && committed.value() == 7);
        QIVEN_VERIFY(!unknown.is_ok());
        QIVEN_VERIFY(unknown.reason() == StoreFailure::OutcomeUnknown);
    }

    // small-struct reason: domain layers substitute their own detail carrier
    {
        PinError pin {};
        pin.category = error_category::resource_exhausted;
        pin.code     = 3;

        const Result<int, PinError> r = Result<int, PinError>::fail(pin);
        QIVEN_VERIFY(!r);
        QIVEN_VERIFY(r.reason().category == error_category::resource_exhausted);
        QIVEN_VERIFY(r.reason().code == 3);
    }

    // copy semantics preserve both alternatives
    {
        const Result<int> ok(5);
        const auto ok_copy = ok;
        QIVEN_VERIFY(ok_copy.is_ok() && ok_copy.value() == 5);

        const Result<int> bad = Result<int>::fail(Error::make(error_category::timeout, 9, "slow"));
        const auto bad_copy   = bad;
        QIVEN_VERIFY(!bad_copy.is_ok());
        QIVEN_VERIFY(bad_copy.reason() == bad.reason());
        QIVEN_VERIFY(bad_copy.reason().message == "slow");
    }

    // move-only value type (the Runtime's pinned-cognition shape is move-only)
    {
        Result<std::unique_ptr<int>> r(std::make_unique<int>(11));
        QIVEN_VERIFY(r.is_ok() && *r.value() == 11);

        auto moved_into = std::move(r);
        QIVEN_VERIFY(moved_into.is_ok() && *moved_into.value() == 11);
        // the moved-from Result stays valid and destructible; its state flag
        // is unchanged by the transfer
        QIVEN_VERIFY(r.is_ok());

        auto owned = std::move(moved_into).value();
        QIVEN_VERIFY(*owned == 11);
        QIVEN_VERIFY(moved_into.value() == nullptr);

        Result<std::unique_ptr<int>, StoreFailure> f = Result<std::unique_ptr<int>, StoreFailure>::fail(
            StoreFailure::CompareFailed);
        auto f_moved = std::move(f);
        QIVEN_VERIFY(!f_moved.is_ok());
        QIVEN_VERIFY(f_moved.reason() == StoreFailure::CompareFailed);
    }

    // assignment switches alternatives in both directions
    {
        Result<int> r(1);
        r = Result<int>::fail(Error::make(error_category::unavailable, 2));
        QIVEN_VERIFY(!r.is_ok() && r.reason().code == 2);

        r = Result<int>(3);
        QIVEN_VERIFY(r.is_ok() && r.value() == 3);

        r = Result<int>(4);
        QIVEN_VERIFY(r.is_ok() && r.value() == 4);

        Result<int> alias_target(5);
        Result<int>& alias = alias_target;
        alias_target       = alias; // self-assignment keeps the active value
        QIVEN_VERIFY(alias_target.is_ok() && alias_target.value() == 5);
    }

    // union lifetime: exactly the active alternative is constructed/destroyed
    {
        QIVEN_VERIFY(Counted::alive == 0);
        {
            Result<Counted> r(Counted(1)); // temporary + moved-in → alive == 1
            QIVEN_VERIFY(Counted::alive == 1);

            const auto copy = r; // +1
            QIVEN_VERIFY(Counted::alive == 2);

            const auto moved = std::move(copy); // +1; the source stays alive
            QIVEN_VERIFY(Counted::alive == 3);

            r = Result<Counted>(Counted(2)); // old value destroyed, new value in
            QIVEN_VERIFY(Counted::alive == 3);
            QIVEN_VERIFY(r.value().payload == 2);

            r = Result<Counted>::fail(Error::make(error_category::internal, 1));
            QIVEN_VERIFY(!r.is_ok());
            QIVEN_VERIFY(Counted::alive == 2); // r's value alternative is gone
        }
        QIVEN_VERIFY(Counted::alive == 0);
    }

    // a fail() with an empty message constructs without allocation on the
    // failure path (Error contract; zero-allocation evidence is its empty SBO)
    {
        const Result<int> r = Result<int>::fail(Error::make(error_category::timeout, 1));
        QIVEN_VERIFY(!r.is_ok());
        QIVEN_VERIFY(r.reason().message.empty());
    }

    std::printf("[ OK ] result\n");
    return 0;
}
