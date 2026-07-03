/* ==========================================================================
 * Achilles-Search | include/core/search.h
 * ==========================================================================
 * Search Engine query interface and results definition.
 * ========================================================================== */

#ifndef ACH_SEARCH_H
#define ACH_SEARCH_H

#include "common/types.h"
#include "common/errors.h"
#include "core/index.h"

/* Search filters and options */
typedef struct SearchConfig {
    bool match_case;        /* Case-sensitive search (default is false) */
    bool match_whole_word;   /* Match whole word only (default is false) */
    bool search_files;      /* Include files in results (default is true) */
    bool search_dirs;       /* Include directories in results (default is true) */
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
 *   config  - Configuration (if NULL, defaults: case-insensitive, files & dirs).
 *   results - Pointer to initialized SearchEngineResults.
 * Returns:
 *   ACH_SUCCESS on success, or an error code. */
AchErrorCode search_execute(const Index *index, const wchar_t *query, const SearchConfig *config, SearchEngineResults *results);

#endif /* ACH_SEARCH_H */
