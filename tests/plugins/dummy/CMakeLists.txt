cmake_minimum_required(VERSION 3.25)
project(makine-dummy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

add_library(makine-dummy SHARED dummy_plugin.cpp)

target_include_directories(makine-dummy PRIVATE
    ${CMAKE_SOURCE_DIR}/../../../core/include
)

set_target_properties(makine-dummy PROPERTIES
    PREFIX ""
    OUTPUT_NAME "makine-dummy"
)
