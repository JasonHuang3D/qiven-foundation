cmake_minimum_required(VERSION 3.28)

# F1 packaging smoke: an EXTERNAL consumer of the installed
# qiven-foundation package. These are the TRACKED templates (durability:
# the gate task must reproduce from a clean clone); the driver
# materializes this project OUTSIDE the repository build under
# .generated-temp/f1-smoke/consumer-src at run time. The driver passes
# the install prefix and the link variant as -D flags at configure
# time, and the run-time arguments (sink/marker paths, --overhead N) as
# argv.

project(
    qiven_f1_packaging_consumer
    DESCRIPTION "External consumer proving the installed qiven-foundation package"
    LANGUAGES CXX
)

option(QIVEN_CONSUMER_LINK_SHARED "Link qiven::foundation-shared instead of qiven::foundation" OFF)

find_package(qiven-foundation CONFIG REQUIRED)

add_executable(f1-consumer main.cpp)

target_compile_features(f1-consumer PRIVATE cxx_std_20)
set_target_properties(f1-consumer PROPERTIES CXX_EXTENSIONS OFF)

if(QIVEN_CONSUMER_LINK_SHARED)
    target_link_libraries(f1-consumer PRIVATE qiven::foundation-shared)
else()
    target_link_libraries(f1-consumer PRIVATE qiven::foundation)
endif()
