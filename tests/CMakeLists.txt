add_executable(qiven-foundation-smoke smoke.cpp)

target_link_libraries(qiven-foundation-smoke PRIVATE qiven::foundation)

set_target_properties(
    qiven-foundation-smoke
    PROPERTIES
        CXX_EXTENSIONS OFF
        FOLDER "Tests"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(MSVC)
    target_compile_options(qiven-foundation-smoke PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(qiven-foundation-smoke PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
endif()

add_test(NAME qiven-foundation.smoke COMMAND qiven-foundation-smoke)
