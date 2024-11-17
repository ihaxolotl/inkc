#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "arena.h"
#include "platform.h"
#include "tree.h"

/**
 * Node buffer.
 */
struct ink_node_buffer {
    size_t count;
    size_t capacity;
    struct ink_syntax_node **entries;
};

#define T(name, description) description,
static const char *INK_TT_STR[] = {INK_TT(T)};
#undef T

#define T(name, description) description,
static const char *INK_NODE_TYPE_STR[] = {INK_NODE(T)};
#undef T

static const char *INK_SYNTAX_TREE_EMPTY[] = {"", ""};
static const char *INK_SYNTAX_TREE_INNER[] = {"+-- ", "|   "};
static const char *INK_SYNTAX_TREE_FINAL[] = {"`-- ", "    "};

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

static void ink_node_buffer_initialize(struct ink_node_buffer *buffer)
{
    buffer->count = 0;
    buffer->capacity = 0;
    buffer->entries = NULL;
}

static int ink_node_buffer_append(struct ink_node_buffer *buffer,
                                  struct ink_syntax_node *item)
{
    size_t capacity, old_size, new_size;
    struct ink_syntax_node **entries;

    if (buffer->count + 1 > buffer->capacity) {
        if (buffer->capacity < 16) {
            capacity = 16;
        } else {
            capacity = buffer->capacity * 2;
        }

        old_size = buffer->capacity * sizeof(entries);
        new_size = capacity * sizeof(entries);

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

static void ink_node_buffer_cleanup(struct ink_node_buffer *buffer)
{
    size_t mem_size;

    if (buffer->capacity > 0) {
        mem_size = sizeof(buffer->entries) * buffer->capacity;

        platform_mem_dealloc(buffer->entries, mem_size);
    }
}

static void ink_syntax_tree_print_node(const struct ink_syntax_tree *tree,
                                       const struct ink_syntax_node *node,
                                       const char *prefix,
                                       const char **pointers)
{
    char output[1024];
    const char *node_type_str = ink_syntax_node_type_strz(node->type);
    const unsigned char *bytes = tree->source->bytes;
    const unsigned char *lexeme = bytes + node->start_offset;
    const size_t lexeme_length = node->end_offset - node->start_offset;

    switch (node->type) {
    case INK_NODE_STRING_LITERAL:
    case INK_NODE_STRING_EXPR:
    case INK_NODE_NUMBER_EXPR:
    case INK_NODE_IDENTIFIER_EXPR:
    case INK_NODE_PARAM_DECL:
    case INK_NODE_REF_PARAM_DECL:
        snprintf(output, sizeof(output), "%s `%.*s`", node_type_str,
                 (int)lexeme_length, lexeme);
        break;
    case INK_NODE_FILE:
        snprintf(output, sizeof(output), "%s \"%s\"", node_type_str,
                 tree->source->filename);
        break;
    default:
        snprintf(output, sizeof(output), "%s", node_type_str);
        break;
    }

    printf("%s%s%s\n", prefix, pointers[0], output);
}

/**
 * Walk the syntax tree and print each node.
 */
static void ink_syntax_tree_print_walk(const struct ink_syntax_tree *tree,
                                       const struct ink_syntax_node *node,
                                       const char *prefix,
                                       const char **pointers)
{
    char new_prefix[1024];
    struct ink_node_buffer nodes;

    ink_node_buffer_initialize(&nodes);

    if (node->lhs) {
        ink_node_buffer_append(&nodes, node->lhs);
    }
    if (node->rhs) {
        ink_node_buffer_append(&nodes, node->rhs);
    }
    if (node->seq) {
        for (size_t i = 0; i < node->seq->count; i++) {
            ink_node_buffer_append(&nodes, node->seq->nodes[i]);
        }
    }
    for (size_t i = 0; i < nodes.count; i++) {
        const char **pointers = i == nodes.count - 1 ? INK_SYNTAX_TREE_FINAL
                                                     : INK_SYNTAX_TREE_INNER;

        ink_syntax_tree_print_node(tree, nodes.entries[i], prefix, pointers);
        snprintf(new_prefix, sizeof(new_prefix), "%s%s", prefix, pointers[1]);
        ink_syntax_tree_print_walk(tree, nodes.entries[i], new_prefix,
                                   pointers);
    }

    ink_node_buffer_cleanup(&nodes);
}

/**
 * Print a syntax tree.
 */
void ink_syntax_tree_print(const struct ink_syntax_tree *tree)
{
    if (tree->root) {
        ink_syntax_tree_print_node(tree, tree->root, "", INK_SYNTAX_TREE_EMPTY);
        ink_syntax_tree_print_walk(tree, tree->root, "", INK_SYNTAX_TREE_EMPTY);
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
    tree->source = source;
    tree->root = NULL;
    return 0;
}

void ink_syntax_tree_cleanup(struct ink_syntax_tree *tree)
{
}
