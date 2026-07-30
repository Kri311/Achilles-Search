/* ==========================================================================
 * Achilles-Search | tests/test_content.c
 * ==========================================================================
 * Unit tests for the Content Index (Inverted Index) module.
 * ========================================================================== */

#include "core/content_index.h"
#include "data/vector.h"
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
static bool test_content_lifecycle(void) {
    ContentIndex cindex;
    AchErrorCode err = content_index_init(&cindex);
    ASSERT(err == ACH_SUCCESS);
    ASSERT(cindex.initialized);

    content_index_destroy(&cindex);
    ASSERT(!cindex.initialized);
    return true;
}

/* ---- Test Case: Index and Search file ---------------------------------- */
static bool test_content_index_search(void) {
    ContentIndex cindex;
    content_index_init(&cindex);

    /* Create temporary text file */
    const wchar_t *temp_file = L"temp_content_test.txt";
    _wremove(temp_file);

    FILE *f = _wfopen(temp_file, L"w, ccs=UTF-8");
    ASSERT(f != NULL);
    fwprintf(f, L"Achilles search engine is fast!");
    fclose(f);

    /* Index the file with ID 42 */
    AchErrorCode err = content_index_add_file(&cindex, 42, temp_file);
    ASSERT(err == ACH_SUCCESS);

    /* Search terms */
    Vector results;
    vector_init(&results, sizeof(u32), 4);

    /* Term 1: "achilles" (case-insensitive check) */
    err = content_index_search(&cindex, L"achilles", &results);
    ASSERT(err == ACH_SUCCESS);
    ASSERT(vector_length(&results) == 1);
    u32 *val = (u32*)vector_get(&results, 0);
    ASSERT(val != NULL && *val == 42);

    /* Term 2: "fast" */
    vector_clear(&results);
    err = content_index_search(&cindex, L"fast", &results);
    ASSERT(err == ACH_SUCCESS);
    ASSERT(vector_length(&results) == 1);
    val = (u32*)vector_get(&results, 0);
    ASSERT(val != NULL && *val == 42);

    /* Term 3: Nonexistent word */
    vector_clear(&results);
    err = content_index_search(&cindex, L"missing", &results);
    ASSERT(err == ACH_ERROR_NOT_FOUND);

    /* Cleanup */
    vector_destroy(&results);
    content_index_destroy(&cindex);
    _wremove(temp_file);

    return true;
}

/* ---- Test Case: Consecutive Deduplication ------------------------------ */
static bool test_content_deduplication(void) {
    ContentIndex cindex;
    content_index_init(&cindex);

    const wchar_t *temp_file = L"temp_content_dedup.txt";
    _wremove(temp_file);

    FILE *f = _wfopen(temp_file, L"w, ccs=UTF-8");
    ASSERT(f != NULL);
    fwprintf(f, L"engine engine engine engine");
    fclose(f);

    /* Index with ID 100 */
    AchErrorCode err = content_index_add_file(&cindex, 100, temp_file);
    ASSERT(err == ACH_SUCCESS);

    Vector results;
    vector_init(&results, sizeof(u32), 4);

    /* Search for "engine" -> postings list should have size 1 (deduplicated) */
    err = content_index_search(&cindex, L"engine", &results);
    ASSERT(err == ACH_SUCCESS);
    ASSERT(vector_length(&results) == 1);
    u32 *val = (u32*)vector_get(&results, 0);
    ASSERT(val != NULL && *val == 100);

    /* Cleanup */
    vector_destroy(&results);
    content_index_destroy(&cindex);
    _wremove(temp_file);

    return true;
}

/* ---- Test Case: Extension Filtering ------------------------------------ */
static bool test_content_filters(void) {
    ContentIndex cindex;
    content_index_init(&cindex);

    /* Create temporary binary file format (skipped by extension checker) */
    const wchar_t *temp_file = L"temp_content_filter.png";
    _wremove(temp_file);

    FILE *f = _wfopen(temp_file, L"wb");
    ASSERT(f != NULL);
    char dummy_data[] = "dummy text in png file";
    fwrite(dummy_data, 1, sizeof(dummy_data), f);
    fclose(f);

    /* Attempt to index it -> should return success but do nothing */
    AchErrorCode err = content_index_add_file(&cindex, 50, temp_file);
    ASSERT(err == ACH_SUCCESS);

    Vector results;
    vector_init(&results, sizeof(u32), 4);

    err = content_index_search(&cindex, L"dummy", &results);
    ASSERT(err == ACH_ERROR_NOT_FOUND);

    /* Cleanup */
    vector_destroy(&results);
    content_index_destroy(&cindex);
    _wremove(temp_file);

    return true;
}

/* ---- Test Case: Content Index Clear -------------------------------------- */
static bool test_content_clear(void) {
    ContentIndex cindex;
    content_index_init(&cindex);

    const wchar_t *temp_file = L"temp_content_clear.txt";
    _wremove(temp_file);

    FILE *f = _wfopen(temp_file, L"w, ccs=UTF-8");
    ASSERT(f != NULL);
    fwprintf(f, L"clear_test");
    fclose(f);

    index_init(NULL); /* Dummy check */
    AchErrorCode err = content_index_add_file(&cindex, 99, temp_file);
    ASSERT(err == ACH_SUCCESS);

    /* Clear index */
    content_index_clear(&cindex);

    Vector results;
    vector_init(&results, sizeof(u32), 4);
    err = content_index_search(&cindex, L"clear_test", &results);
    ASSERT(err == ACH_ERROR_NOT_FOUND);

    vector_destroy(&results);
    content_index_destroy(&cindex);
    _wremove(temp_file);

    return true;
}

/* ---- Main --------------------------------------------------------------- */
int main(void) {
    printf("\n");
    printf("============================================================\n");
    printf("  Achilles-Search | Content Index Test Suite\n");
    printf("============================================================\n\n");

    TEST(test_content_lifecycle);
    TEST(test_content_index_search);
    TEST(test_content_deduplication);
    TEST(test_content_filters);
    TEST(test_content_clear);

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
