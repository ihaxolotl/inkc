#ifndef INK_MEMORY_H
#define INK_MEMORY_H

#include <stddef.h>

#include <ink/ink.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ink_allocator {
    void *(*allocate)(struct ink_allocator *self, size_t size);
    void *(*resize)(struct ink_allocator *self, void *memory, size_t size);
    void (*free)(struct ink_allocator *self, void *memory);
};

INK_API void ink_set_global_allocator(struct ink_allocator *gpa);
INK_API void ink_get_global_allocator(struct ink_allocator **gpa);
INK_API void *ink_malloc(size_t size);
INK_API void *ink_realloc(void *memory, size_t size);
INK_API void ink_free(void *memory);

#ifdef __cplusplus
}
#endif

#endif
