# =============================================================================
# Achilles-Search - CMake Build Configuration
# =============================================================================
# Minimum CMake version: 3.20 (for C17 support and modern features)
# =============================================================================
cmake_minimum_required(VERSION 3.20)

project(AchillesSearch
    VERSION 0.1.0
    DESCRIPTION "A blazing-fast Windows desktop search engine"
    LANGUAGES C
)

# ---------------------------------------------------------------------------
# C Standard Configuration
# ---------------------------------------------------------------------------
# C17 is our target standard. We disable extensions to ensure strict
# conformance and portability (no GNU extensions, no MSVC extensions).
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Output Directories
# ---------------------------------------------------------------------------
# Place all build artifacts in predictable locations under build/.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ---------------------------------------------------------------------------
# Source Files
# ---------------------------------------------------------------------------
# We explicitly list every source file rather than using GLOB.
# Rationale: GLOB doesn't re-run CMake when new files are added, leading to
# mysterious "undefined reference" errors. Explicit listing is the
# professional practice recommended by CMake documentation.
set(ACHILLES_SOURCES
    src/main.c
    src/common/logger.c
    src/common/utils.c
    src/data/vector.c
    src/data/hashmap.c
    src/core/scanner.c
    src/core/index.c
    src/storage/database.c
    src/core/search.c
    src/core/content_index.c
    src/core/monitor.c
)

# ---------------------------------------------------------------------------
# Executable Target
# ---------------------------------------------------------------------------
add_executable(${PROJECT_NAME} WIN32 ${ACHILLES_SOURCES})

# ---------------------------------------------------------------------------
# Include Directories
# ---------------------------------------------------------------------------
# PUBLIC: propagated to anything that links against this target.
# We use the include/ directory as our public header root, so all includes
# look like: #include "common/logger.h" — clean and unambiguous.
target_include_directories(${PROJECT_NAME}
    PUBLIC ${CMAKE_SOURCE_DIR}/include
)

# ---------------------------------------------------------------------------
# Compile Definitions
# ---------------------------------------------------------------------------
# UNICODE / _UNICODE: Use wide-char versions of Win32 APIs by default.
# Windows paths can contain non-ASCII characters (e.g., Japanese, Cyrillic),
# so we MUST use the wide-char API surface (CreateFileW, FindFirstFileW, etc.)
# from the very beginning. Retrofitting Unicode support later is painful.
#
# _CRT_SECURE_NO_WARNINGS: Suppress MSVC warnings about "unsafe" C standard
# functions (sprintf, fopen, etc.). We handle safety ourselves.
target_compile_definitions(${PROJECT_NAME} PRIVATE
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS
    ACH_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    ACH_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    ACH_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# ---------------------------------------------------------------------------
# Compiler Flags (MSVC-specific)
# ---------------------------------------------------------------------------
# /W4: Warning level 4 — catches subtle bugs (uninitialized variables,
#      unreachable code, signed/unsigned mismatches, etc.)
# /WX: Warnings as errors in Release — forces clean code before shipping.
# /permissive-: Strict standards conformance mode.
# /utf-8: Source and execution charset = UTF-8.
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4
        /permissive-
        /utf-8
    )
    # In Release builds, treat warnings as errors
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Release>:/WX>
    )
endif()

# ---------------------------------------------------------------------------
# Platform Libraries
# ---------------------------------------------------------------------------
target_link_libraries(${PROJECT_NAME} PRIVATE comctl32 ole32 shell32)

# ---------------------------------------------------------------------------
# Test Targets
# ---------------------------------------------------------------------------
# Each test is a separate executable that links the library sources it needs.
# Tests do NOT link main.c (they have their own main function).

# Shared library sources (everything except main.c)
set(ACHILLES_LIB_SOURCES
    src/common/logger.c
    src/common/utils.c
    src/data/vector.c
    src/data/hashmap.c
    src/core/scanner.c
    src/core/index.c
    src/storage/database.c
    src/core/search.c
    src/core/content_index.c
    src/core/monitor.c
)

# Vector tests
add_executable(test_vector
    tests/test_vector.c
    ${ACHILLES_LIB_SOURCES}
)
target_include_directories(test_vector PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(test_vector PRIVATE
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS
    ACH_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    ACH_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    ACH_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# Scanner tests
add_executable(test_scanner
    tests/test_scanner.c
    ${ACHILLES_LIB_SOURCES}
)
target_include_directories(test_scanner PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(test_scanner PRIVATE
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS
    ACH_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    ACH_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    ACH_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# Hash Map tests
add_executable(test_hashmap
    tests/test_hashmap.c
    ${ACHILLES_LIB_SOURCES}
)
target_include_directories(test_hashmap PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(test_hashmap PRIVATE
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS
    ACH_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    ACH_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    ACH_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# Index tests
add_executable(test_index
    tests/test_index.c
    ${ACHILLES_LIB_SOURCES}
)
target_include_directories(test_index PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(test_index PRIVATE
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS
    ACH_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    ACH_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    ACH_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# Database tests
add_executable(test_database
    tests/test_database.c
    ${ACHILLES_LIB_SOURCES}
)
target_include_directories(test_database PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(test_database PRIVATE
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS
    ACH_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    ACH_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    ACH_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# Search Engine tests
add_executable(test_search
    tests/test_search.c
    ${ACHILLES_LIB_SOURCES}
)
target_include_directories(test_search PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(test_search PRIVATE
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS
    ACH_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    ACH_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    ACH_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# Content Index tests
add_executable(test_content
    tests/test_content.c
    ${ACHILLES_LIB_SOURCES}
)
target_include_directories(test_content PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(test_content PRIVATE
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS
    ACH_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    ACH_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    ACH_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# Filesystem Monitor tests
add_executable(test_monitor
    tests/test_monitor.c
    ${ACHILLES_LIB_SOURCES}
)
target_include_directories(test_monitor PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(test_monitor PRIVATE
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS
    ACH_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    ACH_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    ACH_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# ---------------------------------------------------------------------------
# Build Info
# ---------------------------------------------------------------------------
message(STATUS "=== Achilles Search Engine ===")
message(STATUS "Version:    ${PROJECT_VERSION}")
message(STATUS "C Standard: C${CMAKE_C_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler:   ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
