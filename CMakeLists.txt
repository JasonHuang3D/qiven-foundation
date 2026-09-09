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

add_library(qiven-foundation INTERFACE)
add_library(qiven::foundation ALIAS qiven-foundation)

set_target_properties(qiven-foundation PROPERTIES FOLDER "Foundation")

target_compile_features(qiven-foundation INTERFACE cxx_std_20)

target_include_directories(
    qiven-foundation
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

set(
    QIVEN_FOUNDATION_PUBLIC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/architecture.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/compiler.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/config.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/platform.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qiven/types.hpp
)

target_sources(
    qiven-foundation
    INTERFACE
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
        FILES ${QIVEN_FOUNDATION_PUBLIC_HEADERS}
)

if(CMAKE_GENERATOR MATCHES "^Visual Studio")
    add_custom_target(
        qiven-foundation-headers
        SOURCES ${QIVEN_FOUNDATION_PUBLIC_HEADERS}
    )

    set_target_properties(
        qiven-foundation-headers
        PROPERTIES
            FOLDER "Foundation"
    )

    source_group(
        TREE "${CMAKE_CURRENT_SOURCE_DIR}/include"
        PREFIX "Header Files"
        FILES ${QIVEN_FOUNDATION_PUBLIC_HEADERS}
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
