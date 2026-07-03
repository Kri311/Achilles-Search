/* ==========================================================================
 * Achilles-Search | include/common/config.h
 * ==========================================================================
 * Compile-time project configuration and constants.
 *
 * WHY THIS FILE EXISTS:
 *   Magic numbers scattered across source files are a maintenance nightmare.
 *   Every tunable constant lives here so that:
 *   1. Changing a limit means editing ONE place
 *   2. Constants are documented with their rationale
 *   3. We can validate constraints at compile time with _Static_assert
 *
 * CATEGORIES:
 *   - Version info (injected by CMake or defined here as fallback)
 *   - Path limits
 *   - Buffer sizes
 *   - Performance tuning parameters
 *
 * NOTE: Runtime configuration (user preferences, config files) will be
 * handled by a separate config_manager module in a future phase. This
 * file is strictly for compile-time constants.
 * ========================================================================== */

#ifndef ACH_CONFIG_H
#define ACH_CONFIG_H

#include "common/types.h"
#include "common/macros.h"

/* ---- Version ------------------------------------------------------------ */
/* CMake injects ACH_VERSION_MAJOR/MINOR/PATCH via -D flags.
 * If compiling without CMake, fall back to hardcoded values. */
#ifndef ACH_VERSION_MAJOR
    #define ACH_VERSION_MAJOR 0
#endif
#ifndef ACH_VERSION_MINOR
    #define ACH_VERSION_MINOR 1
#endif
#ifndef ACH_VERSION_PATCH
    #define ACH_VERSION_PATCH 0
#endif

#define ACH_VERSION_STRING \
    ACH_STRINGIFY(ACH_VERSION_MAJOR) "." \
    ACH_STRINGIFY(ACH_VERSION_MINOR) "." \
    ACH_STRINGIFY(ACH_VERSION_PATCH)

/* ---- Application Name --------------------------------------------------- */
#define ACH_APP_NAME        "Achilles Search Engine"
#define ACH_APP_NAME_SHORT  "Achilles"

/* ---- Path Limits --------------------------------------------------------
 * Windows MAX_PATH is 260 characters, but long path support (Win10 1607+)
 * allows up to ~32,767 characters with the \\?\ prefix.
 *
 * We use 1024 as a practical limit for internal buffers. Paths exceeding
 * this are still handled via dynamic allocation in the scanner.
 * ----------------------------------------------------------------------- */
#define ACH_MAX_PATH_LENGTH     1024
#define ACH_MAX_FILENAME_LENGTH 256

/* ---- Buffer Sizes ------------------------------------------------------- */
#define ACH_LOG_BUFFER_SIZE     2048    /* Max formatted log message length   */
#define ACH_TIMESTAMP_BUFFER    32      /* "2026-07-03 22:30:15" + null       */

/* ---- Performance Tuning ------------------------------------------------- */
#define ACH_DEFAULT_HASHMAP_CAPACITY  4096
#define ACH_DEFAULT_VECTOR_CAPACITY   64
#define ACH_ARENA_DEFAULT_BLOCK_SIZE  ACH_MB(1)  /* 1 MB arena blocks        */

/* ---- Scanner (Phase 1) ------------------------------------------------- */
#define ACH_SCANNER_MAX_DEPTH         128  /* Max directory recursion depth   */
#define ACH_SCANNER_BATCH_SIZE        1024 /* Files processed per batch       */

/* ---- Index (Phase 4+) --------------------------------------------------- */
#define ACH_INDEX_SHARD_COUNT         16   /* Number of index shards          */

/* ---- Compile-Time Validation -------------------------------------------- */
ACH_STATIC_ASSERT(ACH_MAX_PATH_LENGTH >= 260,
    "ACH_MAX_PATH_LENGTH must be at least Windows MAX_PATH (260)");
ACH_STATIC_ASSERT(ACH_LOG_BUFFER_SIZE >= 256,
    "Log buffer must be at least 256 bytes");
ACH_STATIC_ASSERT(ACH_DEFAULT_VECTOR_CAPACITY >= 8,
    "Default vector capacity must be at least 8");

#endif /* ACH_CONFIG_H */
