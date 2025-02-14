#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "astgen.h"
#include "common.h"
#include "hashmap.h"
#include "ir.h"
#include "vec.h"

#define INK_NUMBER_BUFSZ 24
#define INK_IR_INVALID (size_t)(-1)
#define INK_SYMTAB_LOAD_MAX 80u
#define INK_E_FAIL (-1)

struct ink_string_ref {
    const uint8_t *bytes;
    size_t length;
};

enum ink_symbol_type {
    INK_SYMBOL_VAR,
    INK_SYMBOL_KNOT,
    INK_SYMBOL_FUNC,
};

struct ink_symbol {
    enum ink_symbol_type type;
    bool is_const;
    bool is_used;
    size_t ir_inst_index;
    size_t ir_name_index;
    struct ink_symtab *local_names;
    const struct ink_ast_node *node;
};

INK_VEC_T(ink_astgen_scratch, size_t)
INK_HASHMAP_T(ink_symtab, struct ink_string_ref, struct ink_symbol)

static uint32_t ink_symtab_hash(const void *bytes, size_t length)
{
    const struct ink_string_ref *const key = bytes;

    return ink_fnv32a(key->bytes, key->length);
}

static bool ink_symtab_cmp(const void *a, size_t alen, const void *b,
                           size_t blen)
{
    const struct ink_string_ref *const key_1 = a;
    const struct ink_string_ref *const key_2 = b;

    return (key_1->length == key_2->length) &&
           memcmp(key_1->bytes, key_2->bytes, key_1->length) == 0;
}

struct ink_symtab_node {
    struct ink_symtab_node *next;
    struct ink_symtab table;
};

struct ink_astgen_global {
    struct ink_ast *tree;
    struct ink_ir *ircode;
    struct ink_symtab_node *symtab_pool;
    struct ink_astgen_scratch scratch;
};

static void ink_astgen_global_init(struct ink_astgen_global *global,
                                   struct ink_ast *tree, struct ink_ir *ircode)
{
    global->tree = tree;
    global->ircode = ircode;
    global->symtab_pool = NULL;

    ink_astgen_scratch_init(&global->scratch);
}

static void ink_astgen_global_deinit(struct ink_astgen_global *global)
{
    while (global->symtab_pool) {
        struct ink_symtab_node *tmp = global->symtab_pool;

        global->symtab_pool = tmp->next;
        ink_symtab_deinit(&tmp->table);
        ink_free(tmp);
    }
    ink_astgen_scratch_deinit(&global->scratch);
}

static struct ink_symtab *ink_symtab_make(struct ink_astgen_global *global)
{
    struct ink_symtab_node *const node = ink_malloc(sizeof(*node));

    if (!node) {
        return NULL;
    }

    ink_symtab_init(&node->table, INK_SYMTAB_LOAD_MAX, ink_symtab_hash,
                    ink_symtab_cmp);

    node->next = global->symtab_pool;
    global->symtab_pool = node;
    return &node->table;
}

struct ink_astgen {
    struct ink_astgen *parent;
    struct ink_astgen_global *global;
    struct ink_symtab *symbol_table;
    struct ink_astgen_scratch *scratch;
    size_t scratch_offset;
    size_t namespace_index;
};

static int ink_astgen_make(struct ink_astgen *scope,
                           struct ink_astgen *parent_scope,
                           struct ink_symtab *symbol_table)
{
    struct ink_astgen_global *const global = parent_scope->global;

    if (!symbol_table) {
        symbol_table = ink_symtab_make(global);
        if (!symbol_table) {
            return INK_E_FAIL;
        }
    }

    scope->parent = parent_scope;
    scope->global = parent_scope->global;
    scope->symbol_table = symbol_table;
    scope->scratch = parent_scope->scratch;
    scope->scratch_offset = parent_scope->scratch->count;
    scope->namespace_index = parent_scope->namespace_index;
    return INK_E_OK;
}

static void ink_astgen_deinit(struct ink_astgen *scope)
{
    scope->parent = NULL;
    scope->global = NULL;
    scope->scratch = NULL;
    scope->symbol_table = NULL;
    scope->scratch_offset = 0;
}

/**
 * Retrieve source byte range from an AST node.
 */
static void ink_astgen_string_ref(const struct ink_astgen *astgen,
                                  const struct ink_ast_node *node,
                                  struct ink_string_ref *string_ref)
{
    struct ink_astgen_global *const global = astgen->global;
    const struct ink_ast *const tree = global->tree;

    string_ref->bytes = &tree->source_bytes[node->start_offset];
    string_ref->length = node->end_offset - node->start_offset;
}

/**
 * Insert a name relative to the current scope.
 *
 * Will fail if the name already exists or an internal error occurs.
 */
static int ink_astgen_insert_name(struct ink_astgen *scope,
                                  const struct ink_ast_node *node,
                                  struct ink_symbol sym)
{
    struct ink_string_ref key;

    ink_astgen_string_ref(scope, node, &key);
    return ink_symtab_insert(scope->symbol_table, key, sym);
}

/**
 * Perform a lookup for an identifier node.
 *
 * Lookup is performed relative to the current scope. The scope chain is
 * traversed recursively until a match is found or if the lookup fails.
 */
static int ink_astgen_lookup_name(struct ink_astgen *scope,
                                  const struct ink_ast_node *node,
                                  struct ink_symbol *sym)
{
    int rc = INK_E_FAIL;
    struct ink_string_ref key;

    ink_astgen_string_ref(scope, node, &key);

    while (scope) {
        rc = ink_symtab_lookup(scope->symbol_table, key, sym);
        if (rc < 0) {
            scope = scope->parent;
        } else {
            return rc;
        }
    }
    return rc;
}

/**
 * Recursive function to perform a qualified lookup on two nodes.
 */
int ink_astgen_lookup_expr_r(struct ink_astgen *scope,
                             const struct ink_ast_node *lhs,
                             const struct ink_ast_node *rhs,
                             struct ink_symbol *sym)

{
    int rc = INK_E_FAIL;
    struct ink_string_ref lhs_key, rhs_key;

    if (!lhs) {
        return rc;
    }
    if (lhs->type == INK_AST_SELECTOR_EXPR) {
        rc = ink_astgen_lookup_expr_r(scope, lhs->lhs, lhs->rhs, sym);
        if (rc < 0) {
            return rc;
        }

        rc = ink_astgen_lookup_expr_r(scope, lhs->rhs, rhs, sym);
    } else if (lhs->type == INK_AST_IDENTIFIER) {
        ink_astgen_string_ref(scope, lhs, &lhs_key);

        rc = ink_symtab_lookup(scope->symbol_table, lhs_key, sym);
        if (rc < 0) {
            return rc;
        }
        if (sym->local_names) {
            ink_astgen_string_ref(scope, rhs, &rhs_key);
            return ink_symtab_lookup(sym->local_names, rhs_key, sym);
        }
        rc = INK_E_FAIL;
    }
    return rc;
}

/**
 * Perform a qualified lookup for a selector expression node.
 *
 * Lookup is performed relative to the current scope. The scope chain is
 * traversed recursively until a match is found or if the lookup fails.
 */
static int ink_astgen_lookup_qualified(struct ink_astgen *scope,
                                       const struct ink_ast_node *node,
                                       struct ink_symbol *sym)
{
    int rc = INK_E_FAIL;

    if (!node) {
        return rc;
    }
    while (scope) {
        rc = ink_astgen_lookup_expr_r(scope, node->lhs, node->rhs, sym);
        if (rc < 0) {
            scope = scope->parent;
        } else {
            return rc;
        }
    }
    return rc;
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
        .as.unary.lhs = lhs,
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
        .as.binary.lhs = lhs,
        .as.binary.rhs = rhs,
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

static void ink__(struct ink_astgen *astgen, struct ink_string_ref *str)
{
    struct ink_astgen_global *const global = astgen->global;
    struct ink_ir_byte_vec *const strings = &global->ircode->string_bytes;

    if (!astgen->parent) {
        ink_astgen_add_str(astgen, str->bytes, str->length);
        return;
    } else {
        const size_t str_index = astgen->namespace_index;
        const uint8_t *const chars = &strings->entries[str_index];
        const size_t length = strlen((char *)chars);

        for (size_t i = 0; i < length; i++) {
            ink_ir_byte_vec_push(strings, chars[i]);
        }

        ink_ir_byte_vec_push(strings, '.');
        ink__(astgen->parent, str);
        return;
    }
}

static size_t ink_astgen_add_qualified_str(struct ink_astgen *astgen,
                                           struct ink_string_ref *base)
{
    struct ink_astgen_global *const global = astgen->global;
    struct ink_ir_byte_vec *const strings = &global->ircode->string_bytes;
    const size_t pos = strings->count;

    ink__(astgen, base);
    return pos;
}

static size_t ink_astgen_expr(struct ink_astgen *, const struct ink_ast_node *);
static void ink_astgen_block_stmt(struct ink_astgen *,
                                  const struct ink_ast_node *);

static size_t ink_astgen_unary_op(struct ink_astgen *astgen,
                                  const struct ink_ast_node *node,
                                  enum ink_ir_inst_op op)
{
    return ink_astgen_add_unary(astgen, op, ink_astgen_expr(astgen, node->lhs));
}

static size_t ink_astgen_binary_op(struct ink_astgen *astgen,
                                   const struct ink_ast_node *node,
                                   enum ink_ir_inst_op op)
{
    const size_t lhs = ink_astgen_expr(astgen, node->lhs);
    const size_t rhs = ink_astgen_expr(astgen, node->rhs);

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
    struct ink_string_ref str;
    struct ink_ir_inst inst;

    ink_astgen_string_ref(astgen, node, &str);

    if (str.length > INK_NUMBER_BUFSZ) {
        return INK_IR_INVALID;
    }

    memcpy(buf, str.bytes, str.length);
    buf[str.length] = '\0';
    inst.op = INK_IR_INST_NUMBER;
    inst.as.number = strtod(buf, NULL);
    return ink_astgen_add_inst(astgen, inst);
}

static size_t ink_astgen_string(struct ink_astgen *astgen,
                                const struct ink_ast_node *node)
{
    struct ink_string_ref str;
    struct ink_ir_inst inst;

    ink_astgen_string_ref(astgen, node, &str);

    inst.op = INK_IR_INST_STRING;
    inst.as.string = ink_astgen_add_str(astgen, str.bytes, str.length);
    return ink_astgen_add_inst(astgen, inst);
}

static size_t ink_astgen_identifier(struct ink_astgen *astgen,
                                    const struct ink_ast_node *node)
{
    struct ink_symbol sym;
    struct ink_ir_inst inst;

    if (ink_astgen_lookup_name(astgen, node, &sym) < 0) {
        ink_astgen_error(astgen, INK_AST_IDENT_UNKNOWN, node);
        return INK_IR_INVALID;
    }

    inst.op = INK_IR_INST_LOAD;
    inst.as.unary.lhs = sym.ir_inst_index;
    return ink_astgen_add_inst(astgen, inst);
}

static size_t ink_astgen_expr(struct ink_astgen *astgen,
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
        return ink_astgen_identifier(astgen, node);
    case INK_AST_ADD_EXPR:
        return ink_astgen_binary_op(astgen, node, INK_IR_INST_ADD);
    case INK_AST_SUB_EXPR:
        return ink_astgen_binary_op(astgen, node, INK_IR_INST_SUB);
    case INK_AST_MUL_EXPR:
        return ink_astgen_binary_op(astgen, node, INK_IR_INST_MUL);
    case INK_AST_DIV_EXPR:
        return ink_astgen_binary_op(astgen, node, INK_IR_INST_DIV);
    case INK_AST_MOD_EXPR:
        return ink_astgen_binary_op(astgen, node, INK_IR_INST_MOD);
    case INK_AST_EQUAL_EXPR:
        return ink_astgen_binary_op(astgen, node, INK_IR_INST_CMP_EQ);
    case INK_AST_NOT_EQUAL_EXPR:
        return ink_astgen_binary_op(astgen, node, INK_IR_INST_CMP_NEQ);
    case INK_AST_LESS_EXPR:
        return ink_astgen_binary_op(astgen, node, INK_IR_INST_CMP_LT);
    case INK_AST_LESS_EQUAL_EXPR:
        return ink_astgen_binary_op(astgen, node, INK_IR_INST_CMP_LTE);
    case INK_AST_GREATER_EXPR:
        return ink_astgen_binary_op(astgen, node, INK_IR_INST_CMP_GT);
    case INK_AST_GREATER_EQUAL_EXPR:
        return ink_astgen_binary_op(astgen, node, INK_IR_INST_CMP_GTE);
    case INK_AST_NEGATE_EXPR:
        return ink_astgen_unary_op(astgen, node, INK_IR_INST_NEG);
    case INK_AST_NOT_EXPR:
        return ink_astgen_unary_op(astgen, node, INK_IR_INST_BOOL_NOT);
    default:
        assert(false);
        return INK_IR_INVALID;
    }
}

static size_t ink_astgen_inline_logic(struct ink_astgen *astgen,
                                      const struct ink_ast_node *node)
{
    return ink_astgen_expr(astgen, node->lhs);
}

struct ink_conditional_info {
    bool has_initial;
    bool has_block;
    bool has_else;
};

static size_t ink_astgen_if_expr(struct ink_astgen *astgen,
                                 const struct ink_ast_node *expr_node,
                                 const struct ink_ast_node *then_node,
                                 const struct ink_ast_node *else_node)
{
    struct ink_astgen then_ctx, else_ctx;
    struct ink_astgen_scratch *const scratch = astgen->scratch;
    const size_t scratch_top = scratch->count;
    const size_t payload_index = ink_astgen_expr(astgen, expr_node);
    const size_t condbr_index = ink_astgen_add_condbr(astgen, payload_index);
    const size_t block_index = ink_astgen_add_block(astgen, INK_IR_INST_BLOCK);

    ink_astgen_make(&then_ctx, astgen, NULL);
    ink_astgen_block_stmt(&then_ctx, then_node);
    ink_astgen_add_br(astgen, block_index);
    ink_astgen_make(&else_ctx, astgen, NULL);

    if (else_node && else_node->type == INK_AST_CONDITIONAL_ELSE_BRANCH) {
        ink_astgen_block_stmt(&else_ctx, else_node->rhs);
    }

    ink_astgen_add_br(astgen, block_index);
    ink_astgen_set_condbr(astgen, condbr_index, &then_ctx, &else_ctx);
    ink_astgen_set_block(astgen, block_index, scratch_top);
    return block_index;
}

static size_t ink_astgen_if_else_expr(struct ink_astgen *astgen,
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
    const size_t payload_index = ink_astgen_expr(astgen, then_node->lhs);
    const size_t condbr_index = ink_astgen_add_condbr(astgen, payload_index);
    const size_t block_index = ink_astgen_add_block(astgen, INK_IR_INST_BLOCK);

    ink_astgen_make(&then_ctx, astgen, NULL);
    ink_astgen_block_stmt(&then_ctx, then_node->rhs);
    ink_astgen_add_br(astgen, block_index);
    ink_astgen_make(&else_ctx, astgen, NULL);

    if (node_index + 1 < children->count) {
        struct ink_ast_node *const else_node = children->nodes[node_index + 1];

        if (else_node->type == INK_AST_CONDITIONAL_ELSE_BRANCH) {
            ink_astgen_block_stmt(astgen, else_node->rhs);
        } else {
            const size_t alt_index =
                ink_astgen_if_else_expr(&else_ctx, children, node_index + 1);

            ink_astgen_scratch_push(scratch, alt_index);
        }
    }

    ink_astgen_add_br(astgen, block_index);
    ink_astgen_set_condbr(astgen, condbr_index, &then_ctx, &else_ctx);
    ink_astgen_set_block(astgen, block_index, scratch_top);
    return block_index;
}

static size_t ink_astgen_switch_expr(struct ink_astgen *astgen,
                                     const struct ink_ast_node *expr,
                                     const struct ink_ast_node *body)
{
    return INK_IR_INVALID;
}

static size_t ink_astgen_conditional(struct ink_astgen *astgen,
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
        case INK_AST_BLOCK:
            info.has_block = true;
            break;
        case INK_AST_CONDITIONAL_BRANCH:
            if (info.has_block) {
                return ink_astgen_error(
                    astgen, INK_AST_CONDITIONAL_EXPECTED_ELSE, child);
            }
            break;
        case INK_AST_CONDITIONAL_ELSE_BRANCH:
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
        default:
            assert(false);
            break;
        }
    }
    if (info.has_initial && info.has_block) {
        return ink_astgen_if_expr(astgen, node->lhs, first,
                                  first == last ? NULL : last);
    } else if (info.has_initial) {
        return ink_astgen_switch_expr(astgen, node->lhs, node->rhs);
    } else {
        return ink_astgen_if_else_expr(astgen, node->rhs->seq, 0);
    }
}

static void ink_astgen_content_expr(struct ink_astgen *astgen,
                                    const struct ink_ast_node *node)
{
    size_t node_index;
    struct ink_ast_seq *const children = node->seq;
    struct ink_astgen_scratch *const scratch = astgen->scratch;

    for (size_t i = 0; i < children->count; i++) {
        struct ink_ast_node *const child = children->nodes[i];

        switch (child->type) {
        case INK_AST_STRING:
            node_index = ink_astgen_string(astgen, child);
            ink_astgen_add_unary(astgen, INK_IR_INST_CONTENT_PUSH, node_index);
            break;
        case INK_AST_INLINE_LOGIC:
            ink_astgen_inline_logic(astgen, child);
            break;
        case INK_AST_CONDITIONAL_CONTENT:
            node_index = ink_astgen_conditional(astgen, child);
            ink_astgen_scratch_push(scratch, node_index);
            break;
        default:
            assert(false);
            break;
        }
    }
}

static size_t ink_astgen_var_decl(struct ink_astgen *astgen,
                                  const struct ink_ast_node *node)
{
    size_t name_index;
    struct ink_astgen_global *const global = astgen->global;
    struct ink_ast *const tree = global->tree;
    struct ink_ast_node *const name_node = node->lhs;
    struct ink_ast_node *const expr_node = node->rhs;
    struct ink_ir_inst_vec *const code = &global->ircode->instructions;
    struct ink_symbol sym = {
        .type = INK_SYMBOL_VAR,
        .node = node,
        .ir_inst_index = code->count,
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
                           sym.is_const);
    }

    if (ink_astgen_insert_name(astgen, name_node, sym) < 0) {
        ink_astgen_error(astgen, INK_AST_IDENT_REDEFINED, name_node);
        return INK_IR_INVALID;
    }
    return ink_astgen_add_binary(astgen, INK_IR_INST_STORE, sym.ir_inst_index,
                                 ink_astgen_expr(astgen, expr_node));
}

static void ink_astgen_content_stmt(struct ink_astgen *astgen,
                                    const struct ink_ast_node *node)
{
    ink_astgen_content_expr(astgen, node->lhs);
}

static void ink_astgen_divert_expr(struct ink_astgen *astgen,
                                   const struct ink_ast_node *node)
{
    struct ink_symbol sym;
    struct ink_string_ref str;
    struct ink_ast_node *const name_node = node->lhs;

    if (name_node->type == INK_AST_SELECTOR_EXPR) {
        if (ink_astgen_lookup_qualified(astgen, name_node, &sym) < 0) {
            ink_astgen_error(astgen, INK_AST_IDENT_UNKNOWN, name_node);
            return;
        }
    } else if (name_node->type == INK_AST_IDENTIFIER) {
        ink_astgen_string_ref(astgen, name_node, &str);

        switch (str.length) {
        case 3:
            if (memcmp(str.bytes, "END", str.length) == 0) {
                ink_astgen_add_simple(astgen, INK_IR_INST_END);
                return;
            }
            break;
        case 4:
            if (memcmp(str.bytes, "DONE", str.length) == 0) {
                ink_astgen_add_simple(astgen, INK_IR_INST_DONE);
                return;
            }
            break;
        default:
            break;
        }
        if (ink_astgen_lookup_name(astgen, name_node, &sym) < 0) {
            ink_astgen_error(astgen, INK_AST_IDENT_UNKNOWN, name_node);
            return;
        }
    } else {
        assert(false);
    }

    ink_astgen_add_unary(astgen, INK_IR_INST_DIVERT, sym.ir_name_index);
}

static void ink_astgen_divert_stmt(struct ink_astgen *astgen,
                                   const struct ink_ast_node *node)
{
    struct ink_ast_seq *const node_list = node->seq;

    for (size_t i = 0; i < node_list->count; i++) {
        ink_astgen_divert_expr(astgen, node_list->nodes[i]);
    }
}

static size_t ink_astgen_expr_stmt(struct ink_astgen *astgen,
                                   const struct ink_ast_node *node)
{
    const size_t payload_index = ink_astgen_expr(astgen, node->lhs);

    return ink_astgen_add_unary(astgen, INK_IR_INST_CHECK_RESULT,
                                payload_index);
}

static void ink_astgen_stmt(struct ink_astgen *astgen,
                            const struct ink_ast_node *node)
{
    switch (node->type) {
    case INK_AST_VAR_DECL:
    case INK_AST_CONST_DECL:
    case INK_AST_TEMP_DECL:
        ink_astgen_var_decl(astgen, node);
        break;
    case INK_AST_CONTENT_STMT:
        ink_astgen_content_stmt(astgen, node);
        break;
    case INK_AST_DIVERT_STMT:
        ink_astgen_divert_stmt(astgen, node);
        break;
    case INK_AST_EXPR_STMT:
        ink_astgen_expr_stmt(astgen, node);
        break;
    default:
        assert(false);
        break;
    }
}

static void ink_astgen_block_stmt(struct ink_astgen *astgen,
                                  const struct ink_ast_node *node)
{
    struct ink_astgen block_scope;
    struct ink_ast_seq *const node_list = node->seq;

    if (!astgen->parent) {
        for (size_t i = 0; i < node_list->count; i++) {
            ink_astgen_stmt(astgen, node_list->nodes[i]);
        }
    } else {
        ink_astgen_make(&block_scope, astgen, NULL);

        for (size_t i = 0; i < node_list->count; i++) {
            ink_astgen_stmt(&block_scope, node_list->nodes[i]);
        }
    }
}

static size_t ink_astgen_stitch_decl(struct ink_astgen *parent_scope,
                                     const struct ink_ast_node *node)
{
    struct ink_symbol sym;
    struct ink_astgen stitch_scope;
    struct ink_ast_node *const proto_node = node->lhs;
    struct ink_ast_seq *const body_list = node->seq;
    struct ink_ast_seq *const proto_list = proto_node->seq;
    struct ink_ast_node *const name_node = proto_list->nodes[0];

    if (ink_astgen_lookup_name(parent_scope, name_node, &sym) < 0) {
        ink_astgen_error(parent_scope, INK_AST_IDENT_UNKNOWN, node);
        return INK_IR_INVALID;
    }

    const size_t node_index = ink_astgen_add_knot(
        parent_scope, INK_IR_INST_DECL_KNOT, sym.ir_name_index);

    ink_astgen_make(&stitch_scope, parent_scope, sym.local_names);

    if (!body_list) {
        ink_astgen_add_unary(&stitch_scope, INK_IR_INST_RET_IMPLICIT, 0);
        ink_astgen_set_knot(parent_scope, node_index,
                            stitch_scope.scratch_offset);
        return node_index;
    }

    assert(body_list->count == 1);

    struct ink_ast_node *const body_node = body_list->nodes[0];

    ink_astgen_block_stmt(&stitch_scope, body_node);
    ink_astgen_add_unary(&stitch_scope, INK_IR_INST_RET_IMPLICIT, 0);
    ink_astgen_set_knot(parent_scope, node_index, stitch_scope.scratch_offset);
    return node_index;
}

static size_t ink_astgen_knot_decl(struct ink_astgen *parent_scope,
                                   const struct ink_ast_node *node)
{
    size_t i = 0;
    struct ink_symbol sym;
    struct ink_astgen knot_scope;
    struct ink_ast_node *const proto_node = node->lhs;
    struct ink_ast_seq *const body_list = node->seq;
    struct ink_ast_seq *const proto_list = proto_node->seq;
    struct ink_ast_node *const name_node = proto_list->nodes[0];

    if (ink_astgen_lookup_name(parent_scope, name_node, &sym) < 0) {
        ink_astgen_error(parent_scope, INK_AST_IDENT_UNKNOWN, node);
        return INK_IR_INVALID;
    }

    const size_t node_index = ink_astgen_add_knot(
        parent_scope, INK_IR_INST_DECL_KNOT, sym.ir_name_index);

    ink_astgen_make(&knot_scope, parent_scope, sym.local_names);

    if (!body_list) {
        ink_astgen_add_unary(&knot_scope, INK_IR_INST_RET_IMPLICIT, 0);
        ink_astgen_set_knot(parent_scope, node_index,
                            knot_scope.scratch_offset);
        return node_index;
    }

    struct ink_ast_node *const first = body_list->nodes[0];

    if (first->type == INK_AST_BLOCK) {
        ink_astgen_block_stmt(&knot_scope, first);
        i++;
    }

    ink_astgen_add_unary(&knot_scope, INK_IR_INST_RET_IMPLICIT, 0);
    ink_astgen_set_knot(parent_scope, node_index, knot_scope.scratch_offset);

    for (; i < body_list->count; i++) {
        struct ink_ast_node *const body_node = body_list->nodes[i];
        const size_t body_index =
            ink_astgen_stitch_decl(&knot_scope, body_node);

        ink_astgen_scratch_push(knot_scope.scratch, body_index);
    }
    return node_index;
}

static void ink_astgen_default_body(struct ink_astgen *parent_scope,
                                    const struct ink_ast_node *node)
{
    struct ink_astgen_global *const global = parent_scope->global;
    struct ink_astgen_scratch *const scratch = &global->scratch;
    const uint8_t *bytes = (uint8_t *)INK_DEFAULT_PATH;
    const size_t length = strlen(INK_DEFAULT_PATH);
    const size_t name_index = ink_astgen_add_str(parent_scope, bytes, length);
    const size_t node_index =
        ink_astgen_add_knot(parent_scope, INK_IR_INST_DECL_KNOT, name_index);
    const size_t scratch_top = scratch->count;

    ink_astgen_block_stmt(parent_scope, node);
    ink_astgen_add_unary(parent_scope, INK_IR_INST_RET_IMPLICIT, 0);
    ink_astgen_set_knot(parent_scope, node_index, scratch_top);
    ink_astgen_scratch_push(scratch, node_index);
}

static int ink_astgen_record_proto(struct ink_astgen *parent_scope,
                                   const struct ink_ast_node *node,
                                   struct ink_symtab *symtab, size_t *tmp)
{
    struct ink_string_ref str;
    struct ink_ast_seq *const proto_list = node->seq;
    struct ink_ast_node *const name_node = proto_list->nodes[0];

    ink_astgen_string_ref(parent_scope, name_node, &str);

    const size_t str_index = ink_astgen_add_qualified_str(parent_scope, &str);

    struct ink_symbol sym = {
        .type = INK_SYMBOL_KNOT,
        .node = node,
        .local_names = symtab,
        .ir_name_index = str_index,
    };

    if (ink_astgen_insert_name(parent_scope, name_node, sym) < 0) {
        ink_astgen_error(parent_scope, INK_AST_IDENT_REDEFINED, name_node);
        return INK_E_FAIL;
    }
    if (tmp) {
        *tmp = str_index;
    }
    return INK_E_OK;
}

/**
 * Collect information for a stitch prototype.
 */
static int ink_astgen_intern_stitch(struct ink_astgen *parent_scope,
                                    const struct ink_ast_node *root_node)
{
    int rc;
    struct ink_ast_node *const proto_node = root_node->lhs;
    struct ink_ast_seq *const body_list = root_node->seq;

    rc = ink_astgen_record_proto(parent_scope, proto_node, NULL, NULL);
    if (rc < 0) {
        return rc;
    }
    if (!body_list) {
        return rc;
    }
    return rc;
}

/**
 * Collect information for a knot prototype.
 */
static int ink_astgen_intern_knot(struct ink_astgen *parent_scope,
                                  const struct ink_ast_node *root_node)
{
    int rc;
    size_t tmp = 0;
    struct ink_astgen knot_scope;
    struct ink_astgen_global *const global = parent_scope->global;
    struct ink_ast_node *const proto_node = root_node->lhs;
    struct ink_ast_seq *const body_list = root_node->seq;
    struct ink_symtab *const symtab = ink_symtab_make(global);

    rc = ink_astgen_record_proto(parent_scope, proto_node, symtab, &tmp);
    if (rc < 0) {
        return rc;
    }
    if (!body_list) {
        return rc;
    }

    ink_astgen_make(&knot_scope, parent_scope, symtab);
    knot_scope.namespace_index = tmp;

    for (size_t i = 0; i < body_list->count; i++) {
        struct ink_ast_node *const body_node = body_list->nodes[i];

        if (body_node->type == INK_AST_STITCH_DECL) {
            rc = ink_astgen_intern_stitch(&knot_scope, body_node);
            if (rc < 0) {
                break;
            }
        }
    }

    ink_astgen_deinit(&knot_scope);
    return rc;
}

/**
 * Perform a pass over the AST, gathering prototype information.
 */
static int ink_astgen_intern_paths(struct ink_astgen *parent_scope,
                                   const struct ink_ast_node *root_node)
{
    int rc = 0;
    struct ink_ast_seq *const decl_list = root_node->seq;

    if (!decl_list) {
        return rc;
    }
    for (size_t i = 0; i < decl_list->count; i++) {
        struct ink_ast_node *const decl_node = decl_list->nodes[i];

        if (decl_node->type == INK_AST_KNOT_DECL) {
            rc = ink_astgen_intern_knot(parent_scope, decl_node);
            if (rc < 0) {
                break;
            }
        } else if (decl_node->type == INK_AST_STITCH_DECL) {
            rc = ink_astgen_intern_stitch(parent_scope, decl_node);
            if (rc < 0) {
                break;
            }
        } else {
            assert(decl_node->type == INK_AST_BLOCK);
        }
    }
    return rc;
}

static struct ink_ir_inst_seq *ink_astgen_file(struct ink_astgen_global *global,
                                               const struct ink_ast_node *node)
{
    struct ink_ast_seq *const node_list = node->seq;
    struct ink_astgen file_scope = {
        .parent = NULL,
        .global = global,
        .symbol_table = ink_symtab_make(global),
        .scratch = &global->scratch,
        .scratch_offset = global->scratch.count,
    };

    if (node_list->count > 0) {
        size_t i = 0;
        struct ink_ast_node *const first = node_list->nodes[0];

        ink_astgen_intern_paths(&file_scope, node);

        if (first->type == INK_AST_BLOCK) {
            ink_astgen_default_body(&file_scope, first);
            i++;
        }
        for (; i < node_list->count; i++) {
            size_t inst_index = 0;
            struct ink_ast_node *const node = node_list->nodes[i];

            if (node->type == INK_AST_KNOT_DECL) {
                inst_index = ink_astgen_knot_decl(&file_scope, node);
            } else if (node->type == INK_AST_STITCH_DECL) {
                inst_index = ink_astgen_stitch_decl(&file_scope, node);
            } else {
                assert(false);
            }

            ink_astgen_scratch_push(file_scope.scratch, inst_index);
        }
    }
    return ink_astgen_seq_from_scratch(&file_scope, file_scope.scratch_offset,
                                       file_scope.scratch->count);
}

int ink_astgen(struct ink_ast *tree, struct ink_ir *ircode, int flags)
{
    struct ink_astgen_global global_store;

    if (!ink_ast_error_vec_is_empty(&tree->errors)) {
        return INK_E_FAIL;
    }

    ink_ir_init(ircode);
    ink_ir_byte_vec_push(&ircode->string_bytes, '\0');
    ink_astgen_global_init(&global_store, tree, ircode);
    ink_astgen_file(&global_store, tree->root);
    ink_astgen_global_deinit(&global_store);

    if (!ink_ast_error_vec_is_empty(&tree->errors)) {
        ink_ast_render_errors(tree);
        return INK_E_FAIL;
    }
    return INK_E_OK;
}
