#pragma once

#if defined(_WIN32)
    #define QIVEN_PLATFORM_WINDOWS 1
    #define QIVEN_PLATFORM_LINUX 0
    #define QIVEN_PLATFORM_MACOS 0
#elif defined(__APPLE__) && defined(__MACH__)
    #define QIVEN_PLATFORM_WINDOWS 0
    #define QIVEN_PLATFORM_LINUX 0
    #define QIVEN_PLATFORM_MACOS 1
#elif defined(__linux__)
    #define QIVEN_PLATFORM_WINDOWS 0
    #define QIVEN_PLATFORM_LINUX 1
    #define QIVEN_PLATFORM_MACOS 0
#else
    #error "Qiven Foundation: unsupported platform"
#endif
