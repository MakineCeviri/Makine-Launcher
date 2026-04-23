# ADR-0001: Native C++ Architecture

## Status

Accepted

## Date

2026-01-01

## Context

Makine-Launcher requires:
- High-performance file parsing (game assets can be hundreds of MB)
- Low-level binary manipulation for patching
- Cross-platform filesystem operations
- Integration with game-specific formats (Unity bundles, Unreal paks)

The original v0.0.8 was built with Flutter + Dart FFI calling into C++.
This architecture had several issues:
- FFI overhead for frequent calls
- Complex build system (Dart + CMake + vcpkg)
- Debugging difficulties across language boundary
- Limited by Dart's type system for binary operations

## Decision

Rebuild Makine-Launcher with a pure C++20/23 core library and Qt6/QML for the UI.

Architecture:
```
┌─────────────────────────────────┐
│          Qt6/QML UI             │
├─────────────────────────────────┤
│      Qt C++ Service Layer       │
├─────────────────────────────────┤
│      Makine-Launcher C++ Core    │
│  ┌────────────────────────────┐ │
│  │ Handlers │ Parsers │ TM    │ │
│  ├────────────────────────────┤ │
│  │ Security │ Patch │ Package │ │
│  └────────────────────────────┘ │
└─────────────────────────────────┘
```

- C++20 minimum, C++23 where available
- Static library (`makine_core.lib`) linked directly
- Qt6 for UI (QML for modern, declarative interface)
- vcpkg for dependency management

## Consequences

### Positive

- No FFI overhead - direct function calls
- Single build system (CMake)
- Full C++ debugging capabilities
- Better type safety with `std::expected` and concepts
- Access to modern C++ features (ranges, coroutines)
- Native performance for all operations

### Negative

- Longer compile times than interpreted languages
- Need for careful memory management
- Platform-specific code for some operations (Windows registry)
- Qt dependency for UI (but Qt is mature and stable)

### Neutral

- Team must be proficient in modern C++
- Different testing approach (Google Test vs Flutter test)

## Alternatives Considered

### Alternative 1: Flutter + Rust FFI

Use Rust for the core library instead of C++.

**Rejected because:**
- Existing C++ codebase would need rewrite
- Game modding community predominantly uses C++
- Unity/Unreal SDK documentation is C++ focused
- Rust's learning curve for team members

### Alternative 2: Electron + Node.js

Use JavaScript/TypeScript with native modules.

**Rejected because:**
- Memory overhead unacceptable for large asset parsing
- JavaScript's weak typing problematic for binary protocols
- Native module compilation complexity
- Poor performance for CPU-intensive operations

### Alternative 3: Keep Flutter + C++ FFI

Continue with the existing architecture.

**Rejected because:**
- FFI overhead measured at 15-20% for frequent calls
- Build system complexity causing CI failures
- Debugging across Dart/C++ boundary very difficult
- Dart's async model incompatible with some C++ patterns

## Related

- [ADR-0002](0002-result-based-error-handling.md) - Error handling in C++

## Notes

The migration from Flutter to Qt/QML was completed in January 2026.
The old Flutter version is archived at `archive/v0.0.8-flutter/`.

Performance benchmarks show:
- 40% faster asset parsing
- 60% reduction in memory usage for large files
- Build times reduced from 8min to 3min
