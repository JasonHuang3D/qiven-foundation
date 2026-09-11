#include <qiven/memory/system_allocator.hpp>

#include <bit>

#include <qiven/alignment.hpp>
#include <qiven/contracts.hpp>
#include <qiven/platform.hpp>

#if QIVEN_PLATFORM_WINDOWS
    #include <malloc.h>
#elif QIVEN_PLATFORM_LINUX || QIVEN_PLATFORM_MACOS
    #include <stdlib.h>
#endif

namespace qiven::memory
{
namespace
{
constexpr usize minimum_native_alignment = sizeof(void*);

static_assert(std::has_single_bit(minimum_native_alignment));

[[nodiscard]] constexpr usize normalize_alignment(usize alignment) noexcept
{
    return alignment < minimum_native_alignment ? minimum_native_alignment : alignment;
}
} // namespace

void* SystemAllocator::try_allocate(usize size, usize alignment) noexcept
{
    QIVEN_ASSERT(std::has_single_bit(alignment));

    if (size == 0)
        return nullptr;

    const usize native_alignment = normalize_alignment(alignment);

#if QIVEN_PLATFORM_WINDOWS
    return _aligned_malloc(size, native_alignment);
#elif QIVEN_PLATFORM_LINUX || QIVEN_PLATFORM_MACOS
    void* memory = nullptr;
    return ::posix_memalign(&memory, native_alignment, size) == 0 ? memory : nullptr;
#endif
}

void SystemAllocator::deallocate(void* memory, usize size, usize alignment) noexcept
{
    QIVEN_ASSERT(std::has_single_bit(alignment));

    if (memory == nullptr)
        return;

    QIVEN_ASSERT(size != 0);
    QIVEN_ASSERT(qiven::is_aligned(static_cast<usize>(reinterpret_cast<uptr>(memory)), alignment));

    static_cast<void>(size);
    static_cast<void>(alignment);

#if QIVEN_PLATFORM_WINDOWS
    _aligned_free(memory);
#elif QIVEN_PLATFORM_LINUX || QIVEN_PLATFORM_MACOS
    ::free(memory);
#endif
}
} // namespace qiven::memory
