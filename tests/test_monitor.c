/* ==========================================================================
 * Achilles-Search | tests/test_monitor.c
 * ==========================================================================
 * Unit tests for the Filesystem Monitor (Real-time Watcher) module.
 * ========================================================================== */

#include "core/monitor.h"
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

/* ---- Shared Test Context ------------------------------------------------ */
typedef struct TestCallbackCtx {
    volatile bool seen_added;
    volatile bool seen_removed;
    volatile bool seen_modified;
    volatile bool seen_renamed;
    wchar_t       last_old_path[MAX_PATH];
    wchar_t       last_new_path[MAX_PATH];
    volatile LONG event_count;
} TestCallbackCtx;

static void test_monitor_cb(AchMonitorAction action, const wchar_t *old_path, const wchar_t *new_path, void *context) {
    TestCallbackCtx *ctx = (TestCallbackCtx*)context;
    if (ctx == NULL) return;

    switch (action) {
        case ACH_MONITOR_ADDED:    ctx->seen_added = true; break;
        case ACH_MONITOR_REMOVED:  ctx->seen_removed = true; break;
        case ACH_MONITOR_MODIFIED: ctx->seen_modified = true; break;
        case ACH_MONITOR_RENAMED:  ctx->seen_renamed = true; break;
    }

    if (old_path != NULL) {
        wcscpy_s(ctx->last_old_path, MAX_PATH, old_path);
    } else {
        ctx->last_old_path[0] = L'\0';
    }

    if (new_path != NULL) {
        wcscpy_s(ctx->last_new_path, MAX_PATH, new_path);
    } else {
        ctx->last_new_path[0] = L'\0';
    }

    InterlockedIncrement(&ctx->event_count);
}

/* ---- Test Case: Lifecycle ----------------------------------------------- */
static bool test_monitor_lifecycle(void) {
    Monitor monitor;
    TestCallbackCtx ctx = {0};

    const wchar_t *watch_dir = L"test_watch_lifecycle";
    CreateDirectoryW(watch_dir, NULL);

    AchErrorCode err = monitor_init(&monitor, watch_dir, test_monitor_cb, &ctx);
    ASSERT(err == ACH_SUCCESS);

    err = monitor_start(&monitor);
    ASSERT(err == ACH_SUCCESS);
    ASSERT(monitor.running);

    /* Allow the thread to initialize and register the handle */
    Sleep(100);

    monitor_stop(&monitor);
    ASSERT(!monitor.running);

    monitor_destroy(&monitor);
    RemoveDirectoryW(watch_dir);

    return true;
}

/* ---- Test Case: Live File Actions -------------------------------------- */
static bool test_monitor_file_actions(void) {
    Monitor monitor;
    TestCallbackCtx ctx = {0};

    const wchar_t *watch_dir = L"test_watch_live";
    _wremove(L"test_watch_live\\file.txt");
    _wremove(L"test_watch_live\\file_renamed.txt");
    RemoveDirectoryW(watch_dir);

    /* Create directory */
    BOOL dir_ok = CreateDirectoryW(watch_dir, NULL);
    ASSERT(dir_ok);

    AchErrorCode err = monitor_init(&monitor, watch_dir, test_monitor_cb, &ctx);
    ASSERT(err == ACH_SUCCESS);

    err = monitor_start(&monitor);
    ASSERT(err == ACH_SUCCESS);

    /* Let background thread call ReadDirectoryChangesW first */
    Sleep(150);

    /* 1. Test ADDED (which may also trigger a MODIFIED event depending on writes) */
    const wchar_t *file_path = L"test_watch_live\\file.txt";
    FILE *f = _wfopen(file_path, L"w");
    ASSERT(f != NULL);
    fwprintf(f, L"Initial text");
    fclose(f);

    /* Wait for event propagation */
    int retries = 20;
    while (!ctx.seen_added && retries-- > 0) {
        Sleep(50);
    }
    ASSERT(ctx.seen_added);
    ASSERT(wcsstr(ctx.last_new_path, L"file.txt") != NULL);

    /* 2. Test MODIFIED */
    ctx.seen_modified = false;
    f = _wfopen(file_path, L"a");
    ASSERT(f != NULL);
    fwprintf(f, L"Appended content");
    fclose(f);

    retries = 20;
    while (!ctx.seen_modified && retries-- > 0) {
        Sleep(50);
    }
    ASSERT(ctx.seen_modified);
    ASSERT(wcsstr(ctx.last_new_path, L"file.txt") != NULL);

    /* 3. Test RENAMED */
    const wchar_t *renamed_path = L"test_watch_live\\file_renamed.txt";
    _wrename(file_path, renamed_path);

    retries = 20;
    while (!ctx.seen_renamed && retries-- > 0) {
        Sleep(50);
    }
    ASSERT(ctx.seen_renamed);
    ASSERT(wcsstr(ctx.last_old_path, L"file.txt") != NULL);
    ASSERT(wcsstr(ctx.last_new_path, L"file_renamed.txt") != NULL);

    /* 4. Test REMOVED */
    _wremove(renamed_path);

    retries = 20;
    while (!ctx.seen_removed && retries-- > 0) {
        Sleep(50);
    }
    ASSERT(ctx.seen_removed);
    ASSERT(wcsstr(ctx.last_new_path, L"file_renamed.txt") != NULL);

    /* Clean up */
    monitor_stop(&monitor);
    monitor_destroy(&monitor);
    RemoveDirectoryW(watch_dir);

    return true;
}

/* ---- Main --------------------------------------------------------------- */
int main(void) {
    printf("\n");
    printf("============================================================\n");
    printf("  Achilles-Search | Filesystem Monitor Test Suite\n");
    printf("============================================================\n\n");

    TEST(test_monitor_lifecycle);
    TEST(test_monitor_file_actions);

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
