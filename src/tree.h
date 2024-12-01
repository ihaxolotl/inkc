#ifndef __INK_SYNTAX_TREE_H__
#define __INK_SYNTAX_TREE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

#include "source.h"
#include "vec.h"

struct ink_arena;
struct ink_syntax_node;

#define INK_NODE(T)                                                            \
    T(NODE_FILE, "File")                                                       \
    T(NODE_ADD_EXPR, "AddExpr")                                                \
    T(NODE_AND_EXPR, "AndExpr")                                                \
    T(NODE_ARG_LIST, "ArgumentList")                                           \
    T(NODE_ASSIGN_EXPR, "AssignExpr")                                          \
    T(NODE_BLOCK_STMT, "CompoundStmt")                                         \
    T(NODE_CALL_EXPR, "CallExpr")                                              \
    T(NODE_CHOICE_STMT, "ChoiceStmt")                                          \
    T(NODE_CHOICE_PLUS_STMT, "ChoicePlusStmt")                                 \
    T(NODE_CHOICE_STAR_STMT, "ChoiceStarStmt")                                 \
    T(NODE_CHOICE_CONTENT_EXPR, "ChoiceContentExpr")                           \
    T(NODE_CONDITIONAL_STMT, "ConditionalStmt")                                \
    T(NODE_CONDITIONAL_BRANCH, "ConditionalBranchStmt")                        \
    T(NODE_CONTAINS_EXPR, "ContainsExpr")                                      \
    T(NODE_CONST_DECL, "ConstDecl")                                            \
    T(NODE_CONTENT_EXPR, "ContentExpr")                                        \
    T(NODE_CONTENT_STMT, "ContentStmt")                                        \
    T(NODE_IDENTIFIER_EXPR, "Name")                                            \
    T(NODE_DIV_EXPR, "DivideExpr")                                             \
    T(NODE_DIVERT, "Divert")                                                   \
    T(NODE_DIVERT_EXPR, "DivertExpr")                                          \
    T(NODE_DIVERT_STMT, "DivertStmt")                                          \
    T(NODE_EQUAL_EXPR, "LogicalEqualityExpr")                                  \
    T(NODE_LOGIC_STMT, "LogicStmt")                                            \
    T(NODE_FALSE_EXPR, "False")                                                \
    T(NODE_FUNCTION_DECL, "FunctionDecl")                                      \
    T(NODE_GATHER_STMT, "GatherStmt")                                          \
    T(NODE_GREATER_EXPR, "LogicalGreaterExpr")                                 \
    T(NODE_GREATER_EQUAL_EXPR, "LogicalGreaterOrEqualExpr")                    \
    T(NODE_KNOT_DECL, "KnotDecl")                                              \
    T(NODE_LESS_EQUAL_EXPR, "LogicalLesserOrEqualExpr")                        \
    T(NODE_LESS_EXPR, "LogicalLesserExpr")                                     \
    T(NODE_LOGIC_EXPR, "LogicExpr")                                            \
    T(NODE_MUL_EXPR, "MultiplyExpr")                                           \
    T(NODE_MOD_EXPR, "ModExpr")                                                \
    T(NODE_NEGATE_EXPR, "NegateExpr")                                          \
    T(NODE_NOT_EQUAL_EXPR, "LogicalInequalityExpr")                            \
    T(NODE_NOT_EXPR, "NotExpr")                                                \
    T(NODE_NUMBER_EXPR, "NumberLiteral")                                       \
    T(NODE_OR_EXPR, "OrExpr")                                                  \
    T(NODE_PARAM_LIST, "ParamList")                                            \
    T(NODE_PARAM_DECL, "ParamDecl")                                            \
    T(NODE_REF_PARAM_DECL, "ParamRefDecl")                                     \
    T(NODE_RETURN_STMT, "ReturnStmt")                                          \
    T(NODE_SEQUENCE_EXPR, "SequenceExpr")                                      \
    T(NODE_STRING_EXPR, "StringExpr")                                          \
    T(NODE_STRING_LITERAL, "StringLiteral")                                    \
    T(NODE_SUB_EXPR, "SubtractExpr")                                           \
    T(NODE_TEMP_STMT, "TempStmt")                                              \
    T(NODE_THREAD_EXPR, "ThreadExpr")                                          \
    T(NODE_THREAD_STMT, "ThreadStmt")                                          \
    T(NODE_TRUE_EXPR, "True")                                                  \
    T(NODE_TUNNEL_STMT, "TunnelStmt")                                          \
    T(NODE_TUNNEL_ONWARDS, "TunnelOnwards")                                    \
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
 * source positions by index.
 *
 * TODO(Brett): Pack node data to reduce node size?
 */
struct ink_syntax_node {
    enum ink_syntax_node_type type;
    size_t start_offset;
    size_t end_offset;
    struct ink_syntax_node *lhs;
    struct ink_syntax_node *rhs;
    struct ink_syntax_seq *seq;
};

/**
 * Syntax Tree.
 *
 * The syntax tree's memory is arranged for reasonably efficient
 * storage.
 */
struct ink_syntax_tree {
    const struct ink_source *source;
    struct ink_syntax_node *root;
};

extern const char *ink_syntax_node_type_strz(enum ink_syntax_node_type type);

extern struct ink_syntax_node *
ink_syntax_node_new(struct ink_arena *arena, enum ink_syntax_node_type type,
                    size_t start_offset, size_t end_offset,
                    struct ink_syntax_node *lhs, struct ink_syntax_node *rhs,
                    struct ink_syntax_seq *seq);

extern int ink_syntax_tree_initialize(const struct ink_source *source,
                                      struct ink_syntax_tree *tree);
extern void ink_syntax_tree_cleanup(struct ink_syntax_tree *tree);
extern void ink_syntax_tree_print(const struct ink_syntax_tree *tree,
                                  bool colors);

#ifdef __cplusplus
}
#endif

#endif
