#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "arena.h"
#include "memory.h"

/**
 * A block of pre-allocated memory.
 *
 * Tracked by a memory arena.
 */
struct ink_arena_block {
    /* Link to the next block. */
    struct ink_arena_block *next;

    /* Total number of bytes owned by this block.
     * Read-only after initialization.
     */
    size_t size;

    /* Total number of bytes provisioned within this block. The next available
     * byte will be available at `block[offset]`.
     */
    size_t offset;

    /* Pointer to available block memory. */
    uint8_t bytes[];
};

/**
 * Round a specified size up to a particular alignment.
 */
static inline size_t ink_align_size(size_t size, size_t alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

/**
 * Allocate a new block for the arena.
 *
 * Only the block header is initialized.
 */
static struct ink_arena_block *ink_arena_block_new(size_t size)
{
    struct ink_arena_block *block;

    block = ink_malloc(sizeof(*block) + size);
    if (block == NULL) {
        return NULL;
    }

    block->next = NULL;
    block->size = size;
    block->offset = 0;
    return block;
}

/**
 * Provision memory within a block.
 *
 * If the specified block does not contain an adequate capacity for an
 * allocation of the requested size, a new block will be created and chained to
 * the supplied block.
 *
 * The supplied block MUST not be NULL and MUST be the most recently allocated
 * allocated block; that is, a block with no `next` link.
 */
static void *ink_arena_block_alloc(struct ink_arena_block *block, size_t size,
                                   size_t alignment, size_t fallback_size)
{
    struct ink_arena_block *new_block;
    void *address;

    assert(block != NULL);

    size = ink_align_size(size, alignment);

    if (block->offset + size > block->size) {
        /*
         * TODO(Brett): We may want to over-allocate here.
         * Ask someone who knows a lot about allocators.
         */
        new_block =
            ink_arena_block_new(size < fallback_size ? fallback_size : size);
        if (new_block == NULL)
            return NULL;

        assert(block->next == NULL);

        block->next = new_block;
        block = new_block;
    }

    /* SANITY: Detect heap-overflow. */
    /* TODO(Brett): This may cause an integer overflow. Do we care? */
    assert(block->offset + size <= block->size);

    address = (uint8_t *)block->bytes + block->offset;
    block->offset += size;
    return address;
}

/**
 * Free an arena block.
 */
static void ink_arena_block_free(struct ink_arena_block *block)
{
    ink_free(block);
}

/**
 * Initialize memory arena.
 *
 * No dynamic allocations are performed here.
 */
void ink_arena_init(struct ink_arena *arena, size_t block_size,
                    size_t alignment)
{
    /* TODO(Brett): I am not sure if this is correct. Revisit this. */
    assert(alignment != 0 && !(alignment & (alignment - 1)));
    assert(block_size != 0 && !(block_size & (block_size - 1)));
    assert(block_size >= alignment);

    arena->block_first = NULL;
    arena->block_current = NULL;
    arena->default_block_size = block_size;
    arena->alignment = alignment;
    arena->total_bytes = 0;
    arena->total_blocks = 0;
    arena->total_block_size = 0;
    arena->total_oversized_blocks = 0;
    arena->total_allocations = 0;
}

/**
 * Arena allocator.
 *
 * The design of this allocator was heavily referenced from CPython's
 * `_PyArena_Malloc`.
 *
 * Will return a memory address aligned to the arena's alignment boundary.
 *
 * Blocks are created lazily, with the allocator's initial state containing
 * no blocks. If an allocation larger than the remaining space in the current
 * block is requested, a new block will be created and the allocation will be
 * provisioned to the newly created block.
 */
void *ink_arena_allocate(struct ink_arena *arena, size_t size)
{
    void *address;
    struct ink_arena_block *block;

    if (arena->block_first == NULL) {
        // SANITY: First block initialization should only happen once.
        assert(arena->total_blocks == 0 && arena->total_allocations == 0);

        block = ink_arena_block_new(arena->default_block_size);
        if (block == NULL) {
            return NULL;
        }

        arena->block_first = block;
        arena->block_current = block;
        arena->total_blocks++;
    } else {
        block = arena->block_current;
    }

    address = ink_arena_block_alloc(block, size, arena->alignment,
                                    arena->default_block_size);
    if (address == NULL) {
        return NULL;
    }

    arena->total_allocations++;
    arena->total_bytes += size;

    if (arena->block_current->next) {
        arena->block_current = arena->block_current->next;
        arena->total_blocks++;
        arena->total_block_size += arena->block_current->size;

        if (arena->block_current->size > arena->default_block_size) {
            arena->total_oversized_blocks++;
        }
    }
    return address;
}

/**
 * Release any memory tracked by the arena.
 *
 * Allocation statistics will remain intact.
 */
void ink_arena_release(struct ink_arena *arena)
{
    struct ink_arena_block *block;
    struct ink_arena_block *head = arena->block_first;

    while (head != NULL) {
        block = head;
        head = head->next;
        ink_arena_block_free(block);
    }

    arena->block_first = NULL;
    arena->block_current = NULL;
}
