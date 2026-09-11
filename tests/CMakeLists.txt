function(qiven_enable_test_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
    endif()
endfunction()

add_executable(qiven-foundation-smoke smoke.cpp)

target_link_libraries(qiven-foundation-smoke PRIVATE qiven::foundation)
qiven_enable_test_warnings(qiven-foundation-smoke)

set_target_properties(
    qiven-foundation-smoke
    PROPERTIES
        CXX_EXTENSIONS OFF
        FOLDER "Tests"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_test(NAME qiven-foundation.smoke COMMAND qiven-foundation-smoke)

add_executable(qiven-foundation-platform-primitives platform_primitives.cpp)

target_link_libraries(qiven-foundation-platform-primitives PRIVATE qiven::foundation)
qiven_enable_test_warnings(qiven-foundation-platform-primitives)

set_target_properties(
    qiven-foundation-platform-primitives
    PROPERTIES
        CXX_EXTENSIONS OFF
        FOLDER "Tests"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_test(NAME qiven-foundation.platform-primitives COMMAND qiven-foundation-platform-primitives)

add_executable(qiven-foundation-alignment alignment.cpp)

target_link_libraries(qiven-foundation-alignment PRIVATE qiven::foundation)
qiven_enable_test_warnings(qiven-foundation-alignment)

set_target_properties(
    qiven-foundation-alignment
    PROPERTIES
        CXX_EXTENSIONS OFF
        FOLDER "Tests"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_test(NAME qiven-foundation.alignment COMMAND qiven-foundation-alignment)

add_executable(qiven-foundation-allocator allocator.cpp)

target_link_libraries(qiven-foundation-allocator PRIVATE qiven::foundation)
qiven_enable_test_warnings(qiven-foundation-allocator)

set_target_properties(
    qiven-foundation-allocator
    PROPERTIES
        CXX_EXTENSIONS OFF
        FOLDER "Tests"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_test(NAME qiven-foundation.allocator COMMAND qiven-foundation-allocator)

add_executable(qiven-foundation-linear-arena linear_arena.cpp)

target_link_libraries(qiven-foundation-linear-arena PRIVATE qiven::foundation)
qiven_enable_test_warnings(qiven-foundation-linear-arena)

set_target_properties(
    qiven-foundation-linear-arena
    PROPERTIES
        CXX_EXTENSIONS OFF
        FOLDER "Tests"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_test(NAME qiven-foundation.linear-arena COMMAND qiven-foundation-linear-arena)

add_executable(qiven-foundation-system-allocator system_allocator.cpp)

target_link_libraries(qiven-foundation-system-allocator PRIVATE qiven::foundation)
qiven_enable_test_warnings(qiven-foundation-system-allocator)

set_target_properties(
    qiven-foundation-system-allocator
    PROPERTIES
        CXX_EXTENSIONS OFF
        FOLDER "Tests"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_test(NAME qiven-foundation.system-allocator COMMAND qiven-foundation-system-allocator)

add_executable(qiven-foundation-checked-arithmetic checked_arithmetic.cpp)

target_link_libraries(qiven-foundation-checked-arithmetic PRIVATE qiven::foundation)
qiven_enable_test_warnings(qiven-foundation-checked-arithmetic)

set_target_properties(
    qiven-foundation-checked-arithmetic
    PROPERTIES
        CXX_EXTENSIONS OFF
        FOLDER "Tests"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_test(NAME qiven-foundation.checked-arithmetic COMMAND qiven-foundation-checked-arithmetic)

add_executable(qiven-foundation-checked-span checked_span.cpp)

target_link_libraries(qiven-foundation-checked-span PRIVATE qiven::foundation)
qiven_enable_test_warnings(qiven-foundation-checked-span)

set_target_properties(
    qiven-foundation-checked-span
    PROPERTIES
        CXX_EXTENSIONS OFF
        FOLDER "Tests"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_test(NAME qiven-foundation.checked-span COMMAND qiven-foundation-checked-span)

add_executable(qiven-foundation-byte-cursor byte_cursor.cpp)

target_link_libraries(qiven-foundation-byte-cursor PRIVATE qiven::foundation)
qiven_enable_test_warnings(qiven-foundation-byte-cursor)

set_target_properties(
    qiven-foundation-byte-cursor
    PROPERTIES
        CXX_EXTENSIONS OFF
        FOLDER "Tests"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_test(NAME qiven-foundation.byte-cursor COMMAND qiven-foundation-byte-cursor)

add_executable(qiven-foundation-contracts contracts.cpp)

target_link_libraries(qiven-foundation-contracts PRIVATE qiven::foundation)
qiven_enable_test_warnings(qiven-foundation-contracts)

set_target_properties(
    qiven-foundation-contracts
    PROPERTIES
        CXX_EXTENSIONS OFF
        FOLDER "Tests"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_test(NAME qiven-foundation.contracts COMMAND qiven-foundation-contracts)

add_library(
    qiven-foundation-header-check
    OBJECT
        headers/alignment.cpp
        headers/allocator.cpp
        headers/architecture.cpp
        headers/byte_cursor.cpp
        headers/checked_arithmetic.cpp
        headers/checked_span.cpp
        headers/compiler.cpp
        headers/config.cpp
        headers/contracts.cpp
        headers/linear_arena.cpp
        headers/platform.cpp
        headers/system_allocator.cpp
        headers/types.cpp
)

target_link_libraries(qiven-foundation-header-check PRIVATE qiven::foundation)
qiven_enable_test_warnings(qiven-foundation-header-check)

set_target_properties(
    qiven-foundation-header-check
    PROPERTIES
        CXX_EXTENSIONS OFF
        FOLDER "Tests/Header Checks"
)
