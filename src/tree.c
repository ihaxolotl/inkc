#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "arena.h"
#include "platform.h"
#include "tree.h"

#define T(name, description) description,
static const char *INK_TT_STR[] = {INK_TT(T)};
#undef T

#define T(name, description) description,
static const char *INK_NODE_TYPE_STR[] = {INK_NODE(T)};
#undef T

static void ink_syntax_node_print_walk(const struct ink_syntax_tree *tree,
                                       const struct ink_syntax_node *node,
                                       int level);

const char *ink_token_type_strz(enum ink_token_type type)
{
    return INK_TT_STR[type];
}

/**
 * Return a NULL-terminated string representing the type description of a
 * syntax tree node.
 */
const char *ink_syntax_node_type_strz(enum ink_syntax_node_type type)
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
    const char *node_type_str;
    const unsigned char *lexeme, *bytes;
    size_t lexeme_length;

    if (node == NULL)
        return;

    bytes = tree->source->bytes;
    node_type_str = ink_syntax_node_type_strz(node->type);
    lexeme_length = node->end_offset - node->start_offset;
    lexeme = bytes + node->start_offset;

    for (int i = 0; i < level; i++) {
        printf("--");
    }

    switch (node->type) {
    case INK_NODE_STRING_LITERAL:
    case INK_NODE_STRING_EXPR:
    case INK_NODE_NUMBER_EXPR:
    case INK_NODE_IDENTIFIER_EXPR:
    case INK_NODE_PARAM_DECL:
    case INK_NODE_REF_PARAM_DECL:
        printf("%s `%.*s`\n", node_type_str, (int)lexeme_length, lexeme);
        break;
    default:
        printf("%s\n", node_type_str);
        break;
    }

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

void ink_token_buffer_initialize(struct ink_token_buffer *buffer)
{
    buffer->count = 0;
    buffer->capacity = 0;
    buffer->entries = NULL;
}

/**
 * Reserve storage space for the token stream.
 */
int ink_token_buffer_reserve(struct ink_token_buffer *buffer, size_t item_count)
{
    struct ink_token *entries = buffer->entries;
    size_t old_capacity = buffer->capacity * sizeof(*entries);
    size_t new_capacity = item_count * sizeof(*entries);

    assert(new_capacity > old_capacity);

    entries = platform_mem_realloc(entries, old_capacity, new_capacity);
    if (entries == NULL) {
        buffer->entries = NULL;
        return -1;
    }

    buffer->count = buffer->count;
    buffer->capacity = item_count;
    buffer->entries = entries;

    return 0;
}

/**
 * Append a token to the token stream.
 */
int ink_token_buffer_append(struct ink_token_buffer *buffer,
                            struct ink_token item)
{
    size_t capacity, old_size, new_size;
    struct ink_token *entries;

    if (buffer->count + 1 > buffer->capacity) {
        if (buffer->capacity < INK_TOKEN_STREAM_MIN_COUNT) {
            capacity = INK_TOKEN_STREAM_MIN_COUNT;
        } else {
            capacity = buffer->capacity * INK_TOKEN_STREAM_GROWTH_FACTOR;
        }

        old_size = buffer->capacity * sizeof(*entries);
        new_size = capacity * sizeof(*entries);

        entries = platform_mem_realloc(buffer->entries, old_size, new_size);
        if (entries == NULL) {
            buffer->entries = NULL;
            return -1;
        }

        buffer->capacity = capacity;
        buffer->entries = entries;
    }

    buffer->entries[buffer->count++] = item;
    return 0;
}

/**
 * Release the memory for the token stream.
 */
void ink_token_buffer_cleanup(struct ink_token_buffer *buffer)
{
    size_t mem_size;

    if (buffer->capacity > 0) {
        mem_size = sizeof(*buffer->entries) * buffer->capacity;

        platform_mem_dealloc(buffer->entries, mem_size);
    }
}

void ink_token_print(const struct ink_source *source,
                     const struct ink_token *token)
{
    const size_t start = token->start_offset;
    const size_t end = token->end_offset;

    switch (token->type) {
    case INK_TT_EOF:
        printf("[DEBUG] %s(%zu, %zu): `\\0`\n",
               ink_token_type_strz(token->type), start, end);
        break;
    case INK_TT_NL:
        printf("[DEBUG] %s(%zu, %zu): `\\n`\n",
               ink_token_type_strz(token->type), start, end);
        break;
    default:
        printf("[DEBUG] %s(%zu, %zu): `%.*s`\n",
               ink_token_type_strz(token->type), start, end, (int)(end - start),
               source->bytes + start);
        break;
    }
}

void ink_token_buffer_print(const struct ink_source *source,
                            const struct ink_token_buffer *buffer)
{
    for (size_t i = 0; i < buffer->count; i++) {
        ink_token_print(source, &buffer->entries[i]);
    }
}

/**
 * Create a syntax tree node.
 */
struct ink_syntax_node *
ink_syntax_node_new(struct ink_arena *arena, enum ink_syntax_node_type type,
                    size_t start_offset, size_t end_offset,
                    struct ink_syntax_node *lhs, struct ink_syntax_node *rhs,
                    struct ink_syntax_seq *seq)
{
    struct ink_syntax_node *node;

    assert(start_offset <= end_offset);

    node = ink_arena_allocate(arena, sizeof(*node));
    if (node == NULL)
        return NULL;

    node->type = type;
    node->start_offset = start_offset;
    node->end_offset = end_offset;
    node->lhs = lhs;
    node->rhs = rhs;
    node->seq = seq;

    return node;
}

/**
 * Initialize a syntax tree.
 *
 * Storage space for the token buffer will be reserved for later use here.
 */
int ink_syntax_tree_initialize(const struct ink_source *source,
                               struct ink_syntax_tree *tree)
{
    struct ink_token_buffer *tokens = &tree->tokens;

    ink_token_buffer_initialize(tokens);
    if (ink_token_buffer_reserve(tokens, INK_TOKEN_STREAM_MIN_COUNT) < 0)
        return -1;

    tree->source = source;
    tree->root = NULL;

    return 0;
}

void ink_syntax_tree_cleanup(struct ink_syntax_tree *tree)
{
    ink_token_buffer_cleanup(&tree->tokens);
}
