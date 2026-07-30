# Phase 2: Dynamic Vector

**Status:** IN PROGRESS
**Date:** 2026-07-03


## Objective

Build a generic, growable array (dynamic vector) — the most fundamental data
structure in systems programming. The scanner (Phase 1) needs this to store
discovered files. The indexer, search engine, and virtually every other module
will depend on it too.

## Why C Doesn't Have This

C arrays are fixed-size. You declare `int arr[100]` and that's it — 100 elements,
no more. If you need to store an unknown number of items, you have three choices:

1. **Over-allocate** — Guess a huge size. Wastes memory, still fails if you guess wrong.
2. **Linked list** — Dynamic, but terrible cache performance (nodes scattered in memory).
3. **Dynamic array** — `malloc` a buffer, `realloc` when full. Best of both worlds.

Every serious C project implements option 3. It's called a "vector" (C++ STL),
"ArrayList" (Java), "list" (Python), or "slice" (Go). The concept is identical.

## How It Works

```
 capacity = 8
 length   = 5
 ┌───┬───┬───┬───┬───┬───┬───┬───┐
 │ A │ B │ C │ D │ E │   │   │   │
 └───┴───┴───┴───┴───┴───┴───┴───┘
   0   1   2   3   4   5   6   7
                         ↑
                    unused space
```

- **length**: How many elements are stored (5)
- **capacity**: How many elements the buffer CAN hold (8)
- When `length == capacity`, we **grow** the buffer

### Growth Strategy: Why Double

When the buffer is full and we push a new element, we need to `realloc`. The question
is: how much bigger should the new buffer be

**Option A: Grow by 1**
- Push N elements → N reallocations → each copies all existing data
- Total copies: 1 + 2 + 3 + ... + N = O(N^2). Catastrophic for large N.

**Option B: Double the capacity (growth factor = 2)**
- Push N elements → log2(N) reallocations
- Total copies: 1 + 2 + 4 + 8 + ... + N = 2N - 1 = O(N)
- Each individual push is **amortized O(1)**

This is why every standard library uses geometric growth. The factor is usually
2 (GCC libstdc++, MSVC STL) or 1.5 (Facebook Folly, Clang libc++).

We use 2 for simplicity. The tradeoff is ~50% wasted capacity in the worst case
(when length = capacity/2 + 1, meaning we just doubled). Factor 1.5 wastes less
but grows more often. For our use case, the difference is negligible.

### Cache Locality: Why Contiguous Memory Matters

A vector stores elements in one contiguous block of memory:
```
[elem0][elem1][elem2][elem3][elem4]...
```

When the CPU reads `elem0`, it loads an entire **cache line** (64 bytes on x86)
into L1 cache. If `elem1` through `elem3` fit in that same 64 bytes, accessing
them is essentially free — they're already in cache.

A linked list stores each element in a separate heap allocation:
```
elem0 → [node @ 0x7A00] → elem1 → [node @ 0x3F80] → elem2 → [node @ 0xB200]
```

Each node is at a random memory address. Iterating the list causes a **cache miss**
per element. On modern hardware, an L1 cache hit is ~1ns; a cache miss is ~100ns.
For 10 million files, that's 10ms (vector) vs 1 second (linked list).

## Design Decisions

### 1. Generic via `void*` + `element_size`

C has no templates. We have two options for generics:

**Option A: void* with element_size**
```c
typedef struct Vector {
    void  *data;
    usize  length;
    usize  capacity;
    usize  element_size;
} Vector;
```
One implementation handles any type. Access requires `memcpy` or pointer arithmetic.
We provide type-safe convenience macros on top.

**Option B: Macro-generated typed vectors**
```c
#define VECTOR_DEFINE(type) \
    typedef struct { type *data; usize len, cap; } Vector_##type;
```
Generates separate code for each type. Type-safe but results in code bloat
and difficult debugging.

We choose **Option A**. One debuggable implementation. Type-safe macros for
the common case. This is what most production C projects do (Redis, SQLite
internal arrays, etc.).

### 2. Init/Destroy Pattern (Not Create/Free)

```c
// Our approach: struct lives on stack or embedded in another struct
Vector vec;
vector_init(&vec, sizeof(int), 64);
// ... use it ...
vector_destroy(&vec);
```

vs.

```c
// Alternative: heap-allocated handle
Vector *vec = vector_create(sizeof(int), 64);
// ... use it ...
vector_free(vec);
```

We use init/destroy because:
- The Vector struct can live on the stack (no heap allocation for the struct itself)
- It can be embedded inside other structs (Scanner has a Vector of files)
- The caller controls struct lifetime; we only manage the data buffer

### 3. Overflow Protection

`capacity * element_size` can overflow `size_t`. Before every `realloc`, we check:
```c
if (new_capacity > SIZE_MAX / element_size) {
    return ACH_ERROR_OUT_OF_MEMORY;  // would overflow
}
```

This is a real security concern. Integer overflow in allocation size → small
allocation → buffer overflow → potential code execution.

## API Design

```c
// Lifecycle
AchErrorCode vector_init(Vector *vec, usize element_size, usize initial_capacity);
void         vector_destroy(Vector *vec);

// Adding / Removing
AchErrorCode vector_push(Vector *vec, const void *element);
AchErrorCode vector_pop(Vector *vec, void *out_element);

// Access (returns pointer into the buffer — valid until next push/realloc)
void*        vector_get(const Vector *vec, usize index);
AchErrorCode vector_set(Vector *vec, usize index, const void *element);

// Size management
usize        vector_length(const Vector *vec);
usize        vector_capacity(const Vector *vec);
bool         vector_is_empty(const Vector *vec);
void         vector_clear(Vector *vec);
AchErrorCode vector_reserve(Vector *vec, usize min_capacity);
AchErrorCode vector_shrink_to_fit(Vector *vec);
```

### Type-Safe Convenience Macros

```c
// Instead of:
int *val = (int*)vector_get(&vec, 3);

// You write:
int *val = VECTOR_GET_AS(&vec, 3, int);

// Instead of:
int x = 42; vector_push(&vec, &x);

// You write:
VECTOR_PUSH_VAL(&vec, 42, int);
```

## Files

| File | Action |
|------|--------|
| `include/data/vector.h` | Create — public API |
| `src/data/vector.c` | Create — implementation |
| `tests/test_vector.c` | Create — test program |
| `CMakeLists.txt` | Update — add vector.c to sources, add test target |

## Complexity

| Operation | Time | Notes |
|-----------|------|-------|
| `vector_push` | Amortized O(1) | O(N) on reallocation, but rare |
| `vector_pop` | O(1) | No reallocation |
| `vector_get` | O(1) | Direct pointer arithmetic |
| `vector_set` | O(1) | Direct memcpy |
| `vector_reserve` | O(N) | Copies existing data if realloc moves |
| `vector_clear` | O(1) | Just resets length, no deallocation |
| `vector_shrink_to_fit` | O(N) | Reallocs to exact size |
