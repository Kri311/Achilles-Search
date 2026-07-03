/* ==========================================================================
 * Achilles-Search | tests/test_scanner.c
 * ==========================================================================
 * Tests for the filesystem scanner.
 *
 * STRATEGY:
 *   We scan the Achilles-Search project directory itself. We know its
 *   structure, so we can verify the scanner finds expected files and dirs.
 *   This avoids creating temp directories and works in any build environment.
 * ========================================================================== */

#include "core/scanner.h"
#include "common/types.h"
#include "common/errors.h"
#include "common/config.h"
#include "common/logger.h"
#include "common/utils.h"

#include <stdio.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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

/* ---- Robust Test Path Resolution ---------------------------------------- */
static wchar_t test_root_path[MAX_PATH] = L".";

/* Find a directory that actually has subdirectories to scan.
 * If we run the test executable directly from the build output directory
 * (e.g. build/bin), there are no subdirectories. We resolve the exe's folder
 * and climb up the tree (up to 3 levels) until we find a directory with subdirectories.
 */
static void get_test_scan_root(wchar_t *out_path, usize max_len) {
    wcsncpy(out_path, L".", max_len);
    out_path[max_len - 1] = L'\0';

    wchar_t exe_path[MAX_PATH];
    DWORD res = GetModuleFileNameW(NULL, exe_path, MAX_PATH);
    if (res == 0) {
        return;
    }

    wchar_t *last_slash = wcsrchr(exe_path, L'\\');
    if (last_slash != NULL) {
        *last_slash = L'\0';
    }

    wchar_t current_dir[MAX_PATH];
    wcsncpy(current_dir, exe_path, MAX_PATH);
    current_dir[MAX_PATH - 1] = L'\0';

    for (int i = 0; i < 3; i++) {
        wchar_t search_pattern[MAX_PATH];
        int written = swprintf(search_pattern, MAX_PATH, L"%ls\\*", current_dir);
        if (written < 0 || written >= MAX_PATH) {
            break;
        }

        WIN32_FIND_DATAW find_data;
        HANDLE hFind = FindFirstFileW(search_pattern, &find_data);
        bool has_subdirs = false;

        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                bool is_directory = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                if (is_directory) {
                    if (wcscmp(find_data.cFileName, L".") != 0 && wcscmp(find_data.cFileName, L"..") != 0) {
                        has_subdirs = true;
                        break;
                    }
                }
            } while (FindNextFileW(hFind, &find_data));
            FindClose(hFind);
        }

        if (has_subdirs) {
            wcsncpy(out_path, current_dir, max_len);
            out_path[max_len - 1] = L'\0';
            return;
        }

        wchar_t *parent_slash = wcsrchr(current_dir, L'\\');
        if (parent_slash == NULL) {
            break;
        }
        *parent_slash = L'\0';
    }
}

/* ---- Test: Initialization ----------------------------------------------- */
static bool test_scanner_init(void) {
    Scanner scanner;
    AchErrorCode err = scanner_init(&scanner);
    ASSERT(ACH_SUCCEEDED(err));
    ASSERT(scanner.initialized == true);
    scanner_destroy(&scanner);
    return true;
}

static bool test_scanner_init_null(void) {
    AchErrorCode err = scanner_init(NULL);
    ASSERT(err == ACH_ERROR_INVALID_ARG);
    return true;
}

static bool test_scanner_destroy_safe(void) {
    /* Destroying a never-initialized scanner should not crash */
    Scanner scanner;
    memset(&scanner, 0, sizeof(Scanner));
    scanner_destroy(&scanner);

    /* Double destroy should also be safe */
    scanner_init(&scanner);
    scanner_destroy(&scanner);
    scanner_destroy(&scanner);
    return true;
}

/* ---- Test: Scan the project directory ----------------------------------- */
static bool test_scan_project_dir(void) {
    Scanner scanner;
    scanner_init(&scanner);

    /* Scan the project's src/ directory (small, known structure) */
    ScanConfig config = {
        .root_path      = test_root_path,
        .max_depth       = 2,  /* Only go 2 levels deep */
        .include_hidden  = false,
        .include_system  = false,
    };

    AchErrorCode err = scanner_scan(&scanner, &config);
    ASSERT(ACH_SUCCEEDED(err));

    /* We should find at least some files and directories */
    const ScanStats *stats = scanner_get_stats(&scanner);
    ASSERT(stats != NULL);
    ASSERT(stats->files_found > 0);
    ASSERT(stats->dirs_found > 0);
    ASSERT(stats->duration_ms >= 0.0);

    /* Results vector should not be empty */
    const Vector *results = scanner_get_results(&scanner);
    ASSERT(results != NULL);
    ASSERT(vector_length(results) > 0);
    ASSERT(vector_length(results) == stats->files_found + stats->dirs_found);

    scanner_destroy(&scanner);
    return true;
}

/* ---- Test: Scan nonexistent path ---------------------------------------- */
static bool test_scan_nonexistent_path(void) {
    Scanner scanner;
    scanner_init(&scanner);

    ScanConfig config = {
        .root_path      = L"C:\\This\\Path\\Does\\Not\\Exist\\At\\All",
        .max_depth       = 0,
        .include_hidden  = false,
        .include_system  = false,
    };

    AchErrorCode err = scanner_scan(&scanner, &config);
    ASSERT(err == ACH_ERROR_FILE_NOT_FOUND);

    scanner_destroy(&scanner);
    return true;
}

/* ---- Test: Null config -------------------------------------------------- */
static bool test_scan_null_config(void) {
    Scanner scanner;
    scanner_init(&scanner);

    AchErrorCode err = scanner_scan(&scanner, NULL);
    ASSERT(err == ACH_ERROR_INVALID_ARG);

    scanner_destroy(&scanner);
    return true;
}

/* ---- Test: Scan uninitialized scanner ----------------------------------- */
static bool test_scan_uninitialized(void) {
    Scanner scanner;
    memset(&scanner, 0, sizeof(Scanner));

    ScanConfig config = {
        .root_path = test_root_path,
        .max_depth = 1,
        .include_hidden = false,
        .include_system = false,
    };

    AchErrorCode err = scanner_scan(&scanner, &config);
    ASSERT(ACH_FAILED(err));

    return true;
}

/* ---- Test: Depth limit -------------------------------------------------- */
static bool test_scan_depth_limit(void) {
    Scanner scanner_shallow, scanner_deep;
    scanner_init(&scanner_shallow);
    scanner_init(&scanner_deep);

    ScanConfig shallow_config = {
        .root_path      = test_root_path,
        .max_depth       = 1,   /* Only immediate children */
        .include_hidden  = false,
        .include_system  = false,
    };

    ScanConfig deep_config = {
        .root_path      = test_root_path,
        .max_depth       = 0,   /* Unlimited */
        .include_hidden  = false,
        .include_system  = false,
    };

    scanner_scan(&scanner_shallow, &shallow_config);
    scanner_scan(&scanner_deep, &deep_config);

    const ScanStats *shallow_stats = scanner_get_stats(&scanner_shallow);
    const ScanStats *deep_stats = scanner_get_stats(&scanner_deep);

    /* A deeper scan should find at least as many entries as a shallow one */
    u64 shallow_total = shallow_stats->files_found + shallow_stats->dirs_found;
    u64 deep_total = deep_stats->files_found + deep_stats->dirs_found;
    ASSERT(deep_total >= shallow_total);

    scanner_destroy(&scanner_shallow);
    scanner_destroy(&scanner_deep);
    return true;
}

/* ---- Test: Clear results ------------------------------------------------ */
static bool test_clear_results(void) {
    Scanner scanner;
    scanner_init(&scanner);

    ScanConfig config = {
        .root_path      = test_root_path,
        .max_depth       = 1,
        .include_hidden  = false,
        .include_system  = false,
    };

    scanner_scan(&scanner, &config);
    ASSERT(vector_length(&scanner.results) > 0);

    scanner_clear_results(&scanner);
    ASSERT(vector_length(&scanner.results) == 0);

    const ScanStats *stats = scanner_get_stats(&scanner);
    ASSERT(stats->files_found == 0);
    ASSERT(stats->dirs_found == 0);

    scanner_destroy(&scanner);
    return true;
}

/* ---- Test: FileInfo fields are populated -------------------------------- */
static bool test_fileinfo_fields(void) {
    Scanner scanner;
    scanner_init(&scanner);

    ScanConfig config = {
        .root_path      = test_root_path,
        .max_depth       = 1,
        .include_hidden  = false,
        .include_system  = false,
    };

    scanner_scan(&scanner, &config);

    const Vector *results = scanner_get_results(&scanner);
    ASSERT(vector_length(results) > 0);

    /* Check first entry has valid fields */
    const FileInfo *info = (const FileInfo*)vector_get(results, 0);
    ASSERT(info != NULL);
    ASSERT(info->path != NULL);
    ASSERT(wcslen(info->path) > 0);
    ASSERT(info->last_modified > 0);      /* Should have a timestamp */
    ASSERT(info->attributes != 0);         /* Should have some attributes */

    /* If it's a file, it should have size > 0 (project has no empty files) */
    /* If it's a dir, size should be 0 */
    if (info->is_directory) {
        ASSERT(info->file_size == 0);
        ASSERT(info->attributes & FILE_ATTRIBUTE_DIRECTORY);
    }

    scanner_destroy(&scanner);
    return true;
}

/* ---- Test: Rescan (scan multiple times) --------------------------------- */
static bool test_rescan(void) {
    Scanner scanner;
    scanner_init(&scanner);

    ScanConfig config = {
        .root_path      = test_root_path,
        .max_depth       = 1,
        .include_hidden  = false,
        .include_system  = false,
    };

    /* First scan */
    scanner_scan(&scanner, &config);
    usize first_count = vector_length(&scanner.results);
    ASSERT(first_count > 0);

    /* Second scan appends to results */
    scanner_scan(&scanner, &config);
    usize second_count = vector_length(&scanner.results);
    ASSERT(second_count > first_count);

    /* Clear and rescan should give same count as first */
    scanner_clear_results(&scanner);
    scanner_scan(&scanner, &config);
    usize third_count = vector_length(&scanner.results);
    ASSERT(third_count == first_count);

    scanner_destroy(&scanner);
    return true;
}

/* ---- Main --------------------------------------------------------------- */
int main(void) {
    /* Resolve robust test root path */
    get_test_scan_root(test_root_path, MAX_PATH);

    /* Initialize logger for scanner log messages */
    LoggerConfig log_config = {
        .min_level     = LOG_LEVEL_WARNING,  /* Only show warnings/errors */
        .log_file_path = NULL,
        .use_colors    = true,
    };
    logger_init(&log_config);

    printf("\n");
    printf("============================================================\n");
    printf("  Achilles-Search | Scanner Test Suite\n");
    printf("============================================================\n\n");

    /* Initialization */
    TEST(test_scanner_init);
    TEST(test_scanner_init_null);
    TEST(test_scanner_destroy_safe);

    /* Scanning */
    TEST(test_scan_project_dir);
    TEST(test_scan_nonexistent_path);
    TEST(test_scan_null_config);
    TEST(test_scan_uninitialized);
    TEST(test_scan_depth_limit);

    /* Results management */
    TEST(test_clear_results);
    TEST(test_fileinfo_fields);
    TEST(test_rescan);

    /* Summary */
    printf("\n------------------------------------------------------------\n");
    printf("  Results: %d / %d passed", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("  --  ALL PASSED\n");
    } else {
        printf("  --  %d FAILED\n", tests_run - tests_passed);
    }
    printf("------------------------------------------------------------\n\n");

    logger_shutdown();

    int exit_code = (tests_passed == tests_run) ? 0 : 1;

    if (utils_should_pause_on_exit()) {
        printf("\nPress Enter to exit...");
        getchar();
    }

    return exit_code;
}
