# Phase 4: Filename Index Design Specification

This document details the architecture, data structures, and indexing algorithms for the **Filename Index** in Achilles-Search. The index serves as the central in-memory database that allows instant search of filenames, folder names, and path reconstruction with minimal memory footprint.


## 1. Architectural Decisions

### 1.1 Memory-Efficient Hierarchical Index
Storing full paths for every file in a filesystem with millions of files wastes significant RAM. For example, the path prefix `C:\Windows\System32\` would be duplicated thousands of times.

To solve this, we store the directory structure hierarchically:
1.  **Directory Table:** A contiguous `Vector` of `IndexDir` entries. Each entry contains only its folder name (e.g. `"System32"`) and a `u32 parent_id` referencing its parent directory in the same table.
2.  **File Table:** A contiguous `Vector` of `IndexFile` entries. Each entry contains only its filename (e.g. `"cmd.exe"`), its `u32 parent_id` referencing its parent directory, and metadata (size, time, attributes).

```
                 Hierarchical Index Relationship
                 
   dirs Vector:
   [0] Name: "C:"         Parent: ROOT (-1)
   [1] Name: "Windows"    Parent: 0
   [2] Name: "System32"   Parent: 1
   
   files Vector:
   [0] Name: "cmd.exe"     Parent: 2  ==> Path: C:\Windows\System32\cmd.exe
   [1] Name: "notepad.exe" Parent: 2  ==> Path: C:\Windows\System32\notepad.exe
```

### 1.2 Path Reconstruction on the Fly
Because we do not store full paths, we reconstruct them dynamically by climbing the directory tree starting from the parent ID:
1.  Follow the `parent_id` chain up to the root (where `parent_id == ACH_INDEX_ROOT_ID`).
2.  Accumulate the folder names.
3.  Join them in order with backslashes (`\`) and append the filename.

This saves ~70% of RAM compared to storing full path strings, while path reconstruction is only performed on search matches, keeping it extremely fast.

### 1.3 Fast Construction using Hash Map
When inserting `FileInfo` entries from the scanner:
1.  Split the entry's full path into parent directory path and filename.
2.  Look up the parent directory path in a temporary helper **Hash Map** (`HashMap` mapping `const wchar_t*` full path to `u32 dir_id`).
3.  If the parent path is found, associate the file with that `dir_id`.
4.  If not found, recursively split, insert the parent directories, add them to the Directory Table, record their new IDs in the Hash Map, and associate the file.


## 2. API Design

### 2.1 Types and Constants
```c
#define ACH_INDEX_ROOT_ID ((u32)-1)

typedef struct IndexDir {
    wchar_t *name;          /* Just the folder name (e.g., "System32") */
    u32 parent_id;          /* Index of parent folder in Index.dirs, or ACH_INDEX_ROOT_ID */
} IndexDir;

typedef struct IndexFile {
    wchar_t *name;          /* Just the filename (e.g., "cmd.exe") */
    u32 parent_id;          /* Index of parent folder in Index.dirs */
    u64 file_size;          /* File size in bytes */
    u64 last_modified;      /* Last modification timestamp */
    u32 attributes;         /* Win32 file attributes */
} IndexFile;

typedef struct Index {
    Vector dirs;            /* Vector storing IndexDir elements */
    Vector files;           /* Vector storing IndexFile elements */
    bool initialized;
} Index;
```

### 2.2 Core Functions
```c
/* Lifecycle */
AchErrorCode index_init(Index *index);
void index_destroy(Index *index);
void index_clear(Index *index);

/* Construction */
AchErrorCode index_add_file_info(Index *index, HashMap *dir_map, const FileInfo *info);

/* Path Resolution */
AchErrorCode index_get_file_path(const Index *index, u32 file_idx, wchar_t *out_path, usize max_len);
AchErrorCode index_get_dir_path(const Index *index, u32 dir_idx, wchar_t *out_path, usize max_len);

/* Information */
usize index_file_count(const Index *index);
usize index_dir_count(const Index *index);
```


## 3. Test Strategy
We will implement unit tests in `tests/test_index.c` verifying:
1.  **Basic Construction:** Creating an empty index, adding files, and verifying sizes.
2.  **Parent Path Parsing:** Verifying correct extraction of folders and files from path strings.
3.  **Path Reconstruction:** Reconstructing both file paths and directory paths and comparing with original paths.
4.  **Duplicates and Nesting:** Correctly handling deeply nested folders (e.g. `C:\a\b\c\d\e.txt`) and ensuring folders are not duplicated in the Directory Table.
5.  **Robust Error Handling:** Resisting null pointers, invalid IDs, and boundary overflows.
