# Phase 0: Foundation

**Status:** COMPLETE
**Date:** 2026-07-03


## Objective

Establish the load-bearing infrastructure that every future module depends on:
type system, error handling, logging, configuration, and build system.

## Why Foundation First

Unlike languages with rich standard libraries, C provides almost nothing out of the
box. No standard logging. No standard error handling beyond `errno`. No dynamic arrays.
You build all of it yourself.

If the foundation is wrong, every subsequent module pays the tax. Bad error handling
means silent bugs. No logging means blind debugging.

## What Was Built

### Build System

| File | Purpose |
|------|---------|
| `CMakeLists.txt` | CMake config: C17, /W4 warnings, Unicode, explicit source listing |
| `.gitignore` | Excludes build artifacts, IDE files, runtime data |

### Headers (`include/common/`)

| File | Purpose | Key Concepts |
|------|---------|-------------|
| `types.h` | Fixed-width type aliases (`u8`-`u64`, `f32`, `f64`, `usize`) | Platform-independent types via `<stdint.h>` |
| `macros.h` | `ACH_MIN/MAX`, `ACH_ARRAY_LENGTH`, `ACH_ALIGN_UP`, `ACH_KB/MB/GB` | Namespace-prefixed, double-evaluation warning |
| `errors.h` | `AchErrorCode` enum, `ach_error_to_string()`, `ACH_TRY` macro | Named error codes vs raw ints; error propagation |
| `config.h` | Version strings, path limits, buffer sizes | Compile-time constants, `_Static_assert` validation |
| `utils.h` | `utils_get_timestamp()` | Thread-safe time formatting |
| `logger.h` | `LOG_INFO/DEBUG/WARNING/ERROR/FATAL` macros | `__FILE__`/`__LINE__` capture, leveled logging |

### Implementation (`src/`)

| File | Purpose | Key Concepts |
|------|---------|-------------|
| `utils.c` | Timestamp formatting | `localtime_s()` (thread-safe), `snprintf()` (buffer-safe) |
| `logger.c` | Console coloring, optional file logging | `SetConsoleTextAttribute()`, module-private static state |
| `main.c` | Orchestrator: banner, init, log, shutdown, smart pause | `GetConsoleProcessList()` for pause detection |

## Dependency Graph

```
types.h          (root - no dependencies)
  |
  +-- macros.h
  +-- errors.h
  +-- utils.h
  +-- logger.h
  +-- config.h (also depends on macros.h)
  |
utils.c          (implements utils.h, uses config.h)
logger.c         (implements logger.h, uses config.h, utils.h, macros.h)
main.c           (uses all headers - leaf node)
```

No circular dependencies. Every arrow points downward.

## Design Decisions

### 1. Named Error Enum Over Raw Integers

**Problem:** Returning `-3` from a function tells the caller nothing.

**Solution:** `AchErrorCode` enum with descriptive names:
```c
ACH_ERROR_FILE_NOT_FOUND = 100
ACH_ERROR_OUT_OF_MEMORY  = 3
```

The `ACH_TRY` macro enables exception-like propagation:
```c
AchErrorCode init(void) {
    ACH_TRY(logger_init(&config));   // returns error if this fails
    ACH_TRY(scanner_init(&config));  // only runs if logger succeeded
    return ACH_SUCCESS;
}
```

### 2. Module-Private State (Not Globals)

The logger state is a `static` struct inside `logger.c`. No other file can access it.
This is C's equivalent of a private class — encapsulation without OOP.

### 3. Win32 Console Colors Over ANSI Escapes

ANSI escape codes require `SetConsoleMode(ENABLE_VIRTUAL_TERMINAL_PROCESSING)`, which
can fail on older Windows versions. `SetConsoleTextAttribute()` works universally from
Windows XP through Windows 11.

### 4. Smart Console Pause

`GetConsoleProcessList()` detects whether we created the console (double-click from
Explorer, count == 1) or inherited it (run from cmd/PowerShell, count >= 2).
We only pause in the former case, so developers aren't annoyed by unnecessary prompts.

### 5. Stack-Based Log Formatting

`logger_log()` formats messages on the stack (`char message[2048]`). No heap
allocation per log call. `malloc` costs ~100ns; stack allocation costs ~0ns.

## Build & Run

```
       Achilles Search Engine v0.1.0
       Build: Debug   | Jul  3 2026

[2026-07-03 22:42:37] [INFO ] Achilles Search Engine starting...
[2026-07-03 22:42:37] [INFO ] Version: 0.1.0
[2026-07-03 22:42:37] [DEBUG] Debug logging is enabled
[2026-07-03 22:42:37] [INFO ] Achilles Search Engine shutting down...
```

- **Compiler:** GCC 15.1.0
- **Warnings:** Zero
- **Errors:** Zero

## What's Next

**Phase 2: Dynamic Vector** — A growable array data structure needed by the
scanner (Phase 1) to store discovered files. Data structures before algorithms.
