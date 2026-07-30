# Phase 6: Search Engine Design Specification

This document details the architecture, multi-threading model, and search algorithms for the **Search Engine** in Achilles-Search. The engine enables instant, parallel queries over the in-memory index using Windows threads.


## 1. Architectural Decisions

### 1.1 Multi-Threaded Parallel Search
To search millions of filenames in milliseconds, we parallelize the search across all available CPU cores:
1.  **Work Partitioning:** We divide the flat directory and file vectors into equal chunks based on the number of logical cores (obtained via `GetSystemInfo`).
2.  **Zero-Lock Contention:** Each thread performs matches on its own chunk and writes matches to a thread-local results vector. This avoids the cost of cross-thread locks (critical sections) during the search.
3.  **Fast Merge:** Once all threads complete, the main thread computes the total count, pre-allocates the exact capacity in the final results list, and performs a fast merge.

### 1.2 Threading Overhead Optimization
Spawning OS threads has cost (~50-100 microseconds). For small indexes (e.g. less than 1000 total items) or single-core machines, spawning threads is slower than scanning sequentially.
*   **Threshold Fallback:** If the sum of files and directories in the index is < 1000, or if logical processor count is 1, the engine executes the query synchronously on the calling thread, avoiding context switching.

### 1.3 Case-Insensitive Substring Match & Whole Word Filters
We implement a custom wide-character case-insensitive substring finder (`wcsistr`) using standard `<wctype.h>` functions (`towlower`).
Additionally, we support a **Whole Word** filter which verifies that any match is bounded by non-alphanumeric characters (using `iswalnum`).


## 2. API Design

### 2.1 Interface Definition (`include/core/search.h`)
```c
#ifndef ACH_SEARCH_H
#define ACH_SEARCH_H

#include "common/types.h"
#include "common/errors.h"
#include "core/index.h"

/* Search filters and options */
typedef struct SearchConfig {
    bool match_case;        /* Case-sensitive search */
    bool match_whole_word;   /* Match whole word only */
    bool search_files;      /* Include files in results */
    bool search_dirs;       /* Include directories in results */
} SearchConfig;

/* Individual search result */
typedef struct SearchResult {
    u32 index_id;           /* Index in Index.dirs or Index.files */
    bool is_directory;      /* True if directory, false if file */
} SearchResult;

/* Collection of search results */
typedef struct SearchEngineResults {
    Vector items;           /* Vector of SearchResult entries */
} SearchEngineResults;

/* ---- Results Lifecycle --------------------------------------------------- */
AchErrorCode search_results_init(SearchEngineResults *results);
void search_results_destroy(SearchEngineResults *results);
void search_results_clear(SearchEngineResults *results);
usize search_results_count(const SearchEngineResults *results);
const SearchResult* search_results_get(const SearchEngineResults *results, usize index);

/* ---- Search Operations -------------------------------------------------- */

/* Performs a multi-threaded search over the index.
 * Parameters:
 *   index   - Pointer to the initialized filename Index.
 *   query   - Search term (wide string).
 *   config  - Configuration options (if NULL, defaults are used).
 *   results - Pointer to initialized SearchEngineResults.
 * Returns:
 *   ACH_SUCCESS on success, or an error code. */
AchErrorCode search_execute(const Index *index, const wchar_t *query, const SearchConfig *config, SearchEngineResults *results);

#endif /* ACH_SEARCH_H */
```


## 3. Thread Safety and Synchronization
*   The `Index` is read-only during search queries. Therefore, no read-write locking is required on the index itself during search execution.
*   We use Windows native `CreateThread` and `WaitForMultipleObjects` to launch and synchronize search threads.
