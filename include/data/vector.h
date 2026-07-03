/* ==========================================================================
 * Achilles-Search | include/data/vector.h
 * ==========================================================================
 * Generic dynamic array (growable vector).
 *
 * WHY THIS EXISTS:
 *   C has no built-in growable array. Every module in Achilles-Search that
 *   collects an unknown number of items (files, index entries, search
 *   results) needs this data structure.
 *
 * DESIGN:
 *   - Generic via void* + element_size (one implementation for all types)
 *   - Contiguous memory layout for cache-friendly iteration
 *   - Geometric growth (factor 2) for amortized O(1) push
 *   - Init/destroy pattern: struct can live on stack or be embedded
 *   - Type-safe convenience macros for common operations
 *
 * MEMORY OWNERSHIP:
 *   - vector_init() allocates the internal data buffer
 *   - vector_destroy() frees it
 *   - The caller owns the Vector struct itself (stack or embedded)
 *   - Pointers returned by vector_get() are INVALIDATED by any operation
 *     that may reallocate (push, reserve). Do not cache them across pushes.
 *
 * THREAD SAFETY:
 *   None. External synchronization required for concurrent access.
 *
 * EXAMPLE:
 *   Vector vec;
 *   vector_init(&vec, sizeof(int), 16);
 *
 *   int x = 42;
 *   vector_push(&vec, &x);
 *
 *   int *val = (int*)vector_get(&vec, 0);  // val points to 42
 *   // OR: int *val = VECTOR_GET_AS(&vec, 0, int);
 *
 *   vector_destroy(&vec);
 * ========================================================================== */

#ifndef ACH_VECTOR_H
#define ACH_VECTOR_H

#include "common/types.h"
#include "common/errors.h"

/* ---- Vector Structure --------------------------------------------------- */
typedef struct Vector {
    void   *data;           /* Contiguous buffer holding elements             */
    usize   length;         /* Number of elements currently stored            */
    usize   capacity;       /* Number of elements the buffer can hold         */
    usize   element_size;   /* Size of each element in bytes                  */
} Vector;

/* ---- Lifecycle ---------------------------------------------------------- */

/* Initialize a vector.
 *
 * Parameters:
 *   vec              - Pointer to caller-owned Vector struct
 *   element_size     - sizeof(element_type), must be > 0
 *   initial_capacity - Pre-allocated slot count (0 = use default)
 *
 * Returns:
 *   ACH_SUCCESS           - Ready to use
 *   ACH_ERROR_INVALID_ARG - vec is NULL or element_size is 0
 *   ACH_ERROR_OUT_OF_MEMORY - malloc failed
 *
 * The initial_capacity is a hint. If 0, we use ACH_DEFAULT_VECTOR_CAPACITY.
 * Pre-allocating the right capacity avoids early reallocations. If you know
 * you'll store ~10,000 items, pass 10000.
 */
AchErrorCode vector_init(Vector *vec, usize element_size, usize initial_capacity);

/* Free the internal data buffer and zero the struct.
 * Safe to call on an already-destroyed or zero-initialized vector.
 * Does NOT free the Vector struct itself (caller owns that).
 */
void vector_destroy(Vector *vec);

/* ---- Adding / Removing -------------------------------------------------- */

/* Append an element to the end. Grows the buffer if needed.
 *
 * Parameters:
 *   vec     - Initialized vector
 *   element - Pointer to the element to copy in (element_size bytes read)
 *
 * Returns:
 *   ACH_SUCCESS           - Element appended
 *   ACH_ERROR_INVALID_ARG - vec or element is NULL
 *   ACH_ERROR_OUT_OF_MEMORY - realloc failed during growth
 *
 * WARNING: Any pointers returned by previous vector_get() calls may be
 *          INVALIDATED if this push triggers a reallocation.
 */
AchErrorCode vector_push(Vector *vec, const void *element);

/* Append multiple elements to the end. Grows the buffer if needed.
 *
 * Parameters:
 *   vec      - Initialized vector
 *   elements - Pointer to contiguous array of elements to copy in
 *   count    - Number of elements to append
 *
 * Returns:
 *   ACH_SUCCESS           - Elements appended
 *   ACH_ERROR_INVALID_ARG - vec or elements is NULL
 *   ACH_ERROR_OUT_OF_MEMORY - realloc failed during growth
 */
AchErrorCode vector_push_many(Vector *vec, const void *elements, usize count);

/* Remove and optionally return the last element.
 *
 * Parameters:
 *   vec         - Initialized vector
 *   out_element - If non-NULL, the removed element is copied here
 *                 (element_size bytes written). Pass NULL to discard.
 *
 * Returns:
 *   ACH_SUCCESS           - Element removed
 *   ACH_ERROR_INVALID_ARG - vec is NULL
 *   ACH_ERROR_NOT_FOUND   - Vector is empty
 */
AchErrorCode vector_pop(Vector *vec, void *out_element);

/* ---- Element Access ----------------------------------------------------- */

/* Get a pointer to the element at the given index.
 *
 * Returns:
 *   Non-NULL pointer on success (points directly into the buffer)
 *   NULL if vec is NULL, not initialized, or index >= length
 *
 * The returned pointer is valid until the next push/reserve/shrink
 * operation that may trigger reallocation.
 *
 * PERFORMANCE: O(1). Just pointer arithmetic: data + index * element_size
 */
void* vector_get(const Vector *vec, usize index);

/* Copy an element into the vector at the given index (overwrite).
 *
 * Returns:
 *   ACH_SUCCESS           - Element overwritten
 *   ACH_ERROR_INVALID_ARG - vec or element is NULL, or index >= length
 */
AchErrorCode vector_set(Vector *vec, usize index, const void *element);

/* ---- Size Management ---------------------------------------------------- */

/* Returns the number of elements stored. 0 if vec is NULL. */
usize vector_length(const Vector *vec);

/* Returns the current buffer capacity. 0 if vec is NULL. */
usize vector_capacity(const Vector *vec);

/* Returns true if the vector has zero elements. */
bool vector_is_empty(const Vector *vec);

/* Reset length to 0 without freeing or reallocating the buffer.
 * The capacity remains unchanged. This is useful when you want to
 * reuse the vector for a new batch of data without the cost of
 * free + malloc.
 */
void vector_clear(Vector *vec);

/* Ensure capacity is at least min_capacity.
 * If the current capacity is already >= min_capacity, this is a no-op.
 * If growth is needed, the buffer is reallocated.
 *
 * Returns:
 *   ACH_SUCCESS           - Capacity is now >= min_capacity
 *   ACH_ERROR_INVALID_ARG - vec is NULL
 *   ACH_ERROR_OUT_OF_MEMORY - realloc failed
 */
AchErrorCode vector_reserve(Vector *vec, usize min_capacity);

/* Reallocate the buffer to exactly fit the current length.
 * Frees unused capacity. Useful after building a vector that won't
 * grow further (e.g., final search results).
 *
 * Returns:
 *   ACH_SUCCESS           - Buffer shrunk (or was already exact)
 *   ACH_ERROR_INVALID_ARG - vec is NULL
 *   ACH_ERROR_OUT_OF_MEMORY - realloc failed (original buffer preserved)
 */
AchErrorCode vector_shrink_to_fit(Vector *vec);

/* ---- Type-Safe Convenience Macros --------------------------------------- */

/* Get a typed pointer to the element at index.
 *   int *val = VECTOR_GET_AS(&vec, 3, int);
 */
#define VECTOR_GET_AS(vec, index, type) \
    ((type*)vector_get((vec), (index)))

/* Push a value by creating a temporary.
 *   VECTOR_PUSH_VAL(&vec, 42, int);
 *   VECTOR_PUSH_VAL(&vec, 3.14, double);
 *
 * Note: creates a stack temporary, takes its address, passes to vector_push.
 */
#define VECTOR_PUSH_VAL(vec, value, type) \
    do { \
        type _ach_vec_tmp = (value); \
        vector_push((vec), &_ach_vec_tmp); \
    } while (0)

/* Get the raw typed data pointer for direct iteration.
 *   int *data = VECTOR_DATA_AS(&vec, int);
 *   for (usize i = 0; i < vector_length(&vec); i++) {
 *       printf("%d\n", data[i]);
 *   }
 */
#define VECTOR_DATA_AS(vec, type) \
    ((type*)((vec)->data))

#endif /* ACH_VECTOR_H */
