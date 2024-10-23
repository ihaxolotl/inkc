#include <stddef.h>

#include "unix.h"

/* TODO(Brett): Add a Win32 abstraction. */

/**
 * Request the platform to load a file into a buffer of bytes.
 */
int platform_load_file(const char *filename, unsigned char **bytes,
                       size_t *length)
{
    return unix_load_file(filename, bytes, length);
}

/**
 * Request the platform to allocate memory.
 */
void *platform_mem_alloc(size_t size)
{
    return unix_alloc(size);
}

/**
 * Request the platform to resize a block of memory.
 */
void *platform_mem_realloc(void *address, size_t old_size, size_t new_size)
{
    return unix_realloc(address, new_size);
}

/**
 * Release memory from the system allocator.
 */
void platform_mem_dealloc(void *address, size_t size)
{
    unix_dealloc(address, size);
}
