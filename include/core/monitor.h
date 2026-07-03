/* ==========================================================================
 * Achilles-Search | include/core/monitor.h
 * ==========================================================================
 * Real-time filesystem monitor public interface (Asynchronous Overlapped).
 * ========================================================================== */

#ifndef ACH_MONITOR_H
#define ACH_MONITOR_H

#include "common/types.h"
#include "common/errors.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Change action types */
typedef enum AchMonitorAction {
    ACH_MONITOR_ADDED,
    ACH_MONITOR_REMOVED,
    ACH_MONITOR_MODIFIED,
    ACH_MONITOR_RENAMED
} AchMonitorAction;

/* Callback function type for file changes.
 * Parameters:
 *   action    - The change action.
 *   old_path  - The old path (only set for RENAMED, otherwise NULL).
 *   new_path  - The new path (set for ADDED, REMOVED, MODIFIED, and RENAMED).
 *   context   - User context pointer passed to monitor_start. */
typedef void (*AchMonitorCallback)(AchMonitorAction action, const wchar_t *old_path, const wchar_t *new_path, void *context);

typedef struct Monitor {
    wchar_t            *root_path;      /* Root path being monitored (heap-owned) */
    HANDLE              dir_handle;     /* Handle to root directory */
    HANDLE              thread_handle;  /* Background worker thread */
    HANDLE              stop_event;     /* Event to signal thread shutdown */
    AchMonitorCallback  callback;       /* User callback */
    void               *context;        /* User context */
    bool                running;        /* Thread status flag */
} Monitor;

/* ---- Lifecycle ----------------------------------------------------------- */
AchErrorCode monitor_init(Monitor *monitor, const wchar_t *root_path, AchMonitorCallback callback, void *context);
void monitor_destroy(Monitor *monitor);

/* ---- Operations ---------------------------------------------------------- */
AchErrorCode monitor_start(Monitor *monitor);
void monitor_stop(Monitor *monitor);

#endif /* ACH_MONITOR_H */
