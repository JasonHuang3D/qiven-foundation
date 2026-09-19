#include <qiven/contracts.hpp>
#include <qiven/error.hpp>

#include <string>

using namespace qiven;

int main()
{
    // ok_value is the canonical non-error
    {
        const Error ok = Error::ok_value();
        QIVEN_VERIFY(ok.ok());
        QIVEN_VERIFY(!static_cast<bool>(ok));
        QIVEN_VERIFY(ok.code == 0);
        QIVEN_VERIFY(ok.category == error_category::none);
    }

    // make() produces a typed error with a message
    {
        const auto err = Error::make(error_category::not_found, 404, "resource not found");
        QIVEN_VERIFY(!err.ok());
        QIVEN_VERIFY(static_cast<bool>(err));
        QIVEN_VERIFY(err.code == 404);
        QIVEN_VERIFY(err.category == error_category::not_found);
        QIVEN_VERIFY(err.message == "resource not found");
    }

    // make() without a message is zero-allocation
    {
        const auto err = Error::make(error_category::timeout, 1);
        QIVEN_VERIFY(!err.ok());
        QIVEN_VERIFY(err.message.empty());
    }

    // equality: same category+code (message ignored for identity)
    {
        const auto a = Error::make(error_category::not_found, 1, "a");
        const auto b = Error::make(error_category::not_found, 1, "b");
        QIVEN_VERIFY(a == b);
        const auto c = Error::make(error_category::timeout, 1);
        QIVEN_VERIFY(a != c);
    }

    // to_string: human-readable category names
    {
        QIVEN_VERIFY(to_string(error_category::not_found) == "not-found");
        QIVEN_VERIFY(to_string(error_category::resource_exhausted) == "resource-exhausted");
        QIVEN_VERIFY(to_string(error_category::none) == "none");
    }

    // zero-allocation happy path: default-constructed Error has empty message
    {
        const Error e {};
        QIVEN_VERIFY(e.ok());
        QIVEN_VERIFY(e.message.empty());
    }

    std::printf("[ OK ] error\n");
    return 0;
}
