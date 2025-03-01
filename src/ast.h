#ifndef __INK_TREE_H__
#define __INK_TREE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

#include "source.h"
#include "vec.h"

struct ink_arena;
struct ink_ast_node;

enum ink_ast_error_type {
    INK_AST_OK = 0,
    INK_AST_E_IDENT_UNKNOWN,
    INK_AST_E_IDENT_REDEFINED,
    INK_AST_E_CONDITIONAL_EMPTY,
    INK_AST_E_ELSE_EXPECTED,
    INK_AST_E_ELSE_MULTIPLE,
    INK_AST_E_ELSE_FINAL,
    INK_AST_E_ARGS_TOO_FEW,
    INK_AST_E_ARGS_TOO_MANY,
};

struct ink_ast_error {
    enum ink_ast_error_type type;
    size_t source_start;
    size_t source_end;
};

INK_VEC_T(ink_ast_error_vec, struct ink_ast_error)

#define INK_MAKE_AST_NODES(T)                                                  \
    T(AST_FILE, "File")                                                        \
    T(AST_ADD_EXPR, "AddExpr")                                                 \
    T(AST_AND_EXPR, "AndExpr")                                                 \
    T(AST_ARG_LIST, "ArgumentList")                                            \
    T(AST_ASSIGN_STMT, "AssignStmt")                                           \
    T(AST_BLOCK, "BlockStmt")                                                  \
    T(AST_CALL_EXPR, "CallExpr")                                               \
    T(AST_CHOICE_PLUS_STMT, "ChoicePlusStmt")                                  \
    T(AST_CHOICE_STAR_STMT, "ChoiceStarStmt")                                  \
    T(AST_CHOICE_STMT, "ChoiceStmt")                                           \
    T(AST_CHOICE_EXPR, "ChoiceContentExpr")                                    \
    T(AST_CHOICE_START_EXPR, "ChoiceStartContentExpr")                         \
    T(AST_CHOICE_OPTION_EXPR, "ChoiceOptionOnlyContentExpr")                   \
    T(AST_CHOICE_INNER_EXPR, "ChoiceInnerContentExpr")                         \
    T(AST_MULTI_IF_STMT, "MultiIfStmt")                                        \
    T(AST_IF_BRANCH, "IfBranch")                                               \
    T(AST_IF_EXPR, "IfExpr")                                                   \
    T(AST_IF_STMT, "IfStmt")                                                   \
    T(AST_ELSE_BRANCH, "ElseBranch")                                           \
    T(AST_SWITCH_STMT, "SwitchStmt")                                           \
    T(AST_SWITCH_CASE, "SwitchCase")                                           \
    T(AST_CONTAINS_EXPR, "ContainsExpr")                                       \
    T(AST_CONST_DECL, "ConstDecl")                                             \
    T(AST_CONTENT, "Content")                                                  \
    T(AST_CONTENT_STMT, "ContentStmt")                                         \
    T(AST_DIV_EXPR, "DivideExpr")                                              \
    T(AST_DIVERT, "Divert")                                                    \
    T(AST_DIVERT_EXPR, "DivertExpr")                                           \
    T(AST_DIVERT_STMT, "DivertStmt")                                           \
    T(AST_EMPTY_CONTENT, "EmptyContent")                                       \
    T(AST_EQUAL_EXPR, "LogicalEqualityExpr")                                   \
    T(AST_EXPR_STMT, "ExprStmt")                                               \
    T(AST_FALSE, "False")                                                      \
    T(AST_FUNC_DECL, "FunctionDecl")                                           \
    T(AST_FUNC_PROTO, "FunctionProto")                                         \
    T(AST_GATHER_STMT, "GatherStmt")                                           \
    T(AST_GATHERED_CHOICE_STMT, "GatheredChoiceStmt")                          \
    T(AST_GLUE, "GlueExpr")                                                    \
    T(AST_GREATER_EXPR, "LogicalGreaterExpr")                                  \
    T(AST_GREATER_EQUAL_EXPR, "LogicalGreaterOrEqualExpr")                     \
    T(AST_IDENTIFIER, "Identifier")                                            \
    T(AST_INLINE_LOGIC, "InlineLogicExpr")                                     \
    T(AST_KNOT_DECL, "KnotDecl")                                               \
    T(AST_KNOT_PROTO, "KnotProto")                                             \
    T(AST_LESS_EQUAL_EXPR, "LogicalLesserOrEqualExpr")                         \
    T(AST_LESS_EXPR, "LogicalLesserExpr")                                      \
    T(AST_LIST_DECL, "ListDecl")                                               \
    T(AST_MUL_EXPR, "MultiplyExpr")                                            \
    T(AST_MOD_EXPR, "ModExpr")                                                 \
    T(AST_NEGATE_EXPR, "NegateExpr")                                           \
    T(AST_NOT_EQUAL_EXPR, "LogicalInequalityExpr")                             \
    T(AST_NOT_EXPR, "NotExpr")                                                 \
    T(AST_NUMBER, "NumberLiteral")                                             \
    T(AST_OR_EXPR, "OrExpr")                                                   \
    T(AST_PARAM_DECL, "ParamDecl")                                             \
    T(AST_PARAM_LIST, "ParamList")                                             \
    T(AST_REF_PARAM_DECL, "ParamRefDecl")                                      \
    T(AST_RETURN_STMT, "ReturnStmt")                                           \
    T(AST_SELECTED_LIST_ELEMENT, "SelectionListElementExpr")                   \
    T(AST_SELECTOR_EXPR, "SelectorExpr")                                       \
    T(AST_SEQUENCE_EXPR, "SequenceExpr")                                       \
    T(AST_STITCH_DECL, "StitchDecl")                                           \
    T(AST_STITCH_PROTO, "StitchProto")                                         \
    T(AST_STRING_EXPR, "StringExpr")                                           \
    T(AST_STRING, "StringLiteral")                                             \
    T(AST_SUB_EXPR, "SubtractExpr")                                            \
    T(AST_TEMP_DECL, "TempDecl")                                               \
    T(AST_THREAD_EXPR, "ThreadExpr")                                           \
    T(AST_THREAD_STMT, "ThreadStmt")                                           \
    T(AST_TRUE, "True")                                                        \
    T(AST_TUNNEL_STMT, "TunnelStmt")                                           \
    T(AST_TUNNEL_ONWARDS, "TunnelOnwards")                                     \
    T(AST_VAR_DECL, "VarDecl")                                                 \
    T(AST_INVALID, "Invalid")

#define T(name, description) INK_##name,
enum ink_ast_node_type {
    INK_MAKE_AST_NODES(T)
};
#undef T

enum ink_syntax_node_flags {
    INK_AST_F_ERROR = (1 << 0),
    INK_AST_F_FUNCTION = (1 << 1),
    INK_AST_F_SEQ_STOPPING = (1 << 2),
    INK_AST_F_SEQ_CYCLE = (1 << 3),
    INK_AST_F_SEQ_SHUFFLE = (1 << 4),
    INK_AST_F_SEQ_ONCE = (1 << 5),
};

/**
 * Sequence of syntax tree nodes.
 */
struct ink_ast_seq {
    size_t count;
    struct ink_ast_node *nodes[1];
};

/**
 * Abstract syntax tree node.
 *
 * Each node's memory is arranged for reasonably efficient storage. Nodes do
 * not directly store token information, instead opting to reference source
 * positions by index.
 */
struct ink_ast_node {
    enum ink_ast_node_type type;
    int flags;
    size_t start_offset;
    size_t end_offset;
    struct ink_ast_node *lhs;
    struct ink_ast_node *rhs;
    struct ink_ast_seq *seq;
};

/**
 * Abstract syntax tree.
 */
struct ink_ast {
    const uint8_t *filename;         /* Source filename. NULL-Terminated. */
    const uint8_t *source_bytes;     /* Source code bytes. NULL-Terminated. */
    struct ink_ast_node *root;       /* Root node for the tree. */
    struct ink_ast_error_vec errors; /* Syntax errors. */
};

extern const char *ink_ast_node_type_strz(enum ink_ast_node_type type);

extern struct ink_ast_node *
ink_ast_node_new(enum ink_ast_node_type type, size_t start_offset,
                 size_t end_offset, struct ink_ast_node *lhs,
                 struct ink_ast_node *rhs, struct ink_ast_seq *seq,
                 struct ink_arena *arena);

extern void ink_ast_init(struct ink_ast *tree, const uint8_t *filename,
                         const uint8_t *source_bytes);
extern void ink_ast_deinit(struct ink_ast *tree);
extern void ink_ast_print(const struct ink_ast *tree, bool colors);
extern void ink_ast_render_errors(const struct ink_ast *tree);

#ifdef __cplusplus
}
#endif

#endif
