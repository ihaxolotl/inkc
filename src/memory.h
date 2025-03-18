#ifndef __INK_MEMORY_H__
#define __INK_MEMORY_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ink_allocator {
    void *(*allocate)(struct ink_allocator *self, size_t size);
    void *(*resize)(struct ink_allocator *self, void *memory, size_t size);
    void (*free)(struct ink_allocator *self, void *memory);
};

extern void ink_set_global_allocator(struct ink_allocator *gpa);
extern void ink_get_global_allocator(struct ink_allocator **gpa);
extern void *ink_malloc(size_t size);
extern void *ink_realloc(void *memory, size_t size);
extern void ink_free(void *memory);

#ifdef __cplusplus
}
#endif

#endif
