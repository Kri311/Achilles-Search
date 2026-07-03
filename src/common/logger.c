/* ==========================================================================
 * Achilles-Search | src/common/logger.c
 * ==========================================================================
 * Implementation of the logging system.
 *
 * INTERNAL ARCHITECTURE:
 *   All logger state is stored in a file-scoped static struct (g_logger).
 *   This is NOT a global variable in the traditional sense — it's module-
 *   private. No other file can access it. This gives us encapsulation
 *   without needing opaque pointers or heap allocation.
 *
 * CONSOLE COLORS (Windows):
 *   We use Win32 SetConsoleTextAttribute() rather than ANSI escape codes.
 *   Reason: ANSI support in cmd.exe requires enabling VT processing
 *   (SetConsoleMode), which may fail on older systems. The Win32 API
 *   works everywhere from Windows XP to Windows 11.
 *
 * THREAD SAFETY:
 *   Not implemented yet. The g_logger struct is accessed without locking.
 *   This is acceptable in Phase 0 (single-threaded). Phase 8 will add
 *   a CRITICAL_SECTION or SRWLOCK around the write path.
 * ========================================================================== */

#include "common/logger.h"
#include "common/config.h"
#include "common/utils.h"
#include "common/macros.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* ---- Win32 Headers for Console Colors ----------------------------------- */
#define WIN32_LEAN_AND_MEAN   /* Exclude rarely-used Win32 APIs              */
#include <windows.h>

/* ---- Win32 Console Color Codes ------------------------------------------ */
/* These are attribute flags for SetConsoleTextAttribute().
 * Each is a combination of foreground intensity and color bits.
 *
 * Windows console colors work differently from ANSI:
 *   Bits 0-3: foreground color
 *   Bits 4-7: background color
 *   Bit 3: intensity (makes color brighter)
 */
#define COLOR_DEFAULT   7                                         /* White   */
#define COLOR_DEBUG     (FOREGROUND_GREEN | FOREGROUND_BLUE)       /* Cyan    */
#define COLOR_INFO      (FOREGROUND_GREEN | FOREGROUND_INTENSITY)  /* Green   */
#define COLOR_WARNING   (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY) /* Yellow */
#define COLOR_ERROR     (FOREGROUND_RED | FOREGROUND_INTENSITY)    /* Red     */
#define COLOR_FATAL     (FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY)  /* Magenta */

/* ---- Logger State (Module-Private) -------------------------------------- */
typedef struct LoggerState {
    bool        initialized;    /* Guard against use-before-init             */
    LogLevel    min_level;      /* Messages below this are suppressed        */
    FILE       *log_file;       /* Optional file output (NULL if disabled)   */
    bool        use_colors;     /* Whether to colorize console output        */
    HANDLE      console_handle; /* Win32 console handle for color changes    */
    WORD        default_attrs;  /* Original console attributes (restored)    */
} LoggerState;

/* File-scoped static: visible only within this translation unit.
 * Zero-initialized by C standard (all fields = 0/NULL/false). */
static LoggerState g_logger = {0};

/* ---- Internal Helpers --------------------------------------------------- */

/* Maps a LogLevel to a Win32 console color attribute. */
static WORD level_to_color(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_DEBUG:   return COLOR_DEBUG;
        case LOG_LEVEL_INFO:    return COLOR_INFO;
        case LOG_LEVEL_WARNING: return COLOR_WARNING;
        case LOG_LEVEL_ERROR:   return COLOR_ERROR;
        case LOG_LEVEL_FATAL:   return COLOR_FATAL;
        default:                return COLOR_DEFAULT;
    }
}

/* Extracts just the filename from a full path.
 * "d:\\Projects\\Achilles-Search\\src\\main.c" → "main.c"
 *
 * WHY: Full paths in log output are noisy and make logs hard to read.
 *      The filename alone is usually sufficient to locate the source.
 */
static const char* extract_filename(const char *path) {
    if (path == NULL) return "unknown";

    const char *last_slash = strrchr(path, '\\');
    if (last_slash == NULL) {
        last_slash = strrchr(path, '/');
    }

    return (last_slash != NULL) ? (last_slash + 1) : path;
}

/* ---- Public API Implementation ------------------------------------------ */

AchErrorCode logger_init(const LoggerConfig *config) {
    /* Prevent double initialization */
    if (g_logger.initialized) {
        return ACH_SUCCESS;
    }

    /* Apply configuration (or defaults) */
    if (config != NULL) {
        g_logger.min_level  = config->min_level;
        g_logger.use_colors = config->use_colors;
    } else {
        g_logger.min_level  = LOG_LEVEL_INFO;
        g_logger.use_colors = true;
    }

    /* Open log file if requested */
    g_logger.log_file = NULL;
    if (config != NULL && config->log_file_path != NULL) {
        g_logger.log_file = fopen(config->log_file_path, "a");
        if (g_logger.log_file == NULL) {
            /* Don't fail hard — logging to console still works */
            fprintf(stderr, "[LOGGER] Warning: Could not open log file: %s\n",
                    config->log_file_path);
        }
    }

    /* Initialize console handle for colored output */
    if (g_logger.use_colors) {
        g_logger.console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
        if (g_logger.console_handle != INVALID_HANDLE_VALUE) {
            /* Save original console attributes so we can restore them */
            CONSOLE_SCREEN_BUFFER_INFO info;
            if (GetConsoleScreenBufferInfo(g_logger.console_handle, &info)) {
                g_logger.default_attrs = info.wAttributes;
            } else {
                g_logger.default_attrs = COLOR_DEFAULT;
            }
        } else {
            g_logger.use_colors = false;
        }
    }

    g_logger.initialized = true;
    return ACH_SUCCESS;
}

void logger_shutdown(void) {
    if (!g_logger.initialized) {
        return;
    }

    /* Flush and close log file */
    if (g_logger.log_file != NULL) {
        fflush(g_logger.log_file);
        fclose(g_logger.log_file);
        g_logger.log_file = NULL;
    }

    /* Restore console colors */
    if (g_logger.use_colors &&
        g_logger.console_handle != INVALID_HANDLE_VALUE) {
        SetConsoleTextAttribute(g_logger.console_handle,
                                g_logger.default_attrs);
    }

    g_logger.initialized = false;
}

/* --------------------------------------------------------------------------
 * logger_log  — Core logging function
 * --------------------------------------------------------------------------
 * This is called by the LOG_* macros. Do NOT call directly.
 *
 * FORMAT:
 *   [2026-07-03 22:30:15] [INFO ] [main.c:42] Message here
 *
 * FLOW:
 *   1. Check if initialized and if level >= min_level
 *   2. Format timestamp
 *   3. Format the user's message (printf-style)
 *   4. Output to console (with colors if enabled)
 *   5. Output to file (without colors, with source location)
 *
 * PERFORMANCE NOTE:
 *   This function does string formatting on the stack (no heap allocation).
 *   The ACH_LOG_BUFFER_SIZE (2048) buffer lives on the stack and is cheap
 *   to allocate/deallocate (~2KB of stack per call, well within limits).
 * -------------------------------------------------------------------------- */
void logger_log(LogLevel level, const char *file, int line,
                const char *fmt, ...) {
    /* Early exit: not initialized or level filtered out */
    if (!g_logger.initialized || level < g_logger.min_level) {
        return;
    }

    /* 1. Format timestamp */
    char timestamp[ACH_TIMESTAMP_BUFFER];
    if (ACH_FAILED(utils_get_timestamp(timestamp, sizeof(timestamp)))) {
        strncpy(timestamp, "????-??-?? ??:??:??", sizeof(timestamp) - 1);
        timestamp[sizeof(timestamp) - 1] = '\0';
    }

    /* 2. Format the user's message using va_list */
    char message[ACH_LOG_BUFFER_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    /* 3. Extract filename from full path */
    const char *filename = extract_filename(file);

    /* 4. Get level string */
    const char *level_str = logger_level_to_string(level);

    /* 5. Output to console */
    if (g_logger.use_colors &&
        g_logger.console_handle != INVALID_HANDLE_VALUE) {
        /* Print timestamp and bracket in default color */
        fprintf(stdout, "[%s] [", timestamp);

        /* Print level in its designated color */
        fflush(stdout);  /* Flush before changing color */
        SetConsoleTextAttribute(g_logger.console_handle,
                                level_to_color(level));
        fprintf(stdout, "%s", level_str);
        fflush(stdout);  /* Flush colored text */

        /* Restore default color for the rest */
        SetConsoleTextAttribute(g_logger.console_handle,
                                g_logger.default_attrs);
        fprintf(stdout, "] %s\n", message);
    } else {
        /* No colors: plain text output */
        fprintf(stdout, "[%s] [%s] %s\n", timestamp, level_str, message);
    }

    /* 6. Output to file (if configured) */
    if (g_logger.log_file != NULL) {
        fprintf(g_logger.log_file, "[%s] [%s] [%s:%d] %s\n",
                timestamp, level_str, filename, line, message);
        fflush(g_logger.log_file);  /* Ensure log is written immediately */
    }
}
