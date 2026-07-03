/* ==========================================================================
 * Achilles-Search | include/common/utils.h
 * ==========================================================================
 * General-purpose utility functions.
 *
 * WHY THIS FILE EXISTS:
 *   Small helper functions that don't belong to any specific subsystem.
 *   Currently contains timestamp formatting (used by the logger).
 *   Will grow organically as the project needs shared utilities.
 *
 * DESIGN RULE:
 *   If a utility is used by only one module, keep it in that module.
 *   Only promote to utils.h when it's used by 2+ modules.
 * ========================================================================== */

#ifndef ACH_UTILS_H
#define ACH_UTILS_H

#include "common/types.h"
#include "common/errors.h"

/* ---- Timestamp Formatting -----------------------------------------------
 * Formats the current local time into the provided buffer.
 * Format: "YYYY-MM-DD HH:MM:SS"
 *
 * Parameters:
 *   buffer     - Output buffer (caller-allocated)
 *   buffer_len - Size of the buffer in bytes (must be >= ACH_TIMESTAMP_BUFFER)
 *
 * Returns:
 *   ACH_SUCCESS on success
 *   ACH_ERROR_INVALID_ARG if buffer is NULL or buffer_len is too small
 *
 * Thread Safety:
 *   Safe. Uses localtime_s() on Windows (thread-safe variant).
 *
 * OWNERSHIP:
 *   Caller owns the buffer. This function writes into it but does not
 *   allocate or free anything.
 * ------------------------------------------------------------------------- */
AchErrorCode utils_get_timestamp(char *buffer, usize buffer_len);

/* ---- Console Utilities --------------------------------------------------
 * Detects if the current process console window was spawned specifically
 * for this program (e.g., launched from Windows Explorer by double-clicking).
 *
 * If it returns true, the console will close the instant our main function
 * returns. We should prompt the user (e.g. "Press Enter to exit...") to
 * keep the window open so they can read the output.
 *
 * Returns:
 *   true if the console should be paused before exit.
 * ------------------------------------------------------------------------- */
bool utils_should_pause_on_exit(void);

#endif /* ACH_UTILS_H */
