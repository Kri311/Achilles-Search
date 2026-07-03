/* ==========================================================================
 * Achilles-Search | include/core/index.h
 * ==========================================================================
 * High-performance hierarchical in-memory filename index.
 * Maps files and directories to compact representations to save RAM.
 * ========================================================================== */

#ifndef ACH_INDEX_H
#define ACH_INDEX_H

#include "common/types.h"
#include "common/errors.h"
#include "data/vector.h"
#include "data/hashmap.h"
#include "core/scanner.h"

#define ACH_INDEX_ROOT_ID ((u32)-1)

/* Compact representation of a directory entry */
typedef struct IndexDir {
    wchar_t *name;          /* Directory name (e.g. "System32"), dynamically allocated */
    u32 parent_id;          /* Index of parent directory in Index.dirs, or ACH_INDEX_ROOT_ID */
} IndexDir;

/* Compact representation of a file entry */
typedef struct IndexFile {
    wchar_t *name;          /* Filename (e.g. "cmd.exe"), dynamically allocated */
    u32 parent_id;          /* Index of parent directory in Index.dirs */
    u64 file_size;          /* Size in bytes */
    u64 last_modified;      /* Last modification filetime */
    u32 attributes;         /* Win32 attributes */
} IndexFile;

/* In-memory index database */
typedef struct Index {
    Vector dirs;            /* Vector of IndexDir entries */
    Vector files;           /* Vector of IndexFile entries */
    bool initialized;
} Index;

/* ---- Lifecycle ----------------------------------------------------------- */

/* Initializes the index structure.
 * Returns:
 *   ACH_SUCCESS on success, error code on failure. */
AchErrorCode index_init(Index *index);

/* Destroys the index, freeing all allocated names and internal memory.
 * Idempotent. */
void index_destroy(Index *index);

/* Clears all entries in the index, freeing all name strings but keeping
 * allocated capacities. */
void index_clear(Index *index);

/* ---- Construction ---------------------------------------------------------- */

/* Adds a FileInfo result from the scanner to the index.
 * Handles directory decomposition and deduplication using a directory cache map.
 * Parameters:
 *   index   - Pointer to initialized Index.
 *   dir_map - Pointer to initialized HashMap used to cache directory paths to dir IDs.
 *             Keys in the map should be full paths (wchar_t*), values are u32 IDs.
 *   info    - Pointer to FileInfo from scanner.
 * Returns:
 *   ACH_SUCCESS on success, error code on failure. */
AchErrorCode index_add_file_info(Index *index, HashMap *dir_map, const FileInfo *info);

/* ---- Path Resolution ------------------------------------------------------ */

/* Reconstructs the full path of a file in the index.
 * Parameters:
 *   index    - Pointer to Index.
 *   file_idx - Index of the file entry.
 *   out_path - Destination buffer.
 *   max_len  - Maximum characters (including null terminator) of out_path.
 * Returns:
 *   ACH_SUCCESS on success, ACH_ERROR_BUFFER_TOO_SMALL, or other error codes. */
AchErrorCode index_get_file_path(const Index *index, u32 file_idx, wchar_t *out_path, usize max_len);

/* Reconstructs the full path of a directory in the index.
 * Parameters:
 *   index    - Pointer to Index.
 *   dir_idx  - Index of the directory entry.
 *   out_path - Destination buffer.
 *   max_len  - Maximum characters (including null terminator) of out_path.
 * Returns:
 *   ACH_SUCCESS on success, ACH_ERROR_BUFFER_TOO_SMALL, or other error codes. */
AchErrorCode index_get_dir_path(const Index *index, u32 dir_idx, wchar_t *out_path, usize max_len);

/* ---- Information --------------------------------------------------------- */

/* Returns the total number of files indexed. */
usize index_file_count(const Index *index);

/* Returns the total number of directories indexed. */
usize index_dir_count(const Index *index);

#endif /* ACH_INDEX_H */
