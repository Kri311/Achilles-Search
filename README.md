# Achilles-Search

[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows-lightgray.svg)](#)

A blazing-fast, native Windows desktop file search engine written in portable C17. It features a responsive Win32 GUI, a multi-threaded parallel search matcher, persistent database storage, and real-time filesystem change monitoring.

## Features

- **Blazing Fast**: Built in pure C17 for maximum performance with no bloat.
- **Native GUI**: Uses responsive Win32 GUI, keeping resource usage extremely low.
- **System Tray Integration**: Instantly accessible. Global shortcut **Alt + Shift + Space** toggles the window.
- **Real-Time Monitoring**: Asynchronous overlapped directory watcher tracks file changes in real-time.
- **Parallel Searching**: Multi-threaded parallel file matcher for massive directory structures.

## Installation

You do not need to build the project yourself to use it! We provide a pre-compiled installer for Windows.

1. Go to the **Releases** page of this repository.
2. Download the latest `AchillesSearch-x.x.x-win64.exe` installer.
3. Run the installer and follow the setup wizard.
4. Launch the app from your Start Menu!

*(A portable `.zip` version is also available in the releases).*

## Usage Guide

1. **Launch:** Run Achilles-Search. It will sit quietly in your system tray.
2. **Toggle Interface:** Press **Alt + Shift + Space** to instantly open the search bar.
3. **Index Files:** Click **Browse...** to pick a folder, then click **Index**. A background worker will index the files at blistering speeds.
4. **Search:** Type your query. The results will populate instantly.
5. **Interact:**
   - **Double-click** or press **Enter** to open the file location.
   - **Right-click** to copy the path.

## Building from Source

If you want to contribute or build the project from source, you will need **CMake** and a C compiler (like MSVC).

```bash
# Clone the repository
git clone https://github.com/USERNAME/Achilles-Search.git
cd Achilles-Search

# Configure the project
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Build the executable
cmake --build build --config Release
```

The compiled binary will be located in `build/bin/AchillesSearch.exe`.
