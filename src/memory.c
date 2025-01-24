#include <assert.h>
#include <stdlib.h>

#include "memory.h"

static void *ink_default_alloc(struct ink_allocator *gpa, size_t size)
{
    (void)gpa;
    assert(size > 0);
    return malloc(size);
}

static void *ink_default_realloc(struct ink_allocator *gpa, void *address,
                                 size_t size)
{
    (void)gpa;
    assert(size > 0);
    return realloc(address, size);
}

static void ink_default_dealloc(struct ink_allocator *gpa, void *pointer)
{
    (void)gpa;
    free(pointer);
}

static struct ink_allocator INK_DEFAULT_ALLOCATOR = {
    .allocate = ink_default_alloc,
    .resize = ink_default_realloc,
    .free = ink_default_dealloc,
};

static struct ink_allocator *INK_ALLOCATOR = &INK_DEFAULT_ALLOCATOR;

void ink_set_global_allocator(struct ink_allocator *gpa)
{
    INK_ALLOCATOR = gpa;
}

void *ink_malloc(size_t size)
{
    struct ink_allocator *const gpa = INK_ALLOCATOR;

    return gpa->allocate(gpa, size);
}

void *ink_realloc(void *memory, size_t size)
{
    struct ink_allocator *const gpa = INK_ALLOCATOR;

    return gpa->resize(gpa, memory, size);
}

void ink_free(void *memory)
{
    struct ink_allocator *const gpa = INK_ALLOCATOR;

    gpa->free(gpa, memory);
}
