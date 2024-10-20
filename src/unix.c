#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "unix.h"

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
