#include <assert.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

#include "unix.h"

int unix_load_file(const char *filename, unsigned char **bytes, size_t *length)
{
    int fd;
    ssize_t nread;
    off_t bufsz;
    unsigned char *buf;

    fd = open(filename, O_RDONLY);
    if (fd == -1)
        return -1;

    bufsz = lseek(fd, 0, SEEK_END);
    if (bufsz == -1)
        goto err_file;
    if (lseek(fd, 0, SEEK_SET) == -1)
        goto err_file;

    buf = unix_alloc((size_t)bufsz + 1);
    if (buf == NULL)
        goto err_file;

    nread = read(fd, buf, (size_t)bufsz);
    if (nread != bufsz)
        goto err_memory;

    buf[bufsz] = '\0';
    *bytes = buf;
    *length = (size_t)bufsz;

    close(fd);
    return 0;
err_memory:
    unix_dealloc(buf, (size_t)bufsz);
err_file:
    close(fd);
    return -1;
}

/**
 * Request the system allocator for a block of memory.
 */
void *unix_alloc(size_t size)
{
    assert(size > 0);

    return malloc(size);
}

/**
 * Request the system allocator to resize a block of memory.
 */
void *unix_realloc(void *address, size_t size)
{
    assert(size > 0);

    return realloc(address, size);
}

/**
 * Request the system allocator to deallocate a block of memory.
 */
void unix_dealloc(void *pointer, size_t size)
{
    assert(pointer != NULL && size > 0);
    free(pointer);
}
