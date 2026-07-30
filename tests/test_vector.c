/* ==========================================================================
 * Achilles-Search | tests/test_vector.c
 * ==========================================================================
 * Unit tests for the dynamic vector.
 *
 * This is a simple test harness — no external testing framework needed.
 * Each test function returns true on success. The main function runs all
 * tests and reports results.
 *
 * WHY NOT USE A TESTING FRAMEWORK
 *   We avoid external dependencies in Phase 0-2. A simple assert-based
 *   harness is sufficient. We can adopt Unity or CMocka later if needed.
 * ========================================================================== */

#include "data/vector.h"
#include "common/types.h"
#include "common/errors.h"
#include "common/config.h"
#include "common/macros.h"
#include "common/utils.h"

#include <stdio.h>
#include <string.h>

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

/* ---- Test: Initialization ----------------------------------------------- */
static bool test_init_basic(void) {
    Vector vec;
    AchErrorCode err = vector_init(&vec, sizeof(int), 16);
    ASSERT(ACH_SUCCEEDED(err));
    ASSERT(vec.data != NULL);
    ASSERT(vec.length == 0);
    ASSERT(vec.capacity == 16);
    ASSERT(vec.element_size == sizeof(int));
    vector_destroy(&vec);
    return true;
}

static bool test_init_default_capacity(void) {
    Vector vec;
    AchErrorCode err = vector_init(&vec, sizeof(double), 0);
    ASSERT(ACH_SUCCEEDED(err));
    ASSERT(vec.capacity == ACH_DEFAULT_VECTOR_CAPACITY);
    vector_destroy(&vec);
    return true;
}

static bool test_init_null_vec(void) {
    AchErrorCode err = vector_init(NULL, sizeof(int), 16);
    ASSERT(err == ACH_ERROR_INVALID_ARG);
    return true;
}

static bool test_init_zero_element_size(void) {
    Vector vec;
    AchErrorCode err = vector_init(&vec, 0, 16);
    ASSERT(err == ACH_ERROR_INVALID_ARG);
    return true;
}

/* ---- Test: Push --------------------------------------------------------- */
static bool test_push_single(void) {
    Vector vec;
    vector_init(&vec, sizeof(int), 4);

    int val = 42;
    AchErrorCode err = vector_push(&vec, &val);
    ASSERT(ACH_SUCCEEDED(err));
    ASSERT(vec.length == 1);

    int *stored = (int*)vector_get(&vec, 0);
    ASSERT(stored != NULL);
    ASSERT(*stored == 42);

    vector_destroy(&vec);
    return true;
}

static bool test_push_multiple(void) {
    Vector vec;
    vector_init(&vec, sizeof(int), 4);

    for (int i = 0; i < 100; i++) {
        AchErrorCode err = vector_push(&vec, &i);
        ASSERT(ACH_SUCCEEDED(err));
    }
    ASSERT(vec.length == 100);
    ASSERT(vec.capacity >= 100);

    /* Verify all values are correct */
    for (int i = 0; i < 100; i++) {
        int *val = (int*)vector_get(&vec, (usize)i);
        ASSERT(val != NULL);
        ASSERT(*val == i);
    }

    vector_destroy(&vec);
    return true;
}

static bool test_push_triggers_growth(void) {
    Vector vec;
    vector_init(&vec, sizeof(int), 2);
    ASSERT(vec.capacity == 2);

    int a = 1, b = 2, c = 3;
    vector_push(&vec, &a);
    vector_push(&vec, &b);
    ASSERT(vec.capacity == 2);

    /* This push should trigger a growth */
    vector_push(&vec, &c);
    ASSERT(vec.capacity > 2);  /* Should have doubled to 4 */
    ASSERT(vec.length == 3);

    /* Verify data survived the reallocation */
    ASSERT(*(int*)vector_get(&vec, 0) == 1);
    ASSERT(*(int*)vector_get(&vec, 1) == 2);
    ASSERT(*(int*)vector_get(&vec, 2) == 3);

    vector_destroy(&vec);
    return true;
}

/* ---- Test: Push with macro ---------------------------------------------- */
static bool test_push_val_macro(void) {
    Vector vec;
    vector_init(&vec, sizeof(double), 4);

    VECTOR_PUSH_VAL(&vec, 3.14, double);
    VECTOR_PUSH_VAL(&vec, 2.71, double);

    ASSERT(vec.length == 2);

    double *v0 = VECTOR_GET_AS(&vec, 0, double);
    double *v1 = VECTOR_GET_AS(&vec, 1, double);
    ASSERT(v0 != NULL && *v0 == 3.14);
    ASSERT(v1 != NULL && *v1 == 2.71);

    vector_destroy(&vec);
    return true;
}

/* ---- Test: Pop ---------------------------------------------------------- */
static bool test_pop_basic(void) {
    Vector vec;
    vector_init(&vec, sizeof(int), 4);

    int a = 10, b = 20, c = 30;
    vector_push(&vec, &a);
    vector_push(&vec, &b);
    vector_push(&vec, &c);

    int popped;
    AchErrorCode err = vector_pop(&vec, &popped);
    ASSERT(ACH_SUCCEEDED(err));
    ASSERT(popped == 30);
    ASSERT(vec.length == 2);

    err = vector_pop(&vec, &popped);
    ASSERT(ACH_SUCCEEDED(err));
    ASSERT(popped == 20);
    ASSERT(vec.length == 1);

    vector_destroy(&vec);
    return true;
}

static bool test_pop_empty(void) {
    Vector vec;
    vector_init(&vec, sizeof(int), 4);

    int out;
    AchErrorCode err = vector_pop(&vec, &out);
    ASSERT(err == ACH_ERROR_NOT_FOUND);

    vector_destroy(&vec);
    return true;
}

static bool test_pop_null_output(void) {
    Vector vec;
    vector_init(&vec, sizeof(int), 4);

    int val = 99;
    vector_push(&vec, &val);

    /* Pop without retrieving the value (discard) */
    AchErrorCode err = vector_pop(&vec, NULL);
    ASSERT(ACH_SUCCEEDED(err));
    ASSERT(vec.length == 0);

    vector_destroy(&vec);
    return true;
}

/* ---- Test: Get / Set ---------------------------------------------------- */
static bool test_get_out_of_bounds(void) {
    Vector vec;
    vector_init(&vec, sizeof(int), 4);

    int val = 5;
    vector_push(&vec, &val);

    ASSERT(vector_get(&vec, 0) != NULL);
    ASSERT(vector_get(&vec, 1) == NULL);  /* Out of bounds */
    ASSERT(vector_get(&vec, 100) == NULL);

    vector_destroy(&vec);
    return true;
}

static bool test_set_basic(void) {
    Vector vec;
    vector_init(&vec, sizeof(int), 4);

    int val = 10;
    vector_push(&vec, &val);
    ASSERT(*(int*)vector_get(&vec, 0) == 10);

    int new_val = 99;
    AchErrorCode err = vector_set(&vec, 0, &new_val);
    ASSERT(ACH_SUCCEEDED(err));
    ASSERT(*(int*)vector_get(&vec, 0) == 99);

    vector_destroy(&vec);
    return true;
}

static bool test_set_out_of_bounds(void) {
    Vector vec;
    vector_init(&vec, sizeof(int), 4);

    int val = 5;
    AchErrorCode err = vector_set(&vec, 0, &val);
    ASSERT(err == ACH_ERROR_INVALID_ARG);  /* No elements yet */

    vector_destroy(&vec);
    return true;
}

/* ---- Test: Size Management ---------------------------------------------- */
static bool test_clear(void) {
    Vector vec;
    vector_init(&vec, sizeof(int), 4);

    for (int i = 0; i < 10; i++) {
        vector_push(&vec, &i);
    }
    ASSERT(vec.length == 10);
    usize old_capacity = vec.capacity;

    vector_clear(&vec);
    ASSERT(vec.length == 0);
    ASSERT(vec.capacity == old_capacity);  /* Capacity unchanged */
    ASSERT(vec.data != NULL);              /* Buffer still allocated */

    vector_destroy(&vec);
    return true;
}

static bool test_reserve(void) {
    Vector vec;
    vector_init(&vec, sizeof(int), 4);

    AchErrorCode err = vector_reserve(&vec, 1000);
    ASSERT(ACH_SUCCEEDED(err));
    ASSERT(vec.capacity >= 1000);
    ASSERT(vec.length == 0);

    /* Reserve less than current capacity — should be a no-op */
    usize cap = vec.capacity;
    err = vector_reserve(&vec, 10);
    ASSERT(ACH_SUCCEEDED(err));
    ASSERT(vec.capacity == cap);

    vector_destroy(&vec);
    return true;
}

static bool test_shrink_to_fit(void) {
    Vector vec;
    vector_init(&vec, sizeof(int), 100);

    for (int i = 0; i < 5; i++) {
        vector_push(&vec, &i);
    }
    ASSERT(vec.capacity == 100);
    ASSERT(vec.length == 5);

    AchErrorCode err = vector_shrink_to_fit(&vec);
    ASSERT(ACH_SUCCEEDED(err));
    ASSERT(vec.capacity == 5);
    ASSERT(vec.length == 5);

    /* Verify data survived */
    for (int i = 0; i < 5; i++) {
        ASSERT(*(int*)vector_get(&vec, (usize)i) == i);
    }

    vector_destroy(&vec);
    return true;
}

static bool test_is_empty(void) {
    Vector vec;
    vector_init(&vec, sizeof(int), 4);
    ASSERT(vector_is_empty(&vec) == true);

    int val = 1;
    vector_push(&vec, &val);
    ASSERT(vector_is_empty(&vec) == false);

    vector_pop(&vec, NULL);
    ASSERT(vector_is_empty(&vec) == true);

    vector_destroy(&vec);
    return true;
}

/* ---- Test: Struct elements ---------------------------------------------- */
typedef struct TestPoint {
    f64 x;
    f64 y;
    i32 id;
} TestPoint;

static bool test_struct_elements(void) {
    Vector vec;
    vector_init(&vec, sizeof(TestPoint), 4);

    TestPoint p1 = { .x = 1.5, .y = 2.5, .id = 1 };
    TestPoint p2 = { .x = 3.0, .y = 4.0, .id = 2 };
    TestPoint p3 = { .x = 5.5, .y = 6.5, .id = 3 };

    vector_push(&vec, &p1);
    vector_push(&vec, &p2);
    vector_push(&vec, &p3);

    TestPoint *stored = VECTOR_GET_AS(&vec, 1, TestPoint);
    ASSERT(stored != NULL);
    ASSERT(stored->x == 3.0);
    ASSERT(stored->y == 4.0);
    ASSERT(stored->id == 2);

    vector_destroy(&vec);
    return true;
}

/* ---- Test: Destroy is idempotent ---------------------------------------- */
static bool test_destroy_idempotent(void) {
    Vector vec;
    vector_init(&vec, sizeof(int), 4);

    int val = 1;
    vector_push(&vec, &val);

    vector_destroy(&vec);
    ASSERT(vec.data == NULL);
    ASSERT(vec.length == 0);
    ASSERT(vec.capacity == 0);

    /* Calling destroy again should not crash */
    vector_destroy(&vec);
    return true;
}

/* ---- Test: data_as macro for iteration ---------------------------------- */
static bool test_data_as_iteration(void) {
    Vector vec;
    vector_init(&vec, sizeof(int), 8);

    for (int i = 0; i < 5; i++) {
        vector_push(&vec, &i);
    }

    /* Use VECTOR_DATA_AS for direct array-style access */
    int *data = VECTOR_DATA_AS(&vec, int);
    int sum = 0;
    for (usize i = 0; i < vector_length(&vec); i++) {
        sum += data[i];
    }
    ASSERT(sum == 0 + 1 + 2 + 3 + 4);  /* 10 */

    vector_destroy(&vec);
    return true;
}

static bool test_push_many(void) {
    Vector vec;
    vector_init(&vec, sizeof(int), 2);

    int vals[5] = {10, 20, 30, 40, 50};
    AchErrorCode err = vector_push_many(&vec, vals, 5);
    ASSERT(ACH_SUCCEEDED(err));
    ASSERT(vec.length == 5);
    ASSERT(vec.capacity >= 5);

    for (int i = 0; i < 5; i++) {
        int *v = (int*)vector_get(&vec, i);
        ASSERT(v != NULL);
        ASSERT(*v == vals[i]);
    }

    vector_destroy(&vec);
    return true;
}

/* ---- Main --------------------------------------------------------------- */
int main(void) {
    printf("\n");
    printf("============================================================\n");
    printf("  Achilles-Search | Vector Test Suite\n");
    printf("============================================================\n\n");

    /* Initialization */
    TEST(test_init_basic);
    TEST(test_init_default_capacity);
    TEST(test_init_null_vec);
    TEST(test_init_zero_element_size);

    /* Push */
    TEST(test_push_single);
    TEST(test_push_multiple);
    TEST(test_push_triggers_growth);
    TEST(test_push_val_macro);
    TEST(test_push_many);

    /* Pop */
    TEST(test_pop_basic);
    TEST(test_pop_empty);
    TEST(test_pop_null_output);

    /* Get / Set */
    TEST(test_get_out_of_bounds);
    TEST(test_set_basic);
    TEST(test_set_out_of_bounds);

    /* Size Management */
    TEST(test_clear);
    TEST(test_reserve);
    TEST(test_shrink_to_fit);
    TEST(test_is_empty);

    /* Complex Types */
    TEST(test_struct_elements);

    /* Edge Cases */
    TEST(test_destroy_idempotent);
    TEST(test_data_as_iteration);

    /* Summary */
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
