/* ==========================================================================
 * Achilles-Search | src/storage/database.c
 * ==========================================================================
 * Database serialization and deserialization implementation.
 * ========================================================================== */

#include "storage/database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

AchErrorCode db_save(const Index *index, const wchar_t *file_path) {
    if (index == NULL || !index->initialized || file_path == NULL) {
        return ACH_ERROR_INVALID_ARG;
    }

    FILE *f = _wfopen(file_path, L"wb");
    if (f == NULL) {
        return ACH_ERROR_FILE_OPEN_FAILED;
    }

    char magic[4];
    memcpy(magic, ACH_DB_MAGIC, 4);
    u32 version = ACH_DB_VERSION;
    u32 dir_count = (u32)vector_length(&index->dirs);
    u32 file_count = (u32)vector_length(&index->files);

    /* 1. Write Header */
    if (fwrite(magic, 1, 4, f) != 4 ||
        fwrite(&version, sizeof(u32), 1, f) != 1 ||
        fwrite(&dir_count, sizeof(u32), 1, f) != 1 ||
        fwrite(&file_count, sizeof(u32), 1, f) != 1) {
        fclose(f);
        return ACH_ERROR_FILE_WRITE_FAILED;
    }

    /* 2. Write Directory Table */
    for (u32 i = 0; i < dir_count; i++) {
        const IndexDir *dir = vector_get(&index->dirs, i);
        if (dir == NULL || dir->name == NULL) {
            fclose(f);
            return ACH_ERROR_INDEX_CORRUPT;
        }

        u32 name_len = (u32)wcslen(dir->name);

        if (fwrite(&dir->parent_id, sizeof(u32), 1, f) != 1 ||
            fwrite(&name_len, sizeof(u32), 1, f) != 1 ||
            fwrite(dir->name, sizeof(wchar_t), name_len, f) != name_len) {
            fclose(f);
            return ACH_ERROR_FILE_WRITE_FAILED;
        }
    }

    /* 3. Write File Table */
    for (u32 i = 0; i < file_count; i++) {
        const IndexFile *file = vector_get(&index->files, i);
        if (file == NULL || file->name == NULL) {
            fclose(f);
            return ACH_ERROR_INDEX_CORRUPT;
        }

        u32 name_len = (u32)wcslen(file->name);

        if (fwrite(&file->parent_id, sizeof(u32), 1, f) != 1 ||
            fwrite(&file->file_size, sizeof(u64), 1, f) != 1 ||
            fwrite(&file->last_modified, sizeof(u64), 1, f) != 1 ||
            fwrite(&file->attributes, sizeof(u32), 1, f) != 1 ||
            fwrite(&name_len, sizeof(u32), 1, f) != 1 ||
            fwrite(file->name, sizeof(wchar_t), name_len, f) != name_len) {
            fclose(f);
            return ACH_ERROR_FILE_WRITE_FAILED;
        }
    }

    fclose(f);
    return ACH_SUCCESS;
}

AchErrorCode db_load(Index *index, const wchar_t *file_path) {
    if (index == NULL || !index->initialized || file_path == NULL) {
        return ACH_ERROR_INVALID_ARG;
    }

    FILE *f = _wfopen(file_path, L"rb");
    if (f == NULL) {
        return ACH_ERROR_FILE_NOT_FOUND;
    }

    char magic[4];
    u32 version = 0;
    u32 dir_count = 0;
    u32 file_count = 0;

    /* 1. Read and Validate Header */
    if (fread(magic, 1, 4, f) != 4 ||
        fread(&version, sizeof(u32), 1, f) != 1 ||
        fread(&dir_count, sizeof(u32), 1, f) != 1 ||
        fread(&file_count, sizeof(u32), 1, f) != 1) {
        fclose(f);
        return ACH_ERROR_FILE_READ_FAILED;
    }

    if (memcmp(magic, ACH_DB_MAGIC, 4) != 0 || version != ACH_DB_VERSION) {
        fclose(f);
        return ACH_ERROR_INDEX_CORRUPT;
    }

    /* 2. Clear Existing Index */
    index_clear(index);

    /* 3. Pre-allocate Vectors */
    AchErrorCode err = vector_reserve(&index->dirs, dir_count);
    if (ACH_FAILED(err)) {
        fclose(f);
        return err;
    }
    err = vector_reserve(&index->files, file_count);
    if (ACH_FAILED(err)) {
        fclose(f);
        return err;
    }

    /* 4. Read Directory Table */
    for (u32 i = 0; i < dir_count; i++) {
        IndexDir dir;
        u32 name_len = 0;

        if (fread(&dir.parent_id, sizeof(u32), 1, f) != 1 ||
            fread(&name_len, sizeof(u32), 1, f) != 1) {
            index_clear(index);
            fclose(f);
            return ACH_ERROR_FILE_READ_FAILED;
        }

        /* Prevent integer overflow and excessive allocation vulnerabilities */
        if (name_len > 32767) {
            index_clear(index);
            fclose(f);
            return ACH_ERROR_INDEX_CORRUPT;
        }

        dir.name = malloc((name_len + 1) * sizeof(wchar_t));
        if (dir.name == NULL) {
            index_clear(index);
            fclose(f);
            return ACH_ERROR_OUT_OF_MEMORY;
        }

        if (fread(dir.name, sizeof(wchar_t), name_len, f) != name_len) {
            free(dir.name);
            index_clear(index);
            fclose(f);
            return ACH_ERROR_FILE_READ_FAILED;
        }
        dir.name[name_len] = L'\0';

        err = vector_push(&index->dirs, &dir);
        if (ACH_FAILED(err)) {
            free(dir.name);
            index_clear(index);
            fclose(f);
            return err;
        }
    }

    /* 5. Read File Table */
    for (u32 i = 0; i < file_count; i++) {
        IndexFile file;
        u32 name_len = 0;

        if (fread(&file.parent_id, sizeof(u32), 1, f) != 1 ||
            fread(&file.file_size, sizeof(u64), 1, f) != 1 ||
            fread(&file.last_modified, sizeof(u64), 1, f) != 1 ||
            fread(&file.attributes, sizeof(u32), 1, f) != 1 ||
            fread(&name_len, sizeof(u32), 1, f) != 1) {
            index_clear(index);
            fclose(f);
            return ACH_ERROR_FILE_READ_FAILED;
        }

        /* Prevent integer overflow and excessive allocation vulnerabilities */
        if (name_len > 32767) {
            index_clear(index);
            fclose(f);
            return ACH_ERROR_INDEX_CORRUPT;
        }

        file.name = malloc((name_len + 1) * sizeof(wchar_t));
        if (file.name == NULL) {
            index_clear(index);
            fclose(f);
            return ACH_ERROR_OUT_OF_MEMORY;
        }

        if (fread(file.name, sizeof(wchar_t), name_len, f) != name_len) {
            free(file.name);
            index_clear(index);
            fclose(f);
            return ACH_ERROR_FILE_READ_FAILED;
        }
        file.name[name_len] = L'\0';

        err = vector_push(&index->files, &file);
        if (ACH_FAILED(err)) {
            free(file.name);
            index_clear(index);
            fclose(f);
            return err;
        }
    }

    fclose(f);
    return ACH_SUCCESS;
}
