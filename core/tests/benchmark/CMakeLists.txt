# Makine Benchmark Tests
# Copyright (c) 2026 Makine Team
#
# Requires: Google Benchmark library
# Install: vcpkg install benchmark
#
# Build:
#   cmake -B build -DBUILD_BENCHMARKS=ON
#   cmake --build build --target benchmarks
#
# Run:
#   ./build/bench_json_parsing --benchmark_repetitions=3
#   ./build/bench_translation_memory --benchmark_format=console

cmake_minimum_required(VERSION 3.28)

# Find Google Benchmark
find_package(benchmark CONFIG QUIET)

if(NOT benchmark_FOUND)
    message(STATUS "Google Benchmark not found - benchmarks will not be built")
    message(STATUS "Install with: vcpkg install benchmark")
    return()
endif()

message(STATUS "Google Benchmark found - building benchmarks")

# Common benchmark settings
set(BENCHMARK_COMMON_FLAGS
    -DBENCHMARK_STATIC_DEFINE
)

# ===========================================================================
# JSON Parsing Benchmark
# ===========================================================================

add_executable(bench_json_parsing
    bench_json_parsing.cpp
)

target_link_libraries(bench_json_parsing PRIVATE
    makine_core
    benchmark::benchmark
    benchmark::benchmark_main
)

target_include_directories(bench_json_parsing PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(bench_json_parsing PRIVATE
    ${BENCHMARK_COMMON_FLAGS}
)

# ===========================================================================
# Translation Memory Benchmark
# ===========================================================================

add_executable(bench_translation_memory
    bench_translation_memory.cpp
)

target_link_libraries(bench_translation_memory PRIVATE
    makine_core
    benchmark::benchmark
    benchmark::benchmark_main
)

target_include_directories(bench_translation_memory PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(bench_translation_memory PRIVATE
    ${BENCHMARK_COMMON_FLAGS}
)

# ===========================================================================
# Custom Target for All Benchmarks
# ===========================================================================

add_custom_target(benchmarks
    DEPENDS
        bench_json_parsing
        bench_translation_memory
    COMMENT "Building all benchmarks"
)

# ===========================================================================
# Benchmark Run Target
# ===========================================================================

add_custom_target(run_benchmarks
    COMMAND $<TARGET_FILE:bench_json_parsing> --benchmark_format=console --benchmark_repetitions=3
    COMMAND $<TARGET_FILE:bench_translation_memory> --benchmark_format=console --benchmark_repetitions=3
    DEPENDS benchmarks
    COMMENT "Running all benchmarks"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# ===========================================================================
# Benchmark JSON Output Target (for CI)
# ===========================================================================

add_custom_target(benchmark_json
    COMMAND $<TARGET_FILE:bench_json_parsing>
        --benchmark_format=json
        --benchmark_out=${CMAKE_BINARY_DIR}/bench_json_parsing.json
    COMMAND $<TARGET_FILE:bench_translation_memory>
        --benchmark_format=json
        --benchmark_out=${CMAKE_BINARY_DIR}/bench_translation_memory.json
    DEPENDS benchmarks
    COMMENT "Running benchmarks with JSON output"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# ===========================================================================
# Installation
# ===========================================================================

install(TARGETS
    bench_json_parsing
    bench_translation_memory
    DESTINATION bin/benchmarks
    OPTIONAL
)

# Print status
message(STATUS "Benchmarks configured:")
message(STATUS "  - bench_json_parsing")
message(STATUS "  - bench_translation_memory")
message(STATUS "")
message(STATUS "Build with: cmake --build . --target benchmarks")
message(STATUS "Run with:   cmake --build . --target run_benchmarks")
