/* ==========================================================================
 * Achilles-Search | src/data/vector.c
 * ==========================================================================
 * Implementation of the generic dynamic vector.
 *
 * GROWTH STRATEGY:
 *   When a push would exceed capacity, we double it. This gives amortized
 *   O(1) push at the cost of up to 50% unused capacity. The alternative
 *   (grow by 1) gives O(N^2) total copies — unacceptable for 10M files.
 *
 * OVERFLOW PROTECTION:
 *   Before every allocation, we check that (capacity * element_size) won't
 *   overflow size_t. An overflow here would produce a small allocation that
 *   the caller then writes past → buffer overflow → security vulnerability.
 *
 * MEMORY:
 *   We use malloc/realloc/free from the standard library. In a future phase,
 *   hot-path vectors may use the arena allocator instead, but for now the
 *   standard allocator is fine — it's well-optimized on Windows (the NT heap
 *   uses a Low Fragmentation Heap by default since Vista).
 * ========================================================================== */

#include "data/vector.h"
#include "common/config.h"
#include "common/macros.h"

#include <stdlib.h>     /* malloc, realloc, free */
#include <string.h>     /* memcpy, memset */

/* ---- Internal Helpers --------------------------------------------------- */

/* Compute the byte offset of element at 'index' within the buffer.
 *
 * WHY a helper?
 *   The expression (char*)data + index * element_size appears in multiple
 *   places. Centralizing it prevents copy-paste errors and makes the
 *   arithmetic explicit.
 *
 *   We cast to (char*) because void* arithmetic is undefined in standard C
 *   (GCC allows it as an extension, but we don't rely on extensions).
 */
static inline void* vector_element_ptr(const Vector *vec, usize index) {
    return (char*)vec->data + (index * vec->element_size);
}

/* Check if a new capacity would overflow when multiplied by element_size.
 * Returns true if safe, false if it would overflow.
 */
static inline bool vector_capacity_is_safe(usize capacity, usize element_size) {
    /* If element_size is 0, something is very wrong (caught at init) */
    if (element_size == 0) return false;
    return capacity <= (SIZE_MAX / element_size);
}

/* Grow the internal buffer to at least 'min_capacity'.
 * Uses geometric growth (double) but ensures at least min_capacity.
 *
 * Returns ACH_SUCCESS or ACH_ERROR_OUT_OF_MEMORY.
 */
static AchErrorCode vector_grow(Vector *vec, usize min_capacity) {
    /* Calculate new capacity: double current, but at least min_capacity */
    usize new_capacity = vec->capacity * 2;

    /* Handle overflow from doubling */
    if (new_capacity < vec->capacity) {
        new_capacity = min_capacity;  /* Doubling overflowed, use exact */
    }

    /* Ensure we meet the minimum */
    if (new_capacity < min_capacity) {
        new_capacity = min_capacity;
    }

    /* Safety check: would new_capacity * element_size overflow? */
    if (!vector_capacity_is_safe(new_capacity, vec->element_size)) {
        return ACH_ERROR_OUT_OF_MEMORY;
    }

    usize new_size_bytes = new_capacity * vec->element_size;

    /* realloc: if data is NULL, behaves like malloc.
     * If it succeeds, the old block is freed (or extended in place).
     * If it fails, the old block is PRESERVED (we don't lose data). */
    void *new_data = realloc(vec->data, new_size_bytes);
    if (new_data == NULL) {
        return ACH_ERROR_OUT_OF_MEMORY;
    }

    vec->data = new_data;
    vec->capacity = new_capacity;
    return ACH_SUCCESS;
}

/* ---- Lifecycle ---------------------------------------------------------- */

AchErrorCode vector_init(Vector *vec, usize element_size, usize initial_capacity) {
    if (vec == NULL) {
        return ACH_ERROR_INVALID_ARG;
    }

    if (element_size == 0) {
        return ACH_ERROR_INVALID_ARG;
    }

    /* Use default capacity if none specified */
    if (initial_capacity == 0) {
        initial_capacity = ACH_DEFAULT_VECTOR_CAPACITY;
    }

    /* Overflow check before allocation */
    if (!vector_capacity_is_safe(initial_capacity, element_size)) {
        return ACH_ERROR_OUT_OF_MEMORY;
    }

    usize alloc_size = initial_capacity * element_size;
    void *data = malloc(alloc_size);
    if (data == NULL) {
        return ACH_ERROR_OUT_OF_MEMORY;
    }

    vec->data = data;
    vec->length = 0;
    vec->capacity = initial_capacity;
    vec->element_size = element_size;

    return ACH_SUCCESS;
}

void vector_destroy(Vector *vec) {
    if (vec == NULL) {
        return;
    }

    /* Free the data buffer (free(NULL) is safe per C standard) */
    free(vec->data);

    /* Zero out the struct to prevent use-after-free.
     * Any subsequent vector_get() will return NULL (data == NULL).
     * Any subsequent vector_push() will fail (element_size == 0). */
    vec->data = NULL;
    vec->length = 0;
    vec->capacity = 0;
    vec->element_size = 0;
}

/* ---- Adding / Removing -------------------------------------------------- */

AchErrorCode vector_push(Vector *vec, const void *element) {
    if (vec == NULL || element == NULL) {
        return ACH_ERROR_INVALID_ARG;
    }

    if (vec->element_size == 0) {
        return ACH_ERROR_INVALID_ARG;  /* Not initialized */
    }

    /* Grow if full */
    if (vec->length >= vec->capacity) {
        AchErrorCode err = vector_grow(vec, vec->length + 1);
        if (ACH_FAILED(err)) {
            return err;
        }
    }

    /* Copy element into the buffer at position [length] */
    void *dest = vector_element_ptr(vec, vec->length);
    memcpy(dest, element, vec->element_size);
    vec->length++;

    return ACH_SUCCESS;
}

AchErrorCode vector_push_many(Vector *vec, const void *elements, usize count) {
    if (vec == NULL || elements == NULL) {
        return ACH_ERROR_INVALID_ARG;
    }

    if (vec->element_size == 0) {
        return ACH_ERROR_INVALID_ARG; /* Not initialized */
    }

    if (count == 0) {
        return ACH_SUCCESS;
    }

    usize new_length = vec->length + count;
    if (new_length > vec->capacity) {
        AchErrorCode err = vector_grow(vec, new_length);
        if (ACH_FAILED(err)) {
            return err;
        }
    }

    void *dest = vector_element_ptr(vec, vec->length);
    memcpy(dest, elements, count * vec->element_size);
    vec->length = new_length;

    return ACH_SUCCESS;
}

AchErrorCode vector_pop(Vector *vec, void *out_element) {
    if (vec == NULL) {
        return ACH_ERROR_INVALID_ARG;
    }

    if (vec->length == 0) {
        return ACH_ERROR_NOT_FOUND;
    }

    vec->length--;

    /* Copy the removed element out if the caller wants it */
    if (out_element != NULL) {
        void *src = vector_element_ptr(vec, vec->length);
        memcpy(out_element, src, vec->element_size);
    }

    return ACH_SUCCESS;
}

/* ---- Element Access ----------------------------------------------------- */

void* vector_get(const Vector *vec, usize index) {
    if (vec == NULL || vec->data == NULL || index >= vec->length) {
        return NULL;
    }

    return vector_element_ptr(vec, index);
}

AchErrorCode vector_set(Vector *vec, usize index, const void *element) {
    if (vec == NULL || element == NULL) {
        return ACH_ERROR_INVALID_ARG;
    }

    if (index >= vec->length) {
        return ACH_ERROR_INVALID_ARG;
    }

    void *dest = vector_element_ptr(vec, index);
    memcpy(dest, element, vec->element_size);

    return ACH_SUCCESS;
}

/* ---- Size Management ---------------------------------------------------- */

usize vector_length(const Vector *vec) {
    return (vec != NULL) ? vec->length : 0;
}

usize vector_capacity(const Vector *vec) {
    return (vec != NULL) ? vec->capacity : 0;
}

bool vector_is_empty(const Vector *vec) {
    return (vec == NULL) || (vec->length == 0);
}

void vector_clear(Vector *vec) {
    if (vec != NULL) {
        vec->length = 0;
        /* Buffer is NOT freed. Capacity remains for reuse.
         * This is intentional: clear + refill is a common pattern
         * (e.g., clearing search results between queries). */
    }
}

AchErrorCode vector_reserve(Vector *vec, usize min_capacity) {
    if (vec == NULL) {
        return ACH_ERROR_INVALID_ARG;
    }

    /* Already have enough capacity — nothing to do */
    if (vec->capacity >= min_capacity) {
        return ACH_SUCCESS;
    }

    /* Grow directly to the requested capacity (no doubling here —
     * the caller explicitly asked for this size). */
    if (!vector_capacity_is_safe(min_capacity, vec->element_size)) {
        return ACH_ERROR_OUT_OF_MEMORY;
    }

    usize new_size_bytes = min_capacity * vec->element_size;
    void *new_data = realloc(vec->data, new_size_bytes);
    if (new_data == NULL) {
        return ACH_ERROR_OUT_OF_MEMORY;
    }

    vec->data = new_data;
    vec->capacity = min_capacity;
    return ACH_SUCCESS;
}

AchErrorCode vector_shrink_to_fit(Vector *vec) {
    if (vec == NULL) {
        return ACH_ERROR_INVALID_ARG;
    }

    /* Nothing to shrink */
    if (vec->length == vec->capacity) {
        return ACH_SUCCESS;
    }

    /* Special case: empty vector — free the buffer entirely */
    if (vec->length == 0) {
        free(vec->data);
        vec->data = NULL;
        vec->capacity = 0;
        return ACH_SUCCESS;
    }

    usize new_size_bytes = vec->length * vec->element_size;
    void *new_data = realloc(vec->data, new_size_bytes);
    if (new_data == NULL) {
        /* realloc failed, but original buffer is still valid.
         * The vector still works, just with extra capacity. */
        return ACH_ERROR_OUT_OF_MEMORY;
    }

    vec->data = new_data;
    vec->capacity = vec->length;
    return ACH_SUCCESS;
}
