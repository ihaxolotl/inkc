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

struct ink_astgen_block {
    size_t scratch_offset;
};

INK_VEC_T(ink_astgen_scratch, size_t)
INK_VEC_T(ink_astgen_block_stack, struct ink_astgen_block)
INK_HASHMAP_T(ink_symtab, struct ink_symtab_key, struct ink_symbol)

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

struct ink_astgen {
    struct ink_ast *tree;
    struct ink_ir *ircode;
    struct ink_symtab symbol_table;
    struct ink_astgen_scratch scratch;
    struct ink_astgen_block_stack blocks;
};

static struct ink_ir_inst_seq *
ink_astgen_seq_from_scratch(struct ink_astgen *astgen, size_t start_offset,
                            size_t end_offset)
{
    struct ink_ir_inst_seq *seq = NULL;
    struct ink_astgen_scratch *const scratch = &astgen->scratch;
    struct ink_ir *const ircode = astgen->ircode;

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

static void ink_astgen_init(struct ink_astgen *astgen, struct ink_ast *tree,
                            struct ink_ir *ircode)
{
    astgen->tree = tree;
    astgen->ircode = ircode;

    ink_symtab_init(&astgen->symbol_table, 80ul, ink_symtab_hash,
                    ink_symtab_cmp);
    ink_astgen_block_stack_init(&astgen->blocks);
    ink_astgen_scratch_init(&astgen->scratch);
}

static void ink_astgen_deinit(struct ink_astgen *astgen)
{
    ink_symtab_deinit(&astgen->symbol_table);
    ink_astgen_block_stack_deinit(&astgen->blocks);
    ink_astgen_scratch_deinit(&astgen->scratch);
}

static void ink_astgen_error(struct ink_astgen *astgen,
                             enum ink_ast_error_type type,
                             const struct ink_ast_node *node)
{
    const struct ink_ast_error err = {
        .type = type,
        .source_start = node->start_offset,
        .source_end = node->end_offset,
    };

    ink_ast_error_vec_push(&astgen->tree->errors, err);
}

static size_t ink_astgen_add_inst(struct ink_astgen *astgen,
                                  struct ink_ir_inst inst)
{
    struct ink_astgen_scratch *const scratch = &astgen->scratch;
    struct ink_ir_inst_vec *const code = &astgen->ircode->instructions;
    const size_t index = code->count;

    ink_ir_inst_vec_push(code, inst);
    ink_astgen_scratch_push(scratch, index);
    return index;
}

static void ink_astgen_identifier_token(struct ink_astgen *astgen,
                                        const struct ink_ast_node *node,
                                        struct ink_symtab_key *token)
{
    const struct ink_ast *const tree = astgen->tree;

    token->bytes = &tree->source_bytes[node->start_offset];
    token->length = node->end_offset - node->start_offset;
}

/** Create a unary operation. */
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

/** Create a binary operation. */
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

/** Create a dominant block. */
static size_t ink_astgen_add_block(struct ink_astgen *astgen,
                                   enum ink_ir_inst_op op)
{
    struct ink_astgen_scratch *const scratch = &astgen->scratch;
    struct ink_ir_inst_vec *const code = &astgen->ircode->instructions;
    const size_t index = code->count;
    const struct ink_ir_inst inst = {
        .op = op,
    };

    ink_astgen_add_inst(astgen, inst);
    ink_astgen_scratch_pop(scratch, NULL);
    return index;
}

static void ink_astgen_set_block(struct ink_astgen *astgen, size_t index,
                                 struct ink_ir_inst_seq *body)
{
    struct ink_ir_inst_vec *const code = &astgen->ircode->instructions;
    struct ink_ir_inst *const inst = &code->entries[index];

    inst->as.block.seq = body;
}

/** Create a conditional branching operation. */
static size_t ink_astgen_add_condbr(struct ink_astgen *astgen,
                                    enum ink_ir_inst_op op,
                                    size_t payload_index)
{
    const struct ink_ir_inst inst = {
        .op = op,
        .as.cond_br.payload_index = payload_index,
    };
    return ink_astgen_add_inst(astgen, inst);
}

static void ink_astgen_set_condbr(struct ink_astgen *astgen, size_t index,
                                  struct ink_ir_inst_seq *then_seq,
                                  struct ink_ir_inst_seq *else_seq)
{
    struct ink_ir_inst_vec *const code = &astgen->ircode->instructions;
    struct ink_ir_inst *const inst = &code->entries[index];

    inst->as.cond_br.then_ = then_seq;
    inst->as.cond_br.else_ = else_seq;
}

/** Create a breaking operation. */
static size_t ink_astgen_add_br(struct ink_astgen *astgen,
                                enum ink_ir_inst_op op, size_t payload_index)
{
    const struct ink_ir_inst inst = {
        .op = op,
        .as.unary.lhs = payload_index,
    };
    return ink_astgen_add_inst(astgen, inst);
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
    struct ink_ir_inst inst;
    struct ink_ast *const tree = astgen->tree;
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
    struct ink_ast *const tree = astgen->tree;
    struct ink_ir_byte_vec *const strings = &astgen->ircode->string_bytes;
    const size_t pos = strings->count;
    const uint8_t *const chars = &tree->source_bytes[node->start_offset];
    const size_t len = node->end_offset - node->start_offset;

    for (size_t i = 0; i < len; i++) {
        ink_ir_byte_vec_push(strings, chars[i]);
    }

    ink_ir_byte_vec_push(strings, '\0');

    inst.op = INK_IR_INST_STRING;
    inst.as.string = pos;
    return ink_astgen_add_inst(astgen, inst);
}

static size_t ink_astgen_identifier(struct ink_astgen *astgen,
                                    const struct ink_ast_node *node)
{
    struct ink_symbol symbol;
    struct ink_ir_inst inst;
    const struct ink_ast *const tree = astgen->tree;
    const struct ink_symtab_key key = {
        .bytes = &tree->source_bytes[node->start_offset],
        .length = node->end_offset - node->start_offset,
    };

    if (ink_symtab_lookup(&astgen->symbol_table, key, &symbol) < 0) {
        ink_astgen_error(astgen, INK_AST_IDENT_UNKNOWN, node);
        return INK_IR_INVALID;
    }

    inst.op = INK_IR_INST_LOAD;
    inst.as.unary.lhs = symbol.index;
    return ink_astgen_add_inst(astgen, inst);
}

static size_t ink_astgen_expr(struct ink_astgen *astgen,
                              const struct ink_ast_node *node)
{
    if (!node) {
        return INK_IR_INVALID;
    }
    switch (node->type) {
    case INK_NODE_NUMBER:
        return ink_astgen_number(astgen, node);
    case INK_NODE_TRUE:
        return ink_astgen_true(astgen);
    case INK_NODE_FALSE:
        return ink_astgen_false(astgen);
    case INK_NODE_IDENTIFIER:
        return ink_astgen_identifier(astgen, node);
    case INK_NODE_ADD_EXPR:
        return ink_astgen_binary_op(astgen, node, INK_IR_INST_ADD);
    case INK_NODE_SUB_EXPR:
        return ink_astgen_binary_op(astgen, node, INK_IR_INST_SUB);
    case INK_NODE_MUL_EXPR:
        return ink_astgen_binary_op(astgen, node, INK_IR_INST_MUL);
    case INK_NODE_DIV_EXPR:
        return ink_astgen_binary_op(astgen, node, INK_IR_INST_DIV);
    case INK_NODE_MOD_EXPR:
        return ink_astgen_binary_op(astgen, node, INK_IR_INST_MOD);
    case INK_NODE_EQUAL_EXPR:
        return ink_astgen_binary_op(astgen, node, INK_IR_INST_CMP_EQ);
    case INK_NODE_NOT_EQUAL_EXPR:
        return ink_astgen_binary_op(astgen, node, INK_IR_INST_CMP_NEQ);
    case INK_NODE_LESS_EXPR:
        return ink_astgen_binary_op(astgen, node, INK_IR_INST_CMP_LT);
    case INK_NODE_LESS_EQUAL_EXPR:
        return ink_astgen_binary_op(astgen, node, INK_IR_INST_CMP_LTE);
    case INK_NODE_GREATER_EXPR:
        return ink_astgen_binary_op(astgen, node, INK_IR_INST_CMP_GT);
    case INK_NODE_GREATER_EQUAL_EXPR:
        return ink_astgen_binary_op(astgen, node, INK_IR_INST_CMP_GTE);
    case INK_NODE_NEGATE_EXPR:
        return ink_astgen_unary_op(astgen, node, INK_IR_INST_NEG);
    case INK_NODE_NOT_EXPR:
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

static size_t
ink_astgen_simple_conditional(struct ink_astgen *astgen,
                              const struct ink_ast_node *cond_expr,
                              const struct ink_ast_node *inner)
{
    size_t block = INK_IR_INVALID;
    struct ink_ir_inst_seq *then_seq = NULL;
    struct ink_ir_inst_seq *else_seq = NULL;
    struct ink_ast_seq *const seq = inner->seq;
    struct ink_ast_node *const first = seq->nodes[0];
    struct ink_astgen_scratch *const scratch = &astgen->scratch;
    const size_t stmt_start = scratch->count;

    assert(seq->count <= 2);

    // IN zig, the existing instructions are added to the block before new
    // instructions are appened. Our problem is that we dont create resizable
    // arrays... will need to pop scratch after the block is created.
    if (first->type == INK_NODE_BLOCK) {
        const size_t expr_index = ink_astgen_expr(astgen, cond_expr);
        const size_t condbr =
            ink_astgen_add_condbr(astgen, INK_IR_INST_CONDBR, expr_index);
        const size_t then_start = scratch->count;

        block = ink_astgen_add_block(astgen, INK_IR_INST_BLOCK);
        ink_astgen_block_stmt(astgen, first);
        ink_astgen_add_br(astgen, INK_IR_INST_BR, block);

        then_seq =
            ink_astgen_seq_from_scratch(astgen, then_start, scratch->count);

        if (seq->count == 2) {
            struct ink_ast_node *const else_ = seq->nodes[1];
            const size_t else_start = scratch->count;

            assert(else_->type == INK_NODE_CONDITIONAL_ELSE_BRANCH);
            ink_astgen_block_stmt(astgen, else_->rhs);
            ink_astgen_add_br(astgen, INK_IR_INST_BR, block);
            else_seq =
                ink_astgen_seq_from_scratch(astgen, else_start, scratch->count);
        }
        ink_astgen_set_condbr(astgen, condbr, then_seq, else_seq);
    } else {
        assert(false);
    }

    ink_astgen_set_block(
        astgen, block,
        ink_astgen_seq_from_scratch(astgen, stmt_start, scratch->count));
    return block;
}

static size_t ink_astgen_multi_conditional(struct ink_astgen *astgen,
                                           const struct ink_ast_node *node)
{
    /*
    size_t then_jmp, else_jmp;
    struct ink_block condbr;
    struct ink_block_stack *const block_stack = &astgen->blocks;
    struct ink_ast_seq *const seq = node->seq;
    const size_t block_top = block_stack->count;

    for (size_t i = 0; i < seq->count; i++) {
        const struct ink_ast_node *const node = seq->nodes[i];

        switch (node->type) {
        case INK_NODE_CONDITIONAL_BRANCH: {
            ink_astgen_expr(astgen, node->lhs);
            then_jmp = ink_astgen_add_inst(astgen, INK_OP_JMP_F, 0xff);
            ink_astgen_block_stmt(astgen, node->rhs);
            else_jmp = ink_astgen_add_inst(astgen, INK_OP_JMP, 0xff);
            ink_astgen_patch_jmp(astgen, then_jmp);
            break;
        }
        case INK_NODE_CONDITIONAL_ELSE_BRANCH: {
            ink_astgen_block_stmt(astgen, node->rhs);
            else_jmp = ink_astgen_add_inst(astgen, INK_OP_JMP, 0xff);
            break;
        }
        default:
            assert(false);
            break;
        }

        condbr.tmp = else_jmp;
        ink_block_stack_push(block_stack, condbr);
    }
    while (!ink_block_stack_is_empty(block_stack) &&
           block_stack->count >= block_top) {
        ink_block_stack_pop(block_stack, &condbr);
        ink_astgen_patch_jmp(astgen, condbr.tmp);
    }
*/
    return INK_IR_INVALID;
}

static size_t ink_astgen_conditional_content(struct ink_astgen *astgen,
                                             const struct ink_ast_node *node)

{
    bool has_block = false;
    const struct ink_ast_seq *const seq = node->rhs->seq;

    if (!node->lhs) {
        if (!seq || seq->count == 0) {
            ink_astgen_error(astgen, INK_AST_CONDITIONAL_EMPTY, node);
            return INK_IR_INVALID;
        }
    }

    const struct ink_ast_node *const last = seq->nodes[seq->count - 1];

    for (size_t i = 0; i < seq->count; i++) {
        const struct ink_ast_node *const node = seq->nodes[i];

        switch (node->type) {
        case INK_NODE_BLOCK: {
            has_block = true;
            break;
        }
        case INK_NODE_CONDITIONAL_BRANCH: {
            if (has_block) {
                ink_astgen_error(astgen, INK_AST_CONDITIONAL_EXPECTED_ELSE,
                                 node);
                return INK_IR_INVALID;
            }
            break;
        }
        case INK_NODE_CONDITIONAL_ELSE_BRANCH: {
            /* Only the last branch can be an else. */
            if (node != last) {
                if (last->type == node->type) {
                    ink_astgen_error(astgen, INK_AST_CONDITIONAL_MULTIPLE_ELSE,
                                     node);
                    return INK_IR_INVALID;
                } else {
                    ink_astgen_error(astgen, INK_AST_CONDITIONAL_FINAL_ELSE,
                                     node);
                    return INK_IR_INVALID;
                }
            }
            break;
        }
        default:
            break;
        }
    }
    if (node->lhs) {
        return ink_astgen_simple_conditional(astgen, node->lhs, node->rhs);
    }
    return ink_astgen_multi_conditional(astgen, node->rhs);
}

static void ink_astgen_content_expr(struct ink_astgen *astgen,
                                    const struct ink_ast_node *node)
{
    size_t inst_index;
    struct ink_ast_seq *const seq = node->seq;
    struct ink_astgen_scratch *const scratch = &astgen->scratch;

    for (size_t i = 0; i < seq->count; i++) {
        struct ink_ast_node *const node = seq->nodes[i];

        switch (node->type) {
        case INK_NODE_STRING: {
            inst_index = ink_astgen_string(astgen, node);
            break;
        }
        case INK_NODE_INLINE_LOGIC: {
            inst_index = ink_astgen_inline_logic(astgen, node);
            break;
        }
        case INK_NODE_CONDITIONAL_CONTENT: {
            inst_index = ink_astgen_conditional_content(astgen, node);
            break;
        }
        default:
            assert(false);
            break;
        }

        ink_astgen_scratch_push(scratch, inst_index);
    }
}

static size_t ink_astgen_var_decl(struct ink_astgen *astgen,
                                  const struct ink_ast_node *node)
{
    struct ink_symtab_key key;
    struct ink_ir_inst inst;
    struct ink_ir_inst_vec *const code = &astgen->ircode->instructions;
    const struct ink_symbol symbol = {
        .node = node,
        .index = code->count,
        .is_const = node->type == INK_NODE_CONST_DECL,
    };

    inst.op = INK_IR_INST_ALLOC;
    ink_ir_inst_vec_push(code, inst);
    ink_astgen_identifier_token(astgen, node->lhs, &key);

    if (ink_symtab_insert(&astgen->symbol_table, key, symbol) < 0) {
        ink_astgen_error(astgen, INK_AST_IDENT_REDEFINED, node->lhs);
        return INK_IR_INVALID;
    }
    return ink_astgen_add_binary(astgen, INK_IR_INST_STORE, symbol.index,
                                 ink_astgen_expr(astgen, node->rhs));
}

static void ink_astgen_content_stmt(struct ink_astgen *astgen,
                                    const struct ink_ast_node *node)
{
    ink_astgen_content_expr(astgen, node->lhs);
}

static size_t ink_astgen_expr_stmt(struct ink_astgen *astgen,
                                   const struct ink_ast_node *node)
{
    return ink_astgen_expr(astgen, node->lhs);
}

static void ink_astgen_block_stmt(struct ink_astgen *astgen,
                                  const struct ink_ast_node *node)
{
    size_t inst_index;
    struct ink_ast_seq *const seq = node->seq;
    struct ink_astgen_scratch *const scratch = &astgen->scratch;

    for (size_t i = 0; i < seq->count; i++) {
        struct ink_ast_node *const node = seq->nodes[i];

        switch (node->type) {
        case INK_NODE_VAR_DECL:
        case INK_NODE_CONST_DECL:
        case INK_NODE_TEMP_DECL: {
            inst_index = ink_astgen_var_decl(astgen, node);
            ink_astgen_scratch_push(scratch, inst_index);
            break;
        }
        case INK_NODE_CONTENT_STMT: {
            ink_astgen_content_stmt(astgen, node);
            break;
        }
        case INK_NODE_EXPR_STMT: {
            ink_astgen_expr_stmt(astgen, node);
            break;
        }
        default:
            assert(false);
            break;
        }
    }
}

static struct ink_ir_inst_seq *ink_astgen_file(struct ink_astgen *astgen,
                                               const struct ink_ast_node *node)
{
    size_t i, inst_index;
    struct ink_ast_node *first;
    struct ink_ast_seq *const node_seq = node->seq;
    struct ink_astgen_scratch *const scratch = &astgen->scratch;
    const size_t scratch_offset = scratch->count;

    if (node_seq->count == 0) {
        return NULL;
    }

    first = node_seq->nodes[0];

    if (first->type == INK_NODE_BLOCK) {
        ink_astgen_block_stmt(astgen, first);
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
    return ink_astgen_seq_from_scratch(astgen, scratch_offset, scratch->count);
}

int ink_astgen(struct ink_ast *tree, struct ink_ir *ircode, int flags)
{
    struct ink_astgen astgen;

    if (!ink_ast_error_vec_is_empty(&tree->errors)) {
        return -1;
    }

    ink_astgen_init(&astgen, tree, ircode);
    ink_astgen_file(&astgen, tree->root);
    ink_astgen_deinit(&astgen);

    if (!ink_ast_error_vec_is_empty(&tree->errors)) {
        ink_ast_render_errors(tree);
        return -1;
    }
    return INK_E_OK;
}
