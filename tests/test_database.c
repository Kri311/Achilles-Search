/* ==========================================================================
 * Achilles-Search | tests/test_database.c
 * ==========================================================================
 * Unit tests for the Database Serialization & Deserialization module.
 * ========================================================================== */

#include "storage/database.h"
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

/* ---- Test Case: Basic Save and Load ------------------------------------ */
static bool test_save_load_basic(void) {
    Index index1;
    index_init(&index1);

    HashMap dir_map;
    hashmap_init(&dir_map, sizeof(u32), 16);

    /* Populate Index 1 with directories and files */
    FileInfo files[] = {
        { .path = L"C:\\Windows\\System32\\cmd.exe", .file_size = 280000, .last_modified = 111111, .attributes = 0x20, .is_directory = false },
        { .path = L"C:\\Windows\\System32\\notepad.exe", .file_size = 190000, .last_modified = 222222, .attributes = 0x20, .is_directory = false },
        { .path = L"C:\\Windows\\explorer.exe", .file_size = 4500000, .last_modified = 333333, .attributes = 0x20, .is_directory = false },
        { .path = L"C:\\Program Files\\Git", .is_directory = true }
    };

    for (int i = 0; i < 4; i++) {
        AchErrorCode err = index_add_file_info(&index1, &dir_map, &files[i]);
        ASSERT(ACH_SUCCEEDED(err));
    }

    hashmap_destroy(&dir_map);

    /* Save to disk */
    const wchar_t *db_path = L"test_basic.db";
    _wremove(db_path); /* Clean old run if exists */

    AchErrorCode err = db_save(&index1, db_path);
    ASSERT(ACH_SUCCEEDED(err));

    /* Load into Index 2 */
    Index index2;
    index_init(&index2);

    err = db_load(&index2, db_path);
    ASSERT(ACH_SUCCEEDED(err));

    /* Verify sizes */
    ASSERT(index_dir_count(&index1) == index_dir_count(&index2));
    ASSERT(index_file_count(&index1) == index_file_count(&index2));

    /* Verify paths and metadata for all files */
    for (u32 i = 0; i < (u32)index_file_count(&index1); i++) {
        wchar_t path1[MAX_PATH];
        wchar_t path2[MAX_PATH];

        ASSERT(ACH_SUCCEEDED(index_get_file_path(&index1, i, path1, MAX_PATH)));
        ASSERT(ACH_SUCCEEDED(index_get_file_path(&index2, i, path2, MAX_PATH)));
        ASSERT(wcscmp(path1, path2) == 0);

        const IndexFile *f1 = vector_get(&index1.files, i);
        const IndexFile *f2 = vector_get(&index2.files, i);

        ASSERT(f1->file_size == f2->file_size);
        ASSERT(f1->last_modified == f2->last_modified);
        ASSERT(f1->attributes == f2->attributes);
    }

    /* Verify directories */
    for (u32 i = 0; i < (u32)index_dir_count(&index1); i++) {
        wchar_t path1[MAX_PATH];
        wchar_t path2[MAX_PATH];

        ASSERT(ACH_SUCCEEDED(index_get_dir_path(&index1, i, path1, MAX_PATH)));
        ASSERT(ACH_SUCCEEDED(index_get_dir_path(&index2, i, path2, MAX_PATH)));
        ASSERT(wcscmp(path1, path2) == 0);
    }

    /* Cleanup */
    index_destroy(&index1);
    index_destroy(&index2);
    _wremove(db_path);

    return true;
}

/* ---- Test Case: Save/Load Empty ---------------------------------------- */
static bool test_save_load_empty(void) {
    Index index1;
    index_init(&index1);

    const wchar_t *db_path = L"test_empty.db";
    _wremove(db_path);

    AchErrorCode err = db_save(&index1, db_path);
    ASSERT(ACH_SUCCEEDED(err));

    Index index2;
    index_init(&index2);

    err = db_load(&index2, db_path);
    ASSERT(ACH_SUCCEEDED(err));

    ASSERT(index_dir_count(&index2) == 0);
    ASSERT(index_file_count(&index2) == 0);

    index_destroy(&index1);
    index_destroy(&index2);
    _wremove(db_path);

    return true;
}

/* ---- Test Case: Error Handling ------------------------------------------ */
static bool test_error_handling(void) {
    Index index;
    index_init(&index);

    /* 1. Load nonexistent file */
    AchErrorCode err = db_load(&index, L"nonexistent_file.db");
    ASSERT(err == ACH_ERROR_FILE_NOT_FOUND);

    /* 2. Null arguments checks */
    ASSERT(db_save(NULL, L"test.db") == ACH_ERROR_INVALID_ARG);
    ASSERT(db_save(&index, NULL) == ACH_ERROR_INVALID_ARG);
    ASSERT(db_load(NULL, L"test.db") == ACH_ERROR_INVALID_ARG);
    ASSERT(db_load(&index, NULL) == ACH_ERROR_INVALID_ARG);

    index_destroy(&index);
    return true;
}

/* ---- Test Case: Corrupt Headers ----------------------------------------- */
static bool test_corrupt_headers(void) {
    const wchar_t *db_path = L"test_corrupt.db";
    _wremove(db_path);

    /* 1. Write file with bad magic */
    FILE *f = _wfopen(db_path, L"wb");
    ASSERT(f != NULL);
    char bad_magic[4] = "BAD!";
    u32 ver = ACH_DB_VERSION;
    u32 count = 0;
    fwrite(bad_magic, 1, 4, f);
    fwrite(&ver, sizeof(u32), 1, f);
    fwrite(&count, sizeof(u32), 1, f);
    fwrite(&count, sizeof(u32), 1, f);
    fclose(f);

    Index index;
    index_init(&index);
    AchErrorCode err = db_load(&index, db_path);
    ASSERT(err == ACH_ERROR_INDEX_CORRUPT);

    /* 2. Write file with bad version */
    f = _wfopen(db_path, L"wb");
    ASSERT(f != NULL);
    char good_magic[4] = ACH_DB_MAGIC;
    u32 bad_ver = 9999;
    fwrite(good_magic, 1, 4, f);
    fwrite(&bad_ver, sizeof(u32), 1, f);
    fwrite(&count, sizeof(u32), 1, f);
    fwrite(&count, sizeof(u32), 1, f);
    fclose(f);

    err = db_load(&index, db_path);
    ASSERT(err == ACH_ERROR_INDEX_CORRUPT);

    index_destroy(&index);
    _wremove(db_path);

    return true;
}

/* ---- Test Case: Truncated File Safety ------------------------------------ */
static bool test_truncated_file(void) {
    const wchar_t *db_path = L"test_trunc.db";
    _wremove(db_path);

    /* Write header claiming 5 directories, but write none */
    FILE *f = _wfopen(db_path, L"wb");
    ASSERT(f != NULL);
    char magic[4] = ACH_DB_MAGIC;
    u32 ver = ACH_DB_VERSION;
    u32 dirs = 5;
    u32 files = 0;
    fwrite(magic, 1, 4, f);
    fwrite(&ver, sizeof(u32), 1, f);
    fwrite(&dirs, sizeof(u32), 1, f);
    fwrite(&files, sizeof(u32), 1, f);
    fclose(f);

    Index index;
    index_init(&index);
    
    /* Load should detect EOF early and cleanly return error without leaks */
    AchErrorCode err = db_load(&index, db_path);
    ASSERT(err == ACH_ERROR_FILE_READ_FAILED);
    ASSERT(index_dir_count(&index) == 0);

    index_destroy(&index);
    _wremove(db_path);

    return true;
}

/* ---- Main --------------------------------------------------------------- */
int main(void) {
    printf("\n");
    printf("============================================================\n");
    printf("  Achilles-Search | Persistent Database Test Suite\n");
    printf("============================================================\n\n");

    TEST(test_save_load_basic);
    TEST(test_save_load_empty);
    TEST(test_error_handling);
    TEST(test_corrupt_headers);
    TEST(test_truncated_file);

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
