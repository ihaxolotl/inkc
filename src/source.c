#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "platform.h"
#include "source.h"

#define INK_SOURCE_BUF_MAX 1024

static const char *INK_FILE_EXT = ".ink";
static const size_t INK_FILE_EXT_LENGTH = 4;

static char *ink_string_copy(const char *chars, size_t length)
{
    char *buf;

    buf = platform_mem_alloc(length + 1);
    if (buf == NULL)
        return NULL;

    memcpy(buf, chars, length);
    buf[length] = '\0';
    return buf;
}

/**
 * Load an Ink source file from STDIN.
 */
int ink_source_load_stdin(struct ink_source *source)
{
    char buf[INK_SOURCE_BUF_MAX];

    source->filename = NULL;
    source->bytes = NULL;
    source->length = 0;

    while (fgets(buf, INK_SOURCE_BUF_MAX, stdin)) {
        const size_t len = source->length;
        const size_t buflen = strlen(buf);
        unsigned char *tmp;

        tmp = realloc(source->bytes, len + buflen + 1);
        if (tmp == NULL) {
            ink_source_free(source);
            return -INK_E_OOM;
        }

        source->bytes = tmp;
        memcpy(source->bytes + len, buf, buflen);
        source->length += buflen;
        source->bytes[source->length] = '\0';
    }
    if (ferror(stdin)) {
        ink_source_free(source);
        return -INK_E_OOM;
    }

    source->filename = ink_string_copy("STDIN", 5);
    if (source->filename == NULL) {
        ink_source_free(source);
        return -INK_E_OOM;
    }
    return INK_E_OK;
}

/**
 * Load an Ink source file from the file system.
 */
int ink_source_load(const char *filename, struct ink_source *source)
{
    int rc;
    const char *ext;
    const size_t namelen = strlen(filename);

    source->filename = NULL;
    source->bytes = NULL;
    source->length = 0;

    if (namelen < INK_FILE_EXT_LENGTH)
        return -INK_E_FILE;

    ext = filename + namelen - INK_FILE_EXT_LENGTH;

    if (strncmp(ext, INK_FILE_EXT, INK_FILE_EXT_LENGTH) != 0)
        return -INK_E_FILE;

    source->filename = ink_string_copy(filename, namelen);
    if (source->filename == NULL)
        return -INK_E_OOM;

    rc = platform_load_file(filename, &source->bytes, &source->length);
    if (rc == -1) {
        platform_mem_dealloc(source->filename, namelen + 1);
        return -INK_E_OS;
    }
    return 0;
}

void ink_source_free(struct ink_source *source)
{
    if (source->bytes) {
        platform_mem_dealloc(source->bytes, source->length + 1);
    }
    if (source->filename) {
        platform_mem_dealloc(source->filename, strlen(source->filename) + 1);
    }

    source->filename = NULL;
    source->bytes = NULL;
    source->length = 0;
}
