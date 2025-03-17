#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "memory.h"
#include "source.h"

#define INK_SOURCE_BUF_MAX 1024

static const char *INK_FILE_EXT = ".ink";
static const size_t INK_FILE_EXT_LENGTH = 4;

static int ink_read_file(const char *file_path, uint8_t **bytes, size_t *length)
{
    size_t sz = 0, nr = 0;
    uint8_t *b = NULL;
    FILE *const fp = fopen(file_path, "rb");

    if (!fp) {
        fprintf(stderr, "Could not read file '%s'\n", file_path);
        return -1;
    }

    fseek(fp, 0u, SEEK_END);
    sz = (size_t)ftell(fp);
    fseek(fp, 0u, SEEK_SET);

    b = ink_malloc(sz + 1);
    if (!b) {
        fclose(fp);
        return -1;
    }

    nr = fread(b, 1u, sz, fp);
    if (nr < sz) {
        fprintf(stderr, "Could not read file '%s'.\n", file_path);
        fclose(fp);
        ink_free(b);
        return -1;
    }

    b[nr] = '\0';
    *bytes = b;
    *length = sz;
    fclose(fp);
    return 0;
}

/**
 * Load an Ink source file from STDIN.
 */
int ink_source_load_stdin(struct ink_source *s)
{
    uint8_t *tmp;
    char b[INK_SOURCE_BUF_MAX];

    s->bytes = NULL;
    s->length = 0;

    while (fgets(b, INK_SOURCE_BUF_MAX, stdin)) {
        const size_t len = s->length;
        const size_t buflen = strlen(b);

        tmp = realloc(s->bytes, len + buflen + 1);
        if (!tmp) {
            ink_source_free(s);
            return -INK_E_OOM;
        }

        s->bytes = tmp;
        memcpy(s->bytes + len, b, buflen);
        s->length += buflen;
        s->bytes[s->length] = '\0';
    }
    if (ferror(stdin)) {
        ink_source_free(s);
        return -INK_E_OOM;
    }
    return INK_E_OK;
}

/**
 * Load an Ink source file from the file system.
 */
int ink_source_load(const char *file_path, struct ink_source *s)
{
    const char *ext;
    const size_t namelen = strlen(file_path);

    s->bytes = NULL;
    s->length = 0;

    if (namelen < INK_FILE_EXT_LENGTH) {
        return -INK_E_FILE;
    }

    ext = file_path + namelen - INK_FILE_EXT_LENGTH;
    if (!(strncmp(ext, INK_FILE_EXT, INK_FILE_EXT_LENGTH) == 0)) {
        return -INK_E_FILE;
    }
    return ink_read_file(file_path, &s->bytes, &s->length);
}

void ink_source_free(struct ink_source *s)
{
    ink_free(s->bytes);
    s->bytes = NULL;
    s->length = 0;
}
