#ifndef __INK_SYNTAX_TREE_H__
#define __INK_SYNTAX_TREE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "lex.h"
#include "source.h"

#define INK_TOKEN_STREAM_MIN_COUNT 16
#define INK_TOKEN_STREAM_GROWTH_FACTOR 2
#define INK_SCRATCH_MIN_COUNT 16
#define INK_SCRATCH_GROWTH_FACTOR 2

struct ink_arena;
struct ink_syntax_node;

#define INK_NODE(T)                                                            \
    T(NODE_FILE, "NODE_FILE")                                                  \
    T(NODE_NUMBER_LITERAL, "NODE_NUMBER_LITERAL")                              \
    T(NODE_STRING_LITERAL, "NODE_STRING_LITERAL")                              \
    T(NODE_CONTENT_STMT, "NODE_CONTENT_STMT")                                  \
    T(NODE_CONTENT_EXPR, "NODE_CONTENT_EXPR")                                  \
    T(NODE_BRACE_EXPR, "NODE_BRACE_EXPR")                                      \
    T(NODE_SEQUENCE_EXPR, "NODE_SEQUENCE_EXPR")

#define T(name, description) INK_##name,
enum ink_syntax_node_type {
    INK_NODE(T)
};
#undef T

/**
 * Scratch buffer for syntax tree nodes.
 *
 * Used to assist in the creation of syntax tree node sequences, avoiding the
 * need for a dynamic array.
 */
struct ink_scratch_buffer {
    size_t count;
    size_t capacity;
    struct ink_syntax_node **entries;
};

/**
 * Sequence of syntax tree nodes.
 */
struct ink_syntax_seq {
    size_t count;
    struct ink_syntax_node *nodes[1];
};

/**
 * Syntax tree node.
 *
 * Nodes do not directly store token information, instead opting to reference
 * them by index. A single node can span a range of tokens within the tokenized
 * buffer.
 */
struct ink_syntax_node {
    /* Type of the syntax tree node */
    enum ink_syntax_node_type type;

    /* Index into the tokenized buffer for the starting token. */
    size_t start_token;

    /* Index into the tokenized buffer for the closing token. */
    size_t end_token;

    /* Left-hand side for the grammar production. */
    struct ink_syntax_node *lhs;

    /* Right-hand side for the grammar production. */
    struct ink_syntax_node *rhs;

    /* TODO(Brett): Temporary? */
    struct ink_syntax_seq *seq;
};

/**
 * Tokenized buffer.
 */
struct ink_token_buffer {
    size_t count;
    size_t capacity;
    struct ink_token *entries;
};

/**
 * Syntax Tree.
 *
 * The syntax tree's memory is arranged for reasonably efficient storage.
 */
struct ink_syntax_tree {
    struct ink_source *source;
    struct ink_token_buffer tokens;
    struct ink_syntax_node *root;
};

extern int ink_token_buffer_reserve(struct ink_token_buffer *stream,
                                    size_t count);
extern int ink_token_buffer_append(struct ink_token_buffer *stream,
                                   struct ink_token token);
extern void ink_token_buffer_cleanup(struct ink_token_buffer *stream);

extern void ink_token_buffer_print(const struct ink_source *source,
                                   const struct ink_token_buffer *buffer);

extern int ink_scratch_reserve(struct ink_scratch_buffer *scratch,
                               size_t item_count);
extern void ink_scratch_append(struct ink_scratch_buffer *scratch,
                               struct ink_syntax_node *node);
extern void ink_scratch_cleanup(struct ink_scratch_buffer *scratch);

extern struct ink_syntax_seq *
ink_seq_from_scratch(struct ink_arena *arena,
                     struct ink_scratch_buffer *scratch, size_t start_offset,
                     size_t end_offset);
extern struct ink_syntax_node *
ink_syntax_node_new(struct ink_arena *arena, enum ink_syntax_node_type type,
                    size_t token_start, size_t token_end,
                    struct ink_syntax_node *lhs, struct ink_syntax_node *rhs,
                    struct ink_syntax_seq *seq);
extern struct ink_syntax_seq *
ink_syntax_seq_new(struct ink_arena *arena, struct ink_scratch_buffer *scratch,
                   size_t start_offset, size_t end_offset);

extern int ink_syntax_tree_initialize(struct ink_source *source,
                                      struct ink_syntax_tree *tree);
extern void ink_syntax_tree_cleanup(struct ink_syntax_tree *tree);
extern void ink_syntax_tree_print(const struct ink_syntax_tree *tree);

#ifdef __cplusplus
}
#endif

#endif
