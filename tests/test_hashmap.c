/* ==========================================================================
 * Achilles-Search | tests/test_hashmap.c
 * ==========================================================================
 * Unit tests for the Hash Map implementation.
 * ========================================================================== */

#include "data/hashmap.h"
#include "common/types.h"
#include "common/errors.h"
#include "common/utils.h"

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
static bool test_init_basic(void) {
    HashMap map;
    AchErrorCode err = hashmap_init(&map, sizeof(int), 16);
    ASSERT(ACH_SUCCEEDED(err));
    ASSERT(hashmap_size(&map) == 0);
    ASSERT(hashmap_capacity(&map) == 16);
    hashmap_destroy(&map);
    return true;
}

static bool test_init_default_capacity(void) {
    HashMap map;
    AchErrorCode err = hashmap_init(&map, sizeof(int), 0);
    ASSERT(ACH_SUCCEEDED(err));
    ASSERT(hashmap_capacity(&map) == ACH_HASHMAP_DEFAULT_CAPACITY);
    hashmap_destroy(&map);
    return true;
}

static bool test_init_null_map(void) {
    AchErrorCode err = hashmap_init(NULL, sizeof(int), 16);
    ASSERT(err == ACH_ERROR_INVALID_ARG);
    return true;
}

static bool test_init_zero_value_size(void) {
    HashMap map;
    AchErrorCode err = hashmap_init(&map, 0, 16);
    ASSERT(err == ACH_ERROR_INVALID_ARG);
    return true;
}

/* ---- Test Case: Put and Get --------------------------------------------- */
static bool test_put_and_get_basic(void) {
    HashMap map;
    hashmap_init(&map, sizeof(int), 16);

    int val1 = 42;
    int val2 = 100;
    AchErrorCode err = hashmap_put(&map, L"apple", &val1);
    ASSERT(ACH_SUCCEEDED(err));
    ASSERT(hashmap_size(&map) == 1);

    err = hashmap_put(&map, L"banana", &val2);
    ASSERT(ACH_SUCCEEDED(err));
    ASSERT(hashmap_size(&map) == 2);

    int out = 0;
    err = hashmap_get(&map, L"apple", &out);
    ASSERT(ACH_SUCCEEDED(err));
    ASSERT(out == 42);

    err = hashmap_get(&map, L"banana", &out);
    ASSERT(ACH_SUCCEEDED(err));
    ASSERT(out == 100);

    /* Get nonexistent key */
    err = hashmap_get(&map, L"orange", &out);
    ASSERT(err == ACH_ERROR_NOT_FOUND);

    hashmap_destroy(&map);
    return true;
}

static bool test_put_update(void) {
    HashMap map;
    hashmap_init(&map, sizeof(int), 16);

    int val = 10;
    hashmap_put(&map, L"key", &val);
    ASSERT(hashmap_size(&map) == 1);

    int new_val = 20;
    hashmap_put(&map, L"key", &new_val);
    ASSERT(hashmap_size(&map) == 1); // Size should not change

    int out = 0;
    hashmap_get(&map, L"key", &out);
    ASSERT(out == 20);

    hashmap_destroy(&map);
    return true;
}

/* ---- Test Case: Remove and Contains ------------------------------------- */
static bool test_remove_and_contains(void) {
    HashMap map;
    hashmap_init(&map, sizeof(int), 16);

    int val = 55;
    hashmap_put(&map, L"target", &val);
    ASSERT(hashmap_contains(&map, L"target") == true);
    ASSERT(hashmap_contains(&map, L"other") == false);

    AchErrorCode err = hashmap_remove(&map, L"target");
    ASSERT(ACH_SUCCEEDED(err));
    ASSERT(hashmap_size(&map) == 0);
    ASSERT(hashmap_contains(&map, L"target") == false);

    /* Double remove */
    err = hashmap_remove(&map, L"target");
    ASSERT(err == ACH_ERROR_NOT_FOUND);

    hashmap_destroy(&map);
    return true;
}

/* ---- Test Case: Tombstone Reuse ----------------------------------------- */
static bool test_tombstone_reuse(void) {
    HashMap map;
    hashmap_init(&map, sizeof(int), 16);

    int val1 = 1;
    int val2 = 2;
    int val3 = 3;

    hashmap_put(&map, L"k1", &val1);
    hashmap_put(&map, L"k2", &val2);
    hashmap_remove(&map, L"k1"); // Creates a tombstone

    // Next insert should reuse the slot or be safe
    AchErrorCode err = hashmap_put(&map, L"k3", &val3);
    ASSERT(ACH_SUCCEEDED(err));
    ASSERT(hashmap_size(&map) == 2);

    int out = 0;
    ASSERT(hashmap_get(&map, L"k3", &out) == ACH_SUCCESS && out == 3);
    ASSERT(hashmap_get(&map, L"k2", &out) == ACH_SUCCESS && out == 2);
    ASSERT(hashmap_get(&map, L"k1", &out) == ACH_ERROR_NOT_FOUND);

    hashmap_destroy(&map);
    return true;
}

/* ---- Test Case: Clear --------------------------------------------------- */
static bool test_clear(void) {
    HashMap map;
    hashmap_init(&map, sizeof(int), 16);

    int val = 99;
    hashmap_put(&map, L"k1", &val);
    hashmap_put(&map, L"k2", &val);
    ASSERT(hashmap_size(&map) == 2);

    hashmap_clear(&map);
    ASSERT(hashmap_size(&map) == 0);
    ASSERT(hashmap_capacity(&map) == 16);
    ASSERT(hashmap_contains(&map, L"k1") == false);

    hashmap_destroy(&map);
    return true;
}

/* ---- Test Case: Resize (Dynamic Rehashing) ------------------------------- */
static bool test_resize_automatic(void) {
    HashMap map;
    /* Initialize with default capacity (16) */
    hashmap_init(&map, sizeof(int), 16);
    ASSERT(hashmap_capacity(&map) == 16);

    int values[20];
    wchar_t keys[20][8];

    /* Insert 20 elements. Since load factor threshold is 70% (11 items max for capacity 16),
     * inserting the 12th element will trigger a resize to 32. */
    for (int i = 0; i < 20; i++) {
        values[i] = i * 10;
        swprintf(keys[i], 8, L"key_%d", i);
        AchErrorCode err = hashmap_put(&map, keys[i], &values[i]);
        ASSERT(ACH_SUCCEEDED(err));
    }

    /* Verify capacity grew to 32 */
    ASSERT(hashmap_capacity(&map) == 32);
    ASSERT(hashmap_size(&map) == 20);

    /* Verify all elements survived and are lookupable */
    for (int i = 0; i < 20; i++) {
        int out = -1;
        AchErrorCode err = hashmap_get(&map, keys[i], &out);
        ASSERT(ACH_SUCCEEDED(err));
        ASSERT(out == i * 10);
    }

    hashmap_destroy(&map);
    return true;
}

/* ---- Test Case: Iteration ----------------------------------------------- */
static bool test_iteration(void) {
    HashMap map;
    hashmap_init(&map, sizeof(int), 16);

    int val = 77;
    hashmap_put(&map, L"k1", &val);
    hashmap_put(&map, L"k2", &val);
    hashmap_put(&map, L"k3", &val);

    HashMapIterator iter = hashmap_iter(&map);
    const wchar_t *key;
    int out;
    int count = 0;

    while (hashmap_iter_next(&iter, &key, &out)) {
        ASSERT(key != NULL);
        ASSERT(out == 77);
        count++;
    }

    ASSERT(count == 3);

    hashmap_destroy(&map);
    return true;
}

/* ---- Test Case: Complex Struct Values ----------------------------------- */
typedef struct TestNode {
    double x;
    double y;
    wchar_t label[16];
} TestNode;

static bool test_struct_values(void) {
    HashMap map;
    hashmap_init(&map, sizeof(TestNode), 8);

    TestNode n1 = { .x = 1.0, .y = 2.0, .label = L"Node1" };
    TestNode n2 = { .x = 3.5, .y = -4.5, .label = L"Node2" };

    hashmap_put(&map, L"first", &n1);
    hashmap_put(&map, L"second", &n2);

    TestNode out;
    AchErrorCode err = hashmap_get(&map, L"first", &out);
    ASSERT(ACH_SUCCEEDED(err));
    ASSERT(out.x == 1.0);
    ASSERT(out.y == 2.0);
    ASSERT(wcscmp(out.label, L"Node1") == 0);

    err = hashmap_get(&map, L"second", &out);
    ASSERT(ACH_SUCCEEDED(err));
    ASSERT(out.x == 3.5);
    ASSERT(out.y == -4.5);
    ASSERT(wcscmp(out.label, L"Node2") == 0);

    hashmap_destroy(&map);
    return true;
}

/* ---- Test Case: Destroy Idempotency ------------------------------------- */
static bool test_destroy_idempotency(void) {
    HashMap map;
    hashmap_init(&map, sizeof(int), 8);
    hashmap_destroy(&map);

    /* Second call should not crash */
    hashmap_destroy(&map);
    return true;
}

/* ---- Main --------------------------------------------------------------- */
int main(void) {
    printf("\n");
    printf("============================================================\n");
    printf("  Achilles-Search | Hash Map Test Suite\n");
    printf("============================================================\n\n");

    /* Lifecycle */
    TEST(test_init_basic);
    TEST(test_init_default_capacity);
    TEST(test_init_null_map);
    TEST(test_init_zero_value_size);

    /* Operations */
    TEST(test_put_and_get_basic);
    TEST(test_put_update);
    TEST(test_remove_and_contains);
    TEST(test_tombstone_reuse);
    TEST(test_clear);

    /* Resize & Iteration */
    TEST(test_resize_automatic);
    TEST(test_iteration);

    /* Edge Cases & Types */
    TEST(test_struct_values);
    TEST(test_destroy_idempotency);

    printf("\n------------------------------------------------------------\n");
    printf("  Results: %d / %d passed", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("  --  ALL PASSED\n");
    } else {
        printf("  --  %d FAILED\n", tests_run - tests_passed);
    }
    printf("------------------------------------------------------------\n\n");

    int exit_code = (tests_passed == tests_run) ? 0 : 1;

    if (utils_should_pause_on_exit()) {
        printf("\nPress Enter to exit...");
        getchar();
    }

    return exit_code;
}
