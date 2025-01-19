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

enum ink_syntax_error_type {
    INK_SYNTAX_OK = 0,
};

struct ink_syntax_error {
    enum ink_syntax_error_type type;
    size_t source_start;
    size_t source_end;
};

INK_VEC_DECLARE(ink_syntax_error_vec, struct ink_syntax_error)

#define INK_NODE(T)                                                            \
    T(NODE_FILE, "File")                                                       \
    T(NODE_ADD_EXPR, "AddExpr")                                                \
    T(NODE_AND_EXPR, "AndExpr")                                                \
    T(NODE_ARG_LIST, "ArgumentList")                                           \
    T(NODE_ASSIGN_EXPR, "AssignExpr")                                          \
    T(NODE_BLOCK, "BlockStmt")                                                 \
    T(NODE_CALL_EXPR, "CallExpr")                                              \
    T(NODE_CHOICE_PLUS_STMT, "ChoicePlusStmt")                                 \
    T(NODE_CHOICE_STAR_STMT, "ChoiceStarStmt")                                 \
    T(NODE_CHOICE_STMT, "ChoiceStmt")                                          \
    T(NODE_CHOICE_EXPR, "ChoiceContentExpr")                                   \
    T(NODE_CHOICE_START_EXPR, "ChoiceStartContentExpr")                        \
    T(NODE_CHOICE_OPTION_EXPR, "ChoiceOptionOnlyContentExpr")                  \
    T(NODE_CHOICE_INNER_EXPR, "ChoiceInnerContentExpr")                        \
    T(NODE_CONDITIONAL_BRANCH, "ConditionalBranch")                            \
    T(NODE_CONDITIONAL_CONTENT, "ConditionalContent")                          \
    T(NODE_CONDITIONAL_ELSE_BRANCH, "ConditionalElseBranch")                   \
    T(NODE_CONTAINS_EXPR, "ContainsExpr")                                      \
    T(NODE_CONST_DECL, "ConstDecl")                                            \
    T(NODE_CONTENT, "Content")                                                 \
    T(NODE_CONTENT_STMT, "ContentStmt")                                        \
    T(NODE_IDENTIFIER, "Identifier")                                           \
    T(NODE_DIV_EXPR, "DivideExpr")                                             \
    T(NODE_DIVERT, "Divert")                                                   \
    T(NODE_DIVERT_EXPR, "DivertExpr")                                          \
    T(NODE_DIVERT_STMT, "DivertStmt")                                          \
    T(NODE_EMPTY_CONTENT, "EmptyContent")                                      \
    T(NODE_EQUAL_EXPR, "LogicalEqualityExpr")                                  \
    T(NODE_FALSE, "False")                                                     \
    T(NODE_GATHER_STMT, "GatherStmt")                                          \
    T(NODE_GATHERED_CHOICE_STMT, "GatheredChoiceStmt")                         \
    T(NODE_GREATER_EXPR, "LogicalGreaterExpr")                                 \
    T(NODE_GREATER_EQUAL_EXPR, "LogicalGreaterOrEqualExpr")                    \
    T(NODE_INLINE_LOGIC, "InlineLogicExpr")                                    \
    T(NODE_KNOT_DECL, "KnotDecl")                                              \
    T(NODE_KNOT_PROTO, "KnotProto")                                            \
    T(NODE_LESS_EQUAL_EXPR, "LogicalLesserOrEqualExpr")                        \
    T(NODE_LESS_EXPR, "LogicalLesserExpr")                                     \
    T(NODE_LIST_DECL, "ListDecl")                                              \
    T(NODE_LOGIC_STMT, "LogicStmt")                                            \
    T(NODE_MUL_EXPR, "MultiplyExpr")                                           \
    T(NODE_MOD_EXPR, "ModExpr")                                                \
    T(NODE_NEGATE_EXPR, "NegateExpr")                                          \
    T(NODE_NOT_EQUAL_EXPR, "LogicalInequalityExpr")                            \
    T(NODE_NOT_EXPR, "NotExpr")                                                \
    T(NODE_NUMBER, "NumberLiteral")                                            \
    T(NODE_OR_EXPR, "OrExpr")                                                  \
    T(NODE_PARAM_LIST, "ParamList")                                            \
    T(NODE_PARAM_DECL, "ParamDecl")                                            \
    T(NODE_REF_PARAM_DECL, "ParamRefDecl")                                     \
    T(NODE_RETURN_STMT, "ReturnStmt")                                          \
    T(NODE_SELECTED_LIST_ELEMENT, "SelectionListElementExpr")                  \
    T(NODE_SEQUENCE_EXPR, "SequenceExpr")                                      \
    T(NODE_STITCH_DECL, "StitchDecl")                                          \
    T(NODE_STITCH_PROTO, "StitchProto")                                        \
    T(NODE_STRING_EXPR, "StringExpr")                                          \
    T(NODE_STRING, "StringLiteral")                                            \
    T(NODE_SUB_EXPR, "SubtractExpr")                                           \
    T(NODE_TEMP_STMT, "TempStmt")                                              \
    T(NODE_THREAD_EXPR, "ThreadExpr")                                          \
    T(NODE_THREAD_STMT, "ThreadStmt")                                          \
    T(NODE_TRUE, "True")                                                       \
    T(NODE_TUNNEL_STMT, "TunnelStmt")                                          \
    T(NODE_TUNNEL_ONWARDS, "TunnelOnwards")                                    \
    T(NODE_VAR_DECL, "VarDecl")                                                \
    T(NODE_INVALID, "Invalid")

#define T(name, description) INK_##name,
enum ink_syntax_node_type {
    INK_NODE(T)
};
#undef T

enum ink_syntax_node_flags {
    INK_NODE_F_ERROR = (1 << 0),
    INK_NODE_F_FUNCTION = (1 << 1),
    INK_NODE_F_SEQ_STOPPING = (1 << 2),
    INK_NODE_F_SEQ_CYCLE = (1 << 3),
    INK_NODE_F_SEQ_SHUFFLE = (1 << 4),
    INK_NODE_F_SEQ_ONCE = (1 << 5),
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
 * The syntax tree's memory is arranged for reasonably efficient
 * storage. Nodes do not directly store token information, instead opting to
 * reference source positions by index.
 *
 * TODO(Brett): Pack node data to reduce node size?
 */
struct ink_syntax_node {
    enum ink_syntax_node_type type;
    int flags;
    size_t start_offset;
    size_t end_offset;
    struct ink_syntax_node *lhs;
    struct ink_syntax_node *rhs;
    struct ink_syntax_seq *seq;
};

/**
 * Syntax Tree.
 */
struct ink_syntax_tree {
    const struct ink_source *source;
    struct ink_syntax_node *root;
    struct ink_syntax_error_vec errors;
};

extern const char *ink_syntax_node_type_strz(enum ink_syntax_node_type type);

extern struct ink_syntax_node *
ink_syntax_node_new(enum ink_syntax_node_type type, size_t start_offset,
                    size_t end_offset, struct ink_syntax_node *lhs,
                    struct ink_syntax_node *rhs, struct ink_syntax_seq *seq,
                    struct ink_arena *arena);

extern int ink_syntax_tree_initialize(const struct ink_source *source,
                                      struct ink_syntax_tree *tree);
extern void ink_syntax_tree_cleanup(struct ink_syntax_tree *tree);
extern void ink_syntax_tree_print(const struct ink_syntax_tree *tree,
                                  bool colors);

#ifdef __cplusplus
}
#endif

#endif
