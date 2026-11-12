cmake_minimum_required(VERSION 3.20)
project(glogger VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(glogger SHARED
    src/glogger.cpp
    src/glogger_c.cpp
)

target_compile_definitions(glogger PRIVATE GLOGGER_EXPORTS)

target_include_directories(glogger
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

add_executable(glogger_example examples/basic.cpp)
target_link_libraries(glogger_example PRIVATE glogger)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(glogger PRIVATE -Wall -Wextra -pedantic)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(glogger PRIVATE /W4)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(glogger_tests tests/test_glogger.cpp)
    target_link_libraries(glogger_tests PRIVATE glogger)
    add_test(NAME glogger_tests COMMAND glogger_tests)
endif()