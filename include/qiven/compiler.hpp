#pragma once

#if defined(__clang__)
    #define QIVEN_COMPILER_CLANG 1
    #define QIVEN_COMPILER_GCC 0
    #define QIVEN_COMPILER_MSVC 0
#elif defined(_MSC_VER)
    #define QIVEN_COMPILER_CLANG 0
    #define QIVEN_COMPILER_GCC 0
    #define QIVEN_COMPILER_MSVC 1
#elif defined(__GNUC__)
    #define QIVEN_COMPILER_CLANG 0
    #define QIVEN_COMPILER_GCC 1
    #define QIVEN_COMPILER_MSVC 0
#else
    #error "Qiven Foundation: unsupported compiler"
#endif
