# Phase 7: Content Index Design Specification

This document details the architecture, text tokenization, and query execution for the **Content Index** in Achilles-Search. The content index enables full-text search within files.


## 1. Architectural Decisions

### 1.1 Inverted Index Structure
Achilles-Search uses an Inverted Index mapping lowercase words to a posting list of file IDs.
*   **Storage Layout:** We use the Phase 3 `HashMap` where the key is a wide-character string (the word) and the value is a `Vector` storing `u32` file IDs.
*   **Consecutive Push Deduplication:** Since files are indexed sequentially (increasing file IDs), a word appearing multiple times in the same file will attempt to push the same file ID consecutively. We optimize deduplication by checking the last element of the posting list in $O(1)$ time; if it matches the current file ID, we skip the push. This avoids allocating a temporary set per file.

### 1.2 Text Parsing and File Extension Filters
To optimize indexing speed and memory, we restrict content indexing to plain-text and source code files.
*   **Allowed Extensions:** `.txt`, `.c`, `.h`, `.cpp`, `.hpp`, `.json`, `.md`, `.py`, `.java`, `.xml`, `.html`, `.css`, `.js`, `.ts`, `.bat`, `.sh`, `.cfg`, `.ini`, `.yaml`.
*   **Encoding Support:** We read files into memory, convert UTF-8 or local ANSI text to wide strings using Windows' `MultiByteToWideChar` API, and perform tokenization.

### 1.3 Tokenization Rules
*   A token is a sequence of alphanumeric characters (`iswalnum` is true).
*   Any punctuation, whitespace, or control characters serve as delimiters.
*   Tokens are converted to lowercase using `towlower` to support case-insensitive full-text search.


## 2. API Design

### 2.1 Interface Definition (`include/core/content_index.h`)
```c
#ifndef ACH_CONTENT_INDEX_H
#define ACH_CONTENT_INDEX_H

#include "common/types.h"
#include "common/errors.h"
#include "core/index.h"
#include "data/hashmap.h"
#include "data/vector.h"

typedef struct ContentIndex {
    HashMap map;            /* Maps wchar_t* (word) -> Vector (of u32 File IDs) */
    bool initialized;
} ContentIndex;

/* ---- Lifecycle ----------------------------------------------------------- */
AchErrorCode content_index_init(ContentIndex *cindex);
void content_index_destroy(ContentIndex *cindex);
void content_index_clear(ContentIndex *cindex);

/* ---- Indexing Operations ------------------------------------------------ */

/* Parses and indexes the contents of a file.
 * Parameters:
 *   cindex    - Pointer to the initialized ContentIndex.
 *   file_id   - The index of the file in the Index.files table.
 *   file_path - Complete absolute path to the file.
 * Returns:
 *   ACH_SUCCESS on success, or an error code. */
AchErrorCode content_index_add_file(ContentIndex *cindex, u32 file_id, const wchar_t *file_path);

/* ---- Search Operations -------------------------------------------------- */

/* Looks up a single term in the content index.
 * Parameters:
 *   cindex      - Pointer to the ContentIndex.
 *   term        - Word to search (wide string).
 *   out_file_ids- Pointer to a Vector that receives the u32 file IDs (copied).
 * Returns:
 *   ACH_SUCCESS on success, or an error code. */
AchErrorCode content_index_search(const ContentIndex *cindex, const wchar_t *term, Vector *out_file_ids);

#endif /* ACH_CONTENT_INDEX_H */
```
