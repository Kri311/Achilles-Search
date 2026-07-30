# Achilles-Search Documentation

## Architecture

```
GUI
 |
Search Engine API
 |
Query Processor
 |
Search Engine
 |
Index Manager
 |
Scanner
 |
Windows File System
```

The GUI never directly interacts with the scanner or database.

## Development Phases

| Phase | Name | Status | Document |
|-------|------|--------|----------|
| 0 | Foundation | COMPLETE | [phase0_foundation.md](phase0_foundation.md) |
| 1 | Filesystem Scanner | COMPLETE | [phase1_scanner.md](phase1_scanner.md) |
| 2 | Dynamic Vector | COMPLETE | [phase2_vector.md](phase2_vector.md) |
| 3 | Hash Map | COMPLETE | [phase3_hashmap.md](phase3_hashmap.md) |
| 4 | Filename Index | COMPLETE | [phase4_index.md](phase4_index.md) |
| 5 | Persistent Database | COMPLETE | [phase5_database.md](phase5_database.md) |
| 6 | Search Engine | COMPLETE | [phase6_search.md](phase6_search.md) |
| 7 | Content Index | COMPLETE | [phase7_content.md](phase7_content.md) |
| 8 | Filesystem Monitoring | COMPLETE | [phase8_monitor.md](phase8_monitor.md) |
| 9 | GUI | COMPLETE | [phase9_gui.md](phase9_gui.md) |
| 10 | Optimization | COMPLETE | [phase10_optimization.md](phase10_optimization.md) |
| 11 | Release | COMPLETE | [phase11_release.md](phase11_release.md) |

## Module Map

| Module | Location | Purpose |
|--------|----------|---------|
| `common/` | `include/common/`, `src/common/` | Shared utilities: types, errors, logging, config |
| `core/` | `include/core/`, `src/core/` | Business logic: scanner, indexer, parser, search |
| `data/` | `include/data/`, `src/data/` | Data structures: vector, hashmap, trie, queue, arena |
| `storage/` | `include/storage/`, `src/storage/` | Persistence: serializer, database, memory mapping |
| `platform/` | `include/platform/`, `src/platform/` | Windows APIs: filesystem, threading, monitoring |
| `gui/` | `include/gui/`, `src/gui/` | Win32 graphical interface |

## Building

```bash
# Configure (from project root)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build

# Run
.\build\bin\AchillesSearch.exe

# Run tests
.\build\bin\test_vector.exe
```

## Coding Standards

- **Language:** C17
- **Indentation:** 4 spaces (no tabs)
- **Brace style:** K&R
- **Naming:** `snake_case` functions, `PascalCase` structs/enums, `UPPER_CASE` macros
- **Prefix:** `ACH_` for macros, `ach_` for global functions
- **Error handling:** Every public function returns `AchErrorCode`
- **Memory:** The module that allocates is responsible for freeing
