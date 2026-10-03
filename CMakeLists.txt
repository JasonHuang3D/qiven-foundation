cmake_minimum_required(VERSION 3.28)

project(
    qiven-foundation
    VERSION 0.1.0
    DESCRIPTION "Low-level C++ foundation for the Qiven ecosystem"
    LANGUAGES CXX
)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(MSVC)
    set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")
endif()

add_library(qiven-foundation STATIC)
add_library(qiven::foundation ALIAS qiven-foundation)

set_target_properties(
    qiven-foundation
    PROPERTIES
        CXX_EXTENSIONS OFF
        FOLDER "Foundation"
        # installed as qiven::foundation (matches the in-repo alias)
        EXPORT_NAME foundation
)

target_compile_features(qiven-foundation PUBLIC cxx_std_20)

target_include_directories(
    qiven-foundation
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# F1 packaging: the static variant (and every static consumer, in-repo or
# via the installed package) skips the generated export header entirely —
# QIVEN_FOUNDATION_API then falls back to empty in <qiven/diag/service.hpp>,
# so the static path's codegen is unchanged from the pre-F1 build.
target_compile_definitions(qiven-foundation PUBLIC QIVEN_FOUNDATION_NO_EXPORT_HEADER)

set(
    QIVEN_FOUNDATION_PUBLIC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/alignment.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/architecture.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/byte_builder.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/byte_cursor.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/byte_writer.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/checked_arithmetic.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/checked_integer_cast.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/checked_span.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/compiler.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/config.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/contracts.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/crt_failure.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/diag/event.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/diag/ring.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/diag/service.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/endian.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/error.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/hashing.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/hashing_sha256.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/memory/allocator.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/memory/layout.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/memory/linear_arena.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/memory/observer.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/memory/owned_allocation.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/memory/owned_array.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/memory/owned_object.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/memory/system_allocator.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/platform.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/result.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/types.hpp
)

set(
    QIVEN_FOUNDATION_PRIVATE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/contracts.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/crt_failure.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/diag/service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/memory/linear_arena.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/memory/owned_allocation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/memory/system_allocator.cpp
)

target_sources(
    qiven-foundation
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
        FILES ${QIVEN_FOUNDATION_PUBLIC_HEADERS}
    PRIVATE
        ${QIVEN_FOUNDATION_PRIVATE_SOURCES}
)

if(MSVC)
    target_compile_options(qiven-foundation PRIVATE /W4 /WX /permissive- /utf-8 /MP)
else()
    target_compile_options(qiven-foundation PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
endif()

if(CMAKE_GENERATOR MATCHES "^Visual Studio")
    source_group(
        TREE "${CMAKE_CURRENT_SOURCE_DIR}/include"
        PREFIX "Header Files"
        FILES ${QIVEN_FOUNDATION_PUBLIC_HEADERS}
    )

    source_group(
        TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"
        PREFIX "Source Files"
        FILES ${QIVEN_FOUNDATION_PRIVATE_SOURCES}
    )
endif()

# Build-performance policy (devkit docs/conventions/build-performance.md):
# the foundation vocabulary stack precompiles once for the library and is
# reused by the test targets; the header-check target is EXEMPT by design
# (injected PCH includes would mask a self-containment defect).
target_precompile_headers(
    qiven-foundation
    PRIVATE
        <qiven/contracts.hpp>
        <qiven/error.hpp>
        <qiven/hashing.hpp>
        <qiven/hashing_sha256.hpp>
        <qiven/result.hpp>
        <qiven/types.hpp>
        <algorithm>
        <array>
        <cstdint>
        <cstddef>
        <limits>
        <memory>
        <optional>
        <span>
        <string>
        <utility>
        <vector>
)

# ---- F1 packaging smoke: shared variant + relocatable install/export ----
# The shared library compiles the SAME private sources and installs the
# SAME public headers as the static library; only the export-macro
# plumbing differs (see <qiven/diag/service.hpp>: QIVEN_FOUNDATION_API on
# the diag free entry points; the full export surface is the F2 module
# ABI slice, not this one).
option(
    QIVEN_FOUNDATION_BUILD_SHARED
    "Build the shared qiven-foundation variant alongside the static library"
    ${PROJECT_IS_TOP_LEVEL}
)

if(QIVEN_FOUNDATION_BUILD_SHARED)
    add_library(qiven-foundation-shared SHARED)
    add_library(qiven::foundation-shared ALIAS qiven-foundation-shared)

    set_target_properties(
        qiven-foundation-shared
        PROPERTIES
            CXX_EXTENSIONS OFF
            FOLDER "Foundation"
            DEFINE_SYMBOL QIVEN_FOUNDATION_EXPORTS
            # installed as qiven::foundation-shared (matches the alias)
            EXPORT_NAME foundation-shared
    )

    target_compile_features(qiven-foundation-shared PUBLIC cxx_std_20)

    # The generated <qiven/foundation_export.hpp> lives in the build tree;
    # the shared target's public include covers it in the build tree and,
    # after install, through $<INSTALL_INTERFACE:include> (the generated
    # header is installed beside the FILE_SET headers below).
    target_include_directories(
        qiven-foundation-shared
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )

    target_sources(
        qiven-foundation-shared
        PUBLIC
            FILE_SET HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
            FILES ${QIVEN_FOUNDATION_PUBLIC_HEADERS}
        PRIVATE
            ${QIVEN_FOUNDATION_PRIVATE_SOURCES}
    )

    if(MSVC)
        target_compile_options(qiven-foundation-shared PRIVATE /W4 /WX /permissive- /utf-8 /MP)
    else()
        target_compile_options(qiven-foundation-shared PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
    endif()

    target_precompile_headers(
        qiven-foundation-shared
        PRIVATE
            <qiven/contracts.hpp>
            <qiven/error.hpp>
            <qiven/hashing.hpp>
            <qiven/hashing_sha256.hpp>
            <qiven/result.hpp>
            <qiven/types.hpp>
            <algorithm>
            <array>
            <cstdint>
            <cstddef>
            <limits>
            <memory>
            <optional>
            <span>
            <string>
            <utility>
            <vector>
    )

    include(GenerateExportHeader)
    generate_export_header(
        qiven-foundation-shared
        BASE_NAME QIVEN_FOUNDATION
        EXPORT_MACRO_NAME QIVEN_FOUNDATION_API
        EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/include/qiven/foundation_export.hpp
    )

    if(CMAKE_GENERATOR MATCHES "^Visual Studio")
        source_group(
            TREE "${CMAKE_CURRENT_SOURCE_DIR}/include"
            PREFIX "Header Files"
            FILES ${QIVEN_FOUNDATION_PUBLIC_HEADERS}
        )

        source_group(
            TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"
            PREFIX "Source Files"
            FILES ${QIVEN_FOUNDATION_PRIVATE_SOURCES}
        )
    endif()
endif()

# Relocatable install: everything the package ships is addressed relative
# to the install prefix, and the handwritten config resolves the exported
# targets file relative to its own location (F1 install-relocation gate).
set(QIVEN_FOUNDATION_INSTALL_TARGETS qiven-foundation)
if(TARGET qiven-foundation-shared)
    list(APPEND QIVEN_FOUNDATION_INSTALL_TARGETS qiven-foundation-shared)
endif()

install(
    TARGETS ${QIVEN_FOUNDATION_INSTALL_TARGETS}
    EXPORT qiven-foundation-targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    FILE_SET HEADERS DESTINATION include
)

if(TARGET qiven-foundation-shared)
    install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/include/qiven/foundation_export.hpp
        DESTINATION include/qiven
    )
endif()

install(
    EXPORT qiven-foundation-targets
    NAMESPACE qiven::
    FILE qiven-foundation-targets.cmake
    DESTINATION lib/cmake/qiven-foundation
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/qiven-foundationConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        cmake/qiven-foundationConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/qiven-foundationConfigVersion.cmake
    DESTINATION lib/cmake/qiven-foundation
)

option(QIVEN_BUILD_TESTS "Build Qiven Foundation tests" ${PROJECT_IS_TOP_LEVEL})

if(QIVEN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)

    if(MSVC)
        set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT qiven-foundation-smoke)
    endif()
endif()
