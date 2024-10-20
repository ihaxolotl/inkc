#ifndef __INK_ARENA_H__
#define __INK_ARENA_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

struct ink_arena_block;

/**
 * Memory arena.
 *
 * Maintains a singly-linked list for memory blocks, along with statistical
 * information on past allocations.
 *
 * TODO(Brett): Should we add a free-list block cache?

 * TODO(Brett): Should we add a panic handler?

 * TODO(Brett): Provide a platform abstraction for system allocators.
 */
struct ink_arena {
    struct ink_arena_block *block_first;
    struct ink_arena_block *block_current;
    size_t default_block_size;
    size_t alignment;
    size_t total_bytes;
    size_t total_blocks;
    size_t total_block_size;
    size_t total_oversized_blocks;
    size_t total_allocations;
};

extern void ink_arena_initialize(struct ink_arena *arena, size_t block_size,
                                 size_t alignment);
extern void *ink_arena_allocate(struct ink_arena *arena, size_t size);
extern void ink_arena_release(struct ink_arena *arena);

#ifdef __cplusplus
}
#endif

#endif
