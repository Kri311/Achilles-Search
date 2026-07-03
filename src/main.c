/* ==========================================================================
 * Achilles-Search | src/main.c
 * ==========================================================================
 * Application entry point.
 *
 * RESPONSIBILITIES:
 *   1. Print startup banner
 *   2. Initialize the logger
 *   3. Log startup confirmation
 *   4. (Future: initialize scanner, indexer, search engine, GUI)
 *   5. Shut down the logger
 *   6. Exit
 *
 * DESIGN:
 *   main.c is deliberately thin. It's an orchestrator, not a worker.
 *   All real logic lives in dedicated modules. main() just wires them
 *   together in the correct order.
 *
 *   Initialization order matters: logger must init before anything that
 *   calls LOG_*. Shutdown order is reversed (last initialized = first
 *   shut down), like a stack.
 * ========================================================================== */

#include "common/types.h"
#include "common/errors.h"
#include "common/config.h"
#include "common/macros.h"
#include "common/logger.h"

#include <stdio.h>

/* ---- Banner ------------------------------------------------------------- */
/* A visual separator that makes it immediately obvious the application
 * has started. Includes version and build info for debugging.
 *
 * __DATE__ and __TIME__ are replaced by the compiler with the build
 * timestamp. Combined with the version, this makes it trivial to identify
 * exactly which build a user is running when they report a bug.
 */
static void print_banner(void) {
    printf("\n");
    printf("============================================================\n");
    printf("       %s v%s\n", ACH_APP_NAME, ACH_VERSION_STRING);
#ifdef NDEBUG
    printf("       Build: Release | %s\n", __DATE__);
#else
    printf("       Build: Debug   | %s\n", __DATE__);
#endif
    printf("============================================================\n");
    printf("\n");
}

/* ---- Entry Point -------------------------------------------------------- */
int main(void) {
    /* 1. Print banner (before logger, so it shows even if logger fails) */
    print_banner();

    /* 2. Configure and initialize logger */
    LoggerConfig log_config = {
        .min_level     = LOG_LEVEL_DEBUG,
        .log_file_path = NULL,      /* No file logging in Phase 0 */
        .use_colors    = true,
    };

    AchErrorCode err = logger_init(&log_config);
    if (ACH_FAILED(err)) {
        fprintf(stderr, "FATAL: Logger initialization failed: %s\n",
                ach_error_to_string(err));
        return 1;
    }

    /* 3. Log startup */
    LOG_INFO("%s starting...", ACH_APP_NAME);
    LOG_INFO("Version: %s", ACH_VERSION_STRING);
    LOG_DEBUG("Debug logging is enabled");

    /* -------------------------------------------------------------------- */
    /* Future phases will add initialization here:                           */
    /*   Phase 1: scanner_init()                                            */
    /*   Phase 4: index_init()                                              */
    /*   Phase 5: database_init()                                           */
    /*   Phase 6: search_engine_init()                                      */
    /*   Phase 9: gui_init() + message loop                                 */
    /* -------------------------------------------------------------------- */

    /* 4. Shutdown (reverse order of initialization) */
    LOG_INFO("%s shutting down...", ACH_APP_NAME);
    logger_shutdown();

    return 0;
}
