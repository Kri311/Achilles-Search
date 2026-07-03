/* ==========================================================================
 * Achilles-Search | include/data/hashmap.h
 * ==========================================================================
 * High-performance wide-string Hash Map.
 *
 * DESIGN:
 *   - Key type: const wchar_t* (Unicode filesystem paths / filenames).
 *   - Value type: Generic (value_size configured at initialization).
 *   - Layout: Structure of Arrays (SoA) for cache-friendly key lookups.
 *   - Collision: Open addressing with linear probing and tombstone reuse.
 *   - Resizing: Automatic doubling when load factor > 70%.
 * ========================================================================== */

#ifndef ACH_HASHMAP_H
#define ACH_HASHMAP_H

#include "common/types.h"
#include "common/errors.h"

#define ACH_HASHMAP_DEFAULT_CAPACITY 16
#define ACH_HASHMAP_LOAD_FACTOR_THRESHOLD 0.70f

/* Buckets can be in one of three states */
typedef enum HashMapState {
    ACH_HASHMAP_STATE_EMPTY     = 0,
    ACH_HASHMAP_STATE_OCCUPIED  = 1,
    ACH_HASHMAP_STATE_TOMBSTONE = 2
} HashMapState;

/* The SoA Hash Map structure */
typedef struct HashMap {
    wchar_t **keys;       /* Dynamic array of wcsdup'd keys */
    u8 *states;           /* Parallel array of HashMapState values */
    u8 *values;           /* Contiguous byte block for inline values */
    
    usize capacity;       /* Number of buckets allocated (must be power of 2) */
    usize size;           /* Number of active entries */
    usize value_size;     /* Size of each value in bytes */
} HashMap;

/* ---- Lifecycle ----------------------------------------------------------- */

/* Initializes the hash map.
 * Parameters:
 *   map              - Pointer to HashMap struct to initialize.
 *   value_size       - Size in bytes of the values to be stored.
 *   initial_capacity - Desired capacity. Pass 0 for default.
 * Returns:
 *   ACH_SUCCESS on success, ACH_ERROR_INVALID_ARG or ACH_ERROR_OUT_OF_MEMORY. */
AchErrorCode hashmap_init(HashMap *map, usize value_size, usize initial_capacity);

/* Destroys the hash map, freeing all internal keys and memory.
 * Idempotent. Map can be re-initialized after destruction. */
void hashmap_destroy(HashMap *map);

/* Clears all entries in the hash map without releasing the allocated capacity. */
void hashmap_clear(HashMap *map);

/* ---- Operations ---------------------------------------------------------- */

/* Inserts or updates an entry in the hash map.
 * The key is duplicated internally. The value is copied into the map.
 * Parameters:
 *   map   - Pointer to initialized HashMap.
 *   key   - Wide string key (cannot be NULL).
 *   value - Pointer to the value bytes to copy in (cannot be NULL).
 * Returns:
 *   ACH_SUCCESS on success, ACH_ERROR_INVALID_ARG or ACH_ERROR_OUT_OF_MEMORY. */
AchErrorCode hashmap_put(HashMap *map, const wchar_t *key, const void *value);

/* Retrieves a copy of the value associated with the key.
 * Parameters:
 *   map       - Pointer to initialized HashMap.
 *   key       - Wide string key to look up.
 *   out_value - Pointer to caller-allocated buffer where the value is copied.
 * Returns:
 *   ACH_SUCCESS if found, ACH_ERROR_NOT_FOUND or ACH_ERROR_INVALID_ARG. */
AchErrorCode hashmap_get(const HashMap *map, const wchar_t *key, void *out_value);

/* Removes an entry from the hash map.
 * The entry is marked as a TOMBSTONE and its key is freed.
 * Returns:
 *   ACH_SUCCESS if removed, ACH_ERROR_NOT_FOUND or ACH_ERROR_INVALID_ARG. */
AchErrorCode hashmap_remove(HashMap *map, const wchar_t *key);

/* Checks if a key exists in the hash map. */
bool hashmap_contains(const HashMap *map, const wchar_t *key);

/* ---- Information --------------------------------------------------------- */

/* Returns the number of active entries in the map. */
usize hashmap_size(const HashMap *map);

/* Returns the total capacity (bucket count) of the map. */
usize hashmap_capacity(const HashMap *map);

/* ---- Iteration ----------------------------------------------------------- */

typedef struct HashMapIterator {
    const HashMap *map;
    usize index;
} HashMapIterator;

/* Returns an iterator positioned at the start of the map. */
HashMapIterator hashmap_iter(const HashMap *map);

/* Advances the iterator to the next occupied entry.
 * Parameters:
 *   iter      - Pointer to the iterator.
 *   out_key   - Out parameter receiving the pointer to the key (owned by map).
 *   out_value - Pointer to buffer where value is copied. Can be NULL to check/skip.
 * Returns:
 *   true if an entry was retrieved, false if iteration is complete. */
bool hashmap_iter_next(HashMapIterator *iter, const wchar_t **out_key, void *out_value);

#endif /* ACH_HASHMAP_H */
