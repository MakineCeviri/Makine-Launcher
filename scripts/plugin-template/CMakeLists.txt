cmake_minimum_required(VERSION 3.25)
project(my-plugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

# ── Plugin DLL ──
add_library(my-plugin SHARED plugin.cpp)

# Plugin API headers (download from Makine-Launcher or copy locally)
# Option 1: Local copy of headers in include/ directory
# Option 2: FetchContent from GitHub
if(EXISTS "${CMAKE_SOURCE_DIR}/include/makine/plugin/plugin_api.h")
    target_include_directories(my-plugin PRIVATE ${CMAKE_SOURCE_DIR}/include)
else()
    include(FetchContent)
    FetchContent_Declare(makine-plugin-sdk
        GIT_REPOSITORY https://github.com/MakineCeviri/Makine-Launcher.git
        GIT_TAG main
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR core  # only need core/include
    )
    FetchContent_MakeAvailable(makine-plugin-sdk)
    target_include_directories(my-plugin PRIVATE
        ${makine-plugin-sdk_SOURCE_DIR}/core/include
    )
endif()

# Output settings
set_target_properties(my-plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "my-plugin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/release"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/release"
)

# Copy manifest.json to output directory
configure_file(
    ${CMAKE_SOURCE_DIR}/manifest.json
    ${CMAKE_BINARY_DIR}/release/manifest.json
    COPYONLY
)
