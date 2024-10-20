#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "platform.h"
#include "source.h"

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
 * Load an Ink source file.
 *
 * `source` will only be initialized upon success.
 */
int ink_source_load(const char *filename, struct ink_source *source)
{
    int rc;
    const char *ext;
    const size_t namelen = strlen(filename);

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
    platform_mem_dealloc(source->bytes, source->length + 1);
    platform_mem_dealloc(source->filename, strlen(source->filename) + 1);

    source->filename = NULL;
    source->bytes = NULL;
    source->length = 0;
}
