#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "astgen.h"
#include "common.h"
#include "hashmap.h"
#include "ir.h"
#include "tree.h"
#include "vec.h"

#define INK_NUMBER_BUFSZ 24
#define INK_IR_INVALID (size_t)(-1)

struct ink_symbol {
    bool is_const;
    size_t index;
    const struct ink_ast_node *node;
};

struct ink_symtab_key {
    const uint8_t *bytes;
    size_t length;
};

INK_VEC_T(ink_astgen_scratch, size_t)
INK_HASHMAP_T(ink_symtab, struct ink_symtab_key, struct ink_symbol)

static const char *INK_DEFAULT_PATH = "@main";

static uint32_t ink_symtab_hash(const void *bytes, size_t length)
{
    const struct ink_symtab_key *const key = bytes;

    return ink_fnv32a(key->bytes, key->length);
}

static bool ink_symtab_cmp(const void *a, size_t alen, const void *b,
                           size_t blen)
{
    const struct ink_symtab_key *const key_1 = a;
    const struct ink_symtab_key *const key_2 = b;

    return (key_1->length == key_2->length) &&
           memcmp(key_1->bytes, key_2->bytes, key_1->length) == 0;
}

/**
 * Symbol scope chain.
 */
struct ink_scope {
    struct ink_scope *parent;
    struct ink_symtab symbol_table;
};

/**
 * Initialize a symbol scope.
 */
static void ink_scope_init(struct ink_scope *scope,
                           struct ink_scope *parent_scope)
{
    scope->parent = parent_scope;

    ink_symtab_init(&scope->symbol_table, 80ul, ink_symtab_hash,
                    ink_symtab_cmp);
}

/**
 * Cleanup a symbol scope.
 */
static void ink_scope_deinit(struct ink_scope *scope)
{
    ink_symtab_deinit(&scope->symbol_table);
}

/**
 * Insert a name relative to a scope.
 *
 * Will fail if the name already exists or an internal error occurs.
 */
static int ink_scope_insert(struct ink_scope *scope, const struct ink_ast *tree,
                            const struct ink_ast_node *node,
                            struct ink_symbol sym)
{
    const struct ink_symtab_key key = {
        .bytes = &tree->source_bytes[node->start_offset],
        .length = node->end_offset - node->start_offset,
    };

    return ink_symtab_insert(&scope->symbol_table, key, sym);
}

/**
 * Lookup a name relative to a scope.
 *
 * The scope chain is traversed recursively until a match is found or if the
 * lookup fails.
 */
static int ink_scope_lookup(struct ink_scope *scope, const struct ink_ast *tree,
                            const struct ink_ast_node *node,
                            struct ink_symbol *sym)
{
    int rc = -1;
    const struct ink_symtab_key key = {
        .bytes = &tree->source_bytes[node->start_offset],
        .length = node->end_offset - node->start_offset,
    };

    for (;;) {
        if (!scope) {
            return rc;
        }

        rc = ink_symtab_lookup(&scope->symbol_table, key, sym);
        if (rc < 0) {
            scope = scope->parent;
        } else {
            return rc;
        }
    }
}

struct ink_astgen_global {
    struct ink_ast *tree;
    struct ink_ir *ircode;
    struct ink_astgen_scratch scratch;
};

static void ink_astgen_global_init(struct ink_astgen_global *global,
                                   struct ink_ast *tree, struct ink_ir *ircode)
{
    global->tree = tree;
    global->ircode = ircode;

    ink_astgen_scratch_init(&global->scratch);
}

static void ink_astgen_global_deinit(struct ink_astgen_global *global)
{
    ink_astgen_scratch_deinit(&global->scratch);
}

struct ink_astgen {
    size_t scratch_offset;
    struct ink_astgen_scratch *scratch;
    struct ink_astgen_global *global;
};

static void ink_astgen_make(struct ink_astgen *astgen,
                            struct ink_astgen *parent)
{
    astgen->scratch_offset = parent->scratch->count;
    astgen->scratch = parent->scratch;
    astgen->global = parent->global;
}

static struct ink_ir_inst_seq *
ink_astgen_seq_from_scratch(struct ink_astgen *astgen, size_t start_offset,
                            size_t end_offset)
{
    struct ink_ir_inst_seq *seq = NULL;
    struct ink_astgen_global *const global = astgen->global;
    struct ink_astgen_scratch *const scratch = astgen->scratch;
    struct ink_ir *const ircode = global->ircode;

    if (start_offset < end_offset) {
        const size_t span = end_offset - start_offset;

        assert(span > 0);

        seq = ink_malloc(sizeof(*seq) + span * sizeof(seq->entries));
        if (seq == NULL) {
            return NULL;
        }
        for (size_t i = start_offset, j = 0; i < end_offset; i++) {
            seq->entries[j++] = scratch->entries[i];
        }

        seq->next = NULL;
        seq->count = span;

        if (ircode->sequence_list_head == NULL) {
            ircode->sequence_list_head = seq;
            ircode->sequence_list_tail = seq;
        } else {
            ircode->sequence_list_tail->next = seq;
            ircode->sequence_list_tail = seq;
        }

        ink_astgen_scratch_shrink(scratch, start_offset);
    }
    return seq;
}

static size_t ink_astgen_error(struct ink_astgen *astgen,
                               enum ink_ast_error_type type,
                               const struct ink_ast_node *node)
{
    struct ink_astgen_global *const global = astgen->global;
    const struct ink_ast_error err = {
        .type = type,
        .source_start = node->start_offset,
        .source_end = node->end_offset,
    };

    ink_ast_error_vec_push(&global->tree->errors, err);
    return INK_IR_INVALID;
}

/**
 * Push a new instruction to the IR instructions buffer.
 *
 * Returns an instruction index.
 */
static size_t ink_astgen_add_inst(struct ink_astgen *astgen,
                                  struct ink_ir_inst inst)
{
    struct ink_astgen_global *const global = astgen->global;
    struct ink_astgen_scratch *const scratch = astgen->scratch;
    struct ink_ir_inst_vec *const code = &global->ircode->instructions;
    const size_t index = code->count;

    ink_ir_inst_vec_push(code, inst);
    ink_astgen_scratch_push(scratch, index);
    return index;
}

/**
 * Create a simple operation.
 */
static size_t ink_astgen_add_simple(struct ink_astgen *astgen,
                                    enum ink_ir_inst_op op)
{
    const struct ink_ir_inst inst = {
        .op = op,
    };
    return ink_astgen_add_inst(astgen, inst);
}

/**
 * Create a unary operation.
 */
static size_t ink_astgen_add_unary(struct ink_astgen *astgen,
                                   enum ink_ir_inst_op op, size_t lhs)
{
    const struct ink_ir_inst inst = {
        .op = op,
        .as.unary =
            {
                .lhs = lhs,
            },
    };
    return ink_astgen_add_inst(astgen, inst);
}

/**
 * Create a binary operation.
 */
static size_t ink_astgen_add_binary(struct ink_astgen *astgen,
                                    enum ink_ir_inst_op op, size_t lhs,
                                    size_t rhs)
{
    const struct ink_ir_inst inst = {
        .op = op,
        .as.binary =
            {
                .lhs = lhs,
                .rhs = rhs,
            },
    };
    return ink_astgen_add_inst(astgen, inst);
}

/**
 * Create a dominant block.
 */
static size_t ink_astgen_add_block(struct ink_astgen *astgen,
                                   enum ink_ir_inst_op op)
{
    struct ink_astgen_global *const global = astgen->global;
    struct ink_astgen_scratch *const scratch = astgen->scratch;
    struct ink_ir_inst_vec *const code = &global->ircode->instructions;
    const size_t index = code->count;
    const struct ink_ir_inst inst = {
        .op = op,
    };

    ink_astgen_add_inst(astgen, inst);
    ink_astgen_scratch_pop(scratch, NULL);
    return index;
}

static void ink_astgen_set_block(struct ink_astgen *astgen, size_t index,
                                 size_t scratch_top)
{
    struct ink_astgen_global *const global = astgen->global;
    struct ink_astgen_scratch *const scratch = astgen->scratch;
    struct ink_ir_inst_vec *const code = &global->ircode->instructions;
    struct ink_ir_inst *const inst = &code->entries[index];

    inst->as.block.seq =
        ink_astgen_seq_from_scratch(astgen, scratch_top, scratch->count);
}

/**
 * Create a conditional branching operation.
 */
static size_t ink_astgen_add_condbr(struct ink_astgen *astgen,
                                    size_t payload_index)
{
    const struct ink_ir_inst inst = {
        .op = INK_IR_INST_CONDBR,
        .as.cond_br.payload_index = payload_index,
    };
    return ink_astgen_add_inst(astgen, inst);
}

static void ink_astgen_set_condbr(struct ink_astgen *astgen, size_t index,
                                  const struct ink_astgen *then_ctx,
                                  const struct ink_astgen *else_ctx)
{
    struct ink_astgen_global *const global = astgen->global;
    struct ink_astgen_scratch *const scratch = astgen->scratch;
    struct ink_ir_inst_vec *const code = &global->ircode->instructions;
    struct ink_ir_inst *const inst = &code->entries[index];

    inst->as.cond_br.else_ = ink_astgen_seq_from_scratch(
        astgen, else_ctx->scratch_offset, scratch->count);
    inst->as.cond_br.then_ = ink_astgen_seq_from_scratch(
        astgen, then_ctx->scratch_offset, scratch->count);
}

/**
 * Create a breaking operation.
 */
static size_t ink_astgen_add_br(struct ink_astgen *astgen, size_t payload_index)
{
    const struct ink_ir_inst inst = {
        .op = INK_IR_INST_BR,
        .as.unary.lhs = payload_index,
    };
    return ink_astgen_add_inst(astgen, inst);
}

/**
 * Create a content path declaration.
 */
static size_t ink_astgen_add_knot(struct ink_astgen *astgen,
                                  enum ink_ir_inst_op op, size_t name_index)
{
    struct ink_astgen_global *const global = astgen->global;
    struct ink_astgen_scratch *const scratch = astgen->scratch;
    struct ink_ir_inst_vec *const code = &global->ircode->instructions;
    const size_t index = code->count;
    const struct ink_ir_inst inst = {
        .op = op,
        .as.knot_decl.name_offset = name_index,
    };

    ink_astgen_add_inst(astgen, inst);
    ink_astgen_scratch_pop(scratch, NULL);
    return index;
}

static void ink_astgen_set_knot(struct ink_astgen *astgen, size_t index,
                                size_t scratch_top)
{
    struct ink_astgen_global *const global = astgen->global;
    struct ink_astgen_scratch *const scratch = astgen->scratch;
    struct ink_ir_inst_vec *const code = &global->ircode->instructions;
    struct ink_ir_inst *const inst = &code->entries[index];

    inst->as.knot_decl.body =
        ink_astgen_seq_from_scratch(astgen, scratch_top, scratch->count);
}

static size_t ink_astgen_add_var(struct ink_astgen *astgen,
                                 enum ink_ir_inst_op op, size_t name_index,
                                 bool is_const)
{
    struct ink_astgen_global *const global = astgen->global;
    struct ink_ir_inst_vec *const code = &global->ircode->instructions;
    const size_t index = code->count;
    const struct ink_ir_inst inst = {
        .op = op,
        .as.var_decl.name_offset = name_index,
        .as.var_decl.is_const = is_const,
    };

    ink_astgen_add_inst(astgen, inst);
    return index;
}

static size_t ink_astgen_add_str(struct ink_astgen *astgen,
                                 const uint8_t *chars, size_t length)
{
    struct ink_astgen_global *const global = astgen->global;
    struct ink_ir_byte_vec *const strings = &global->ircode->string_bytes;
    const size_t pos = strings->count;

    for (size_t i = 0; i < length; i++) {
        ink_ir_byte_vec_push(strings, chars[i]);
    }

    ink_ir_byte_vec_push(strings, '\0');
    return pos;
}

static size_t ink_astgen_expr(struct ink_astgen *, struct ink_scope *,
                              const struct ink_ast_node *);
static void ink_astgen_block_stmt(struct ink_astgen *, struct ink_scope *,
                                  const struct ink_ast_node *);

static size_t ink_astgen_unary_op(struct ink_astgen *astgen,
                                  struct ink_scope *scope,
                                  const struct ink_ast_node *node,
                                  enum ink_ir_inst_op op)
{
    return ink_astgen_add_unary(astgen, op,
                                ink_astgen_expr(astgen, scope, node->lhs));
}

static size_t ink_astgen_binary_op(struct ink_astgen *astgen,
                                   struct ink_scope *scope,
                                   const struct ink_ast_node *node,
                                   enum ink_ir_inst_op op)
{
    const size_t lhs = ink_astgen_expr(astgen, scope, node->lhs);
    const size_t rhs = ink_astgen_expr(astgen, scope, node->rhs);

    return ink_astgen_add_binary(astgen, op, lhs, rhs);
}

static size_t ink_astgen_true(struct ink_astgen *astgen)
{
    const struct ink_ir_inst inst = {
        .op = INK_IR_INST_TRUE,
        .as.boolean = true,
    };
    return ink_astgen_add_inst(astgen, inst);
}

static size_t ink_astgen_false(struct ink_astgen *astgen)
{
    const struct ink_ir_inst inst = {
        .op = INK_IR_INST_TRUE,
        .as.boolean = false,
    };
    return ink_astgen_add_inst(astgen, inst);
}

static size_t ink_astgen_number(struct ink_astgen *astgen,
                                const struct ink_ast_node *node)
{
    char buf[INK_NUMBER_BUFSZ + 1];
    struct ink_ir_inst inst;
    struct ink_astgen_global *const global = astgen->global;
    struct ink_ast *const tree = global->tree;
    const uint8_t *const chars = &tree->source_bytes[node->start_offset];
    const size_t len = node->end_offset - node->start_offset;

    if (len > INK_NUMBER_BUFSZ) {
        return INK_IR_INVALID;
    }

    memcpy(buf, chars, len);
    buf[len] = '\0';
    inst.op = INK_IR_INST_NUMBER;
    inst.as.number = strtod(buf, NULL);
    return ink_astgen_add_inst(astgen, inst);
}

static size_t ink_astgen_string(struct ink_astgen *astgen,
                                const struct ink_ast_node *node)
{
    struct ink_ir_inst inst;
    struct ink_astgen_global *const global = astgen->global;
    struct ink_ast *const tree = global->tree;
    const uint8_t *const chars = &tree->source_bytes[node->start_offset];
    const size_t len = node->end_offset - node->start_offset;

    inst.op = INK_IR_INST_STRING;
    inst.as.string = ink_astgen_add_str(astgen, chars, len);
    return ink_astgen_add_inst(astgen, inst);
}

static size_t ink_astgen_identifier(struct ink_astgen *astgen,
                                    struct ink_scope *scope,
                                    const struct ink_ast_node *node)
{
    struct ink_symbol symbol;
    struct ink_ir_inst inst;
    struct ink_astgen_global *const global = astgen->global;
    const struct ink_ast *const tree = global->tree;

    if (ink_scope_lookup(scope, tree, node, &symbol) < 0) {
        ink_astgen_error(astgen, INK_AST_IDENT_UNKNOWN, node);
        return INK_IR_INVALID;
    }

    inst.op = INK_IR_INST_LOAD;
    inst.as.unary.lhs = symbol.index;
    return ink_astgen_add_inst(astgen, inst);
}

static size_t ink_astgen_expr(struct ink_astgen *astgen,
                              struct ink_scope *scope,
                              const struct ink_ast_node *node)
{
    if (!node) {
        return INK_IR_INVALID;
    }
    switch (node->type) {
    case INK_AST_NUMBER:
        return ink_astgen_number(astgen, node);
    case INK_AST_TRUE:
        return ink_astgen_true(astgen);
    case INK_AST_FALSE:
        return ink_astgen_false(astgen);
    case INK_AST_IDENTIFIER:
        return ink_astgen_identifier(astgen, scope, node);
    case INK_AST_ADD_EXPR:
        return ink_astgen_binary_op(astgen, scope, node, INK_IR_INST_ADD);
    case INK_AST_SUB_EXPR:
        return ink_astgen_binary_op(astgen, scope, node, INK_IR_INST_SUB);
    case INK_AST_MUL_EXPR:
        return ink_astgen_binary_op(astgen, scope, node, INK_IR_INST_MUL);
    case INK_AST_DIV_EXPR:
        return ink_astgen_binary_op(astgen, scope, node, INK_IR_INST_DIV);
    case INK_AST_MOD_EXPR:
        return ink_astgen_binary_op(astgen, scope, node, INK_IR_INST_MOD);
    case INK_AST_EQUAL_EXPR:
        return ink_astgen_binary_op(astgen, scope, node, INK_IR_INST_CMP_EQ);
    case INK_AST_NOT_EQUAL_EXPR:
        return ink_astgen_binary_op(astgen, scope, node, INK_IR_INST_CMP_NEQ);
    case INK_AST_LESS_EXPR:
        return ink_astgen_binary_op(astgen, scope, node, INK_IR_INST_CMP_LT);
    case INK_AST_LESS_EQUAL_EXPR:
        return ink_astgen_binary_op(astgen, scope, node, INK_IR_INST_CMP_LTE);
    case INK_AST_GREATER_EXPR:
        return ink_astgen_binary_op(astgen, scope, node, INK_IR_INST_CMP_GT);
    case INK_AST_GREATER_EQUAL_EXPR:
        return ink_astgen_binary_op(astgen, scope, node, INK_IR_INST_CMP_GTE);
    case INK_AST_NEGATE_EXPR:
        return ink_astgen_unary_op(astgen, scope, node, INK_IR_INST_NEG);
    case INK_AST_NOT_EXPR:
        return ink_astgen_unary_op(astgen, scope, node, INK_IR_INST_BOOL_NOT);
    default:
        assert(false);
        return INK_IR_INVALID;
    }
}

static size_t ink_astgen_inline_logic(struct ink_astgen *astgen,
                                      struct ink_scope *scope,
                                      const struct ink_ast_node *node)
{
    return ink_astgen_expr(astgen, scope, node->lhs);
}

struct ink_conditional_info {
    bool has_initial;
    bool has_block;
    bool has_else;
};

static size_t ink_astgen_if_expr(struct ink_astgen *astgen,
                                 struct ink_scope *scope,
                                 const struct ink_ast_node *expr_node,
                                 const struct ink_ast_node *then_node,
                                 const struct ink_ast_node *else_node)
{
    struct ink_astgen then_ctx, else_ctx;
    struct ink_astgen_scratch *const scratch = astgen->scratch;
    const size_t scratch_top = scratch->count;
    const size_t payload_index = ink_astgen_expr(astgen, scope, expr_node);
    const size_t condbr_index = ink_astgen_add_condbr(astgen, payload_index);
    const size_t block_index = ink_astgen_add_block(astgen, INK_IR_INST_BLOCK);

    ink_astgen_make(&then_ctx, astgen);
    ink_astgen_block_stmt(&then_ctx, scope, then_node);
    ink_astgen_add_br(astgen, block_index);
    ink_astgen_make(&else_ctx, astgen);

    if (else_node && else_node->type == INK_AST_CONDITIONAL_ELSE_BRANCH) {
        ink_astgen_block_stmt(&else_ctx, scope, else_node->rhs);
    }

    ink_astgen_add_br(astgen, block_index);
    ink_astgen_set_condbr(astgen, condbr_index, &then_ctx, &else_ctx);
    ink_astgen_set_block(astgen, block_index, scratch_top);
    return block_index;
}

static size_t ink_astgen_if_else_expr(struct ink_astgen *astgen,
                                      struct ink_scope *scope,
                                      struct ink_ast_seq *children,
                                      size_t node_index)
{
    struct ink_astgen then_ctx, else_ctx;

    if (node_index >= children->count) {
        return INK_IR_INVALID;
    }

    struct ink_ast_node *const then_node = children->nodes[node_index];
    struct ink_astgen_scratch *const scratch = astgen->scratch;
    const size_t scratch_top = scratch->count;
    const size_t payload_index = ink_astgen_expr(astgen, scope, then_node->lhs);
    const size_t condbr_index = ink_astgen_add_condbr(astgen, payload_index);
    const size_t block_index = ink_astgen_add_block(astgen, INK_IR_INST_BLOCK);

    ink_astgen_make(&then_ctx, astgen);
    ink_astgen_block_stmt(&then_ctx, scope, then_node->rhs);
    ink_astgen_add_br(astgen, block_index);
    ink_astgen_make(&else_ctx, astgen);

    if (node_index + 1 < children->count) {
        struct ink_ast_node *const else_node = children->nodes[node_index + 1];

        if (else_node->type == INK_AST_CONDITIONAL_ELSE_BRANCH) {
            ink_astgen_block_stmt(astgen, scope, else_node->rhs);
        } else {
            const size_t alt_index = ink_astgen_if_else_expr(
                &else_ctx, scope, children, node_index + 1);

            ink_astgen_scratch_push(scratch, alt_index);
        }
    }

    ink_astgen_add_br(astgen, block_index);
    ink_astgen_set_condbr(astgen, condbr_index, &then_ctx, &else_ctx);
    ink_astgen_set_block(astgen, block_index, scratch_top);
    return block_index;
}

static size_t ink_astgen_switch_expr(struct ink_astgen *astgen,
                                     struct ink_scope *scope,
                                     const struct ink_ast_node *expr,
                                     const struct ink_ast_node *body)
{
    return INK_IR_INVALID;
}

static size_t ink_astgen_conditional(struct ink_astgen *astgen,
                                     struct ink_scope *scope,
                                     const struct ink_ast_node *node)

{
    struct ink_conditional_info info;

    if (!node->rhs) {
        return ink_astgen_error(astgen, INK_AST_CONDITIONAL_EMPTY, node);
    }
    if (!node->lhs && (!node->rhs->seq || node->rhs->seq->count == 0)) {
        return ink_astgen_error(astgen, INK_AST_CONDITIONAL_EMPTY, node);
    }

    struct ink_ast_seq *const children = node->rhs->seq;
    struct ink_ast_node *const first = children->nodes[0];
    struct ink_ast_node *const last = children->nodes[children->count - 1];

    info.has_block = false;
    info.has_else = false;
    info.has_initial = node->lhs != NULL;

    for (size_t i = 0; i < children->count; i++) {
        struct ink_ast_node *const child = children->nodes[i];

        switch (child->type) {
        case INK_AST_BLOCK: {
            info.has_block = true;
            break;
        }
        case INK_AST_CONDITIONAL_BRANCH: {
            if (info.has_block) {
                return ink_astgen_error(
                    astgen, INK_AST_CONDITIONAL_EXPECTED_ELSE, child);
            }
            break;
        }
        case INK_AST_CONDITIONAL_ELSE_BRANCH: {
            /* Only the last branch can be an else. */
            if (child != last) {
                return ink_astgen_error(astgen, INK_AST_CONDITIONAL_FINAL_ELSE,
                                        child);
            }
            if (info.has_else) {
                return ink_astgen_error(
                    astgen, INK_AST_CONDITIONAL_MULTIPLE_ELSE, child);
            }

            info.has_else = true;
            break;
        }
        default:
            assert(false);
            break;
        }
    }
    if (info.has_initial && info.has_block) {
        return ink_astgen_if_expr(astgen, scope, node->lhs, first,
                                  first == last ? NULL : last);
    } else if (info.has_initial) {
        return ink_astgen_switch_expr(astgen, scope, node->lhs, node->rhs);
    } else {
        return ink_astgen_if_else_expr(astgen, scope, node->rhs->seq, 0);
    }
}

static void ink_astgen_content_expr(struct ink_astgen *astgen,
                                    struct ink_scope *scope,
                                    const struct ink_ast_node *node)
{
    size_t node_index;
    struct ink_ast_seq *const children = node->seq;
    struct ink_astgen_scratch *const scratch = astgen->scratch;

    for (size_t i = 0; i < children->count; i++) {
        struct ink_ast_node *const child = children->nodes[i];

        switch (child->type) {
        case INK_AST_STRING: {
            node_index = ink_astgen_string(astgen, child);
            ink_astgen_add_unary(astgen, INK_IR_INST_CONTENT_PUSH, node_index);
            break;
        }
        case INK_AST_INLINE_LOGIC: {
            ink_astgen_inline_logic(astgen, scope, child);
            break;
        }
        case INK_AST_CONDITIONAL_CONTENT: {
            node_index = ink_astgen_conditional(astgen, scope, child);
            ink_astgen_scratch_push(scratch, node_index);
            break;
        }
        default:
            assert(false);
            break;
        }
    }
}

static size_t ink_astgen_var_decl(struct ink_astgen *astgen,
                                  struct ink_scope *scope,
                                  const struct ink_ast_node *node)
{
    size_t name_index;
    struct ink_astgen_global *const global = astgen->global;
    struct ink_ast *const tree = global->tree;
    struct ink_ast_node *const name_node = node->lhs;
    struct ink_ast_node *const expr_node = node->rhs;
    struct ink_ir_inst_vec *const code = &global->ircode->instructions;
    struct ink_symbol symbol = {
        .node = node,
        .index = code->count,
        .is_const = node->type == INK_AST_CONST_DECL,
    };

    if (node->type == INK_AST_TEMP_DECL) {
        const struct ink_ir_inst inst = {
            .op = INK_IR_INST_ALLOC,
        };

        ink_astgen_add_inst(astgen, inst);
    } else {
        name_index = ink_astgen_add_str(
            astgen, &tree->source_bytes[name_node->start_offset],
            name_node->end_offset - name_node->start_offset);

        ink_astgen_add_var(astgen, INK_IR_INST_DECL_VAR, name_index,
                           symbol.is_const);
    }

    if (ink_scope_insert(scope, tree, name_node, symbol) < 0) {
        ink_astgen_error(astgen, INK_AST_IDENT_REDEFINED, name_node);
        return INK_IR_INVALID;
    }
    return ink_astgen_add_binary(astgen, INK_IR_INST_STORE, symbol.index,
                                 ink_astgen_expr(astgen, scope, expr_node));
}

static void ink_astgen_content_stmt(struct ink_astgen *astgen,
                                    struct ink_scope *scope,
                                    const struct ink_ast_node *node)
{
    ink_astgen_content_expr(astgen, scope, node->lhs);
}

static void ink_astgen_divert_expr(struct ink_astgen *astgen,
                                   struct ink_scope *scope,
                                   const struct ink_ast_node *node)
{
    struct ink_symbol symbol;
    struct ink_ast_node *const ident = node->lhs;
    struct ink_astgen_global *const global = astgen->global;
    struct ink_ast *const tree = global->tree;
    const uint8_t *const chars = &tree->source_bytes[ident->start_offset];
    const size_t len = ident->end_offset - ident->start_offset;

    switch (len) {
    case 3: {
        if (memcmp(chars, "END", len) == 0) {
            ink_astgen_add_simple(astgen, INK_IR_INST_END);
            return;
        }
        goto identifier;
    }
    case 4: {
        if (memcmp(chars, "DONE", len) == 0) {
            ink_astgen_add_simple(astgen, INK_IR_INST_DONE);
            return;
        }
        goto identifier;
    }
    default:
        break;
    }
identifier:
    if (ink_scope_lookup(scope, tree, ident, &symbol) < 0) {
        ink_astgen_error(astgen, INK_AST_IDENT_UNKNOWN, node);
        return;
    }
}

static void ink_astgen_divert_stmt(struct ink_astgen *astgen,
                                   struct ink_scope *scope,
                                   const struct ink_ast_node *node)
{
    struct ink_ast_seq *const seq = node->seq;

    for (size_t i = 0; i < seq->count; i++) {
        struct ink_ast_node *const node = seq->nodes[i];

        ink_astgen_divert_expr(astgen, scope, node);
    }
}

static size_t ink_astgen_expr_stmt(struct ink_astgen *astgen,
                                   struct ink_scope *scope,
                                   const struct ink_ast_node *node)
{
    const size_t payload_index = ink_astgen_expr(astgen, scope, node->lhs);

    return ink_astgen_add_unary(astgen, INK_IR_INST_CHECK_RESULT,
                                payload_index);
}

static void ink_astgen_stmt(struct ink_astgen *astgen, struct ink_scope *scope,
                            const struct ink_ast_node *node)
{
    switch (node->type) {
    case INK_AST_VAR_DECL:
    case INK_AST_CONST_DECL:
    case INK_AST_TEMP_DECL: {
        ink_astgen_var_decl(astgen, scope, node);
        break;
    }
    case INK_AST_CONTENT_STMT: {
        ink_astgen_content_stmt(astgen, scope, node);
        break;
    }
    case INK_AST_DIVERT_STMT: {
        ink_astgen_divert_stmt(astgen, scope, node);
        break;
    }
    case INK_AST_EXPR_STMT: {
        ink_astgen_expr_stmt(astgen, scope, node);
        break;
    }
    default:
        assert(false);
        break;
    }
}

static void ink_astgen_block_stmt(struct ink_astgen *astgen,
                                  struct ink_scope *scope,
                                  const struct ink_ast_node *node)
{
    struct ink_scope block_scope;
    struct ink_ast_seq *const seq = node->seq;

    if (scope->parent == NULL) {
        for (size_t i = 0; i < seq->count; i++) {
            struct ink_ast_node *const node = seq->nodes[i];

            ink_astgen_stmt(astgen, scope, node);
        }
    } else {
        ink_scope_init(&block_scope, scope);

        for (size_t i = 0; i < seq->count; i++) {
            struct ink_ast_node *const node = seq->nodes[i];

            ink_astgen_stmt(astgen, &block_scope, node);
        }

        ink_scope_deinit(&block_scope);
    }
}

static struct ink_ir_inst_seq *ink_astgen_file(struct ink_astgen_global *global,
                                               const struct ink_ast_node *node)
{
    size_t i, inst_index = 0;
    struct ink_scope file_scope;
    struct ink_ast_node *first;
    struct ink_ast_seq *const node_seq = node->seq;
    struct ink_astgen_scratch *const scratch = &global->scratch;
    const size_t scratch_top = scratch->count;

    if (node_seq->count == 0) {
        return NULL;
    }

    struct ink_astgen astgen = {
        .scratch_offset = scratch_top,
        .scratch = scratch,
        .global = global,
    };

    ink_scope_init(&file_scope, NULL);
    first = node_seq->nodes[0];

    if (first->type == INK_AST_BLOCK) {
        const size_t scratch_top = scratch->count;
        const size_t name_index = ink_astgen_add_str(
            &astgen, (uint8_t *)INK_DEFAULT_PATH, strlen(INK_DEFAULT_PATH));
        const size_t knot_index =
            ink_astgen_add_knot(&astgen, INK_IR_INST_DECL_KNOT, name_index);

        ink_astgen_block_stmt(&astgen, &file_scope, first);
        ink_astgen_add_unary(&astgen, INK_IR_INST_RET_IMPLICIT, 0);
        ink_astgen_set_knot(&astgen, knot_index, scratch_top);
        ink_astgen_scratch_push(scratch, knot_index);
        i = 1;
    } else {
        i = 0;
    }
    for (; i < node_seq->count; i++) {
        struct ink_ast_node *const node = node_seq->nodes[i];

        switch (node->type) {
        default:
            assert(false);
            break;
        }

        ink_astgen_scratch_push(scratch, inst_index);
    }
    ink_scope_deinit(&file_scope);
    return ink_astgen_seq_from_scratch(&astgen, scratch_top, scratch->count);
}

int ink_astgen(struct ink_ast *tree, struct ink_ir *ircode, int flags)
{
    struct ink_astgen_global global_store;

    if (!ink_ast_error_vec_is_empty(&tree->errors)) {
        return -1;
    }

    ink_ir_init(ircode);
    ink_astgen_global_init(&global_store, tree, ircode);
    ink_astgen_file(&global_store, tree->root);
    ink_astgen_global_deinit(&global_store);

    if (!ink_ast_error_vec_is_empty(&tree->errors)) {
        ink_ast_render_errors(tree);
        return -1;
    }
    return INK_E_OK;
}
