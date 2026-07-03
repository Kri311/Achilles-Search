/* ==========================================================================
 * Achilles-Search | include/core/scanner.h
 * ==========================================================================
 * Filesystem scanner — recursive directory traversal.
 *
 * WHY THIS EXISTS:
 *   The scanner is the data acquisition layer. It walks a directory tree,
 *   discovers all files and folders, and collects their metadata (path,
 *   size, timestamps, attributes). Everything downstream — indexing,
 *   searching, monitoring — depends on the scanner's output.
 *
 * DESIGN:
 *   - Uses Win32 FindFirstFileExW / FindNextFileW for enumeration
 *   - DFS traversal with an explicit stack (no recursion, no stack overflow)
 *   - Skips reparse points (junction points/symlinks) to prevent loops
 *   - Graceful error handling: access-denied directories are logged, not fatal
 *   - All paths stored as wide strings (wchar_t*) for full Unicode support
 *
 * MEMORY OWNERSHIP:
 *   - scanner_init() allocates internal structures
 *   - scanner_scan() allocates a path string for each discovered entry
 *   - scanner_destroy() frees ALL path strings and internal structures
 *   - Pointers from scanner_get_results() are valid until scanner_destroy()
 *
 * THREAD SAFETY:
 *   Not thread-safe. A single Scanner must only be used from one thread.
 *   Phase 8 will add parallel scanning with per-thread scanners.
 *
 * LIFECYCLE:
 *   Scanner scanner;
 *   scanner_init(&scanner);
 *
 *   ScanConfig config = { .root_path = L"C:\\Users" };
 *   scanner_scan(&scanner, &config);
 *
 *   const ScanStats *stats = scanner_get_stats(&scanner);
 *   printf("Found %llu files\n", stats->files_found);
 *
 *   scanner_destroy(&scanner);
 * ========================================================================== */

#ifndef ACH_SCANNER_H
#define ACH_SCANNER_H

#include "common/types.h"
#include "common/errors.h"
#include "data/vector.h"

/* We need wchar_t for wide path strings */
#include <wchar.h>

/* ---- FileInfo ----------------------------------------------------------- */
/* Metadata for a single discovered file or directory.
 *
 * FIELDS:
 *   path          — Full absolute path (heap-allocated wchar_t string).
 *                   Owned by the Scanner. Valid until scanner_destroy().
 *
 *   file_size     — Size in bytes. 0 for directories. Supports files > 4 GB
 *                   (u64 range: up to 16 exabytes).
 *
 *   last_modified — Windows FILETIME as a 64-bit value.
 *                   100-nanosecond intervals since January 1, 1601 UTC.
 *                   This is the native Windows time format — no conversion
 *                   needed for NTFS metadata or Windows API calls.
 *
 *   attributes    — Raw FILE_ATTRIBUTE_* flags from WIN32_FIND_DATAW.
 *                   Preserves all attribute information: hidden, system,
 *                   readonly, compressed, encrypted, etc.
 *
 *   is_directory  — Convenience flag: true if FILE_ATTRIBUTE_DIRECTORY is set.
 *                   Avoids repeated bitmask checks in calling code.
 */
typedef struct FileInfo {
    wchar_t    *path;
    u64         file_size;
    u64         last_modified;
    u32         attributes;
    bool        is_directory;
} FileInfo;

/* ---- ScanStats ---------------------------------------------------------- */
/* Statistics collected during a scan operation. */
typedef struct ScanStats {
    u64     files_found;        /* Number of regular files discovered         */
    u64     dirs_found;         /* Number of directories discovered           */
    u64     total_size_bytes;   /* Sum of all file sizes                      */
    u64     errors;             /* Count of directories that couldn't be read */
    f64     duration_ms;        /* Wall-clock time for the scan               */
} ScanStats;

/* ---- ScanConfig --------------------------------------------------------- */
/* Configuration for a scan operation.
 *
 * root_path:
 *   The directory to start scanning from. Must be an absolute path.
 *   The scanner will recursively enumerate all entries under this path.
 *
 * max_depth:
 *   Maximum recursion depth. 0 = unlimited (up to ACH_SCANNER_MAX_DEPTH).
 *   Depth 1 = only the root directory's immediate children.
 *
 * include_hidden:
 *   If false, files/dirs with FILE_ATTRIBUTE_HIDDEN are skipped.
 *
 * include_system:
 *   If false, files/dirs with FILE_ATTRIBUTE_SYSTEM are skipped.
 */
typedef struct ScanConfig {
    const wchar_t  *root_path;
    u32             max_depth;
    bool            include_hidden;
    bool            include_system;
} ScanConfig;

/* ---- Scanner ------------------------------------------------------------ */
/* The scanner context. Holds results and state between calls. */
typedef struct Scanner {
    Vector      results;        /* Vector of FileInfo entries                 */
    ScanStats   stats;          /* Scan statistics                            */
    bool        initialized;    /* Guard against use-before-init              */
} Scanner;

/* ---- Lifecycle ---------------------------------------------------------- */

/* Initialize a scanner. Must be called before scanner_scan().
 *
 * Returns:
 *   ACH_SUCCESS           — Ready to use
 *   ACH_ERROR_INVALID_ARG — scanner is NULL
 *   ACH_ERROR_OUT_OF_MEMORY — failed to allocate results vector
 */
AchErrorCode scanner_init(Scanner *scanner);

/* Perform a recursive directory scan.
 *
 * This function enumerates all files and directories under config->root_path,
 * populating the scanner's results vector with FileInfo entries.
 *
 * Can be called multiple times — each call APPENDS to existing results.
 * Call scanner_clear_results() first if you want a fresh scan.
 *
 * Parameters:
 *   scanner — Initialized scanner
 *   config  — Scan configuration (root path, depth limit, filters)
 *
 * Returns:
 *   ACH_SUCCESS                   — Scan completed (some dirs may have errors)
 *   ACH_ERROR_INVALID_ARG         — scanner or config is NULL
 *   ACH_ERROR_SCANNER_INIT_FAILED — scanner not initialized
 *   ACH_ERROR_FILE_NOT_FOUND      — root_path does not exist
 *   ACH_ERROR_OUT_OF_MEMORY       — allocation failure during scan
 */
AchErrorCode scanner_scan(Scanner *scanner, const ScanConfig *config);

/* Free all resources: path strings, results vector, and stats.
 * Safe to call on an already-destroyed or never-initialized scanner.
 */
void scanner_destroy(Scanner *scanner);

/* Clear results and stats without destroying the scanner.
 * The scanner can be reused for another scan after this call.
 */
void scanner_clear_results(Scanner *scanner);

/* ---- Accessors ---------------------------------------------------------- */

/* Get a read-only view of the scan results.
 * Returns NULL if scanner is NULL or not initialized.
 * The returned pointer is valid until scanner_destroy() or scanner_clear_results().
 */
const Vector* scanner_get_results(const Scanner *scanner);

/* Get a read-only view of the scan statistics.
 * Returns NULL if scanner is NULL or not initialized.
 */
const ScanStats* scanner_get_stats(const Scanner *scanner);

#endif /* ACH_SCANNER_H */
