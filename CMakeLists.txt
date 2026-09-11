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
)

target_compile_features(qiven-foundation PUBLIC cxx_std_20)

target_include_directories(
    qiven-foundation
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

set(
    QIVEN_FOUNDATION_PUBLIC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/alignment.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/architecture.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/checked_arithmetic.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/compiler.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/config.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/contracts.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/memory/allocator.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/memory/linear_arena.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/memory/system_allocator.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/platform.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/types.hpp
)

set(
    QIVEN_FOUNDATION_PRIVATE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/contracts.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/memory/linear_arena.cpp
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
    target_compile_options(qiven-foundation PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(qiven-foundation PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
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

option(QIVEN_BUILD_TESTS "Build Qiven Foundation tests" ${PROJECT_IS_TOP_LEVEL})

if(QIVEN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)

    if(MSVC)
        set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT qiven-foundation-smoke)
    endif()
endif()
