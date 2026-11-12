cmake_minimum_required(VERSION 3.20)
project(glogger VERSION 2.0.0 LANGUAGES C CXX)

option(GLOGGER_BUILD_SHARED "Build glogger as a shared library" ON)
option(GLOGGER_BUILD_EXAMPLE "Build the glogger example" ON)
option(GLOGGER_BUILD_BENCHMARK "Build the dedicated performance benchmark" OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(GLOGGER_BUILD_SHARED)
    add_library(glogger SHARED src/glogger.cpp src/glogger_c.cpp)
    target_compile_definitions(glogger PUBLIC GLOGGER_BUILD_SHARED)
    target_compile_definitions(glogger PRIVATE GLOGGER_EXPORTS)
else()
    add_library(glogger STATIC src/glogger.cpp src/glogger_c.cpp)
endif()

add_library(glogger::glogger ALIAS glogger)

target_include_directories(glogger
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_compile_features(glogger PUBLIC cxx_std_23)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(glogger PRIVATE -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(glogger PRIVATE /W4)
endif()

if(GLOGGER_BUILD_EXAMPLE)
    add_executable(glogger_example examples/basic.cpp)
    target_link_libraries(glogger_example PRIVATE glogger::glogger)
endif()

if(GLOGGER_BUILD_BENCHMARK)
    add_executable(glogger_benchmark benchmarks/throughput.cpp)
    target_link_libraries(glogger_benchmark PRIVATE glogger::glogger)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(glogger_tests tests/test_glogger.cpp tests/c_api_smoke.c)
    target_link_libraries(glogger_tests PRIVATE glogger::glogger)
    set_target_properties(glogger_tests PROPERTIES LINKER_LANGUAGE CXX)
    add_test(NAME glogger_tests COMMAND glogger_tests)
endif()
