cmake_minimum_required(VERSION 3.20)
project(glinguist VERSION 1.0.0 LANGUAGES CXX)

find_package(nlohmann_json CONFIG REQUIRED)

add_library(glinguist STATIC src/glinguist.cpp)
target_include_directories(glinguist PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(glinguist PUBLIC nlohmann_json::nlohmann_json)
target_compile_features(glinguist PUBLIC cxx_std_23)
