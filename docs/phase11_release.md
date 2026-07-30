# Phase 11: Release Specification

This document summarizes the release configuration, compilation settings, module map, and visual design parameters for the production delivery of Achilles-Search.


## 1. Release Overview
Achilles-Search is a native, blazing-fast Windows desktop file search engine written in portable C17. It features a responsive Win32 GUI, a multi-threaded parallel search matcher, persistent database storage, real-time filesystem change monitoring, and content-based indexing.


## 2. Compilation & Build Configurations
*   **Target Subsystem:** `WINDOWS` (Win32 GUI mode, console window hidden).
*   **Optimization Profile:** `Release` (standard optimizations enabled, debugging symbols and assertions stripped).
*   **Unicode Compliance:** Wide-character API surface compliance (`UNICODE` and `_UNICODE` enabled) to support global, non-ASCII path hierarchies.
*   **Visual Styles:** Common Controls v6.0 manifest dependency linked directly into the binary header to load modern native UI widgets (flat buttons, rounded panels, gridlist layouts) on Windows 10/11.


## 3. Module Map
| Module | Header | Source | Functionality |
| :--- | :--- | :--- | :--- |
| **Common** | `include/common/` | `src/common/` | Logger, custom type maps, error flags, macros |
| **Data Structures** | `include/data/` | `src/data/` | Cache-friendly dynamic `Vector`, linear-probing `HashMap` |
| **Scanner** | `include/core/scanner.h` | `src/core/scanner.c` | Recursive Windows DFS directory scanner |
| **Indexer** | `include/core/index.h` | `src/core/index.c` | Prefix-path compressed index constructor |
| **Database** | `include/storage/database.h` | `src/storage/database.c` | Custom binary serializer/deserializer |
| **Search Engine** | `include/core/search.h` | `src/core/search.c` | Multi-threaded parallel file matcher |
| **Content Index**| `include/core/content_index.h` | `src/core/content_index.c` | Inverted word indexer for file content search |
| **Monitor** | `include/core/monitor.h` | `src/core/monitor.c` | Asynchronous overlapped directory watcher |
| **GUI** | N/A | `src/main.c` | Responsive native Win32 window and event thread |


## 4. User Guide & Interface Walkthrough
1.  **Launch:** Execute `AchillesSearch.exe`. A native window will appear, and a system tray icon is automatically registered.
2.  **Toggle Visibility:** Use the global system-wide hotkey **Alt + Shift + Space** to instantly toggle the window's visibility, bringing it to the foreground with the search box auto-focused.
3.  **Minimize to Tray:** Closing the main window (using the standard close `X` button) hides it in the background, keeping it active in the system tray. Right-click the tray icon to select **Show Achilles-Search** or **Exit** the application.
4.  **Select Directory:** Click **Browse...** to select a folder from the native folder browser dialog, or manually write a path.
5.  **Perform Scan:** Click **Index**. A background worker thread is spawned to index the filesystem. The status bar will show progress ("Indexed 10,000 files...") and scanning duration when complete.
6.  **Real-Time Search:** Type any query in the search edit box. Matching file and directory names are instantly populated in the ListView results grid.
7.  **Interaction Options:**
    *   **Open File Location:** Press **Enter** on a selected item, **double-click** it, or right-click and choose **Open File Location** to highlight it in Windows Explorer.
    *   **Copy Path:** Right-click a result item and select **Copy Path** to copy the full path to the clipboard.
