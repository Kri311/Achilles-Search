/* ==========================================================================
 * Achilles-Search | src/core/content_index.c
 * ==========================================================================
 * Full-text content indexer implementation.
 * ========================================================================== */

#include "core/content_index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* ---- Static Helpers ------------------------------------------------------ */

/* Checks if a file has an extension that we want to parse for content. */
static bool should_index_file(const wchar_t *path) {
    if (path == NULL) return false;

    const wchar_t *dot = wcsrchr(path, L'.');
    if (dot == NULL) return false;

    /* Supported plain text / source code extensions */
    const wchar_t *exts[] = {
        L".txt", L".c", L".h", L".cpp", L".hpp", L".json", L".md", L".py",
        L".java", L".xml", L".html", L".css", L".js", L".ts", L".bat", L".sh",
        L".cfg", L".ini", L".yaml", L".yml"
    };
    usize num_exts = sizeof(exts) / sizeof(exts[0]);

    for (usize i = 0; i < num_exts; i++) {
        if (_wcsicmp(dot, exts[i]) == 0) {
            return true;
        }
    }

    return false;
}

/* ---- Lifecycle ----------------------------------------------------------- */

AchErrorCode content_index_init(ContentIndex *cindex) {
    if (cindex == NULL) {
        return ACH_ERROR_INVALID_ARG;
    }

    AchErrorCode err = hashmap_init(&cindex->map, sizeof(Vector), 1024);
    if (err != ACH_SUCCESS) {
        return err;
    }

    cindex->initialized = true;
    return ACH_SUCCESS;
}

void content_index_destroy(ContentIndex *cindex) {
    if (cindex == NULL || !cindex->initialized) {
        return;
    }

    /* Free all dynamic vectors stored inside the HashMap values */
    HashMapIterator iter = hashmap_iter(&cindex->map);
    const wchar_t *key;
    Vector vec;
    while (hashmap_iter_next(&iter, &key, &vec)) {
        vector_destroy(&vec);
    }

    hashmap_destroy(&cindex->map);
    cindex->initialized = false;
}

void content_index_clear(ContentIndex *cindex) {
    if (cindex == NULL || !cindex->initialized) {
        return;
    }

    HashMapIterator iter = hashmap_iter(&cindex->map);
    const wchar_t *key;
    Vector vec;
    while (hashmap_iter_next(&iter, &key, &vec)) {
        vector_destroy(&vec);
    }

    hashmap_clear(&cindex->map);
}

/* ---- Indexing Operations ------------------------------------------------ */

AchErrorCode content_index_add_file(ContentIndex *cindex, u32 file_id, const wchar_t *file_path) {
    if (cindex == NULL || !cindex->initialized || file_path == NULL) {
        return ACH_ERROR_INVALID_ARG;
    }

    if (!should_index_file(file_path)) {
        return ACH_SUCCESS; /* Skip files with unsupported extensions */
    }

    FILE *f = _wfopen(file_path, L"rb");
    if (f == NULL) {
        return ACH_SUCCESS; /* Skip files we can't open (locked, access denied) */
    }

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    /* Skip empty files or files larger than 10 MB to prevent huge memory usage */
    if (size <= 0 || size > 10 * 1024 * 1024) {
        fclose(f);
        return ACH_SUCCESS;
    }

    char *buf = malloc(size + 1);
    if (buf == NULL) {
        fclose(f);
        return ACH_ERROR_OUT_OF_MEMORY;
    }

    size_t read_bytes = fread(buf, 1, size, f);
    buf[read_bytes] = '\0';
    fclose(f);

    /* Convert Multi-Byte string to Wide Character String (UTF-8 first) */
    int wlen = MultiByteToWideChar(CP_UTF8, 0, buf, (int)read_bytes, NULL, 0);
    wchar_t *wbuf = NULL;
    if (wlen > 0) {
        wbuf = malloc((wlen + 1) * sizeof(wchar_t));
        if (wbuf != NULL) {
            MultiByteToWideChar(CP_UTF8, 0, buf, (int)read_bytes, wbuf, wlen);
            wbuf[wlen] = L'\0';
        }
    } else {
        /* Fallback: ANSI Code Page */
        wlen = MultiByteToWideChar(CP_ACP, 0, buf, (int)read_bytes, NULL, 0);
        if (wlen > 0) {
            wbuf = malloc((wlen + 1) * sizeof(wchar_t));
            if (wbuf != NULL) {
                MultiByteToWideChar(CP_ACP, 0, buf, (int)read_bytes, wbuf, wlen);
                wbuf[wlen] = L'\0';
            }
        }
    }

    free(buf);

    if (wbuf == NULL) {
        return wlen > 0 ? ACH_ERROR_OUT_OF_MEMORY : ACH_SUCCESS;
    }

    /* Tokenize wide string buffer */
    const wchar_t *p = wbuf;
    while (*p) {
        /* Skip non-alphanumeric characters */
        while (*p && !iswalnum(*p) && *p != L'_') {
            p++;
        }

        if (*p == L'\0') {
            break;
        }

        const wchar_t *start = p;
        while (*p && (iswalnum(*p) || *p == L'_')) {
            p++;
        }

        usize len = p - start;
        /* Index tokens between 1 and 63 characters long */
        if (len > 0 && len < 64) {
            wchar_t token[64];
            for (usize i = 0; i < len; i++) {
                token[i] = towlower(start[i]);
            }
            token[len] = L'\0';

            /* Add token posting entry */
            Vector postings;
            AchErrorCode err = hashmap_get(&cindex->map, token, &postings);
            if (err == ACH_ERROR_NOT_FOUND) {
                err = vector_init(&postings, sizeof(u32), 4);
                if (err == ACH_SUCCESS) {
                    err = vector_push(&postings, &file_id);
                    if (err == ACH_SUCCESS) {
                        err = hashmap_put(&cindex->map, token, &postings);
                    }
                    if (err != ACH_SUCCESS) {
                        vector_destroy(&postings);
                    }
                }
            } else if (err == ACH_SUCCESS) {
                usize post_len = vector_length(&postings);
                bool duplicate = false;
                if (post_len > 0) {
                    u32 *last_id = (u32*)vector_get(&postings, post_len - 1);
                    if (last_id != NULL && *last_id == file_id) {
                        duplicate = true;
                    }
                }

                if (!duplicate) {
                    err = vector_push(&postings, &file_id);
                    if (err == ACH_SUCCESS) {
                        err = hashmap_put(&cindex->map, token, &postings);
                    }
                }
            }
        }
    }

    free(wbuf);
    return ACH_SUCCESS;
}

/* ---- Search Operations -------------------------------------------------- */

AchErrorCode content_index_search(const ContentIndex *cindex, const wchar_t *term, Vector *out_file_ids) {
    if (cindex == NULL || !cindex->initialized || term == NULL || out_file_ids == NULL) {
        return ACH_ERROR_INVALID_ARG;
    }

    usize len = wcslen(term);
    if (len == 0 || len >= 64) {
        return ACH_ERROR_NOT_FOUND;
    }

    wchar_t lower_term[64];
    for (usize i = 0; i < len; i++) {
        lower_term[i] = towlower(term[i]);
    }
    lower_term[len] = L'\0';

    Vector postings;
    AchErrorCode err = hashmap_get(&cindex->map, lower_term, &postings);
    if (err == ACH_ERROR_NOT_FOUND) {
        return ACH_ERROR_NOT_FOUND;
    }

    usize count = vector_length(&postings);
    err = vector_reserve(out_file_ids, count);
    if (err != ACH_SUCCESS) {
        return err;
    }

    for (usize i = 0; i < count; i++) {
        u32 *val = (u32*)vector_get(&postings, i);
        if (val != NULL) {
            vector_push(out_file_ids, val);
        }
    }

    return ACH_SUCCESS;
}
