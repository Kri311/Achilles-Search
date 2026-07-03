/* ==========================================================================
 * Achilles-Search | include/storage/database.h
 * ==========================================================================
 * Serialization and deserialization layer for saving the index to disk.
 * ========================================================================== */

#ifndef ACH_DATABASE_H
#define ACH_DATABASE_H

#include "common/types.h"
#include "common/errors.h"
#include "core/index.h"

#define ACH_DB_MAGIC "ACHD"
#define ACH_DB_VERSION 1

/* Saves the in-memory index to a binary database file.
 * Uses a fast custom binary serialization format.
 * Parameters:
 *   index     - Pointer to the Index to serialize.
 *   file_path - Path to the destination file.
 * Returns:
 *   ACH_SUCCESS on success, or an error code on failure. */
AchErrorCode db_save(const Index *index, const wchar_t *file_path);

/* Loads a binary database file into the in-memory index.
 * Automatically clears the index before loading and pre-allocates vectors.
 * Parameters:
 *   index     - Pointer to an initialized Index to deserialize into.
 *   file_path - Path to the database file to read.
 * Returns:
 *   ACH_SUCCESS on success, or an error code on failure. */
AchErrorCode db_load(Index *index, const wchar_t *file_path);

#endif /* ACH_DATABASE_H */
