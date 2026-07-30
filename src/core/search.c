/* ==========================================================================
 * Achilles-Search | src/core/search.c
 * ==========================================================================
 * Search Engine multi-threaded search implementation.
 * ========================================================================== */

#include "core/search.h"
#include "common/logger.h"
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define MAX_SEARCH_THREADS 64

/* ---- Thread Parameter Struct -------------------------------------------- */
typedef struct SearchThreadParams {
    const Index   *index;
    const wchar_t *query;
    const wchar_t *query_lower;
    SearchConfig   config;

    usize          file_start;
    usize          file_end;
    usize          dir_start;
    usize          dir_end;

    Vector         local_results;
    AchErrorCode   status;
} SearchThreadParams;

/* ---- Static Helpers ------------------------------------------------------ */

static inline wchar_t towlower_ascii(wchar_t c) {
    if (c >= L'A' && c <= L'Z') {
        return c + (L'a' - L'A');
    }
    return towlower(c);
}

/* Case-insensitive wide string substring search with pre-lowered query */
static const wchar_t* wcsistr_prelowered(const wchar_t *haystack, const wchar_t *needle_lower) {
    if (!*needle_lower) {
        return haystack;
    }
    for (; *haystack; haystack++) {
        if (towlower_ascii(*haystack) == *needle_lower) {
            const wchar_t *h = haystack;
            const wchar_t *n = needle_lower;
            while (*h && *n && towlower_ascii(*h) == *n) {
                h++;
                n++;
            }
            if (!*n) {
                return haystack;
            }
        }
    }
    return NULL;
}

/* Match a query against a name with case and whole-word options */
static bool match_name(const wchar_t *name, const wchar_t *query, const wchar_t *query_lower, bool match_case, bool match_whole_word) {
    if (name == NULL || query == NULL) {
        return false;
    }

    usize query_len = wcslen(query);
    if (query_len == 0) {
        return true;
    }

    const wchar_t *pos = name;
    while (true) {
        if (match_case) {
            pos = wcsstr(pos, query);
        } else {
            pos = wcsistr_prelowered(pos, query_lower);
        }

        if (pos == NULL) {
            return false;
        }

        if (match_whole_word) {
            /* Check boundary before match */
            bool prev_ok = true;
            if (pos > name) {
                wchar_t prev_char = *(pos - 1);
                if (iswalnum(prev_char) || prev_char == L'_') {
                    prev_ok = false;
                }
            }

            /* Check boundary after match */
            bool next_ok = true;
            wchar_t next_char = *(pos + query_len);
            if (next_char != L'\0') {
                if (iswalnum(next_char) || next_char == L'_') {
                    next_ok = false;
                }
            }

            if (prev_ok && next_ok) {
                return true;
            }

            /* Try next match starting 1 character after current match pos */
            pos++;
        } else {
            return true;
        }
    }
}

/* ---- Thread Function ----------------------------------------------------- */
static DWORD WINAPI search_thread_func(LPVOID lpParam) {
    SearchThreadParams *params = (SearchThreadParams*)lpParam;
    if (params == NULL) {
        return 1;
    }

    params->status = vector_init(&params->local_results, sizeof(SearchResult), 128);
    if (params->status != ACH_SUCCESS) {
        return 1;
    }

    const Index *index = params->index;
    const wchar_t *query = params->query;
    const wchar_t *query_lower = params->query_lower;
    bool match_case = params->config.match_case;
    bool match_whole_word = params->config.match_whole_word;

    /* 1. Search Files range */
    if (params->config.search_files) {
        for (usize i = params->file_start; i < params->file_end; i++) {
            const IndexFile *file = vector_get(&index->files, i);
            if (file != NULL && match_name(file->name, query, query_lower, match_case, match_whole_word)) {
                SearchResult res = { .index_id = (u32)i, .is_directory = false };
                AchErrorCode err = vector_push(&params->local_results, &res);
                if (err != ACH_SUCCESS) {
                    params->status = err;
                    return 1;
                }
            }
        }
    }

    /* 2. Search Directories range */
    if (params->config.search_dirs) {
        for (usize i = params->dir_start; i < params->dir_end; i++) {
            const IndexDir *dir = vector_get(&index->dirs, i);
            if (dir != NULL && match_name(dir->name, query, query_lower, match_case, match_whole_word)) {
                SearchResult res = { .index_id = (u32)i, .is_directory = true };
                AchErrorCode err = vector_push(&params->local_results, &res);
                if (err != ACH_SUCCESS) {
                    params->status = err;
                    return 1;
                }
            }
        }
    }

    params->status = ACH_SUCCESS;
    return 0;
}

/* ---- Lifecycle ----------------------------------------------------------- */

AchErrorCode search_results_init(SearchEngineResults *results) {
    if (results == NULL) {
        return ACH_ERROR_INVALID_ARG;
    }
    return vector_init(&results->items, sizeof(SearchResult), 256);
}

void search_results_destroy(SearchEngineResults *results) {
    if (results != NULL) {
        vector_destroy(&results->items);
    }
}

void search_results_clear(SearchEngineResults *results) {
    if (results != NULL) {
        vector_clear(&results->items);
    }
}

usize search_results_count(const SearchEngineResults *results) {
    return (results != NULL)  vector_length(&results->items) : 0;
}

const SearchResult* search_results_get(const SearchEngineResults *results, usize index) {
    if (results == NULL) {
        return NULL;
    }
    return (const SearchResult*)vector_get(&results->items, index);
}

/* ---- Search Operation --------------------------------------------------- */

AchErrorCode search_execute(const Index *index, const wchar_t *query, const SearchConfig *config, SearchEngineResults *results) {
    if (index == NULL || !index->initialized || query == NULL || results == NULL) {
        return ACH_ERROR_INVALID_ARG;
    }

    /* Default Configuration */
    SearchConfig active_config = {
        .match_case = false,
        .match_whole_word = false,
        .search_files = true,
        .search_dirs = true
    };
    if (config != NULL) {
        active_config = *config;
    }

    usize total_files = vector_length(&index->files);
    usize total_dirs = vector_length(&index->dirs);

    search_results_clear(results);

    /* Pre-lowercase search query once for case-insensitive search */
    wchar_t *query_lower = NULL;
    if (!active_config.match_case) {
        usize q_len = wcslen(query);
        query_lower = (wchar_t*)malloc((q_len + 1) * sizeof(wchar_t));
        if (query_lower == NULL) {
            return ACH_ERROR_OUT_OF_MEMORY;
        }
        for (usize i = 0; i < q_len; i++) {
            query_lower[i] = towlower_ascii(query[i]);
        }
        query_lower[q_len] = L'\0';
    }

    /* Determine number of worker threads */
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    DWORD thread_count = sys_info.dwNumberOfProcessors;
    if (thread_count == 0) {
        thread_count = 1;
    }
    if (thread_count > MAX_SEARCH_THREADS) {
        thread_count = MAX_SEARCH_THREADS;
    }

    /* Optimization: Run synchronously for tiny sets or single-core machines */
    if (thread_count <= 1 || (total_files + total_dirs < 1000)) {
        SearchThreadParams params = {
            .index = index,
            .query = query,
            .query_lower = query_lower,
            .config = active_config,
            .file_start = 0,
            .file_end = total_files,
            .dir_start = 0,
            .dir_end = total_dirs
        };

        search_thread_func(&params);

        if (params.status == ACH_SUCCESS) {
            usize count = vector_length(&params.local_results);
            AchErrorCode err = vector_reserve(&results->items, count);
            if (err == ACH_SUCCESS) {
                if (count > 0) {
                    vector_push_many(&results->items, params.local_results.data, count);
                }
            } else {
                params.status = err;
            }
        }
        vector_destroy(&params.local_results);
        if (query_lower != NULL) {
            free(query_lower);
        }
        return params.status;
    }

    /* Parallel execution */
    HANDLE threads[MAX_SEARCH_THREADS] = {NULL};
    SearchThreadParams thread_params[MAX_SEARCH_THREADS] = {0};

    usize files_per_thread = total_files / thread_count;
    usize dirs_per_thread = total_dirs / thread_count;

    for (DWORD i = 0; i < thread_count; i++) {
        thread_params[i].index = index;
        thread_params[i].query = query;
        thread_params[i].query_lower = query_lower;
        thread_params[i].config = active_config;

        thread_params[i].file_start = i * files_per_thread;
        thread_params[i].file_end = (i == thread_count - 1)  total_files : (i + 1) * files_per_thread;

        thread_params[i].dir_start = i * dirs_per_thread;
        thread_params[i].dir_end = (i == thread_count - 1)  total_dirs : (i + 1) * dirs_per_thread;

        threads[i] = CreateThread(
            NULL,
            0,
            search_thread_func,
            &thread_params[i],
            0,
            NULL
        );

        if (threads[i] == NULL) {
            /* Thread spawn failed, clean up previously created threads */
            for (DWORD j = 0; j < i; j++) {
                TerminateThread(threads[j], 1);
                CloseHandle(threads[j]);
                vector_destroy(&thread_params[j].local_results);
            }
            if (query_lower != NULL) {
                free(query_lower);
            }
            return ACH_ERROR_UNKNOWN;
        }
    }

    /* Wait for all threads to complete */
    WaitForMultipleObjects(thread_count, threads, TRUE, INFINITE);

    /* Check status codes and calculate total match size */
    usize total_matches = 0;
    AchErrorCode search_err = ACH_SUCCESS;

    for (DWORD i = 0; i < thread_count; i++) {
        CloseHandle(threads[i]);
        if (thread_params[i].status != ACH_SUCCESS) {
            search_err = thread_params[i].status;
        } else {
            total_matches += vector_length(&thread_params[i].local_results);
        }
    }

    if (search_err != ACH_SUCCESS) {
        for (DWORD i = 0; i < thread_count; i++) {
            vector_destroy(&thread_params[i].local_results);
        }
        if (query_lower != NULL) {
            free(query_lower);
        }
        return search_err;
    }

    /* Merge results using vector_push_many optimized range extend */
    AchErrorCode err = vector_reserve(&results->items, total_matches);
    if (err == ACH_SUCCESS) {
        for (DWORD i = 0; i < thread_count; i++) {
            usize count = vector_length(&thread_params[i].local_results);
            if (count > 0) {
                vector_push_many(&results->items, thread_params[i].local_results.data, count);
            }
        }
    } else {
        search_err = err;
    }

    /* Clean up local thread vectors and dynamic memory */
    for (DWORD i = 0; i < thread_count; i++) {
        vector_destroy(&thread_params[i].local_results);
    }
    if (query_lower != NULL) {
        free(query_lower);
    }

    return search_err;
}
