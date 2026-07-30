/* ==========================================================================
 * Achilles-Search | include/common/logger.h
 * ==========================================================================
 * Lightweight leveled logging system.
 *
 * WHY THIS FILE EXISTS:
 *   printf debugging doesn't scale. When scanning millions of files across
 *   multiple threads, you need:
 *     - Log levels to filter noise
 *     - Timestamps to correlate events
 *     - Source location (__FILE__, __LINE__) to pinpoint origins
 *     - Colored console output for instant visual triage
 *
 * ARCHITECTURE:
 *   The public API is a set of macros (LOG_INFO, LOG_ERROR, etc.) that
 *   capture __FILE__ and __LINE__ automatically, then delegate to
 *   logger_log(). Users never call logger_log() directly.
 *
 *   Logger state is encapsulated in a static struct inside logger.c.
 *   There is no global variable — the state is module-private.
 *
 * LIFECYCLE:
 *   1. logger_init()    — call once at startup
 *   2. LOG_INFO(...)    — call from anywhere
 *   3. logger_shutdown() — call once at exit
 *
 * THREAD SAFETY:
 *   NOT thread-safe in Phase 0. Will add mutex protection in Phase 8.
 *   For now, only the main thread should log.
 *
 * OWNERSHIP:
 *   If a log file path is provided to logger_init(), the logger OWNS the
 *   file handle and will close it in logger_shutdown().
 * ========================================================================== */

#ifndef ACH_LOGGER_H
#define ACH_LOGGER_H

#include "common/types.h"
#include "common/errors.h"

/* ---- Log Levels --------------------------------------------------------- */
typedef enum LogLevel {
    LOG_LEVEL_DEBUG   = 0,  /* Verbose debugging info (disabled in Release)   */
    LOG_LEVEL_INFO    = 1,  /* Normal operational messages                    */
    LOG_LEVEL_WARNING = 2,  /* Something unexpected but recoverable          */
    LOG_LEVEL_ERROR   = 3,  /* Something went wrong, operation may fail      */
    LOG_LEVEL_FATAL   = 4,  /* Unrecoverable error, application should exit  */
    LOG_LEVEL_NONE    = 5,  /* Disable all logging                           */
} LogLevel;

/* ---- Logger Configuration ----------------------------------------------- */
typedef struct LoggerConfig {
    LogLevel    min_level;      /* Messages below this level are suppressed   */
    const char *log_file_path;  /* Optional: path to log file (NULL = none)  */
    bool        use_colors;     /* Enable ANSI/Win32 colored console output   */
} LoggerConfig;

/* ---- Lifecycle Functions ------------------------------------------------ */

/* Initialize the logging system.
 *
 * Parameters:
 *   config - Logger configuration. If NULL, uses defaults:
 *            level = INFO, no file, colors enabled.
 *
 * Returns:
 *   ACH_SUCCESS on success
 *   ACH_ERROR_LOGGER_INIT_FAILED if file creation fails
 *
 * MUST be called before any LOG_* macros.
 */
AchErrorCode logger_init(const LoggerConfig *config);

/* Shut down the logging system.
 * Flushes and closes the log file (if any).
 * Safe to call even if logger_init() was never called.
 */
void logger_shutdown(void);

/* ---- Core Logging Function ----------------------------------------------
 * DO NOT call this directly. Use the LOG_* macros below instead.
 * They automatically capture __FILE__ and __LINE__.
 */
void logger_log(LogLevel level, const char *file, int line,
                const char *fmt, ...);

/* ---- Public Logging Macros ----------------------------------------------
 * Usage:
 *   LOG_INFO("Indexing %d files in %s", count, path);
 *   LOG_ERROR("Failed to open: %s (error: %s)", path, ach_error_to_string(err));
 *
 * These macros expand to logger_log() calls with automatic source location.
 * The ... (variadic) allows printf-style format strings.
 * ------------------------------------------------------------------------- */
#define LOG_DEBUG(fmt, ...) \
    logger_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_INFO(fmt, ...) \
    logger_log(LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_WARNING(fmt, ...) \
    logger_log(LOG_LEVEL_WARNING, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
    logger_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_FATAL(fmt, ...) \
    logger_log(LOG_LEVEL_FATAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/* ---- Level Name Conversion ---------------------------------------------- */
static inline const char* logger_level_to_string(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_DEBUG:   return "DEBUG";
        case LOG_LEVEL_INFO:    return "INFO ";
        case LOG_LEVEL_WARNING: return "WARN ";
        case LOG_LEVEL_ERROR:   return "ERROR";
        case LOG_LEVEL_FATAL:   return "FATAL";
        case LOG_LEVEL_NONE:    return "NONE ";
        default:                return "";
    }
}

#endif /* ACH_LOGGER_H */
