# Makine Fuzzing Tests
# Copyright (c) 2026 Makine Team
#
# Supports: LibFuzzer (Clang), AFL++
#
# Build with LibFuzzer (Clang):
#   cmake -B build -DCMAKE_CXX_COMPILER=clang++ -DBUILD_FUZZING=ON
#   cmake --build build --target fuzz_all
#
# Run fuzzer:
#   ./build/fuzz_unity_bundle corpus/unity_bundle -max_len=65536

cmake_minimum_required(VERSION 3.28)

option(BUILD_FUZZING "Build fuzzing tests" OFF)

if(NOT BUILD_FUZZING)
    message(STATUS "Fuzzing tests disabled (BUILD_FUZZING=OFF)")
    return()
endif()

# Check for fuzzer support
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(FUZZER_FLAGS "-fsanitize=fuzzer,address,undefined")
    set(FUZZER_AVAILABLE TRUE)
    message(STATUS "LibFuzzer available (Clang)")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # AFL++ support
    find_program(AFL_COMPILER afl-g++)
    if(AFL_COMPILER)
        set(FUZZER_AVAILABLE TRUE)
        message(STATUS "AFL++ available")
    else()
        message(STATUS "AFL++ not found - fuzzing disabled")
        set(FUZZER_AVAILABLE FALSE)
    endif()
else()
    message(STATUS "No fuzzer support for ${CMAKE_CXX_COMPILER_ID}")
    set(FUZZER_AVAILABLE FALSE)
endif()

if(NOT FUZZER_AVAILABLE)
    return()
endif()

# Common fuzzing settings
set(FUZZ_COMMON_FLAGS
    -g
    -O1
    -fno-omit-frame-pointer
)

# ===========================================================================
# Unity Bundle Parser Fuzzer
# ===========================================================================

add_executable(fuzz_unity_bundle
    fuzz_unity_bundle.cpp
)

target_link_libraries(fuzz_unity_bundle PRIVATE
    makine_core
)

target_include_directories(fuzz_unity_bundle PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_options(fuzz_unity_bundle PRIVATE
    ${FUZZ_COMMON_FLAGS}
    ${FUZZER_FLAGS}
)

target_link_options(fuzz_unity_bundle PRIVATE
    ${FUZZER_FLAGS}
)

# ===========================================================================
# GameMaker Data Parser Fuzzer
# ===========================================================================

add_executable(fuzz_gamemaker_data
    fuzz_gamemaker_data.cpp
)

target_link_libraries(fuzz_gamemaker_data PRIVATE
    makine_core
)

target_include_directories(fuzz_gamemaker_data PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_options(fuzz_gamemaker_data PRIVATE
    ${FUZZ_COMMON_FLAGS}
    ${FUZZER_FLAGS}
)

target_link_options(fuzz_gamemaker_data PRIVATE
    ${FUZZER_FLAGS}
)

# ===========================================================================
# JSON Parser Fuzzer
# ===========================================================================

add_executable(fuzz_json_parser
    fuzz_json_parser.cpp
)

target_link_libraries(fuzz_json_parser PRIVATE
    makine_core
)

target_include_directories(fuzz_json_parser PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_options(fuzz_json_parser PRIVATE
    ${FUZZ_COMMON_FLAGS}
    ${FUZZER_FLAGS}
)

target_link_options(fuzz_json_parser PRIVATE
    ${FUZZER_FLAGS}
)

# ===========================================================================
# Translation Memory Fuzzer (string processing)
# ===========================================================================

add_executable(fuzz_translation_memory
    fuzz_translation_memory.cpp
)

target_link_libraries(fuzz_translation_memory PRIVATE
    makine_core
)

target_include_directories(fuzz_translation_memory PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_options(fuzz_translation_memory PRIVATE
    ${FUZZ_COMMON_FLAGS}
    ${FUZZER_FLAGS}
)

target_link_options(fuzz_translation_memory PRIVATE
    ${FUZZER_FLAGS}
)

# ===========================================================================
# Custom Target for All Fuzzers
# ===========================================================================

add_custom_target(fuzz_all
    DEPENDS
        fuzz_unity_bundle
        fuzz_gamemaker_data
        fuzz_json_parser
        fuzz_translation_memory
    COMMENT "Building all fuzzers"
)

# ===========================================================================
# Corpus Directories
# ===========================================================================

# Create corpus directories
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/corpus/unity_bundle)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/corpus/gamemaker_data)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/corpus/json)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/corpus/strings)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/corpus/paths)

# ===========================================================================
# Installation
# ===========================================================================

install(TARGETS
    fuzz_unity_bundle
    fuzz_gamemaker_data
    fuzz_json_parser
    fuzz_translation_memory
    DESTINATION bin/fuzz
    OPTIONAL
)

# Print status
message(STATUS "Fuzzing tests configured:")
message(STATUS "  - fuzz_unity_bundle")
message(STATUS "  - fuzz_gamemaker_data")
message(STATUS "  - fuzz_json_parser")
message(STATUS "  - fuzz_translation_memory")
message(STATUS "")
message(STATUS "Build with: cmake --build . --target fuzz_all")
message(STATUS "Run example: ./fuzz_unity_bundle corpus/unity_bundle -max_len=65536")
