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
    T(NODE_FILE, "File")                                                       \
    T(NODE_ADD_EXPR, "AddExpr")                                                \
    T(NODE_AND_EXPR, "AndExpr")                                                \
    T(NODE_ASSIGN_EXPR, "AssignExpr")                                          \
    T(NODE_BLOCK_STMT, "BlockStmt")                                            \
    T(NODE_BRACE_EXPR, "BraceExpr")                                            \
    T(NODE_CHOICE_STMT, "ChoiceStmt")                                          \
    T(NODE_CHOICE_PLUS_BRANCH, "ChoicePlusBranch")                             \
    T(NODE_CHOICE_STAR_BRANCH, "ChoiceStarBranch")                             \
    T(NODE_CHOICE_CONTENT_EXPR, "ChoiceContentExpr")                           \
    T(NODE_CONST_DECL, "ConstDecl")                                            \
    T(NODE_CONTENT_EXPR, "ContentExpr")                                        \
    T(NODE_CONTENT_STMT, "ContentStmt")                                        \
    T(NODE_LABELLED_CHOICE_BRANCH, "LabelledChoiceBranch")                     \
    T(NODE_CONDITIONAL_CHOICE_BRANCH, "ConditionalChoiceBranch")               \
    T(NODE_IDENTIFIER_EXPR, "IdentifierExpr")                                  \
    T(NODE_DIV_EXPR, "DivideExpr")                                             \
    T(NODE_EQUAL_EXPR, "LogicalEqualityExpr")                                  \
    T(NODE_FALSE_EXPR, "FalseExpr")                                            \
    T(NODE_GATHERED_CHOICE_STMT, "GatheredChoiceStmt")                         \
    T(NODE_GATHER_STMT, "GatherStmt")                                          \
    T(NODE_GREATER_EXPR, "LogicalGreaterExpr")                                 \
    T(NODE_GREATER_EQUAL_EXPR, "LogicalGreaterOrEqualExpr")                    \
    T(NODE_LESS_EQUAL_EXPR, "LogicalLesserOrEqualExpr")                        \
    T(NODE_LESS_EXPR, "LogicalLesserExpr")                                     \
    T(NODE_MUL_EXPR, "MultiplyExpr")                                           \
    T(NODE_MOD_EXPR, "ModExpr")                                                \
    T(NODE_NEGATE_EXPR, "NegateExpr")                                          \
    T(NODE_NOT_EQUAL_EXPR, "LogicalInequalityExpr")                            \
    T(NODE_NOT_EXPR, "NotExpr")                                                \
    T(NODE_NUMBER_EXPR, "NumberExpr")                                          \
    T(NODE_OR_EXPR, "OrExpr")                                                  \
    T(NODE_SEQUENCE_EXPR, "SequenceExpr")                                      \
    T(NODE_STRING_EXPR, "StringExpr")                                          \
    T(NODE_SUB_EXPR, "SubtractExpr")                                           \
    T(NODE_TRUE_EXPR, "TrueExpr")                                              \
    T(NODE_VAR_DECL, "VarDecl")                                                \
    T(NODE_INVALID, "Invalid")

#define T(name, description) INK_##name,
enum ink_syntax_node_type {
    INK_NODE(T)
};
#undef T

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
    const struct ink_source *source;
    struct ink_token_buffer tokens;
    struct ink_syntax_node *root;
};

extern const char *ink_token_type_strz(enum ink_token_type type);
extern const char *ink_syntax_node_type_strz(enum ink_syntax_node_type type);
extern void ink_token_print(const struct ink_source *source,
                            const struct ink_token *token);

extern void ink_token_buffer_initialize(struct ink_token_buffer *buffer);
extern int ink_token_buffer_reserve(struct ink_token_buffer *buffer,
                                    size_t count);
extern int ink_token_buffer_append(struct ink_token_buffer *buffer,
                                   struct ink_token token);
extern void ink_token_buffer_cleanup(struct ink_token_buffer *buffer);
extern void ink_token_buffer_print(const struct ink_source *source,
                                   const struct ink_token_buffer *buffer);

extern struct ink_syntax_node *
ink_syntax_node_new(struct ink_arena *arena, enum ink_syntax_node_type type,
                    size_t token_start, size_t token_end,
                    struct ink_syntax_node *lhs, struct ink_syntax_node *rhs,
                    struct ink_syntax_seq *seq);

extern int ink_syntax_tree_initialize(const struct ink_source *source,
                                      struct ink_syntax_tree *tree);
extern void ink_syntax_tree_cleanup(struct ink_syntax_tree *tree);
extern void ink_syntax_tree_print(const struct ink_syntax_tree *tree);

#ifdef __cplusplus
}
#endif

#endif
