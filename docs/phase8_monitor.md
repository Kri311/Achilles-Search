# Phase 8: Filesystem Monitoring Design Specification

This document details the architecture, Win32 API interactions, thread dispatch model, and events handling for the **Filesystem Monitoring** module in Achilles-Search. The module enables real-time index synchronization when files are created, modified, deleted, or renamed.


## 1. Architectural Decisions

### 1.1 Dedicated Background Thread Watcher
Achilles-Search uses Windows' native directory change monitoring API, `ReadDirectoryChangesW`.
*   **Execution Model:** The watcher runs on a dedicated background worker thread, calling `ReadDirectoryChangesW` synchronously in a loop.
*   **Cancellation Strategy:** To stop the watcher cleanly, the main thread closes the directory handle. This causes `ReadDirectoryChangesW` on the worker thread to immediately return with error code `ERROR_OPERATION_ABORTED`. The worker thread intercepts this code, frees its stack-allocated resources, and exits cleanly.
*   **Decoupled Callback:** The watcher dispatches events via a callback interface containing the change action, old path (for renames), and new path.

### 1.2 Win32 Directory Handles
Directories must be opened with specific flags:
*   **Access Mask:** `FILE_LIST_DIRECTORY`.
*   **Share Mode:** `FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE`.
*   **Flags:** `FILE_FLAG_BACKUP_SEMANTICS` is mandatory to successfully open directory handles on Windows.

### 1.3 Handling Renames
Renaming a file produces two consecutive notifications from the OS:
1.  `FILE_ACTION_RENAMED_OLD_NAME`
2.  `FILE_ACTION_RENAMED_NEW_NAME`
The monitor caches the old path. When the new path event arrives immediately after, it merges them and dispatches a single atomic `ACH_MONITOR_RENAMED` event.


## 2. API Design

### 2.1 Interface Definition (`include/core/monitor.h`)
```c
#ifndef ACH_MONITOR_H
#define ACH_MONITOR_H

#include "common/types.h"
#include "common/errors.h"

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
    wchar_t            *root_path;      /* Root path being monitored */
    HANDLE              dir_handle;     /* Handle to root directory */
    HANDLE              thread_handle;  /* Background worker thread */
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
```
