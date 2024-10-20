#include <stddef.h>

#include "unix.h"

/* TODO(Brett): Add a Win32 abstraction. */

/**
 * Platform abstracted procedure for loading a file into a buffer of bytes.
 */
int platform_load_file(const char *filename, unsigned char **bytes,
                       size_t *length)
{
    return unix_load_file(filename, bytes, length);
}

/**
 * Invoke the system allocator to request memory.
 */
void *platform_mem_alloc(size_t size)
{
    return unix_alloc(size);
}

/**
 * Invoke the system allocator to release memory.
 */
void platform_mem_dealloc(void *pointer, size_t size)
{
    unix_dealloc(pointer, size);
}
