#include <qiven/architecture.hpp>
#include <qiven/compiler.hpp>
#include <qiven/platform.hpp>
#include <qiven/types.hpp>

#include <type_traits>

static_assert(QIVEN_COMPILER_CLANG + QIVEN_COMPILER_GCC + QIVEN_COMPILER_MSVC == 1);
static_assert(QIVEN_PLATFORM_WINDOWS + QIVEN_PLATFORM_LINUX + QIVEN_PLATFORM_MACOS == 1);
static_assert(QIVEN_ARCH_X86_64 + QIVEN_ARCH_ARM64 == 1);

static_assert(std::is_signed_v<qiven::i8>);
static_assert(std::is_unsigned_v<qiven::u8>);
static_assert(std::is_signed_v<qiven::i16>);
static_assert(std::is_unsigned_v<qiven::u16>);
static_assert(std::is_signed_v<qiven::i32>);
static_assert(std::is_unsigned_v<qiven::u32>);
static_assert(std::is_signed_v<qiven::i64>);
static_assert(std::is_unsigned_v<qiven::u64>);
static_assert(std::is_signed_v<qiven::isize>);
static_assert(std::is_unsigned_v<qiven::usize>);
static_assert(std::is_signed_v<qiven::iptr>);
static_assert(std::is_unsigned_v<qiven::uptr>);

int main()
{
    return 0;
}
