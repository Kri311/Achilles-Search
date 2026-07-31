/* ==========================================================================
 * Achilles-Search | src/core/monitor.c
 * ==========================================================================
 * Real-time filesystem monitor implementation using Overlapped I/O.
 * ========================================================================== */

#include "core/monitor.h"
#include "common/logger.h"
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* ---- Static Helpers ------------------------------------------------------ */

static wchar_t* path_join_w(const wchar_t *dir, const wchar_t *name) {
    if (dir == NULL || name == NULL) return NULL;

    usize dir_len = wcslen(dir);
    usize name_len = wcslen(name);

    bool has_sep = (dir_len > 0 && dir[dir_len - 1] == L'\\');
    usize total_len = dir_len + (has_sep ? 0 : 1) + name_len + 1;

    if (total_len < dir_len) return NULL;

    wchar_t *result = (wchar_t*)malloc(total_len * sizeof(wchar_t));
    if (result == NULL) return NULL;

    memcpy(result, dir, dir_len * sizeof(wchar_t));

    usize pos = dir_len;
    if (!has_sep) {
        result[pos++] = L'\\';
    }

    memcpy(result + pos, name, name_len * sizeof(wchar_t));
    pos += name_len;
    result[pos] = L'\0';

    return result;
}

/* ---- Background Worker Thread -------------------------------------------- */

static DWORD WINAPI monitor_thread_func(LPVOID lpParam) {
    Monitor *monitor = (Monitor*)lpParam;
    if (monitor == NULL || monitor->dir_handle == INVALID_HANDLE_VALUE || monitor->stop_event == NULL) {
        return 1;
    }

    /* 64 KB buffer for filesystem events */
    DWORD buffer_size = 65536;
    BYTE *buffer = malloc(buffer_size);
    if (buffer == NULL) {
        LOG_ERROR("Monitor: thread failed to allocate buffer");
        return 1;
    }

    /* Create event for overlapped structure */
    HANDLE io_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (io_event == NULL) {
        LOG_ERROR("Monitor: thread failed to create I/O event handle");
        free(buffer);
        return 1;
    }

    OVERLAPPED overlapped = {0};
    overlapped.hEvent = io_event;

    wchar_t *old_path_temp = NULL;
    HANDLE wait_handles[2] = { io_event, monitor->stop_event };

    while (monitor->running) {
        ResetEvent(io_event);
        DWORD bytes_returned = 0;

        BOOL success = ReadDirectoryChangesW(
            monitor->dir_handle,
            buffer,
            buffer_size,
            TRUE, /* Monitor subdirectories recursively */
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE |
            FILE_NOTIFY_CHANGE_ATTRIBUTES,
            &bytes_returned,
            &overlapped, /* Pass overlapped structure */
            NULL
        );

        if (!success) {
            DWORD err = GetLastError();
            if (err != ERROR_IO_PENDING) {
                LOG_ERROR("Monitor: ReadDirectoryChangesW initiation failed (error %lu)", (unsigned long)err);
                break;
            }
        }

        /* Wait for either I/O completion or stop signal */
        DWORD wait_res = WaitForMultipleObjects(2, wait_handles, FALSE, INFINITE);

        if (wait_res == WAIT_OBJECT_0 + 1) {
            /* Stop event signaled. Cancel I/O and exit. */
            CancelIoEx(monitor->dir_handle, &overlapped);
            break;
        }

        if (wait_res == WAIT_OBJECT_0) {
            /* I/O completed, read results from buffer */
            DWORD transferred = 0;
            if (GetOverlappedResult(monitor->dir_handle, &overlapped, &transferred, FALSE)) {
                if (transferred == 0) {
                    continue;
                }

                FILE_NOTIFY_INFORMATION *fni = (FILE_NOTIFY_INFORMATION*)buffer;
                while (fni != NULL) {
                    usize char_count = fni->FileNameLength / sizeof(wchar_t);
                    wchar_t *rel_path = malloc((char_count + 1) * sizeof(wchar_t));
                    if (rel_path != NULL) {
                        memcpy(rel_path, fni->FileName, fni->FileNameLength);
                        rel_path[char_count] = L'\0';

                        wchar_t *full_path = path_join_w(monitor->root_path, rel_path);
                        if (full_path != NULL) {
                            /* Dispatch actions */
                            switch (fni->Action) {
                                case FILE_ACTION_ADDED:
                                    if (monitor->callback != NULL) {
                                        monitor->callback(ACH_MONITOR_ADDED, NULL, full_path, monitor->context);
                                    }
                                    break;

                                case FILE_ACTION_REMOVED:
                                    if (monitor->callback != NULL) {
                                        monitor->callback(ACH_MONITOR_REMOVED, NULL, full_path, monitor->context);
                                    }
                                    break;

                                case FILE_ACTION_MODIFIED:
                                    if (monitor->callback != NULL) {
                                        monitor->callback(ACH_MONITOR_MODIFIED, NULL, full_path, monitor->context);
                                    }
                                    break;

                                case FILE_ACTION_RENAMED_OLD_NAME:
                                    if (old_path_temp != NULL) {
                                        free(old_path_temp);
                                    }
                                    old_path_temp = _wcsdup(full_path);
                                    break;

                                case FILE_ACTION_RENAMED_NEW_NAME:
                                    if (monitor->callback != NULL) {
                                        monitor->callback(ACH_MONITOR_RENAMED, old_path_temp, full_path, monitor->context);
                                    }
                                    if (old_path_temp != NULL) {
                                        free(old_path_temp);
                                        old_path_temp = NULL;
                                    }
                                    break;

                                default:
                                    break;
                            }
                            free(full_path);
                        }
                        free(rel_path);
                    }

                    if (fni->NextEntryOffset == 0) {
                        break;
                    }
                    fni = (FILE_NOTIFY_INFORMATION*)((BYTE*)fni + fni->NextEntryOffset);
                }
            }
        }
    }

    if (old_path_temp != NULL) {
        free(old_path_temp);
    }
    CloseHandle(io_event);
    free(buffer);
    return 0;
}

/* ---- Lifecycle ----------------------------------------------------------- */

AchErrorCode monitor_init(Monitor *monitor, const wchar_t *root_path, AchMonitorCallback callback, void *context) {
    if (monitor == NULL || root_path == NULL || callback == NULL) {
        return ACH_ERROR_INVALID_ARG;
    }

    monitor->root_path = _wcsdup(root_path);
    if (monitor->root_path == NULL) {
        return ACH_ERROR_OUT_OF_MEMORY;
    }

    /* Create stop event */
    monitor->stop_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (monitor->stop_event == NULL) {
        free(monitor->root_path);
        monitor->root_path = NULL;
        return ACH_ERROR_UNKNOWN;
    }

    monitor->dir_handle = INVALID_HANDLE_VALUE;
    monitor->thread_handle = NULL;
    monitor->callback = callback;
    monitor->context = context;
    monitor->running = false;

    return ACH_SUCCESS;
}

void monitor_destroy(Monitor *monitor) {
    if (monitor == NULL) {
        return;
    }

    monitor_stop(monitor);

    if (monitor->stop_event != NULL) {
        CloseHandle(monitor->stop_event);
        monitor->stop_event = NULL;
    }

    if (monitor->root_path != NULL) {
        free(monitor->root_path);
        monitor->root_path = NULL;
    }
}

/* ---- Operations ---------------------------------------------------------- */

AchErrorCode monitor_start(Monitor *monitor) {
    if (monitor == NULL || monitor->root_path == NULL || monitor->stop_event == NULL) {
        return ACH_ERROR_INVALID_ARG;
    }

    if (monitor->running) {
        return ACH_SUCCESS; /* Already running */
    }

    /* Open Directory handle with LIST_DIRECTORY, backup semantics, and OVERLAPPED flag */
    monitor->dir_handle = CreateFileW(
        monitor->root_path,
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL
    );

    if (monitor->dir_handle == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        LOG_ERROR("Monitor: failed to open root directory (error %lu)", (unsigned long)err);
        return ACH_ERROR_FILE_NOT_FOUND;
    }

    ResetEvent(monitor->stop_event);
    monitor->running = true;

    /* Start background worker thread */
    monitor->thread_handle = CreateThread(
        NULL,
        0,
        monitor_thread_func,
        monitor,
        0,
        NULL
    );

    if (monitor->thread_handle == NULL) {
        DWORD err = GetLastError();
        LOG_ERROR("Monitor: failed to start monitoring thread (error %lu)", (unsigned long)err);
        CloseHandle(monitor->dir_handle);
        monitor->dir_handle = INVALID_HANDLE_VALUE;
        monitor->running = false;
        return ACH_ERROR_UNKNOWN;
    }

    LOG_INFO("Monitor: started watching directory: %ws", monitor->root_path);
    return ACH_SUCCESS;
}

void monitor_stop(Monitor *monitor) {
    if (monitor == NULL || !monitor->running) {
        return;
    }

    monitor->running = false;

    /* Signal thread to stop */
    if (monitor->stop_event != NULL) {
        SetEvent(monitor->stop_event);
    }

    /* Wait for thread completion and close its handle */
    if (monitor->thread_handle != NULL) {
        WaitForSingleObject(monitor->thread_handle, 3000); /* Wait up to 3 seconds */
        CloseHandle(monitor->thread_handle);
        monitor->thread_handle = NULL;
    }

    /* Close directory handle */
    if (monitor->dir_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(monitor->dir_handle);
        monitor->dir_handle = INVALID_HANDLE_VALUE;
    }

    LOG_INFO("Monitor: stopped watching directory");
}
