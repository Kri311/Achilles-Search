/* ==========================================================================
 * Achilles-Search | src/data/hashmap.c
 * ==========================================================================
 * Implementation of the wide-string structure-of-arrays Hash Map.
 * ========================================================================== */

#include "data/hashmap.h"
#include <stdlib.h>
#include <string.h>

/* ---- Static Helpers ------------------------------------------------------ */

/* FNV-1a 64-bit hash algorithm for wide strings.
 * Reference: http://www.isthe.com/chongo/tech/comp/fnv/index.html */
static u64 hash_fnv1a(const wchar_t *str) {
    u64 hash = 14695981039346656037ULL;
    while (*str) {
        hash ^= (u64)*str;
        hash *= 1099511628211ULL;
        str++;
    }
    return hash;
}

/* Rounds a value up to the next power of 2.
 * This guarantees we can use bitwise AND (& capacity - 1) instead of division. */
static usize round_to_power_of_2(usize val) {
    if (val < ACH_HASHMAP_DEFAULT_CAPACITY) {
        return ACH_HASHMAP_DEFAULT_CAPACITY;
    }
    val--;
    val |= val >> 1;
    val |= val >> 2;
    val |= val >> 4;
    val |= val >> 8;
    val |= val >> 16;
#if defined(_WIN64) || defined(__x86_64__)
    val |= val >> 32;
#endif
    val++;
    return val;
}

/* Allocates and rehashes all active entries into a new array layout.
 * Runs in O(N) time and avoids allocating any new string keys. */
static AchErrorCode hashmap_resize(HashMap *map, usize new_capacity) {
    wchar_t **new_keys = calloc(new_capacity, sizeof(wchar_t*));
    u8 *new_states = calloc(new_capacity, sizeof(u8));
    u8 *new_values = malloc(new_capacity * map->value_size);

    if (new_keys == NULL || new_states == NULL || new_values == NULL) {
        free(new_keys);
        free(new_states);
        free(new_values);
        return ACH_ERROR_OUT_OF_MEMORY;
    }

    usize mask = new_capacity - 1;

    for (usize i = 0; i < map->capacity; i++) {
        if (map->states[i] == ACH_HASHMAP_STATE_OCCUPIED) {
            wchar_t *key = map->keys[i];
            u64 hash = hash_fnv1a(key);
            usize index = (usize)(hash & mask);

            /* Linear probing to find an empty slot in the new table.
             * No tombstones exist in the new table yet. */
            while (new_states[index] == ACH_HASHMAP_STATE_OCCUPIED) {
                index = (index + 1) & mask;
            }

            new_keys[index] = key;
            new_states[index] = ACH_HASHMAP_STATE_OCCUPIED;
            memcpy(&new_values[index * map->value_size],
                   &map->values[i * map->value_size],
                   map->value_size);
        }
    }

    /* Free old layout arrays, but NOT the key strings themselves */
    free(map->keys);
    free(map->states);
    free(map->values);

    map->keys = new_keys;
    map->states = new_states;
    map->values = new_values;
    map->capacity = new_capacity;

    return ACH_SUCCESS;
}

/* ---- Lifecycle ----------------------------------------------------------- */

AchErrorCode hashmap_init(HashMap *map, usize value_size, usize initial_capacity) {
    if (map == NULL || value_size == 0) {
        return ACH_ERROR_INVALID_ARG;
    }

    usize capacity = round_to_power_of_2(initial_capacity);

    map->keys = calloc(capacity, sizeof(wchar_t*));
    map->states = calloc(capacity, sizeof(u8));
    map->values = malloc(capacity * value_size);

    if (map->keys == NULL || map->states == NULL || map->values == NULL) {
        free(map->keys);
        free(map->states);
        free(map->values);
        memset(map, 0, sizeof(HashMap));
        return ACH_ERROR_OUT_OF_MEMORY;
    }

    map->capacity = capacity;
    map->size = 0;
    map->value_size = value_size;

    return ACH_SUCCESS;
}

void hashmap_destroy(HashMap *map) {
    if (map == NULL || map->keys == NULL) {
        return;
    }

    for (usize i = 0; i < map->capacity; i++) {
        if (map->states[i] == ACH_HASHMAP_STATE_OCCUPIED) {
            free(map->keys[i]);
        }
    }

    free(map->keys);
    free(map->states);
    free(map->values);

    memset(map, 0, sizeof(HashMap));
}

void hashmap_clear(HashMap *map) {
    if (map == NULL || map->keys == NULL) {
        return;
    }

    for (usize i = 0; i < map->capacity; i++) {
        if (map->states[i] == ACH_HASHMAP_STATE_OCCUPIED) {
            free(map->keys[i]);
            map->keys[i] = NULL;
        }
        map->states[i] = ACH_HASHMAP_STATE_EMPTY;
    }
    map->size = 0;
}

/* ---- Operations ---------------------------------------------------------- */

AchErrorCode hashmap_put(HashMap *map, const wchar_t *key, const void *value) {
    if (map == NULL || key == NULL || value == NULL) {
        return ACH_ERROR_INVALID_ARG;
    }

    /* Resize if next insert would exceed load factor limit */
    if ((f32)(map->size + 1) / (f32)map->capacity > ACH_HASHMAP_LOAD_FACTOR_THRESHOLD) {
        AchErrorCode err = hashmap_resize(map, map->capacity * 2);
        if (ACH_FAILED(err)) {
            return err;
        }
    }

    u64 hash = hash_fnv1a(key);
    usize mask = map->capacity - 1;
    usize index = (usize)(hash & mask);

    usize insert_index = (usize)-1;
    bool found_tombstone = false;

    while (true) {
        if (map->states[index] == ACH_HASHMAP_STATE_EMPTY) {
            if (!found_tombstone) {
                insert_index = index;
            }
            break;
        } else if (map->states[index] == ACH_HASHMAP_STATE_TOMBSTONE) {
            if (!found_tombstone) {
                insert_index = index;
                found_tombstone = true;
            }
        } else if (map->states[index] == ACH_HASHMAP_STATE_OCCUPIED) {
            if (wcscmp(map->keys[index], key) == 0) {
                /* Key found. Update the value and return. */
                memcpy(&map->values[index * map->value_size], value, map->value_size);
                return ACH_SUCCESS;
            }
        }
        index = (index + 1) & mask;
    }

    /* Inserting a new key: duplicate key string */
    wchar_t *key_copy = _wcsdup(key);
    if (key_copy == NULL) {
        return ACH_ERROR_OUT_OF_MEMORY;
    }

    map->keys[insert_index] = key_copy;
    map->states[insert_index] = ACH_HASHMAP_STATE_OCCUPIED;
    memcpy(&map->values[insert_index * map->value_size], value, map->value_size);
    map->size++;

    return ACH_SUCCESS;
}

AchErrorCode hashmap_get(const HashMap *map, const wchar_t *key, void *out_value) {
    if (map == NULL || key == NULL || out_value == NULL) {
        return ACH_ERROR_INVALID_ARG;
    }

    u64 hash = hash_fnv1a(key);
    usize mask = map->capacity - 1;
    usize index = (usize)(hash & mask);

    while (true) {
        if (map->states[index] == ACH_HASHMAP_STATE_EMPTY) {
            return ACH_ERROR_NOT_FOUND;
        } else if (map->states[index] == ACH_HASHMAP_STATE_OCCUPIED) {
            if (wcscmp(map->keys[index], key) == 0) {
                memcpy(out_value, &map->values[index * map->value_size], map->value_size);
                return ACH_SUCCESS;
            }
        }
        index = (index + 1) & mask;
    }
}

AchErrorCode hashmap_remove(HashMap *map, const wchar_t *key) {
    if (map == NULL || key == NULL) {
        return ACH_ERROR_INVALID_ARG;
    }

    u64 hash = hash_fnv1a(key);
    usize mask = map->capacity - 1;
    usize index = (usize)(hash & mask);

    while (true) {
        if (map->states[index] == ACH_HASHMAP_STATE_EMPTY) {
            return ACH_ERROR_NOT_FOUND;
        } else if (map->states[index] == ACH_HASHMAP_STATE_OCCUPIED) {
            if (wcscmp(map->keys[index], key) == 0) {
                /* Free key and flag as TOMBSTONE */
                free(map->keys[index]);
                map->keys[index] = NULL;
                map->states[index] = ACH_HASHMAP_STATE_TOMBSTONE;
                map->size--;
                return ACH_SUCCESS;
            }
        }
        index = (index + 1) & mask;
    }
}

bool hashmap_contains(const HashMap *map, const wchar_t *key) {
    if (map == NULL || key == NULL) {
        return false;
    }

    u64 hash = hash_fnv1a(key);
    usize mask = map->capacity - 1;
    usize index = (usize)(hash & mask);

    while (true) {
        if (map->states[index] == ACH_HASHMAP_STATE_EMPTY) {
            return false;
        } else if (map->states[index] == ACH_HASHMAP_STATE_OCCUPIED) {
            if (wcscmp(map->keys[index], key) == 0) {
                return true;
            }
        }
        index = (index + 1) & mask;
    }
}

/* ---- Information --------------------------------------------------------- */

usize hashmap_size(const HashMap *map) {
    return map ? map->size : 0;
}

usize hashmap_capacity(const HashMap *map) {
    return map ? map->capacity : 0;
}

/* ---- Iteration ----------------------------------------------------------- */

HashMapIterator hashmap_iter(const HashMap *map) {
    HashMapIterator iter;
    iter.map = map;
    iter.index = 0;
    return iter;
}

bool hashmap_iter_next(HashMapIterator *iter, const wchar_t **out_key, void *out_value) {
    if (iter == NULL || iter->map == NULL || out_key == NULL) {
        return false;
    }

    const HashMap *map = iter->map;

    while (iter->index < map->capacity) {
        usize idx = iter->index;
        iter->index++;

        if (map->states[idx] == ACH_HASHMAP_STATE_OCCUPIED) {
            *out_key = map->keys[idx];
            if (out_value) {
                memcpy(out_value, &map->values[idx * map->value_size], map->value_size);
            }
            return true;
        }
    }

    return false;
}
