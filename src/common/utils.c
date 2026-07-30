/* ==========================================================================
 * Achilles-Search | src/common/utils.c
 * ==========================================================================
 * Implementation of general-purpose utility functions.
 * ========================================================================== */

#include "common/utils.h"
#include "common/config.h"

#include <stdio.h>
#include <time.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* --------------------------------------------------------------------------
 * utils_get_timestamp
 * --------------------------------------------------------------------------
 * Formats the current local time as "YYYY-MM-DD HH:MM:SS".
 *
 * WHY localtime_s()
 *   The standard localtime() returns a pointer to a STATIC internal buffer.
 *   If two threads call localtime() simultaneously, they overwrite each
 *   other's data → race condition. localtime_s() (MSVC) writes to a
 *   caller-provided struct, making it thread-safe.
 *
 * WHY snprintf()
 *   snprintf() guarantees null-termination and prevents buffer overflows,
 *   unlike sprintf(). Always use snprintf() in production code.
 * -------------------------------------------------------------------------- */
AchErrorCode utils_get_timestamp(char *buffer, usize buffer_len) {
    if (buffer == NULL || buffer_len < ACH_TIMESTAMP_BUFFER) {
        return ACH_ERROR_INVALID_ARG;
    }

    time_t now = time(NULL);
    struct tm local_time;

    /* localtime_s is the MSVC thread-safe variant.
     * Note: parameter order is reversed from POSIX localtime_r(). */
    if (localtime_s(&local_time, &now) != 0) {
        /* Fallback: write a placeholder if time conversion fails */
        snprintf(buffer, buffer_len, "0000-00-00 00:00:00");
        return ACH_ERROR_UNKNOWN;
    }

    snprintf(buffer, buffer_len,
             "%04d-%02d-%02d %02d:%02d:%02d",
             local_time.tm_year + 1900,  /* tm_year is years since 1900 */
             local_time.tm_mon + 1,      /* tm_mon is 0-indexed         */
             local_time.tm_mday,
             local_time.tm_hour,
             local_time.tm_min,
             local_time.tm_sec);

    return ACH_SUCCESS;
}

bool utils_should_pause_on_exit(void) {
    DWORD process_list[2];
    DWORD count = GetConsoleProcessList(process_list, 2);
    return (count <= 1);
}
