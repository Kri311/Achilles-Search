/* ==========================================================================
 * Achilles-Search | src/core/scanner.c
 * ==========================================================================
 * Filesystem scanner implementation.
 *
 * ALGORITHM: Iterative Depth-First Search
 *
 *   We maintain an explicit stack of (directory_path, depth) pairs.
 *   For each directory popped from the stack:
 *     1. Open it with FindFirstFileExW(path + "\\*")
 *     2. For each entry (FindNextFileW loop):
 *        a. Skip "." and ".."
 *        b. Skip reparse points (junction points, symlinks → avoids loops)
 *        c. Apply filters (hidden, system)
 *        d. Build full path = dir + "\\" + filename
 *        e. Create FileInfo, push to results vector
 *        f. If it's a directory and within depth limit, push to dir_stack
 *     3. Close handle with FindClose
 *     4. Free the popped directory path
 *
 * PERFORMANCE NOTES:
 *   - FindFirstFileExW with FindExInfoBasic skips 8.3 name lookup (~30% faster)
 *   - FIND_FIRST_EX_LARGE_FETCH uses larger OS-internal buffers
 *   - All paths are heap-allocated (arena allocator will optimize this later)
 *   - The results vector pre-allocates for common case
 *
 * ERROR HANDLING:
 *   Individual directory access failures are logged and counted but never
 *   abort the scan. This is critical: a scan of C:\ will hit many protected
 *   directories (System Volume Information, etc.) and must continue past them.
 * ========================================================================== */

#include "core/scanner.h"
#include "common/config.h"
#include "common/macros.h"
#include "common/logger.h"

#include <stdlib.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* ---- DFS Stack Entry ---------------------------------------------------- */
/* Each entry on the traversal stack holds a directory path and its depth.
 * The depth is tracked to enforce max_depth limits.
 */
typedef struct DirStackEntry {
    wchar_t    *path;       /* Heap-allocated directory path (owned)          */
    u32         depth;      /* Depth relative to root (root children = 1)    */
} DirStackEntry;

/* ---- Internal Path Helpers ---------------------------------------------- */

/* Join a directory path and a filename into a full path.
 *
 * Returns a heap-allocated wide string, or NULL on failure.
 * The caller is responsible for freeing the returned string.
 *
 * Example:
 *   path_join_w(L"C:\\Users", L"Desktop") → L"C:\\Users\\Desktop"
 *   path_join_w(L"C:\\Users\\", L"Desktop") → L"C:\\Users\\Desktop"
 */
static wchar_t* path_join_w(const wchar_t *dir, const wchar_t *name) {
    if (dir == NULL || name == NULL) return NULL;

    usize dir_len = wcslen(dir);
    usize name_len = wcslen(name);

    /* Check if dir already ends with a backslash */
    bool has_sep = (dir_len > 0 && dir[dir_len - 1] == L'\\');

    /* Total: dir + optional separator + name + null terminator */
    usize total_len = dir_len + (has_sep  0 : 1) + name_len + 1;

    /* Overflow check */
    if (total_len < dir_len) return NULL;  /* Would overflow */

    wchar_t *result = (wchar_t*)malloc(total_len * sizeof(wchar_t));
    if (result == NULL) return NULL;

    /* Build the path: copy dir, append separator if needed, append name */
    memcpy(result, dir, dir_len * sizeof(wchar_t));

    usize pos = dir_len;
    if (!has_sep) {
        result[pos++] = L'\\';
    }

    memcpy(result + pos, name, name_len * sizeof(wchar_t));
    pos += name_len;
    result[pos] = L'\0';

    return result;
}

/* Duplicate a wide string (like _wcsdup but using standard malloc).
 * Returns a heap-allocated copy, or NULL on failure.
 */
static wchar_t* wstr_dup(const wchar_t *src) {
    if (src == NULL) return NULL;

    usize len = wcslen(src) + 1;  /* +1 for null terminator */
    wchar_t *copy = (wchar_t*)malloc(len * sizeof(wchar_t));
    if (copy == NULL) return NULL;

    memcpy(copy, src, len * sizeof(wchar_t));
    return copy;
}

/* Convert a WIN32_FIND_DATAW's file size fields into a single u64.
 * NTFS stores file sizes as a pair of 32-bit values (high and low).
 * We combine them into one 64-bit value.
 */
static inline u64 make_file_size(DWORD high, DWORD low) {
    return ((u64)high << 32) | (u64)low;
}

/* Convert a FILETIME struct into a u64.
 * FILETIME is two 32-bit values representing 100ns intervals since 1601.
 */
static inline u64 filetime_to_u64(FILETIME ft) {
    return ((u64)ft.dwHighDateTime << 32) | (u64)ft.dwLowDateTime;
}

/* Check if a path is a "." or ".." pseudo-directory entry.
 * Every directory enumeration returns these; we must skip them or
 * we'd loop infinitely (. = self, .. = parent).
 */
static inline bool is_dot_dir(const wchar_t *name) {
    if (name[0] == L'.') {
        if (name[1] == L'\0') return true;                  /* "."  */
        if (name[1] == L'.' && name[2] == L'\0') return true; /* ".." */
    }
    return false;
}

/* ---- Free Helpers ------------------------------------------------------- */

/* Free all FileInfo path strings in the results vector.
 * Must be called before vector_destroy() to avoid leaking path strings.
 *
 * WHY THIS IS NEEDED:
 *   The Vector stores FileInfo structs by value (memcpy). Each FileInfo
 *   contains a wchar_t* pointer to a heap-allocated path string. When
 *   the Vector is destroyed, it frees its buffer — but the buffer only
 *   contains the FileInfo structs (which include the pointer values),
 *   not the strings those pointers point to. Without this cleanup,
 *   every path string would leak.
 */
static void free_all_file_info_paths(Vector *results) {
    usize len = vector_length(results);
    for (usize i = 0; i < len; i++) {
        FileInfo *info = (FileInfo*)vector_get(results, i);
        if (info != NULL && info->path != NULL) {
            free(info->path);
            info->path = NULL;
        }
    }
}

/* Free all remaining entries on the DFS directory stack.
 * Called during cleanup if the scan is interrupted or on destroy.
 */
static void free_dir_stack(Vector *dir_stack) {
    usize len = vector_length(dir_stack);
    for (usize i = 0; i < len; i++) {
        DirStackEntry *entry = (DirStackEntry*)vector_get(dir_stack, i);
        if (entry != NULL && entry->path != NULL) {
            free(entry->path);
            entry->path = NULL;
        }
    }
}

/* ---- High-Resolution Timer ---------------------------------------------- */

/* Get current time in milliseconds using QueryPerformanceCounter.
 *
 * WHY NOT clock() or GetTickCount64()
 *   - clock() measures CPU time, not wall time (wrong for I/O-bound work)
 *   - GetTickCount64() has ~15ms resolution (too coarse for fast scans)
 *   - QueryPerformanceCounter has sub-microsecond resolution
 */
static f64 get_time_ms(void) {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return ((f64)counter.QuadPart / (f64)freq.QuadPart) * 1000.0;
}

/* ---- Public API Implementation ------------------------------------------ */

AchErrorCode scanner_init(Scanner *scanner) {
    if (scanner == NULL) {
        return ACH_ERROR_INVALID_ARG;
    }

    /* Initialize results vector with space for ~4096 entries.
     * This covers most directories without early reallocations.
     * For larger scans, the vector grows automatically. */
    AchErrorCode err = vector_init(&scanner->results, sizeof(FileInfo), 4096);
    if (ACH_FAILED(err)) {
        return ACH_ERROR_OUT_OF_MEMORY;
    }

    /* Zero out statistics */
    memset(&scanner->stats, 0, sizeof(ScanStats));

    scanner->initialized = true;
    return ACH_SUCCESS;
}

void scanner_destroy(Scanner *scanner) {
    if (scanner == NULL) {
        return;
    }

    if (scanner->initialized) {
        /* Free all heap-allocated path strings BEFORE destroying the vector */
        free_all_file_info_paths(&scanner->results);
        vector_destroy(&scanner->results);
    }

    memset(&scanner->stats, 0, sizeof(ScanStats));
    scanner->initialized = false;
}

void scanner_clear_results(Scanner *scanner) {
    if (scanner == NULL || !scanner->initialized) {
        return;
    }

    /* Free path strings, then clear the vector (keeps capacity) */
    free_all_file_info_paths(&scanner->results);
    vector_clear(&scanner->results);
    memset(&scanner->stats, 0, sizeof(ScanStats));
}

AchErrorCode scanner_scan(Scanner *scanner, const ScanConfig *config) {
    /* ---- Validate inputs ------------------------------------------------ */
    if (scanner == NULL || config == NULL) {
        return ACH_ERROR_INVALID_ARG;
    }

    if (!scanner->initialized) {
        return ACH_ERROR_SCANNER_INIT_FAILED;
    }

    if (config->root_path == NULL || wcslen(config->root_path) == 0) {
        return ACH_ERROR_INVALID_ARG;
    }

    /* ---- Validate root path exists -------------------------------------- */
    DWORD root_attrs = GetFileAttributesW(config->root_path);
    if (root_attrs == INVALID_FILE_ATTRIBUTES) {
        LOG_ERROR("Scanner: root path does not exist or is inaccessible");
        return ACH_ERROR_FILE_NOT_FOUND;
    }
    if (!(root_attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        LOG_ERROR("Scanner: root path is not a directory");
        return ACH_ERROR_INVALID_ARG;
    }

    /* ---- Determine effective max depth ---------------------------------- */
    u32 max_depth = config->max_depth;
    if (max_depth == 0) {
        max_depth = ACH_SCANNER_MAX_DEPTH;
    }

    /* ---- Initialize DFS stack ------------------------------------------- */
    Vector dir_stack;
    AchErrorCode err = vector_init(&dir_stack, sizeof(DirStackEntry), 256);
    if (ACH_FAILED(err)) {
        return ACH_ERROR_OUT_OF_MEMORY;
    }

    /* Push the root directory onto the stack */
    wchar_t *root_copy = wstr_dup(config->root_path);
    if (root_copy == NULL) {
        vector_destroy(&dir_stack);
        return ACH_ERROR_OUT_OF_MEMORY;
    }

    DirStackEntry root_entry = { .path = root_copy, .depth = 0 };
    err = vector_push(&dir_stack, &root_entry);
    if (ACH_FAILED(err)) {
        free(root_copy);
        vector_destroy(&dir_stack);
        return ACH_ERROR_OUT_OF_MEMORY;
    }

    /* ---- Start timer ---------------------------------------------------- */
    f64 start_time = get_time_ms();

    LOG_INFO("Scanner: starting scan of root path");
    LOG_DEBUG("Scanner: max_depth=%u, include_hidden=%d, include_system=%d",
              max_depth, config->include_hidden, config->include_system);

    /* ---- DFS Main Loop -------------------------------------------------- */
    while (!vector_is_empty(&dir_stack)) {
        /* Pop the top directory from the stack */
        DirStackEntry current;
        vector_pop(&dir_stack, &current);

        /* Build the search pattern: dir + "\\*" */
        wchar_t *search_pattern = path_join_w(current.path, L"*");
        if (search_pattern == NULL) {
            LOG_WARNING("Scanner: failed to build search pattern, skipping directory");
            free(current.path);
            scanner->stats.errors++;
            continue;
        }

        /* ---- Open directory for enumeration ----------------------------- */
        WIN32_FIND_DATAW find_data;
        HANDLE find_handle = FindFirstFileExW(
            search_pattern,
            FindExInfoBasic,        /* Skip 8.3 short names (~30% faster)   */
            &find_data,
            FindExSearchNameMatch,
            NULL,
            FIND_FIRST_EX_LARGE_FETCH  /* Use larger OS internal buffers   */
        );

        free(search_pattern);  /* No longer needed after FindFirstFileExW */

        if (find_handle == INVALID_HANDLE_VALUE) {
            DWORD win_err = GetLastError();
            if (win_err == ERROR_ACCESS_DENIED) {
                LOG_DEBUG("Scanner: access denied, skipping directory");
            } else if (win_err != ERROR_FILE_NOT_FOUND) {
                LOG_WARNING("Scanner: FindFirstFileExW failed (error %lu)",
                            (unsigned long)win_err);
            }
            free(current.path);
            scanner->stats.errors++;
            continue;
        }

        /* ---- Enumerate entries in this directory ------------------------ */
        do {
            const wchar_t *entry_name = find_data.cFileName;

            /* Skip "." and ".." — always present, never useful */
            if (is_dot_dir(entry_name)) {
                continue;
            }

            DWORD attrs = find_data.dwFileAttributes;

            /* Skip reparse points (junction points, symlinks).
             * Following these can cause infinite loops if they point to
             * ancestor directories. */
            if (attrs & FILE_ATTRIBUTE_REPARSE_POINT) {
                continue;
            }

            /* Apply hidden/system filters */
            if (!config->include_hidden && (attrs & FILE_ATTRIBUTE_HIDDEN)) {
                continue;
            }
            if (!config->include_system && (attrs & FILE_ATTRIBUTE_SYSTEM)) {
                continue;
            }

            /* Build full path for this entry */
            wchar_t *full_path = path_join_w(current.path, entry_name);
            if (full_path == NULL) {
                scanner->stats.errors++;
                continue;
            }

            /* Create FileInfo entry */
            bool is_dir = (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
            FileInfo info = {
                .path          = full_path,  /* Ownership transferred to results */
                .file_size     = is_dir  0 : make_file_size(
                                     find_data.nFileSizeHigh,
                                     find_data.nFileSizeLow),
                .last_modified = filetime_to_u64(find_data.ftLastWriteTime),
                .attributes    = attrs,
                .is_directory  = is_dir,
            };

            /* Push to results vector */
            err = vector_push(&scanner->results, &info);
            if (ACH_FAILED(err)) {
                free(full_path);
                LOG_ERROR("Scanner: out of memory storing results");
                FindClose(find_handle);
                free(current.path);
                free_dir_stack(&dir_stack);
                vector_destroy(&dir_stack);
                scanner->stats.duration_ms = get_time_ms() - start_time;
                return ACH_ERROR_OUT_OF_MEMORY;
            }

            /* Update statistics */
            if (is_dir) {
                scanner->stats.dirs_found++;

                /* Push subdirectory onto the stack for later traversal
                 * (only if within depth limit) */
                if (current.depth + 1 < max_depth) {
                    wchar_t *dir_copy = wstr_dup(full_path);
                    if (dir_copy != NULL) {
                        DirStackEntry sub = {
                            .path = dir_copy,
                            .depth = current.depth + 1
                        };
                        err = vector_push(&dir_stack, &sub);
                        if (ACH_FAILED(err)) {
                            free(dir_copy);
                            scanner->stats.errors++;
                        }
                    } else {
                        scanner->stats.errors++;
                    }
                }
            } else {
                scanner->stats.files_found++;
                scanner->stats.total_size_bytes += info.file_size;
            }

        } while (FindNextFileW(find_handle, &find_data));

        /* ---- Close directory handle ------------------------------------- */
        FindClose(find_handle);

        /* Free the directory path we popped from the stack */
        free(current.path);
    }

    /* ---- Finalize ------------------------------------------------------- */
    scanner->stats.duration_ms = get_time_ms() - start_time;

    LOG_INFO("Scanner: scan complete - %llu files, %llu dirs in %.2f ms",
             (unsigned long long)scanner->stats.files_found,
             (unsigned long long)scanner->stats.dirs_found,
             scanner->stats.duration_ms);

    /* Clean up the (now empty) directory stack */
    vector_destroy(&dir_stack);

    return ACH_SUCCESS;
}

/* ---- Accessors ---------------------------------------------------------- */

const Vector* scanner_get_results(const Scanner *scanner) {
    if (scanner == NULL || !scanner->initialized) {
        return NULL;
    }
    return &scanner->results;
}

const ScanStats* scanner_get_stats(const Scanner *scanner) {
    if (scanner == NULL || !scanner->initialized) {
        return NULL;
    }
    return &scanner->stats;
}
