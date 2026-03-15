cmake_minimum_required(VERSION 3.25)
project(makineai-dummy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

add_library(makineai-dummy SHARED dummy_plugin.cpp)

target_include_directories(makineai-dummy PRIVATE
    ${CMAKE_SOURCE_DIR}/../../../core/include
)

set_target_properties(makineai-dummy PROPERTIES
    PREFIX ""
    OUTPUT_NAME "makineai-dummy"
)
