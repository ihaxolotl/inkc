#ifndef __INK_UNIX_H__
#define __INK_UNIX_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

extern int unix_load_file(const char *filename, unsigned char **bytes,
                          size_t *length);
extern void *unix_alloc(size_t size);
extern void unix_dealloc(void *pointer, size_t size);

#ifdef __cplusplus
}
#endif

#endif
