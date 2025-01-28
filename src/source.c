#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "memory.h"
#include "source.h"

#define INK_SOURCE_BUF_MAX 1024

static const char *INK_FILE_EXT = ".ink";
static const size_t INK_FILE_EXT_LENGTH = 4;

int unix_load_file(const char *filename, uint8_t **bytes, size_t *length)
{
    int fd;
    ssize_t nread;
    off_t bufsz;
    uint8_t *buf;

    fd = open(filename, O_RDONLY);
    if (fd == -1) {
        return -1;
    }

    bufsz = lseek(fd, 0, SEEK_END);
    if (bufsz == -1) {
        goto err_file;
    }
    if (lseek(fd, 0, SEEK_SET) == -1) {
        goto err_file;
    }

    buf = ink_malloc((size_t)bufsz + 1);
    if (buf == NULL) {
        goto err_file;
    }

    nread = read(fd, buf, (size_t)bufsz);
    if (nread != bufsz) {
        goto err_memory;
    }

    buf[bufsz] = '\0';
    *bytes = buf;
    *length = (size_t)bufsz;
    close(fd);
    return 0;
err_memory:
    ink_free(buf);
err_file:
    close(fd);
    return -1;
}

/**
 * Load an Ink source file from STDIN.
 */
int ink_source_load_stdin(struct ink_source *source)
{
    char buf[INK_SOURCE_BUF_MAX];

    source->bytes = NULL;
    source->length = 0;

    while (fgets(buf, INK_SOURCE_BUF_MAX, stdin)) {
        uint8_t *tmp;
        const size_t len = source->length;
        const size_t buflen = strlen(buf);

        tmp = realloc(source->bytes, len + buflen + 1);
        if (!tmp) {
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
    return INK_E_OK;
}

/**
 * Load an Ink source file from the file system.
 */
int ink_source_load(const char *filename, struct ink_source *source)
{
    const char *ext;
    const size_t namelen = strlen(filename);

    source->bytes = NULL;
    source->length = 0;

    if (namelen < INK_FILE_EXT_LENGTH) {
        return -INK_E_FILE;
    }

    ext = filename + namelen - INK_FILE_EXT_LENGTH;
    if (!(strncmp(ext, INK_FILE_EXT, INK_FILE_EXT_LENGTH) == 0)) {
        return -INK_E_FILE;
    }
    return unix_load_file(filename, &source->bytes, &source->length);
}

void ink_source_free(struct ink_source *source)
{
    ink_free(source->bytes);
    source->bytes = NULL;
    source->length = 0;
}
