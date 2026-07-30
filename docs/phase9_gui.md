# Phase 9: Native GUI Design Specification

This document details the architecture, window hierarchy, message loop, and background threading model for the **Native Windows GUI** in Achilles-Search. The GUI provides a responsive, modern interface for indexing directories and searching filenames.


## 1. Architectural Decisions

### 1.1 Responsive Background Work Thread
To prevent the window from freezing (becoming "Not Responding") during filesystem scanning:
*   **Indexing Thread:** Clicking the "Index" button spawns a background worker thread (`index_worker_thread`) using `CreateThread`. This worker runs `scanner_scan` to index the chosen directory.
*   **Thread Communication:** The worker thread posts custom Win32 messages (e.g., `WM_USER_INDEX_PROGRESS` and `WM_USER_INDEX_COMPLETE`) back to the main window's message queue using `PostMessageW`. This lets the UI thread update status text and list controls safely without cross-thread lock contention.

### 1.2 Visual Aesthetics (Windows Common Controls v6)
We enable native visual styles (flat, rounded, modern Windows 10/11 controls) instead of the Windows 95 classic theme by embedding a Common Controls v6 linker directive:
```c
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
```
We also initialize common controls using `InitCommonControlsEx` with classes `ICC_BAR_CLASSES` (for status bars) and `ICC_LISTVIEW_CLASSES` (for the search results grid).

### 1.3 Control Layout and Grid
We place and size the following controls during `WM_CREATE` and resize them dynamically in response to `WM_SIZE` messages:
1.  **Monitored Folder Edit & Browse:** Edit field showing current root path, paired with a "Browse..." button.
2.  **Search Input:** Real-time search edit control. Typing in this control sends an `EN_CHANGE` notification, which instantly runs `search_execute` and repopulates the list.
3.  **Results ListView:** A report-style grid showing Name, Path, and Type (File/Directory) columns.
4.  **Status Bar:** Displays indexing metrics (total files, scan duration, database state).


## 2. API & Entry Point Design

### 2.1 Entry Point (`src/main.c`)
The entry point transitions from a command-line application to a `wWinMain` function:
```c
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow);
```

### 2.2 Control Identifiers
```c
#define IDC_SEARCH_EDIT      1001
#define IDC_BROWSE_BTN       1002
#define IDC_INDEX_BTN        1003
#define IDC_DIR_EDIT         1004
#define IDC_RESULTS_LIST     1005
#define IDC_STATUS_BAR       1006

/* Custom User Messages */
#define WM_USER_INDEX_PROGRESS (WM_USER + 1)
#define WM_USER_INDEX_COMPLETE (WM_USER + 2)
```
