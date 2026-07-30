/* ==========================================================================
 * Achilles-Search | tests/test_index.c
 * ==========================================================================
 * Unit tests for the Filename Index.
 * ========================================================================== */

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
static bool test_index_init_destroy(void) {
    Index index;
    AchErrorCode err = index_init(&index);
    ASSERT(ACH_SUCCEEDED(err));
    ASSERT(index.initialized == true);
    ASSERT(index_file_count(&index) == 0);
    ASSERT(index_dir_count(&index) == 0);

    index_destroy(&index);
    ASSERT(index.initialized == false);
    return true;
}

static bool test_index_clear(void) {
    Index index;
    index_init(&index);

    HashMap dir_map;
    hashmap_init(&dir_map, sizeof(u32), 16);

    FileInfo info = {
        .path = L"C:\\test.txt",
        .file_size = 100,
        .last_modified = 12345,
        .attributes = 0x20,
        .is_directory = false
    };

    index_add_file_info(&index, &dir_map, &info);
    ASSERT(index_file_count(&index) == 1);
    ASSERT(index_dir_count(&index) == 1);

    index_clear(&index);
    ASSERT(index_file_count(&index) == 0);
    ASSERT(index_dir_count(&index) == 0);

    hashmap_destroy(&dir_map);
    index_destroy(&index);
    return true;
}

/* ---- Test Case: Basic Adding and Path Resolution ------------------------ */
static bool test_add_and_resolve_single(void) {
    Index index;
    index_init(&index);

    HashMap dir_map;
    hashmap_init(&dir_map, sizeof(u32), 16);

    FileInfo info = {
        .path = L"C:\\Windows\\notepad.exe",
        .file_size = 204800,
        .last_modified = 987654321,
        .attributes = 0x20, /* Archive */
        .is_directory = false
    };

    AchErrorCode err = index_add_file_info(&index, &dir_map, &info);
    ASSERT(ACH_SUCCEEDED(err));

    /* Dirs should contain "C:" and "Windows" */
    ASSERT(index_dir_count(&index) == 2);
    /* Files should contain "notepad.exe" */
    ASSERT(index_file_count(&index) == 1);

    /* Verify path resolution of the file */
    wchar_t path[MAX_PATH];
    err = index_get_file_path(&index, 0, path, MAX_PATH);
    ASSERT(ACH_SUCCEEDED(err));
    ASSERT(wcscmp(path, L"C:\\Windows\\notepad.exe") == 0);

    /* Verify path resolution of the parent directory ("C:\Windows") */
    err = index_get_dir_path(&index, 1, path, MAX_PATH);
    ASSERT(ACH_SUCCEEDED(err));
    ASSERT(wcscmp(path, L"C:\\Windows") == 0);

    /* Verify path resolution of the root directory ("C:") */
    err = index_get_dir_path(&index, 0, path, MAX_PATH);
    ASSERT(ACH_SUCCEEDED(err));
    ASSERT(wcscmp(path, L"C:") == 0);

    hashmap_destroy(&dir_map);
    index_destroy(&index);
    return true;
}

/* ---- Test Case: Directory Deduplication --------------------------------- */
static bool test_dir_deduplication(void) {
    Index index;
    index_init(&index);

    HashMap dir_map;
    hashmap_init(&dir_map, sizeof(u32), 16);

    /* Add two files in the same directory */
    FileInfo f1 = { .path = L"C:\\Windows\\cmd.exe", .is_directory = false };
    FileInfo f2 = { .path = L"C:\\Windows\\notepad.exe", .is_directory = false };

    index_add_file_info(&index, &dir_map, &f1);
    index_add_file_info(&index, &dir_map, &f2);

    /* We expect dirs: ["C:", "Windows"] (count = 2) */
    ASSERT(index_dir_count(&index) == 2);
    ASSERT(index_file_count(&index) == 2);

    /* Verify paths match */
    wchar_t path1[MAX_PATH];
    wchar_t path2[MAX_PATH];
    index_get_file_path(&index, 0, path1, MAX_PATH);
    index_get_file_path(&index, 1, path2, MAX_PATH);

    ASSERT(wcscmp(path1, L"C:\\Windows\\cmd.exe") == 0);
    ASSERT(wcscmp(path2, L"C:\\Windows\\notepad.exe") == 0);

    hashmap_destroy(&dir_map);
    index_destroy(&index);
    return true;
}

/* ---- Test Case: Deep Hierarchy ----------------------------------------- */
static bool test_deep_hierarchy(void) {
    Index index;
    index_init(&index);

    HashMap dir_map;
    hashmap_init(&dir_map, sizeof(u32), 16);

    FileInfo info = {
        .path = L"D:\\Projects\\Achilles-Search\\src\\core\\index.c",
        .is_directory = false
    };

    index_add_file_info(&index, &dir_map, &info);

    /* We expect dirs: D:, Projects, Achilles-Search, src, core (5 dirs) */
    ASSERT(index_dir_count(&index) == 5);
    ASSERT(index_file_count(&index) == 1);

    wchar_t path[MAX_PATH];
    index_get_file_path(&index, 0, path, MAX_PATH);
    ASSERT(wcscmp(path, L"D:\\Projects\\Achilles-Search\\src\\core\\index.c") == 0);

    hashmap_destroy(&dir_map);
    index_destroy(&index);
    return true;
}

/* ---- Test Case: Directory Entries --------------------------------------- */
static bool test_directory_entries(void) {
    Index index;
    index_init(&index);

    HashMap dir_map;
    hashmap_init(&dir_map, sizeof(u32), 16);

    /* Add a directory path directly */
    FileInfo info = {
        .path = L"C:\\Windows\\System32",
        .is_directory = true
    };

    AchErrorCode err = index_add_file_info(&index, &dir_map, &info);
    ASSERT(ACH_SUCCEEDED(err));

    /* Dirs: C:, Windows, System32 */
    ASSERT(index_dir_count(&index) == 3);
    ASSERT(index_file_count(&index) == 0);

    wchar_t path[MAX_PATH];
    err = index_get_dir_path(&index, 2, path, MAX_PATH);
    ASSERT(ACH_SUCCEEDED(err));
    ASSERT(wcscmp(path, L"C:\\Windows\\System32") == 0);

    hashmap_destroy(&dir_map);
    index_destroy(&index);
    return true;
}

/* ---- Test Case: Relative Paths ------------------------------------------ */
static bool test_relative_paths(void) {
    Index index;
    index_init(&index);

    HashMap dir_map;
    hashmap_init(&dir_map, sizeof(u32), 16);

    /* Relative file at root */
    FileInfo f1 = { .path = L"README.md", .is_directory = false };
    /* Relative file in folder */
    FileInfo f2 = { .path = L"docs\\spec.md", .is_directory = false };

    index_add_file_info(&index, &dir_map, &f1);
    index_add_file_info(&index, &dir_map, &f2);

    ASSERT(index_dir_count(&index) == 1); // Only "docs"
    ASSERT(index_file_count(&index) == 2);

    wchar_t path[MAX_PATH];
    index_get_file_path(&index, 0, path, MAX_PATH);
    ASSERT(wcscmp(path, L"README.md") == 0);

    index_get_file_path(&index, 1, path, MAX_PATH);
    ASSERT(wcscmp(path, L"docs\\spec.md") == 0);

    hashmap_destroy(&dir_map);
    index_destroy(&index);
    return true;
}

/* ---- Test Case: Buffer Too Small ---------------------------------------- */
static bool test_buffer_too_small(void) {
    Index index;
    index_init(&index);

    HashMap dir_map;
    hashmap_init(&dir_map, sizeof(u32), 16);

    FileInfo info = { .path = L"C:\\Windows\\notepad.exe", .is_directory = false };
    index_add_file_info(&index, &dir_map, &info);

    wchar_t path[10];
    AchErrorCode err = index_get_file_path(&index, 0, path, 10);
    ASSERT(err == ACH_ERROR_PATH_TOO_LONG);

    hashmap_destroy(&dir_map);
    index_destroy(&index);
    return true;
}

/* ---- Main --------------------------------------------------------------- */
int main(void) {
    printf("\n");
    printf("============================================================\n");
    printf("  Achilles-Search | Filename Index Test Suite\n");
    printf("============================================================\n\n");

    TEST(test_index_init_destroy);
    TEST(test_index_clear);
    TEST(test_add_and_resolve_single);
    TEST(test_dir_deduplication);
    TEST(test_deep_hierarchy);
    TEST(test_directory_entries);
    TEST(test_relative_paths);
    TEST(test_buffer_too_small);

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
