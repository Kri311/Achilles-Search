/* ==========================================================================
 * Achilles-Search | include/common/errors.h
 * ==========================================================================
 * Centralized error code system.
 *
 * WHY THIS FILE EXISTS:
 *   C has no exceptions. Every function communicates success or failure
 *   through its return value. The question is: what type?
 *
 *   Option 1: Return int (-1 on failure) — Fragile. What does -3 mean?
 *   Option 2: Return bool — No info about WHAT failed.
 *   Option 3: Return a named enum — Self-documenting. IDE-friendly.
 *             The caller sees ACH_ERROR_FILE_NOT_FOUND, not -17.
 *
 *   We choose Option 3. Every public function in Achilles-Search returns
 *   AchErrorCode. There is one exception: functions that cannot fail
 *   (pure computations, getters) may return their result directly.
 *
 * CONVENTIONS:
 *   - ACH_SUCCESS is always 0 (so `if (result)` means "error occurred")
 *   - Error codes are grouped by subsystem
 *   - ach_error_to_string() converts any code to a human-readable message
 *
 * OWNERSHIP:
 *   errors.h is a leaf dependency — it includes only types.h.
 *   It can be included anywhere without pulling in other modules.
 * ========================================================================== */

#ifndef ACH_ERRORS_H
#define ACH_ERRORS_H

#include "common/types.h"

/* ---- Error Code Enumeration --------------------------------------------- */
typedef enum AchErrorCode {
    /* -- General ---------------------------------------------------------- */
    ACH_SUCCESS             = 0,    /* Operation completed successfully       */
    ACH_ERROR_UNKNOWN       = 1,    /* Unclassified error                     */
    ACH_ERROR_INVALID_ARG   = 2,    /* NULL pointer or invalid parameter      */
    ACH_ERROR_OUT_OF_MEMORY = 3,    /* malloc/realloc/calloc returned NULL    */
    ACH_ERROR_NOT_FOUND     = 4,    /* Requested item does not exist          */
    ACH_ERROR_ALREADY_EXISTS = 5,   /* Item already exists (duplicate)        */

    /* -- I/O -------------------------------------------------------------- */
    ACH_ERROR_FILE_NOT_FOUND   = 100, /* File path does not exist             */
    ACH_ERROR_FILE_OPEN_FAILED = 101, /* fopen/CreateFile failed              */
    ACH_ERROR_FILE_READ_FAILED = 102, /* fread/ReadFile failed                */
    ACH_ERROR_FILE_WRITE_FAILED = 103, /* fwrite/WriteFile failed             */
    ACH_ERROR_PATH_TOO_LONG    = 104, /* Path exceeds MAX_PATH or our limit   */

    /* -- Logger ----------------------------------------------------------- */
    ACH_ERROR_LOGGER_INIT_FAILED  = 200, /* Logger initialization failed      */
    ACH_ERROR_LOGGER_NOT_INIT     = 201, /* Logger used before init           */

    /* -- Scanner (Phase 1) ------------------------------------------------ */
    ACH_ERROR_SCANNER_INIT_FAILED = 300,
    ACH_ERROR_SCANNER_ACCESS_DENIED = 301,

    /* -- Index (Phase 4+) ------------------------------------------------- */
    ACH_ERROR_INDEX_CORRUPT    = 400,
    ACH_ERROR_INDEX_FULL       = 401,

    /* -- Sentinel --------------------------------------------------------- */
    ACH_ERROR_COUNT                  /* Total number of error codes            */
} AchErrorCode;

/* ---- Error-to-String Conversion ----------------------------------------- */
/* Returns a human-readable string for the given error code.
 * The returned pointer is to a static string literal — never free it.
 *
 *   AchErrorCode err = some_function();
 *   if (err != ACH_SUCCESS) {
 *       printf("Error: %s\n", ach_error_to_string(err));
 *   }
 */
static inline const char* ach_error_to_string(AchErrorCode code) {
    switch (code) {
        /* General */
        case ACH_SUCCESS:               return "Success";
        case ACH_ERROR_UNKNOWN:         return "Unknown error";
        case ACH_ERROR_INVALID_ARG:     return "Invalid argument";
        case ACH_ERROR_OUT_OF_MEMORY:   return "Out of memory";
        case ACH_ERROR_NOT_FOUND:       return "Not found";
        case ACH_ERROR_ALREADY_EXISTS:  return "Already exists";

        /* I/O */
        case ACH_ERROR_FILE_NOT_FOUND:      return "File not found";
        case ACH_ERROR_FILE_OPEN_FAILED:    return "Failed to open file";
        case ACH_ERROR_FILE_READ_FAILED:    return "Failed to read file";
        case ACH_ERROR_FILE_WRITE_FAILED:   return "Failed to write file";
        case ACH_ERROR_PATH_TOO_LONG:       return "Path too long";

        /* Logger */
        case ACH_ERROR_LOGGER_INIT_FAILED:  return "Logger initialization failed";
        case ACH_ERROR_LOGGER_NOT_INIT:     return "Logger not initialized";

        /* Scanner */
        case ACH_ERROR_SCANNER_INIT_FAILED:     return "Scanner initialization failed";
        case ACH_ERROR_SCANNER_ACCESS_DENIED:   return "Scanner access denied";

        /* Index */
        case ACH_ERROR_INDEX_CORRUPT:   return "Index corrupt";
        case ACH_ERROR_INDEX_FULL:      return "Index full";

        /* Fallback */
        case ACH_ERROR_COUNT:           return "Invalid error code (sentinel)";
        default:                        return "Unknown error code";
    }
}

/* ---- Convenience Macros ------------------------------------------------- */

/* Check if a result indicates success */
#define ACH_SUCCEEDED(code)  ((code) == ACH_SUCCESS)

/* Check if a result indicates failure */
#define ACH_FAILED(code)     ((code) != ACH_SUCCESS)

/* Propagate an error: if the call fails, return the error immediately.
 * This is the C equivalent of exception propagation.
 *
 *   AchErrorCode init_subsystem(void) {
 *       ACH_TRY(init_logger());    // returns error if init_logger fails
 *       ACH_TRY(init_scanner());   // returns error if init_scanner fails
 *       return ACH_SUCCESS;
 *   }
 */
#define ACH_TRY(expr) \
    do { \
        AchErrorCode _ach_err = (expr); \
        if (ACH_FAILED(_ach_err)) { \
            return _ach_err; \
        } \
    } while (0)

#endif /* ACH_ERRORS_H */
