cmake_minimum_required(VERSION 3.20)
project(gargs VERSION 1.0.0 LANGUAGES CXX)

add_library(gargs STATIC src/gargs.cpp)

# Any target linking to gargs will automatically include this directory
target_include_directories(gargs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Enforce C++23 
target_compile_features(gargs PUBLIC cxx_std_23)

add_executable(gargs_example examples/basic.cpp)
target_link_libraries(gargs_example PRIVATE gargs)

include(CTest)
if(BUILD_TESTING)
	add_executable(gargs_tests tests/test_gargs.cpp)
	target_link_libraries(gargs_tests PRIVATE gargs)
	add_test(NAME gargs_tests COMMAND gargs_tests)
endif()