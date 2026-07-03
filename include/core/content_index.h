/* ==========================================================================
 * Achilles-Search | include/core/content_index.h
 * ==========================================================================
 * Full-text content indexer public interface.
 * ========================================================================== */

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
 *   out_file_ids- Pointer to an initialized Vector that receives the u32 file IDs.
 * Returns:
 *   ACH_SUCCESS on success, or an error code. */
AchErrorCode content_index_search(const ContentIndex *cindex, const wchar_t *term, Vector *out_file_ids);

#endif /* ACH_CONTENT_INDEX_H */
