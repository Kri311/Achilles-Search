/* ==========================================================================
 * Achilles-Search | tests/test_search.c
 * ==========================================================================
 * Unit tests for the Search Engine query and filtering module.
 * ========================================================================== */

#include "core/search.h"
#include "core/index.h"
#include "data/hashmap.h"
#include "common/types.h"
#include "common/errors.h"
#include "common/utils.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

/* ---- Simple Test Framework ---------------------------------------------- */
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        printf("  [TEST] %-45s ", #name); \
        if (name()) { \
            tests_passed++; \
            printf("PASS\n"); \
        } else { \
            printf("FAIL\n"); \
        } \
    } while (0)

#define ASSERT(cond) \
    do { \
        if (!(cond)) { \
            printf("ASSERT FAILED: %s (line %d) ", #cond, __LINE__); \
            return false; \
        } \
    } while (0)

/* ---- Test Case: Lifecycle ----------------------------------------------- */
static bool test_search_lifecycle(void) {
    SearchEngineResults res;
    AchErrorCode err = search_results_init(&res);
    ASSERT(err == ACH_SUCCESS);
    ASSERT(search_results_count(&res) == 0);

    search_results_destroy(&res);
    return true;
}

/* ---- Test Case: Basic Search -------------------------------------------- */
static bool test_search_basic(void) {
    Index index;
    index_init(&index);

    HashMap dir_map;
    hashmap_init(&dir_map, sizeof(u32), 16);

    FileInfo files[] = {
        { .path = L"C:\\Windows\\System32\\cmd.exe", .is_directory = false },
        { .path = L"C:\\Windows\\System32\\notepad.exe", .is_directory = false },
        { .path = L"C:\\Projects\\Achilles\\README.md", .is_directory = false },
        { .path = L"C:\\Projects\\Achilles\\src", .is_directory = true }
    };

    for (int i = 0; i < 4; i++) {
        AchErrorCode err = index_add_file_info(&index, &dir_map, &files[i]);
        ASSERT(ACH_SUCCEEDED(err));
    }
    hashmap_destroy(&dir_map);

    SearchEngineResults results;
    search_results_init(&results);

    /* 1. Case-insensitive substring search (matches cmd.exe, notepad.exe) */
    AchErrorCode err = search_execute(&index, L"exe", NULL, &results);
    ASSERT(err == ACH_SUCCESS);
    ASSERT(search_results_count(&results) == 2);

    /* Verify both are files */
    const SearchResult *r0 = search_results_get(&results, 0);
    const SearchResult *r1 = search_results_get(&results, 1);
    ASSERT(r0 != NULL && !r0->is_directory);
    ASSERT(r1 != NULL && !r1->is_directory);

    /* 2. Substring matching folder (matches System32, src, Projects, Achilles) */
    err = search_execute(&index, L"system", NULL, &results);
    ASSERT(err == ACH_SUCCESS);
    ASSERT(search_results_count(&results) == 1);
    const SearchResult *r_sys = search_results_get(&results, 0);
    ASSERT(r_sys != NULL && r_sys->is_directory);

    search_results_destroy(&results);
    index_destroy(&index);
    return true;
}

/* ---- Test Case: Case Sensitivity ----------------------------------------- */
static bool test_search_case_sensitive(void) {
    Index index;
    index_init(&index);

    HashMap dir_map;
    hashmap_init(&dir_map, sizeof(u32), 16);

    FileInfo files[] = {
        { .path = L"C:\\Windows\\System32\\cmd.exe", .is_directory = false },
        { .path = L"C:\\Windows\\System32\\NOTEPAD.EXE", .is_directory = false }
    };

    for (int i = 0; i < 2; i++) {
        index_add_file_info(&index, &dir_map, &files[i]);
    }
    hashmap_destroy(&dir_map);

    SearchEngineResults results;
    search_results_init(&results);

    SearchConfig config = {
        .match_case = true,
        .match_whole_word = false,
        .search_files = true,
        .search_dirs = true
    };

    /* Search for lowercase "notepad" -> should match 0 files */
    AchErrorCode err = search_execute(&index, L"notepad", &config, &results);
    ASSERT(err == ACH_SUCCESS);
    ASSERT(search_results_count(&results) == 0);

    /* Search for uppercase "NOTEPAD" -> should match 1 file */
    err = search_execute(&index, L"NOTEPAD", &config, &results);
    ASSERT(err == ACH_SUCCESS);
    ASSERT(search_results_count(&results) == 1);

    search_results_destroy(&results);
    index_destroy(&index);
    return true;
}

/* ---- Test Case: Whole Word Filter --------------------------------------- */
static bool test_search_whole_word(void) {
    Index index;
    index_init(&index);

    HashMap dir_map;
    hashmap_init(&dir_map, sizeof(u32), 16);

    FileInfo files[] = {
        { .path = L"C:\\Doc\\my_document.txt", .is_directory = false },
        { .path = L"C:\\Doc\\document.txt", .is_directory = false },
        { .path = L"C:\\Doc\\document_backup.txt", .is_directory = false },
        { .path = L"C:\\Doc\\my document.txt", .is_directory = false }
    };

    for (int i = 0; i < 4; i++) {
        index_add_file_info(&index, &dir_map, &files[i]);
    }
    hashmap_destroy(&dir_map);

    SearchEngineResults results;
    search_results_init(&results);

    SearchConfig config = {
        .match_case = false,
        .match_whole_word = true,
        .search_files = true,
        .search_dirs = true
    };

    /* Search for whole word "document" -> should match "document.txt" and "my document.txt"
     * but NOT "my_document.txt" (underscore connects words) or "document_backup.txt" */
    AchErrorCode err = search_execute(&index, L"document", &config, &results);
    ASSERT(err == ACH_SUCCESS);
    ASSERT(search_results_count(&results) == 2);

    /* Verify matched paths */
    for (usize i = 0; i < 2; i++) {
        const SearchResult *r = search_results_get(&results, i);
        const IndexFile *f = vector_get(&index.files, r->index_id);
        ASSERT(wcscmp(f->name, L"document.txt") == 0 || wcscmp(f->name, L"my document.txt") == 0);
    }

    search_results_destroy(&results);
    index_destroy(&index);
    return true;
}

/* ---- Test Case: File/Dir Filters ----------------------------------------- */
static bool test_search_filters(void) {
    Index index;
    index_init(&index);

    HashMap dir_map;
    hashmap_init(&dir_map, sizeof(u32), 16);

    FileInfo files[] = {
        { .path = L"C:\\Folder\\match.txt", .is_directory = false },
        { .path = L"C:\\Folder\\match", .is_directory = true }
    };

    for (int i = 0; i < 2; i++) {
        index_add_file_info(&index, &dir_map, &files[i]);
    }
    hashmap_destroy(&dir_map);

    SearchEngineResults results;
    search_results_init(&results);

    /* 1. Only Files */
    SearchConfig config = { .match_case = false, .match_whole_word = false, .search_files = true, .search_dirs = false };
    AchErrorCode err = search_execute(&index, L"match", &config, &results);
    ASSERT(err == ACH_SUCCESS);
    ASSERT(search_results_count(&results) == 1);
    ASSERT(!search_results_get(&results, 0)->is_directory);

    /* 2. Only Directories */
    config.search_files = false;
    config.search_dirs = true;
    err = search_execute(&index, L"match", &config, &results);
    ASSERT(err == ACH_SUCCESS);
    ASSERT(search_results_count(&results) == 1);
    ASSERT(search_results_get(&results, 0)->is_directory);

    search_results_destroy(&results);
    index_destroy(&index);
    return true;
}

/* ---- Test Case: Parallel Query Execution --------------------------------- */
static bool test_search_parallel(void) {
    Index index;
    index_init(&index);

    HashMap dir_map;
    hashmap_init(&dir_map, sizeof(u32), 16);

    /* Add 1500 files to trigger multi-threaded code path (limit is > 1000 items) */
    for (int i = 0; i < 1500; i++) {
        wchar_t path[MAX_PATH];
        swprintf(path, MAX_PATH, L"C:\\TempDir\\file_%d.txt", i);

        FileInfo info = { .path = path, .is_directory = false };
        AchErrorCode err = index_add_file_info(&index, &dir_map, &info);
        ASSERT(ACH_SUCCEEDED(err));
    }
    hashmap_destroy(&dir_map);

    SearchEngineResults results;
    search_results_init(&results);

    /* Search for "file_123" -> should match "file_123.txt", "file_1230.txt", etc. */
    AchErrorCode err = search_execute(&index, L"file_123", NULL, &results);
    ASSERT(err == ACH_SUCCESS);
    ASSERT(search_results_count(&results) >= 11); // file_123, file_1230 to 1239, etc.

    search_results_destroy(&results);
    index_destroy(&index);
    return true;
}

/* ---- Main --------------------------------------------------------------- */
int main(void) {
    printf("\n");
    printf("============================================================\n");
    printf("  Achilles-Search | Search Engine Test Suite\n");
    printf("============================================================\n\n");

    TEST(test_search_lifecycle);
    TEST(test_search_basic);
    TEST(test_search_case_sensitive);
    TEST(test_search_whole_word);
    TEST(test_search_filters);
    TEST(test_search_parallel);

    printf("\n------------------------------------------------------------\n");
    printf("  Results: %d / %d passed", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("  --  ALL PASSED\n");
    } else {
        printf("  --  %d FAILED\n", tests_run - tests_passed);
    }
    printf("------------------------------------------------------------\n\n");

    int exit_code = (tests_passed == tests_run)  0 : 1;

    if (utils_should_pause_on_exit()) {
        printf("\nPress Enter to exit...");
        getchar();
    }

    return exit_code;
}
