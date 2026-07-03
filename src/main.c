/* ==========================================================================
 * Achilles-Search | src/main.c
 * ==========================================================================
 * Native Windows GUI Entry Point.
 * ========================================================================== */

#include "common/types.h"
#include "common/errors.h"
#include "common/config.h"
#include "common/logger.h"
#include "common/utils.h"
#include "core/scanner.h"
#include "core/index.h"
#include "core/search.h"
#include "core/content_index.h"
#include "core/monitor.h"
#include "storage/database.h"

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>

/* Embed common controls version 6 manifest for visual styles */
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")

/* ---- Control Identifiers ------------------------------------------------- */
#define IDC_SEARCH_EDIT      1001
#define IDC_BROWSE_BTN       1002
#define IDC_INDEX_BTN        1003
#define IDC_DIR_EDIT         1004
#define IDC_RESULTS_LIST     1005
#define IDC_STATUS_BAR       1006

/* Context Menu IDs */
#define ID_POPUP_OPEN_LOCATION 2001
#define ID_POPUP_COPY_PATH     2002

/* Custom User Messages */
#define WM_USER_INDEX_PROGRESS (WM_USER + 1)
#define WM_USER_INDEX_COMPLETE (WM_USER + 2)

/* ---- Application State Struct -------------------------------------------- */
typedef struct AppState {
    HWND hwnd_main;
    HWND hwnd_search_edit;
    HWND hwnd_dir_edit;
    HWND hwnd_browse_btn;
    HWND hwnd_index_btn;
    HWND hwnd_results_list;
    HWND hwnd_status_bar;

    Index index;
    HashMap dir_map;
    SearchEngineResults search_results;

    wchar_t selected_dir[MAX_PATH];
    bool is_indexing;
    double last_scan_duration_ms;
    int last_context_item;
} AppState;

/* ---- Thread Parameter Struct -------------------------------------------- */
typedef struct IndexThreadParams {
    HWND hwnd_window;
    wchar_t root_path[MAX_PATH];
} IndexThreadParams;

/* ---- Helper: Folder Browse Dialog ---------------------------------------- */
static BOOL browse_folder(HWND hwnd_owner, wchar_t *out_path) {
    BROWSEINFOW bi = {0};
    bi.hwndOwner = hwnd_owner;
    bi.lpszTitle = L"Select Directory to Index";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl != NULL) {
        BOOL success = SHGetPathFromIDListW(pidl, out_path);
        CoTaskMemFree(pidl);
        return success;
    }
    return FALSE;
}

/* ---- Helper: Copy to Clipboard ------------------------------------------- */
static void copy_to_clipboard(HWND hwnd_owner, const wchar_t *text) {
    if (text == NULL) return;
    usize len = wcslen(text);
    HGLOBAL hmem = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(wchar_t));
    if (hmem != NULL) {
        wchar_t *buf = (wchar_t*)GlobalLock(hmem);
        if (buf != NULL) {
            wcscpy_s(buf, len + 1, text);
            GlobalUnlock(hmem);
            if (OpenClipboard(hwnd_owner)) {
                EmptyClipboard();
                SetClipboardData(CF_UNICODETEXT, hmem);
                CloseClipboard();
            } else {
                GlobalFree(hmem);
            }
        } else {
            GlobalFree(hmem);
        }
    }
}

/* ---- Worker Thread: Background Indexer ---------------------------------- */
static DWORD WINAPI index_worker_thread(LPVOID lpParam) {
    IndexThreadParams *params = (IndexThreadParams*)lpParam;
    if (params == NULL) return 1;

    /* Initialize and run directory scanner */
    Scanner scanner;
    scanner_init(&scanner);

    ScanConfig scan_config = {
        .root_path = params->root_path,
        .max_depth = 0,
        .include_hidden = false,
        .include_system = false
    };

    AchErrorCode err = scanner_scan(&scanner, &scan_config);

    if (err == ACH_SUCCESS) {
        AppState *state = (AppState*)GetWindowLongPtrW(params->hwnd_window, GWLP_USERDATA);
        if (state != NULL) {
            /* Thread-safe memory allocation block on Index structure */
            index_clear(&state->index);
            hashmap_init(&state->dir_map, sizeof(u32), 1024);

            const Vector *results = scanner_get_results(&scanner);
            usize count = vector_length(results);

            for (usize i = 0; i < count; i++) {
                const FileInfo *info = (const FileInfo*)vector_get(results, i);
                index_add_file_info(&state->index, &state->dir_map, info);

                /* Post periodic count updates back to the UI thread */
                if (i > 0 && i % 1000 == 0) {
                    PostMessageW(params->hwnd_window, WM_USER_INDEX_PROGRESS, (WPARAM)i, 0);
                }
            }

            hashmap_destroy(&state->dir_map);

            const ScanStats *stats = scanner_get_stats(&scanner);
            state->last_scan_duration_ms = stats->duration_ms;
        }
    }

    scanner_destroy(&scanner);

    /* Post completion back to UI message loop */
    PostMessageW(params->hwnd_window, WM_USER_INDEX_COMPLETE, (WPARAM)err, 0);

    free(params);
    return 0;
}

/* ---- ListView Results Populator ------------------------------------------ */
static void populate_listview(AppState *state) {
    if (state == NULL || state->hwnd_results_list == NULL) return;

    /* Disable redrawing during bulk update to prevent flicker */
    SendMessageW(state->hwnd_results_list, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(state->hwnd_results_list);

    usize count = search_results_count(&state->search_results);
    /* Display maximum of 1000 results for GUI thread performance */
    for (usize i = 0; i < count && i < 1000; i++) {
        const SearchResult *res = search_results_get(&state->search_results, i);
        if (res == NULL) continue;

        wchar_t name[MAX_PATH] = {0};
        wchar_t path[MAX_PATH] = {0};

        if (res->is_directory) {
            const IndexDir *dir = vector_get(&state->index.dirs, res->index_id);
            if (dir != NULL) {
                wcscpy_s(name, MAX_PATH, dir->name);
                index_get_dir_path(&state->index, res->index_id, path, MAX_PATH);
            }
        } else {
            const IndexFile *file = vector_get(&state->index.files, res->index_id);
            if (file != NULL) {
                wcscpy_s(name, MAX_PATH, file->name);
                index_get_file_path(&state->index, res->index_id, path, MAX_PATH);
            }
        }

        LVITEMW lvi = {0};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = (int)i;
        lvi.iSubItem = 0;
        lvi.pszText = name;
        ListView_InsertItem(state->hwnd_results_list, &lvi);

        ListView_SetItemText(state->hwnd_results_list, i, 1, res->is_directory ? L"Directory" : L"File");
        ListView_SetItemText(state->hwnd_results_list, i, 2, path);
    }

    SendMessageW(state->hwnd_results_list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(state->hwnd_results_list, NULL, TRUE);
}

/* ---- Window Resizing Layout handler ------------------------------------- */
static void handle_resize(HWND hwnd, int width, int height) {
    AppState *state = (AppState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (state == NULL) return;

    int padding = 10;
    int top_y = padding;
    int row_height = 25;
    int btn_width = 80;

    /* Row 1: Directory Edit + Browse button + Index button */
    int edit_width = width - (btn_width * 2) - (padding * 4);
    if (edit_width < 50) edit_width = 50;

    MoveWindow(state->hwnd_dir_edit, padding, top_y, edit_width, row_height, TRUE);
    MoveWindow(state->hwnd_browse_btn, padding * 2 + edit_width, top_y, btn_width, row_height, TRUE);
    MoveWindow(state->hwnd_index_btn, padding * 3 + edit_width + btn_width, top_y, btn_width, row_height, TRUE);

    /* Row 2: Search input edit control */
    top_y += row_height + padding;
    MoveWindow(state->hwnd_search_edit, padding, top_y, width - (padding * 2), row_height, TRUE);

    /* Row 3: Grid ListView */
    top_y += row_height + padding;
    int status_height = 20;
    int list_height = height - top_y - status_height - padding;
    if (list_height < 0) list_height = 0;

    MoveWindow(state->hwnd_results_list, padding, top_y, width - (padding * 2), list_height, TRUE);

    /* Auto-position status bar */
    SendMessageW(state->hwnd_status_bar, WM_SIZE, 0, 0);
}

/* ---- Main Win32 Dialog Procedure ---------------------------------------- */
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppState *state = (AppState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_CREATE: {
            /* Create App State */
            state = (AppState*)malloc(sizeof(AppState));
            if (state == NULL) return -1;
            memset(state, 0, sizeof(AppState));

            state->hwnd_main = hwnd;
            index_init(&state->index);
            search_results_init(&state->search_results);
            wcscpy_s(state->selected_dir, MAX_PATH, L"C:\\");
            state->last_context_item = -1;

            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)state);

            /* Create child windows */
            state->hwnd_dir_edit = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", L"C:\\",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
                0, 0, 0, 0, hwnd, (HMENU)IDC_DIR_EDIT, GetModuleHandle(NULL), NULL
            );

            state->hwnd_browse_btn = CreateWindowExW(
                0, L"BUTTON", L"Browse...",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 0, 0, hwnd, (HMENU)IDC_BROWSE_BTN, GetModuleHandle(NULL), NULL
            );

            state->hwnd_index_btn = CreateWindowExW(
                0, L"BUTTON", L"Index",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 0, 0, hwnd, (HMENU)IDC_INDEX_BTN, GetModuleHandle(NULL), NULL
            );

            state->hwnd_search_edit = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                0, 0, 0, 0, hwnd, (HMENU)IDC_SEARCH_EDIT, GetModuleHandle(NULL), NULL
            );

            /* ListView definition */
            state->hwnd_results_list = CreateWindowExW(
                WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
                0, 0, 0, 0, hwnd, (HMENU)IDC_RESULTS_LIST, GetModuleHandle(NULL), NULL
            );

            /* Setup columns in ListView */
            LVCOLUMNW col = {0};
            col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

            col.pszText = L"Name";
            col.cx = 180;
            col.iSubItem = 0;
            ListView_InsertColumn(state->hwnd_results_list, 0, &col);

            col.pszText = L"Type";
            col.cx = 80;
            col.iSubItem = 1;
            ListView_InsertColumn(state->hwnd_results_list, 1, &col);

            col.pszText = L"Path";
            col.cx = 380;
            col.iSubItem = 2;
            ListView_InsertColumn(state->hwnd_results_list, 2, &col);

            ListView_SetExtendedListViewStyle(state->hwnd_results_list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

            /* Create Status Bar */
            state->hwnd_status_bar = CreateStatusWindowW(
                WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                L"Ready. Click 'Index' to scan a directory.",
                hwnd, IDC_STATUS_BAR
            );

            /* Set standard Segoe UI font to all child controls */
            HFONT hfont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                      DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            if (hfont != NULL) {
                SendMessageW(state->hwnd_dir_edit, WM_SETFONT, (WPARAM)hfont, TRUE);
                SendMessageW(state->hwnd_browse_btn, WM_SETFONT, (WPARAM)hfont, TRUE);
                SendMessageW(state->hwnd_index_btn, WM_SETFONT, (WPARAM)hfont, TRUE);
                SendMessageW(state->hwnd_search_edit, WM_SETFONT, (WPARAM)hfont, TRUE);
                SendMessageW(state->hwnd_results_list, WM_SETFONT, (WPARAM)hfont, TRUE);
            }

            /* Set placeholder watermark in Search Edit box */
            Edit_SetCueBannerText(state->hwnd_search_edit, L"Type here to search filenames instantly...");

            break;
        }

        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            handle_resize(hwnd, width, height);
            break;
        }

        case WM_COMMAND: {
            int control_id = LOWORD(wParam);
            int notification = HIWORD(wParam);

            if (control_id == ID_POPUP_OPEN_LOCATION) {
                int item = state->last_context_item;
                if (item >= 0 && item < (int)search_results_count(&state->search_results)) {
                    const SearchResult *res = search_results_get(&state->search_results, item);
                    if (res != NULL) {
                        wchar_t path[MAX_PATH] = {0};
                        if (res->is_directory) {
                            index_get_dir_path(&state->index, res->index_id, path, MAX_PATH);
                        } else {
                            index_get_file_path(&state->index, res->index_id, path, MAX_PATH);
                        }

                        wchar_t args[MAX_PATH + 64];
                        swprintf_s(args, MAX_PATH + 64, L"/select,\"%s\"", path);
                        ShellExecuteW(NULL, L"open", L"explorer.exe", args, NULL, SW_SHOWNORMAL);
                    }
                }
            }
            else if (control_id == ID_POPUP_COPY_PATH) {
                int item = state->last_context_item;
                if (item >= 0 && item < (int)search_results_count(&state->search_results)) {
                    const SearchResult *res = search_results_get(&state->search_results, item);
                    if (res != NULL) {
                        wchar_t path[MAX_PATH] = {0};
                        if (res->is_directory) {
                            index_get_dir_path(&state->index, res->index_id, path, MAX_PATH);
                        } else {
                            index_get_file_path(&state->index, res->index_id, path, MAX_PATH);
                        }

                        copy_to_clipboard(hwnd, path);
                    }
                }
            }
            else if (control_id == IDC_BROWSE_BTN && notification == BN_CLICKED) {
                if (state->is_indexing) break;

                wchar_t path[MAX_PATH];
                if (browse_folder(hwnd, path)) {
                    wcscpy_s(state->selected_dir, MAX_PATH, path);
                    SetWindowTextW(state->hwnd_dir_edit, path);
                }
            }
            else if (control_id == IDC_INDEX_BTN && notification == BN_CLICKED) {
                if (state->is_indexing) break;

                state->is_indexing = true;
                EnableWindow(state->hwnd_index_btn, FALSE);
                EnableWindow(state->hwnd_browse_btn, FALSE);

                SendMessageW(state->hwnd_status_bar, SB_SETTEXTW, 0, (LPARAM)L"Scanning directory filesystem...");

                /* Spawn worker thread */
                IndexThreadParams *params = (IndexThreadParams*)malloc(sizeof(IndexThreadParams));
                if (params != NULL) {
                    params->hwnd_window = hwnd;
                    wcscpy_s(params->root_path, MAX_PATH, state->selected_dir);

                    HANDLE thread = CreateThread(NULL, 0, index_worker_thread, params, 0, NULL);
                    if (thread != NULL) {
                        CloseHandle(thread);
                    } else {
                        free(params);
                        state->is_indexing = false;
                        EnableWindow(state->hwnd_index_btn, TRUE);
                        EnableWindow(state->hwnd_browse_btn, TRUE);
                        SendMessageW(state->hwnd_status_bar, SB_SETTEXTW, 0, (LPARAM)L"Failed to launch index thread.");
                    }
                }
            }
            else if (control_id == IDC_SEARCH_EDIT && notification == EN_CHANGE) {
                /* Realtime search trigger */
                wchar_t query[MAX_PATH];
                GetWindowTextW(state->hwnd_search_edit, query, MAX_PATH);

                if (wcslen(query) > 0) {
                    if (state->index.initialized) {
                        search_execute(&state->index, query, NULL, &state->search_results);
                        populate_listview(state);

                        wchar_t status[128];
                        swprintf(status, 128, L"Matches found: %llu", (unsigned long long)search_results_count(&state->search_results));
                        SendMessageW(state->hwnd_status_bar, SB_SETTEXTW, 0, (LPARAM)status);
                    }
                } else {
                    search_results_clear(&state->search_results);
                    populate_listview(state);
                    SendMessageW(state->hwnd_status_bar, SB_SETTEXTW, 0, (LPARAM)L"Ready.");
                }
            }
            break;
        }

        case WM_NOTIFY: {
            LPNMHDR nmhdr = (LPNMHDR)lParam;
            if (nmhdr->hwndFrom == state->hwnd_results_list && nmhdr->code == NM_RCLICK) {
                LVHITTESTINFO hti = {0};
                GetCursorPos(&hti.pt);
                ScreenToClient(state->hwnd_results_list, &hti.pt);
                ListView_HitTest(state->hwnd_results_list, &hti);

                int clicked_item = -1;
                if (hti.flags & LVHT_ONITEM) {
                    clicked_item = hti.iItem;
                } else {
                    clicked_item = ListView_GetNextItem(state->hwnd_results_list, -1, LVNI_SELECTED);
                }

                if (clicked_item != -1) {
                    HMENU hmenu = CreatePopupMenu();
                    if (hmenu != NULL) {
                        AppendMenuW(hmenu, MF_STRING, ID_POPUP_OPEN_LOCATION, L"Open File Location");
                        AppendMenuW(hmenu, MF_STRING, ID_POPUP_COPY_PATH, L"Copy Path");

                        POINT pt;
                        GetCursorPos(&pt);
                        state->last_context_item = clicked_item;

                        TrackPopupMenu(hmenu, TPM_RIGHTBUTTON | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
                        DestroyMenu(hmenu);
                    }
                }
            }
            break;
        }

        case WM_USER_INDEX_PROGRESS: {
            usize count = (usize)wParam;
            wchar_t status[128];
            swprintf(status, 128, L"Indexed %llu files...", (unsigned long long)count);
            SendMessageW(state->hwnd_status_bar, SB_SETTEXTW, 0, (LPARAM)status);
            break;
        }

        case WM_USER_INDEX_COMPLETE: {
            AchErrorCode err = (AchErrorCode)wParam;
            state->is_indexing = false;
            EnableWindow(state->hwnd_index_btn, TRUE);
            EnableWindow(state->hwnd_browse_btn, TRUE);

            if (err == ACH_SUCCESS) {
                usize total_files = vector_length(&state->index.files);
                usize total_dirs = vector_length(&state->index.dirs);

                wchar_t status[256];
                swprintf(status, 256, L"Index complete: %llu files, %llu dirs (duration: %.2f ms)",
                         (unsigned long long)total_files, (unsigned long long)total_dirs, state->last_scan_duration_ms);
                SendMessageW(state->hwnd_status_bar, SB_SETTEXTW, 0, (LPARAM)status);
            } else {
                SendMessageW(state->hwnd_status_bar, SB_SETTEXTW, 0, (LPARAM)L"Indexing failed.");
            }

            /* Trigger search update to refresh result view on new index */
            SendMessageW(state->hwnd_search_edit, WM_COMMAND, MAKEWPARAM(IDC_SEARCH_EDIT, EN_CHANGE), (LPARAM)state->hwnd_search_edit);
            break;
        }

        case WM_DESTROY: {
            if (state != NULL) {
                index_destroy(&state->index);
                search_results_destroy(&state->search_results);
                free(state);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)NULL);
            }
            PostQuitMessage(0);
            break;
        }

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

/* ---- GUI Entry Point ---------------------------------------------------- */
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    /* Initialize Common Controls */
    INITCOMMONCONTROLSEX icex = {0};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    /* Initialize COM for SHBrowseForFolder */
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    /* Register Window Class */
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"AchillesSearchClass";

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(NULL, L"Window Registration Failed!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    /* Create Main Window */
    HWND hwnd = CreateWindowExW(
        0, L"AchillesSearchClass", L"Achilles File Search Engine",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 700, 500,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) {
        MessageBoxW(NULL, L"Window Creation Failed!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    /* Main Message Loop */
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CoUninitialize();
    return (int)msg.wParam;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)lpCmdLine;
    return wWinMain(hInstance, hPrevInstance, NULL, nCmdShow);
}
