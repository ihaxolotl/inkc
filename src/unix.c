#include <assert.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/mman.h>
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

    buf = unix_alloc(bufsz + 1);
    if (buf == NULL)
        goto err_file;

    nread = read(fd, buf, bufsz);
    if (nread != bufsz)
        goto err_memory;

    buf[bufsz] = '\0';
    *bytes = buf;
    *length = bufsz;

    close(fd);
    return 0;
err_memory:
    unix_dealloc(buf, bufsz);
err_file:
    close(fd);
    return -1;
}

/**
 * Allocates a block of memory using `mmap`.
 */
void *unix_alloc(size_t size)
{
    void *ptr;

    assert(size > 0);

    ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE,
               -1, 0);
    if (ptr == MAP_FAILED)
        return NULL;

    return ptr;
}

/**
 * Deallocates a block of memory using `munmap`.
 */
void unix_dealloc(void *pointer, size_t size)
{
    assert(pointer != NULL && size > 0);
    munmap(pointer, size);
}
