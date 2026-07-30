# Phase 10: Performance Optimization Design Specification

This document details the optimizations introduced to Achilles-Search to speed up search matching and vector manipulation operations.


## 1. Vector Batch Extend (`vector_push_many`)
When merging search results from multiple worker threads, the search manager previously copied elements one-by-one using `vector_push`.
*   **Optimization:** Implement `vector_push_many` in the core `Vector` module. It checks if the vector needs to grow once, allocates memory if needed, and copies the block of elements using a single, highly optimized `memcpy` call.
*   **Result:** Minimizes function call overhead and memory manager lookups in the query coordinator.


## 2. Inlined ASCII Case-Folding
Filename matching is case-insensitive by default. Standard library `towlower` operations check locale lookup tables, introducing overhead for standard ASCII paths.
*   **Optimization:** Introduce an inline helper `towlower_ascii`:
    ```c
    static inline wchar_t towlower_ascii(wchar_t c) {
        if (c >= L'A' && c <= L'Z') {
            return c + (L'a' - L'A');
        }
        return towlower(c);
    }
    ```
*   **Result:** Avoids locale lookup logic for all standard ASCII path characters.


## 3. Pre-lowercased Query Matching
Previously, case-insensitive matching cased both filename characters and search query characters repeatedly in the inner matching loop.
*   **Optimization:** The search coordinator lowercases the search query string once prior to thread scheduling.
*   **Result:** The inner matching loop only has to case-fold filename characters, cutting the number of case-folding operations in half.
