# ADR-0002: Result-Based Error Handling

## Status

Accepted

## Date

2026-01-15

## Context

Error handling is critical for Makine-Launcher because:
- Operations can fail in many ways (file not found, permission denied, corrupt data)
- Failed operations must not corrupt game files
- Users need clear error messages for troubleshooting
- Errors must propagate cleanly through call stacks

Traditional C++ error handling approaches have issues:
- Exceptions: Performance overhead, hard to reason about control flow
- Error codes: Easy to ignore, verbose checking
- errno/GetLastError: Global state, thread-unsafe

## Decision

Use a `Result<T>` type based on `std::expected<T, Error>` (C++23) with fallback
to a custom implementation for C++20.

```cpp
template<typename T>
using Result = std::expected<T, Error>;

using VoidResult = Result<void>;
```

The `Error` class provides:
- Error code enumeration
- Human-readable message
- Context chain (file, game, details)
- JSON serialization for logging
- Recovery suggestions

Usage:
```cpp
Result<GameInfo> detectGame(const fs::path& path) {
    if (!fs::exists(path)) {
        return Error(ErrorCode::NotFound, "Game directory not found")
            .withFile(path);
    }
    // ...
    return game;
}

// Caller
auto result = detectGame(path);
if (!result) {
    logger->error("Detection failed: {}", result.error().detailedMessage());
    return result.error();
}
auto game = *result;
```

## Consequences

### Positive

- Errors cannot be silently ignored (compiler warns on unused Result)
- Clear control flow (no hidden exception jumps)
- Rich error context for debugging
- Zero-cost when operation succeeds
- Composable with `and_then()`, `or_else()`, `transform()`

### Negative

- More verbose than exceptions for simple cases
- Every function signature must declare Result return
- Propagation requires explicit handling at each level

### Neutral

- Team must learn Result pattern (common in Rust, gaining popularity in C++)
- Testing requires checking both success and error paths

## Alternatives Considered

### Alternative 1: C++ Exceptions

Use standard exception handling with `try`/`catch`.

**Rejected because:**
- Performance overhead (even when not thrown, exception tables)
- Hidden control flow makes reasoning difficult
- Hard to ensure RAII cleanup in all exception paths
- Game modding community prefers explicit error handling

### Alternative 2: Error Codes (int/enum)

Return error codes and use output parameters for values.

```cpp
ErrorCode detectGame(const fs::path& path, GameInfo* out);
```

**Rejected because:**
- Easy to ignore return value
- Verbose (need separate out parameter)
- Can't return complex error information
- No type safety for specific errors

### Alternative 3: std::optional + Logging

Return `std::optional<T>` and log errors internally.

**Rejected because:**
- Caller doesn't know why operation failed
- Can't make decisions based on error type
- Testing error conditions difficult

## Related

- [ADR-0001](0001-native-cpp-architecture.md) - C++ architecture decision

## Notes

The `Error` class supports method chaining for rich context:

```cpp
return Error(ErrorCode::ParseError, "Invalid Unity bundle header")
    .withFile(bundlePath)
    .withGame(game.id.full())
    .withDetail("expected_magic", "UnityFS")
    .withDetail("actual_magic", actualMagic);
```

Recovery suggestions are provided via `getSuggestions()`:

```cpp
auto suggestions = getSuggestions(error.code());
for (const auto& s : suggestions) {
    logger->info("Try: {}", s.action);
}
```
