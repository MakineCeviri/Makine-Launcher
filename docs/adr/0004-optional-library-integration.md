# ADR-0004: Optional Library Integration Pattern

## Status

Accepted

## Date

2026-01-20

## Context

Makine-Launcher can benefit from several performance and feature libraries:
- **simdjson**: 10x faster JSON parsing
- **simdutf**: SIMD-accelerated UTF conversion
- **taskflow**: Parallel task execution
- **mio**: Memory-mapped I/O
- **libsodium**: Modern cryptography

However, requiring all these libraries would:
- Increase build complexity
- Make the project harder to compile on minimal systems
- Create dependency on libraries that may become unmaintained

We need these libraries to be **optional**—the core should work without them,
but use them when available for better performance.

## Decision

Implement a compile-time feature detection system with graceful fallback.

Pattern:
1. CMake detects library availability via `find_package(... QUIET)`
2. If found, define `MAKINE_HAS_<LIBRARY>` macro
3. Code uses `#ifdef` to choose optimized or fallback path
4. Runtime feature detection via `Features` struct

CMake:
```cmake
find_package(simdjson CONFIG QUIET)
if(TARGET simdjson::simdjson)
    target_link_libraries(makine_core PUBLIC simdjson::simdjson)
    target_compile_definitions(makine_core PUBLIC MAKINE_HAS_SIMDJSON)
endif()
```

Code:
```cpp
#ifdef MAKINE_HAS_SIMDJSON
    simdjson::dom::parser parser;
    auto doc = parser.parse(jsonStr);
#else
    auto doc = nlohmann::json::parse(jsonStr);
#endif
```

Abstraction layer (preferred):
```cpp
// json_utils.hpp - unified interface
auto doc = json::parseString(jsonStr);  // Uses best available backend
```

## Consequences

### Positive

- Core compiles with minimal dependencies
- Better performance when libraries available
- No forced upgrade path for users
- Clear feature matrix for documentation
- Easy to add new optional libraries

### Negative

- Code duplication (optimized + fallback paths)
- Testing must cover both paths
- Potential behavior differences between backends
- Feature detection logic scattered through codebase

### Neutral

- Build system complexity (CMake detection)
- Need to maintain abstraction layers

## Alternatives Considered

### Alternative 1: All Libraries Required

Make all performance libraries mandatory.

**Rejected because:**
- Barrier to entry for contributors
- Build failures if library unavailable
- vcpkg doesn't have all libraries on all platforms

### Alternative 2: Runtime Plugin System

Load libraries as plugins at runtime.

**Rejected because:**
- Significant complexity (dynamic loading)
- Performance overhead for plugin calls
- Deployment complexity
- Overkill for this use case

### Alternative 3: Build Variants

Multiple builds with different feature sets.

**Rejected because:**
- Build matrix explosion
- Testing burden
- User confusion about which build to use

## Related

- [ADR-0001](0001-native-cpp-architecture.md) - Core architecture

## Notes

Current optional library status:

| Library | Purpose | Detection Macro | Fallback |
|---------|---------|-----------------|----------|
| simdjson | Fast JSON | `MAKINE_HAS_SIMDJSON` | nlohmann-json |
| simdutf | UTF conversion | `MAKINE_HAS_SIMDUTF` | Standard library |
| taskflow | Parallel execution | `MAKINE_HAS_TASKFLOW` | std::async |
| mio | Memory mapping | `MAKINE_HAS_MIO` | std::ifstream |
| libsodium | Cryptography | `MAKINE_HAS_SODIUM` | OpenSSL |
| bit7z | 7-zip support | `MAKINE_HAS_BIT7Z` | None (feature disabled) |
| efsw | File watching | `MAKINE_HAS_EFSW` | Polling |
| SQLiteCpp | SQLite wrapper | `MAKINE_HAS_SQLITECPP` | Raw sqlite3 API |

Abstraction headers:
- `json_utils.hpp` - JSON parsing abstraction
- `parallel.hpp` - Parallel execution abstraction
- `mio_utils.hpp` - Memory-mapped I/O abstraction
