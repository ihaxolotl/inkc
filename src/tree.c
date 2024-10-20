#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "arena.h"
#include "platform.h"
#include "tree.h"

#define T(name, description) description,
static const char *INK_NODE_TYPE_STR[] = {INK_NODE(T)};
#undef T

static void ink_syntax_node_print_walk(const struct ink_syntax_tree *tree,
                                       const struct ink_syntax_node *node,
                                       int level);

/**
 * Return a NULL-terminated string representing the type description of a
 * syntax tree node.
 */
static const char *ink_syntax_node_type_strz(enum ink_syntax_node_type type)
{
    return INK_NODE_TYPE_STR[type];
}

/**
 * Print a syntax tree node sequence to stdout.
 */
static void ink_syntax_seq_print(const struct ink_syntax_tree *tree,
                                 const struct ink_syntax_seq *seq, int level)
{
    for (size_t i = 0; i < seq->count; i++) {
        ink_syntax_node_print_walk(tree, seq->nodes[i], level);
    }
}

/**
 * Walk the syntax tree and print each node.
 */
static void ink_syntax_node_print_walk(const struct ink_syntax_tree *tree,
                                       const struct ink_syntax_node *node,
                                       int level)
{
    const char *type_str;
    unsigned char *lexeme;
    size_t lexeme_length;
    struct ink_token token;

    if (node == NULL)
        return;

    token = tree->tokens.entries[node->main_token];
    lexeme_length = token.end_offset - token.start_offset;
    lexeme = tree->source->bytes + token.start_offset;

    type_str = ink_syntax_node_type_strz(node->type);
    printf("%*s%s `%.*s`\n", level * 2, "", type_str, (int)lexeme_length,
           lexeme);

    level++;

    ink_syntax_node_print_walk(tree, node->lhs, level);
    ink_syntax_node_print_walk(tree, node->rhs, level);

    if (node->seq)
        ink_syntax_seq_print(tree, node->seq, level);
}

/**
 * Print a syntax tree.
 */
void ink_syntax_tree_print(const struct ink_syntax_tree *tree)
{
    if (tree->root)
        ink_syntax_node_print_walk(tree, tree->root, 0);
}

/**
 * Reserve scratch space for a specified number of items.
 */
int ink_scratch_reserve(struct ink_scratch_buffer *scratch, size_t item_count)
{
    struct ink_syntax_node **storage;
    size_t scratch_capacity = item_count * sizeof(storage);

    storage = platform_mem_alloc(scratch_capacity);
    if (scratch == NULL)
        return -1;

    scratch->count = 0;
    scratch->capacity = scratch_capacity;
    scratch->entries = storage;

    return 0;
}

/**
 * Shrink the parser's scratch storage down to a specified size.
 *
 * Re-allocation is not performed here. Therefore, subsequent allocations
 * are amortized.
 */
static void ink_scratch_shrink(struct ink_scratch_buffer *scratch, size_t count)
{
    assert(count <= scratch->capacity);

    scratch->count = count;
}

/**
 * Append a syntax tree node to the parser's scratch storage.
 *
 * These nodes can be retrieved later for creating sequences.
 */
void ink_scratch_append(struct ink_scratch_buffer *scratch,
                        struct ink_syntax_node *node)
{
    size_t capacity;

    if (scratch->count + 1 > scratch->capacity) {
        if (scratch->capacity < INK_SCRATCH_MIN_COUNT) {
            capacity = INK_SCRATCH_MIN_COUNT;
        } else {
            capacity = scratch->capacity * INK_SCRATCH_GROWTH_FACTOR;
        }

        scratch->entries =
            realloc(scratch->entries, capacity * sizeof(scratch->entries));
        scratch->capacity = capacity;
    }

    scratch->entries[scratch->count++] = node;
}

/**
 * Release memory for scratch storage.
 */
void ink_scratch_cleanup(struct ink_scratch_buffer *scratch)
{
    size_t mem_size = sizeof(scratch->entries) * scratch->capacity;

    platform_mem_dealloc(scratch->entries, mem_size);
}

struct ink_syntax_seq *ink_seq_from_scratch(struct ink_arena *arena,
                                            struct ink_scratch_buffer *scratch,
                                            size_t start_offset,
                                            size_t end_offset)
{
    struct ink_syntax_seq *seq = NULL;

    if (start_offset < end_offset) {
        seq = ink_syntax_seq_new(arena, scratch, start_offset, end_offset);

        ink_scratch_shrink(scratch, start_offset);
    }
    return seq;
}

/**
 * Reserve storage space for the token stream.
 */
int ink_token_stream_reserve(struct ink_token_stream *stream, size_t item_count)
{
    struct ink_token *entries;
    size_t capacity = item_count * sizeof(*entries);

    entries = platform_mem_alloc(capacity);
    if (entries == NULL)
        return -1;

    stream->count = 0;
    stream->capacity = capacity;
    stream->entries = entries;

    return 0;
}

/**
 * Append a token to the token stream.
 */
int ink_token_stream_append(struct ink_token_stream *stream,
                            struct ink_token item)
{
    size_t capacity;
    struct ink_token *entries;

    if (stream->count + 1 > stream->capacity) {
        if (stream->capacity < INK_TOKEN_STREAM_MIN_COUNT) {
            capacity = INK_TOKEN_STREAM_MIN_COUNT;
        } else {
            capacity = stream->capacity * INK_TOKEN_STREAM_GROWTH_FACTOR;
        }

        /* TODO(Brett): Platform abstracted realloc. */
        entries = realloc(stream->entries, capacity * sizeof(item));
        if (entries == NULL)
            return -1;

        stream->capacity = capacity;
        stream->entries = entries;
    }

    stream->entries[stream->count++] = item;
    return 0;
}

/**
 * Release the memory for the token stream.
 */
void ink_token_stream_cleanup(struct ink_token_stream *stream)
{
    size_t mem_size;

    if (stream->capacity > 0) {
        mem_size = sizeof(*stream->entries) * stream->capacity;

        platform_mem_dealloc(stream->entries, mem_size);
    }
}

/**
 * Create a syntax tree node.
 */
struct ink_syntax_node *
ink_syntax_node_new(struct ink_arena *arena, enum ink_syntax_node_type type,
                    struct ink_syntax_node *lhs, struct ink_syntax_node *rhs,
                    size_t main_token, struct ink_syntax_seq *seq)
{
    struct ink_syntax_node *node;

    node = ink_arena_allocate(arena, sizeof(*node));
    if (node == NULL)
        return NULL;

    node->type = type;
    node->main_token = main_token;
    node->lhs = lhs;
    node->rhs = rhs;
    node->seq = seq;

    return node;
}

/**
 * Create a syntax tree node sequence.
 */
struct ink_syntax_seq *ink_syntax_seq_new(struct ink_arena *arena,
                                          struct ink_scratch_buffer *scratch,
                                          size_t start_offset,
                                          size_t end_offset)
{
    struct ink_syntax_seq *seq;
    size_t seq_index = 0;
    size_t span = end_offset - start_offset;

    assert(span > 0);

    seq = ink_arena_allocate(arena, sizeof(*seq) + span * sizeof(seq->nodes));
    if (seq == NULL)
        return NULL;

    seq->count = span;

    for (size_t i = start_offset; i < end_offset; i++) {
        seq->nodes[seq_index] = scratch->entries[i];
        seq_index++;
    }
    return seq;
}

/**
 * Initialize a syntax tree.
 *
 * Storage space for the token buffer will be reserved for later use here.
 */
int ink_syntax_tree_initialize(struct ink_source *source,
                               struct ink_syntax_tree *tree)
{
    if (ink_token_stream_reserve(&tree->tokens, INK_TOKEN_STREAM_MIN_COUNT) < 0)
        return -1;

    tree->source = source;
    tree->root = NULL;

    return 0;
}

void ink_syntax_tree_cleanup(struct ink_syntax_tree *tree)
{
}
