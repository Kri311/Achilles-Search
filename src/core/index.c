/* ==========================================================================
 * Achilles-Search | src/core/index.c
 * ==========================================================================
 * Hierarchical filename index implementation.
 * ========================================================================== */

#include "core/index.h"
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

/* ---- Static Helpers ------------------------------------------------------ */

/* Splits a filesystem path into parent directory and filename/dirname components.
 * Example: "C:\Windows\System32" -> parent="C:\Windows", name="System32"
 * Example: "C:\file.txt"          -> parent="C:",        name="file.txt"
 * Example: "file.txt"             -> parent="",          name="file.txt" */
static void split_path(const wchar_t *path, wchar_t *parent, usize parent_len, wchar_t *name, usize name_len) {
    const wchar_t *last_slash = wcsrchr(path, L'\\');
    if (last_slash == NULL) {
        /* No backslash found; it's a relative root filename */
        parent[0] = L'\0';
        wcsncpy(name, path, name_len);
        name[name_len - 1] = L'\0';
        return;
    }

    usize parent_chars = last_slash - path;
    if (parent_chars == 0) {
        /* The path starts with a backslash (e.g. "\file.txt") */
        wcsncpy(parent, L"\\", parent_len);
    } else {
        usize to_copy = (parent_chars < parent_len - 1)  parent_chars : (parent_len - 1);
        wcsncpy(parent, path, to_copy);
        parent[to_copy] = L'\0';
    }

    wcsncpy(name, last_slash + 1, name_len);
    name[name_len - 1] = L'\0';
}

/* Recursively ensures a directory is indexed and returns its directory ID.
 * Leverages the temporary HashMap cache for O(1) lookups of existing paths. */
static AchErrorCode get_or_create_dir(Index *index, HashMap *dir_map, const wchar_t *dir_path, u32 *out_dir_id) {
    if (dir_path == NULL || dir_path[0] == L'\0') {
        *out_dir_id = ACH_INDEX_ROOT_ID;
        return ACH_SUCCESS;
    }

    /* 1. Check temporary map for already indexed directory ID */
    u32 cached_id = 0;
    AchErrorCode err = hashmap_get(dir_map, dir_path, &cached_id);
    if (err == ACH_SUCCESS) {
        *out_dir_id = cached_id;
        return ACH_SUCCESS;
    }

    /* 2. Decompose directory path */
    wchar_t parent[MAX_PATH];
    wchar_t name[MAX_PATH];
    split_path(dir_path, parent, MAX_PATH, name, MAX_PATH);

    u32 parent_id = ACH_INDEX_ROOT_ID;
    bool is_root = false;

    if (parent[0] == L'\0') {
        is_root = true;
    } else {
        /* If split result had an empty name component, it was a root ending in a slash (e.g., "C:\") */
        if (name[0] == L'\0') {
            err = get_or_create_dir(index, dir_map, parent, out_dir_id);
            if (ACH_SUCCEEDED(err)) {
                /* Cache both forms ("C:" and "C:\") to map to the same directory ID */
                hashmap_put(dir_map, dir_path, out_dir_id);
            }
            return err;
        }
    }

    if (!is_root) {
        err = get_or_create_dir(index, dir_map, parent, &parent_id);
        if (ACH_FAILED(err)) {
            return err;
        }
    }

    /* 3. Insert new directory entry */
    IndexDir new_dir;
    new_dir.name = _wcsdup(name[0] == L'\0'  dir_path : name);
    if (new_dir.name == NULL) {
        return ACH_ERROR_OUT_OF_MEMORY;
    }
    new_dir.parent_id = parent_id;

    err = vector_push(&index->dirs, &new_dir);
    if (ACH_FAILED(err)) {
        free(new_dir.name);
        return err;
    }

    u32 new_id = (u32)(vector_length(&index->dirs) - 1);

    /* 4. Cache full directory path to new directory ID */
    err = hashmap_put(dir_map, dir_path, &new_id);
    if (ACH_FAILED(err)) {
        IndexDir popped;
        if (vector_pop(&index->dirs, &popped) == ACH_SUCCESS) {
            free(popped.name);
        }
        return err;
    }

    *out_dir_id = new_id;
    return ACH_SUCCESS;
}

/* ---- Lifecycle ----------------------------------------------------------- */

AchErrorCode index_init(Index *index) {
    if (index == NULL) {
        return ACH_ERROR_INVALID_ARG;
    }

    AchErrorCode err = vector_init(&index->dirs, sizeof(IndexDir), 128);
    if (ACH_FAILED(err)) {
        memset(index, 0, sizeof(Index));
        return err;
    }

    err = vector_init(&index->files, sizeof(IndexFile), 1024);
    if (ACH_FAILED(err)) {
        vector_destroy(&index->dirs);
        memset(index, 0, sizeof(Index));
        return err;
    }

    index->initialized = true;
    return ACH_SUCCESS;
}

void index_destroy(Index *index) {
    if (index == NULL || !index->initialized) {
        return;
    }

    usize dir_count = vector_length(&index->dirs);
    for (usize i = 0; i < dir_count; i++) {
        IndexDir *dir = vector_get(&index->dirs, i);
        if (dir && dir->name) {
            free(dir->name);
        }
    }
    vector_destroy(&index->dirs);

    usize file_count = vector_length(&index->files);
    for (usize i = 0; i < file_count; i++) {
        IndexFile *file = vector_get(&index->files, i);
        if (file && file->name) {
            free(file->name);
        }
    }
    vector_destroy(&index->files);

    index->initialized = false;
}

void index_clear(Index *index) {
    if (index == NULL || !index->initialized) {
        return;
    }

    usize dir_count = vector_length(&index->dirs);
    for (usize i = 0; i < dir_count; i++) {
        IndexDir *dir = vector_get(&index->dirs, i);
        if (dir && dir->name) {
            free(dir->name);
        }
    }
    vector_clear(&index->dirs);

    usize file_count = vector_length(&index->files);
    for (usize i = 0; i < file_count; i++) {
        IndexFile *file = vector_get(&index->files, i);
        if (file && file->name) {
            free(file->name);
        }
    }
    vector_clear(&index->files);
}

/* ---- Construction -------------------------------------------------------- */

AchErrorCode index_add_file_info(Index *index, HashMap *dir_map, const FileInfo *info) {
    if (index == NULL || dir_map == NULL || info == NULL || info->path == NULL) {
        return ACH_ERROR_INVALID_ARG;
    }

    if (info->is_directory) {
        u32 dir_id = 0;
        return get_or_create_dir(index, dir_map, info->path, &dir_id);
    } else {
        wchar_t parent[MAX_PATH];
        wchar_t name[MAX_PATH];
        split_path(info->path, parent, MAX_PATH, name, MAX_PATH);

        u32 parent_id = ACH_INDEX_ROOT_ID;
        AchErrorCode err = get_or_create_dir(index, dir_map, parent, &parent_id);
        if (ACH_FAILED(err)) {
            return err;
        }

        IndexFile new_file;
        new_file.name = _wcsdup(name);
        if (new_file.name == NULL) {
            return ACH_ERROR_OUT_OF_MEMORY;
        }
        new_file.parent_id = parent_id;
        new_file.file_size = info->file_size;
        new_file.last_modified = info->last_modified;
        new_file.attributes = info->attributes;

        err = vector_push(&index->files, &new_file);
        if (ACH_FAILED(err)) {
            free(new_file.name);
            return err;
        }

        return ACH_SUCCESS;
    }
}

/* ---- Path Resolution ----------------------------------------------------- */

AchErrorCode index_get_dir_path(const Index *index, u32 dir_idx, wchar_t *out_path, usize max_len) {
    if (index == NULL || out_path == NULL || max_len == 0) {
        return ACH_ERROR_INVALID_ARG;
    }

    usize dir_count = vector_length(&index->dirs);
    if (dir_idx >= dir_count) {
        return ACH_ERROR_INVALID_ARG;
    }

    /* Climb up parent directory pointers. Keep stack buffer of traversal path. */
    u32 path_indices[64];
    usize depth = 0;
    u32 current_idx = dir_idx;

    while (current_idx != ACH_INDEX_ROOT_ID) {
        if (depth >= 64) {
            return ACH_ERROR_PATH_TOO_LONG;
        }
        path_indices[depth++] = current_idx;

        const IndexDir *dir = vector_get(&index->dirs, current_idx);
        if (dir == NULL) {
            return ACH_ERROR_INDEX_CORRUPT;
        }
        current_idx = dir->parent_id;
    }

    out_path[0] = L'\0';
    usize current_len = 0;

    /* Reconstruct path starting from root directory */
    for (int i = (int)depth - 1; i >= 0; i--) {
        u32 idx = path_indices[i];
        const IndexDir *dir = vector_get(&index->dirs, idx);
        if (dir == NULL || dir->name == NULL) {
            return ACH_ERROR_INDEX_CORRUPT;
        }

        usize name_len = wcslen(dir->name);

        if (current_len > 0) {
            wchar_t last_char = out_path[current_len - 1];
            if (last_char != L'\\' && last_char != L'/') {
                if (current_len + 1 >= max_len) {
                    return ACH_ERROR_PATH_TOO_LONG;
                }
                wcscpy(out_path + current_len, L"\\");
                current_len++;
            }
        }

        if (current_len + name_len >= max_len) {
            return ACH_ERROR_PATH_TOO_LONG;
        }
        wcscpy(out_path + current_len, dir->name);
        current_len += name_len;
    }

    return ACH_SUCCESS;
}

AchErrorCode index_get_file_path(const Index *index, u32 file_idx, wchar_t *out_path, usize max_len) {
    if (index == NULL || out_path == NULL || max_len == 0) {
        return ACH_ERROR_INVALID_ARG;
    }

    usize file_count = vector_length(&index->files);
    if (file_idx >= file_count) {
        return ACH_ERROR_INVALID_ARG;
    }

    const IndexFile *file = vector_get(&index->files, file_idx);
    if (file == NULL || file->name == NULL) {
        return ACH_ERROR_INDEX_CORRUPT;
    }

    if (file->parent_id == ACH_INDEX_ROOT_ID) {
        usize name_len = wcslen(file->name);
        if (name_len >= max_len) {
            return ACH_ERROR_PATH_TOO_LONG;
        }
        wcscpy(out_path, file->name);
        return ACH_SUCCESS;
    }

    /* 1. Reconstruct parent directory path */
    AchErrorCode err = index_get_dir_path(index, file->parent_id, out_path, max_len);
    if (ACH_FAILED(err)) {
        return err;
    }

    /* 2. Append separator and filename */
    usize current_len = wcslen(out_path);
    usize name_len = wcslen(file->name);

    if (current_len > 0) {
        wchar_t last_char = out_path[current_len - 1];
        if (last_char != L'\\' && last_char != L'/') {
            if (current_len + 1 >= max_len) {
                return ACH_ERROR_PATH_TOO_LONG;
            }
            wcscpy(out_path + current_len, L"\\");
            current_len++;
        }
    }

    if (current_len + name_len >= max_len) {
        return ACH_ERROR_PATH_TOO_LONG;
    }
    wcscpy(out_path + current_len, file->name);

    return ACH_SUCCESS;
}

/* ---- Information --------------------------------------------------------- */

usize index_file_count(const Index *index) {
    return index  vector_length(&index->files) : 0;
}

usize index_dir_count(const Index *index) {
    return index  vector_length(&index->dirs) : 0;
}
