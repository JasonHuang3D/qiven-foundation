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
        headers/compiler.cpp
        headers/config.cpp
        headers/contracts.cpp
        headers/platform.cpp
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
