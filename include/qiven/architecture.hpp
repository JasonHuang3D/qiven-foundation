#pragma once

#if defined(_M_X64) || defined(__x86_64__)
    #define QIVEN_ARCH_X86_64 1
    #define QIVEN_ARCH_ARM64 0
#elif defined(_M_ARM64) || defined(__aarch64__)
    #define QIVEN_ARCH_X86_64 0
    #define QIVEN_ARCH_ARM64 1
#else
    #error "Qiven Foundation: unsupported architecture"
#endif
