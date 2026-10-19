# xdelta3 VCDIFF decoder, vendored (Apache-2.0) — see README.md.
# Decoder only: the launcher applies deltas, it never makes them.

add_library(xdelta3 STATIC xdelta3.c)
target_include_directories(xdelta3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# xdelta3.h static_asserts these against the compiler, so every includer needs them.
target_compile_definitions(xdelta3 PUBLIC
    SIZEOF_SIZE_T=${CMAKE_SIZEOF_VOID_P}
    SIZEOF_UNSIGNED_LONG_LONG=8)
# Secondary compressors the legacy deltas use (DJW; FGK costs nothing extra).
target_compile_definitions(xdelta3 PRIVATE
    XD3_ENCODER=0
    XD3_MAIN=0
    SECONDARY_DJW=1
    SECONDARY_FGK=1
    SECONDARY_LZMA=0)

# xdelta3.h uses static_assert without including <assert.h>, which is where
# C11 defines it; this is that definition.
set_target_properties(xdelta3 PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
target_compile_definitions(xdelta3 PRIVATE static_assert=_Static_assert)
# Third-party code: keep the project's warning flags out of it.
target_compile_options(xdelta3 PRIVATE $<IF:$<C_COMPILER_ID:MSVC>,/w,-w>)
