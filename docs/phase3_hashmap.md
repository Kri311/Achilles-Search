# Phase 3: Hash Map Design Specification

This document details the architecture, data structures, and algorithms for the high-performance **Hash Map** in Achilles-Search. The hash map maps wide-character string keys (`const wchar_t*`) to arbitrary generic values.


## 1. Architectural Decisions

### 1.1 Structure of Arrays (SoA) Layout
To maximize CPU L1/L2 cache efficiency during lookups, we split the hash map data into three parallel arrays rather than using an array of structures:
*   `keys`: An array of `wchar_t*` pointers.
*   `states`: An array of `u8` state bytes (`EMPTY`, `OCCUPIED`, `TOMBSTONE`).
*   `values`: A contiguous byte block containing values of size `value_size`.

**Why SoA**
During lookup, the CPU only needs to scan keys and states. The values (which may be large structures) are not loaded into cache lines unless a key match is confirmed.

```
                  Memory Layout (Structure of Arrays)
                  
  states:  [ OCCUPIED ] [ TOMBSTONE ] [ EMPTY     ] [ OCCUPIED ]
                 |            |             |             |
  keys:    [ "file1"  ] [   NULL    ] [   NULL    ] [ "file2"  ]
                 |            |             |             |
  values:  [ Value 1  ] [  Garbage  ] [  Garbage  ] [ Value 2  ]
```

### 1.2 Collision Resolution: Open Addressing with Linear Probing
We use open addressing with linear probing. Linear probing has superior cache locality compared to chaining (linked lists) because all candidate buckets reside in contiguous memory.
*   **Tombstone Reuse:** When a key is deleted, its state is marked as `TOMBSTONE`. During insertion, if we do not find an existing match, we insert into the first tombstone or empty bucket encountered.
*   **Load Factor:** We trigger a rehash and grow the capacity by a factor of 2 when the load factor (number of occupied buckets / capacity) exceeds **70%**.

### 1.3 Hash Function: FNV-1a (64-bit)
We use the Fowler-Noll-Vo (FNV-1a) hash algorithm adapted for 16-bit wide characters (`wchar_t`). FNV-1a is simple, fast, and has excellent avalanche characteristics for filesystem paths.


## 2. API Design

### 2.1 Types and Constants
```c
#define ACH_HASHMAP_DEFAULT_CAPACITY 16
#define ACH_HASHMAP_LOAD_FACTOR_THRESHOLD 0.70f

typedef enum HashMapState {
    ACH_HASHMAP_STATE_EMPTY     = 0,
    ACH_HASHMAP_STATE_OCCUPIED  = 1,
    ACH_HASHMAP_STATE_TOMBSTONE = 2
} HashMapState;

typedef struct HashMap {
    wchar_t **keys;       /* Dynamic array of wcsdup'd keys */
    u8 *states;           /* Parallel array of HashMapState values */
    u8 *values;           /* Contiguous byte block for inline values */
    
    usize capacity;       /* Number of buckets allocated */
    usize size;           /* Number of active entries */
    usize value_size;     /* Size of each value in bytes */
} HashMap;
```

### 2.2 Core Functions
```c
/* Lifecycle */
AchErrorCode hashmap_init(HashMap *map, usize value_size, usize initial_capacity);
void hashmap_destroy(HashMap *map);
void hashmap_clear(HashMap *map);

/* Operations */
AchErrorCode hashmap_put(HashMap *map, const wchar_t *key, const void *value);
AchErrorCode hashmap_get(const HashMap *map, const wchar_t *key, void *out_value);
AchErrorCode hashmap_remove(HashMap *map, const wchar_t *key);
bool hashmap_contains(const HashMap *map, const wchar_t *key);

/* Info */
usize hashmap_size(const HashMap *map);
usize hashmap_capacity(const HashMap *map);

/* Iteration */
typedef struct HashMapIterator {
    const HashMap *map;
    usize index;
} HashMapIterator;

HashMapIterator hashmap_iter(const HashMap *map);
bool hashmap_iter_next(HashMapIterator *iter, const wchar_t **out_key, void *out_value);
```


## 3. Memory Ownership and Safety
1.  **Keys:** The hash map takes ownership of keys by creating an internal copy using `_wcsdup`. When an entry is replaced or deleted, the old key is freed.
2.  **Values:** Values are copied into the hash map's internal `values` buffer via `memcpy`. The caller retains ownership of the original value passed to `hashmap_put`.
3.  **Destruction:** `hashmap_destroy` frees all allocated keys, the keys array, the states array, and the values block.


## 4. Test Strategy
We will implement 15+ tests in `tests/test_hashmap.c` covering:
1.  **Lifecycle:** Initialization, default capacity, clearing, and safe destruction.
2.  **Basic CRUD:** Putting elements, updating existing elements, getting, and removing.
3.  **Collision Resolution:** Verified by inserting many keys that map to similar hash buckets.
4.  **Automatic Resizing:** Inserting keys beyond 70% load factor and verifying all keys remain accessible.
5.  **Iteration:** Iterating through the map and confirming all inserted keys/values are retrieved.
6.  **Edge Cases:** Null parameters, zero capacity, empty string keys, and nonexistent key lookups.
