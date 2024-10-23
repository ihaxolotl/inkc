#ifndef __INK_PLATFORM_H__
#define __INK_PLATFORM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

extern int platform_load_file(const char *filename, unsigned char **bytes,
                              size_t *length);
extern void *platform_mem_alloc(size_t size);
extern void *platform_mem_realloc(void *address, size_t old_size,
                                  size_t new_size);
extern void platform_mem_dealloc(void *pointer, size_t size);

#ifdef __cplusplus
}
#endif

#endif
