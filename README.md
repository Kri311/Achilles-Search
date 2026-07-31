# Achilles-Search

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%2F11-0078D6.svg)](#platform)
[![C Standard](https://img.shields.io/badge/Language-C17-blue.svg)](#)
[![Build](https://github.com/Kri311/Achilles-Search/actions/workflows/release.yml/badge.svg)](https://github.com/Kri311/Achilles-Search/actions/workflows/release.yml)

A blazing-fast, native Windows desktop file search engine built in pure C17. Index hundreds of thousands of files in seconds and search them instantly with real-time results as you type.

---

## What is Achilles-Search?

Achilles-Search is a lightweight desktop search tool that lets you find any file on your computer almost instantly. Unlike the built-in Windows search, Achilles-Search builds its own high-performance in-memory index of your filesystem, so searches return results in real-time as you type each character.

It runs as a native Windows application with no frameworks, no runtimes, and no dependencies. Just a single `.exe` that sits in your system tray and is always one hotkey away.

## How It Works

1. **Scan** - You pick a directory and click **Index**. A background worker thread recursively scans every file and folder using the Windows API, building a compressed in-memory index.
2. **Search** - As you type in the search box, a multi-threaded parallel matcher instantly filters through the entire index and displays matching filenames and paths in real-time.
3. **Interact** - Double-click any result to reveal it in Windows Explorer, or right-click to copy its full path to your clipboard.

The application lives in your system tray. Press **Alt + Shift + Space** from anywhere on your desktop to instantly summon the search window.

### Key Features

- **Instant Search** - Results appear in real-time as you type, powered by a multi-threaded parallel file matcher.
- **Background Indexing** - Filesystem scanning runs on a dedicated worker thread so the UI never freezes.
- **System Tray** - Minimizes to tray on close. Always running, always one hotkey away.
- **Global Hotkey** - **Alt + Shift + Space** toggles the window from anywhere.
- **Real-Time Monitoring** - Watches indexed directories for changes using asynchronous overlapped I/O.
- **Content Indexing** - Inverted word index enables searching inside file contents.
- **Persistent Database** - Save and restore your index across sessions with a custom binary serializer.
- **Zero Dependencies** - Pure C17 with native Win32 APIs. No external libraries required.

---

## Platform

| Requirement | Details |
| :--- | :--- |
| **Operating System** | Windows 10 / Windows 11 (64-bit) |
| **Architecture** | x86-64 (AMD64) |
| **Runtime Dependencies** | None |

---

## Installation

### Download (Recommended)

Pre-built binaries are available on the [**Releases**](https://github.com/Kri311/Achilles-Search/releases) page.

| Download | Description |
| :--- | :--- |
| `AchillesSearch-x.x.x-win64.exe` | Windows installer with Start Menu shortcut |
| `AchillesSearch-x.x.x-win64.zip` | Portable version — extract and run, no install needed |

### Build from Source

If you prefer to compile it yourself, you will need:

- [CMake](https://cmake.org/download/) 3.20+
- A C compiler — **MSVC** (Visual Studio 2022) or **GCC** (MinGW via [MSYS2](https://www.msys2.org/))

#### Using MSVC (Visual Studio)

```bash
git clone https://github.com/Kri311/Achilles-Search.git
cd Achilles-Search

cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

#### Using GCC (MinGW / MSYS2)

Make sure `C:\msys64\mingw64\bin` is added to your system PATH.

```bash
git clone https://github.com/Kri311/Achilles-Search.git
cd Achilles-Search

cmake -G "MinGW Makefiles" -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The compiled executable will be at `build/bin/AchillesSearch.exe`.

#### Packaging an Installer

To generate the distributable ZIP and NSIS installer locally (requires [NSIS](https://nsis.sourceforge.io/Download)):

```bash
cd build
cpack -C Release
```

---

## Project Structure

```
Achilles-Search/
├── include/              # Public header files
│   ├── common/           # Types, errors, config, logger, utilities
│   ├── core/             # Scanner, index, search, content index, monitor
│   ├── data/             # Vector, HashMap
│   └── storage/          # Database serialization
├── src/                  # Source implementation
│   ├── common/           # Logger, utilities
│   ├── core/             # Scanner, indexer, search engine, monitor
│   ├── data/             # Dynamic vector, hash map
│   ├── storage/          # Binary database serializer
│   └── main.c            # Win32 GUI entry point
├── tests/                # Unit tests for every module
├── .github/workflows/    # CI/CD release automation
├── CMakeLists.txt        # Build configuration
└── README.md
```

| Module | What It Does |
| :--- | :--- |
| **Scanner** | Recursively walks directories using Windows `FindFirstFileW` / `FindNextFileW` |
| **Indexer** | Builds a prefix-path compressed in-memory index of all scanned files |
| **Search Engine** | Multi-threaded parallel matcher that filters the index against a query |
| **Content Index** | Inverted word index for searching inside file contents |
| **Monitor** | Watches directories for real-time changes via `ReadDirectoryChangesW` |
| **Database** | Custom binary format to persist and restore the index to/from disk |
| **GUI** | Native Win32 window with ListView, status bar, system tray, and global hotkey |

---

## License

This project is licensed under the **Apache License 2.0**. See the [LICENSE](LICENSE) file for full details.
