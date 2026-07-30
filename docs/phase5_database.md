# Phase 5: Persistent Database Design Specification

This document details the architecture, file format, and serialization/deserialization algorithms for the **Persistent Database** in Achilles-Search. The database layer enables fast saving and loading of the filename index to a custom binary file format on disk.


## 1. Architectural Decisions

### 1.1 Custom Fast Binary Serialization
Rather than using bulky text-based formats (like JSON or XML) or introducing a large dependency (like SQLite), Achilles-Search uses a custom-tailored binary serialization layout.
*   **Zero-Parsing Cost:** The structures are written and read sequentially. No tokenization, parsing, or conversion is required.
*   **Minimal Disk Footprint:** Text is stored as raw UTF-16 characters prefixed by length fields, without null-terminators on disk.
*   **I/O Performance:** By pre-allocating the vectors using `vector_reserve` before deserialization, we eliminate resizing overhead, making loading speeds bounded only by disk read speed.

### 1.2 Binary File Format Layout (`.db`)

```
+-------------------------------------------------------------+
| MAGIC (4 bytes) "ACHD"                                      |
+-------------------------------------------------------------+
| VERSION (4 bytes) u32                                       |
+-------------------------------------------------------------+
| DIR_COUNT (4 bytes) u32                                     |
+-------------------------------------------------------------+
| FILE_COUNT (4 bytes) u32                                    |
+-------------------------------------------------------------+
| DIRECTORY TABLE                                             |
|   For each directory:                                       |
|     - Parent ID (4 bytes) u32                               |
|     - Name Length (4 bytes) u32                             |
|     - Name Characters (Length * sizeof(wchar_t))            |
+-------------------------------------------------------------+
| FILE TABLE                                                  |
|   For each file:                                            |
|     - Parent ID (4 bytes) u32                               |
|     - File Size (8 bytes) u64                               |
|     - Last Modified (8 bytes) u64                           |
|     - Attributes (4 bytes) u32                              |
|     - Name Length (4 bytes) u32                             |
|     - Name Characters (Length * sizeof(wchar_t))            |
+-------------------------------------------------------------+
```


## 2. API Design

### 2.1 Interface Definition (`include/storage/database.h`)
```c
#ifndef ACH_DATABASE_H
#define ACH_DATABASE_H

#include "common/types.h"
#include "common/errors.h"
#include "core/index.h"

#define ACH_DB_MAGIC "ACHD"
#define ACH_DB_VERSION 1

/* Saves the in-memory index to a binary database file.
 * Parameters:
 *   index     - Pointer to the Index to serialize.
 *   file_path - Path to the destination file.
 * Returns:
 *   ACH_SUCCESS on success, or an error code on failure. */
AchErrorCode db_save(const Index *index, const wchar_t *file_path);

/* Loads a binary database file into the in-memory index.
 * Parameters:
 *   index     - Pointer to an initialized Index to deserialize into.
 *   file_path - Path to the database file to read.
 * Returns:
 *   ACH_SUCCESS on success, or an error code on failure. */
AchErrorCode db_load(Index *index, const wchar_t *file_path);

#endif /* ACH_DATABASE_H */
```


## 3. Error and Memory Management
1.  **Atomicity:** If loading fails halfway (due to corrupt data, I/O errors, or out of memory), the index is completely cleared via `index_clear` to prevent partial corrupt states.
2.  **Memory Cleanliness:** In case of failure, all names allocated up to the failure point are immediately freed, leaving zero heap leaks.
3.  **Wide Char Support:** We open the database files using Windows' CRT `_wfopen` function to guarantee support for non-ASCII paths.


## 4. Test Strategy
We will implement unit tests in `tests/test_database.c` verifying:
1.  **Basic Save and Load:** Adding files to an index, saving it to a temp database file, clearing the index, reloading from the file, and verifying that the structure and paths match perfectly.
2.  **Empty Index Preservation:** Verifying that saving and loading an empty index behaves correctly.
3.  **Invalid File Detection:** Verifying that loading a nonexistent file returns `ACH_ERROR_FILE_NOT_FOUND`.
4.  **Magic and Version Checks:** Modifying the header magic or version bytes in a test file and confirming that `db_load` rejects it with `ACH_ERROR_INDEX_CORRUPT`.
5.  **Corrupt File Safety:** Verifying that loading a truncated or corrupt file does not crash and cleans up all allocated memory.
