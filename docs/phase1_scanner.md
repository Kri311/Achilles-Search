# Phase 1: Filesystem Scanner

**Status:** IN PROGRESS
**Date:** 2026-07-03


## Objective

Build a recursive directory scanner that traverses a filesystem tree, discovers
all files and folders, and collects their metadata. This is the data acquisition
layer — everything downstream (indexing, searching) depends on it.

## Why This Is Hard

"Just list all the files" sounds trivial. It's not. Real-world filesystems have:

- **Millions of entries** — C:\Users on a developer machine easily has 1M+ files
- **Deep nesting** — node_modules, .git, build artifacts can nest 100+ levels deep
- **Access denied** — System folders, other user profiles, encrypted directories
- **Reparse points** — Junction points and symlinks can create infinite loops
- **Unicode paths** — Filenames can contain any Unicode character (emoji, CJK, etc.)
- **Long paths** — Paths > 260 characters exist and are increasingly common

A production scanner handles all of these gracefully.

## Key Design Decisions

### 1. Win32 API: FindFirstFileW / FindNextFileW

Windows provides several ways to enumerate directories:

| API | Speed | Documentation | Complexity |
|-----|-------|---------------|------------|
| `FindFirstFileW` / `FindNextFileW` | Good | Excellent | Low |
| `FindFirstFileExW` with `FindExInfoBasic` | Better | Good | Low |
| `NtQueryDirectoryFile` | Fastest | Poor (ntdll) | High |
| `GetFileInformationByHandleEx` | Fast | Good | Medium |

We use `FindFirstFileExW` with `FindExInfoBasic` and `FIND_FIRST_EX_LARGE_FETCH`:
- `FindExInfoBasic` skips the short filename (8.3 name) — saves ~30% time
- `FIND_FIRST_EX_LARGE_FETCH` hints the OS to use larger buffers internally

In Phase 10 we can switch to `NtQueryDirectoryFile` for maximum performance.

### 2. DFS with Explicit Stack (Not Recursion)

**Why not recursive function calls**

Each recursive call consumes ~1-4 KB of stack space. With 128 levels of nesting
(our ACH_SCANNER_MAX_DEPTH), that's 128-512 KB of stack. The default Windows
thread stack is 1 MB. A deeply nested `node_modules` tree can overflow it.

**Solution:** Use a `Vector` as an explicit stack. Push directory paths to visit,
pop one at a time. The stack lives on the heap and can grow dynamically. Stack
depth is limited only by available memory, not thread stack size.

```
DFS with explicit stack:

   dir_stack: [root]
   
   Pop "C:\Users" → enumerate contents
     Push "C:\Users\Desktop"
     Push "C:\Users\Documents"
     Record file entries...
   
   Pop "C:\Users\Documents" → enumerate contents
     Push "C:\Users\Documents\Projects"
     Record file entries...
   
   Pop "C:\Users\Documents\Projects" → enumerate contents
     Record file entries...
   
   Pop "C:\Users\Desktop" → enumerate contents
     Record file entries...
   
   Stack empty → done
```

### 3. Wide Strings (wchar_t)

Windows paths are UTF-16 internally. The Win32 API has two variants of every function:
- `FindFirstFileA` — ANSI (narrow char, locale-dependent, lossy)
- `FindFirstFileW` — Wide (wchar_t, full Unicode, lossless)

We use the W variants exclusively. A filename like "日本語.txt" or "résumé.pdf"
works correctly. The A variants would mangle these characters.

### 4. Skip Reparse Points

Reparse points (junction points, symlinks, mount points) can point anywhere —
including parent directories, creating infinite loops. We detect them via the
`FILE_ATTRIBUTE_REPARSE_POINT` flag and skip them.

### 5. Graceful Error Handling

When `FindFirstFileW` returns `INVALID_HANDLE_VALUE` (access denied, path not
found, etc.), we:
1. Increment the error counter
2. Log a warning
3. Continue scanning other directories

We never abort the entire scan because of one inaccessible directory.

## Data Structures

### FileInfo — Per-File Metadata

```c
typedef struct FileInfo {
    wchar_t *path;          // Full path (heap-allocated, owned by scanner)
    u64      file_size;     // Size in bytes (0 for directories)
    u64      last_modified; // Windows FILETIME as 64-bit value
    u32      attributes;    // FILE_ATTRIBUTE_* flags
    bool     is_directory;  // Convenience: (attributes & DIR) != 0
} FileInfo;
```

**Memory ownership:** The Scanner allocates each `path` string. When the Scanner
is destroyed, it iterates all FileInfo entries and frees their paths before
destroying the results Vector.

### ScanStats — Scan Metrics

```c
typedef struct ScanStats {
    u64  files_found;       // Regular file count
    u64  dirs_found;        // Directory count
    u64  total_size_bytes;  // Sum of all file sizes
    u64  errors;            // Access denied, etc.
    f64  duration_ms;       // Wall clock time for the scan
} ScanStats;
```

## API

```c
AchErrorCode scanner_init(Scanner *scanner);
AchErrorCode scanner_scan(Scanner *scanner, const ScanConfig *config);
void         scanner_destroy(Scanner *scanner);

// Accessors (read-only views — caller does not own these)
const Vector*    scanner_get_results(const Scanner *scanner);
const ScanStats* scanner_get_stats(const Scanner *scanner);
```

## Complexity

| Operation | Time | Space |
|-----------|------|-------|
| Full scan | O(N) where N = total entries | O(N) for results + O(D) for stack where D = max depth |
| Per entry | O(1) amortized (vector push) | O(path_length) per path string |

## Files

| File | Action |
|------|--------|
| `include/core/scanner.h` | Create — FileInfo, ScanConfig, Scanner structs, API |
| `src/core/scanner.c` | Create — DFS traversal with Win32 APIs |
| `tests/test_scanner.c` | Create — scan project directory, verify results |
| `CMakeLists.txt` | Update — add scanner.c, test target |
| `src/main.c` | Update — demo scan of current directory |
