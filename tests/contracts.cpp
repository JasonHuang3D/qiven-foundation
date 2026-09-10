#include <qiven/contracts.hpp>

int main()
{
    int value = 0;

    QIVEN_ASSERT(++value == 1);

#if QIVEN_ENABLE_ASSERTS
    if (value != 1)
        return 1;
#else
    if (value != 0)
        return 1;
#endif

    const int expected = value + 1;
    QIVEN_VERIFY(++value == expected);

    return value == expected ? 0 : 2;
}
