#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "astgen.h"
#include "common.h"
#include "hashmap.h"
#include "object.h"
#include "opcode.h"
#include "story.h"
#include "tree.h"

#define INK_NUMBER_BUFSZ 24

struct ink_symbol {
    bool is_const;
    const struct ink_ast_node *node;
};

struct ink_symtab_key {
    const uint8_t *bytes;
    size_t length;
};

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

    if (key_1->length != key_2->length) {
        return false;
    }
    return memcmp(key_1->bytes, key_2->bytes, key_1->length) == 0;
}

struct ink_astgen {
    struct ink_story *story;
    struct ink_ast *tree;
    struct ink_symtab symbol_table;
};

static void ink_astgen_init(struct ink_astgen *astgen, struct ink_ast *tree,
                            struct ink_story *story)
{
    astgen->story = story;
    astgen->tree = tree;

    ink_symtab_init(&astgen->symbol_table, 80ul, ink_symtab_hash,
                    ink_symtab_cmp);
}

static void ink_astgen_deinit(struct ink_astgen *astgen)
{
    ink_symtab_deinit(&astgen->symbol_table);
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

static struct ink_object *ink_astgen_number_new(struct ink_astgen *astgen,
                                                const struct ink_ast_node *node)
{
    char buf[INK_NUMBER_BUFSZ + 1];
    const struct ink_ast *const tree = astgen->tree;
    const uint8_t *const chars = &tree->source_bytes[node->start_offset];
    const size_t len = node->end_offset - node->start_offset;

    if (len > INK_NUMBER_BUFSZ) {
        return NULL;
    }

    memcpy(buf, chars, len);
    buf[len] = '\0';
    return ink_number_new(astgen->story, strtod(buf, NULL));
}

static struct ink_object *ink_astgen_string_new(struct ink_astgen *astgen,
                                                const struct ink_ast_node *node)
{
    const struct ink_ast *const tree = astgen->tree;

    return ink_string_new(astgen->story,
                          &tree->source_bytes[node->start_offset],
                          node->end_offset - node->start_offset);
}

static size_t ink_astgen_add_inst(struct ink_astgen *astgen,
                                  enum ink_vm_opcode opcode, uint8_t arg)
{
    struct ink_byte_vec *const code = &astgen->story->code;

    ink_byte_vec_push(code, (uint8_t)opcode);
    ink_byte_vec_push(code, arg);
    return code->count - 2;
}

static void ink_astgen_add_const(struct ink_astgen *astgen,
                                 struct ink_object *object)
{
    struct ink_object_vec *const consts = &astgen->story->constants;

    ink_object_vec_push(consts, object);
}

static void ink_astgen_patch_jmp(struct ink_astgen *astgen, size_t offset)
{
    struct ink_byte_vec *const bytes = &astgen->story->code;
    const uint8_t addr = (uint8_t)bytes->count;

    bytes->entries[offset + 1] = addr;
}

static void ink_astgen_identifier_token(struct ink_astgen *astgen,
                                        const struct ink_ast_node *node,
                                        struct ink_symtab_key *token)
{
    const struct ink_ast *const tree = astgen->tree;

    token->bytes = &tree->source_bytes[node->start_offset];
    token->length = node->end_offset - node->start_offset;
}

static void ink_astgen_number(struct ink_astgen *astgen,
                              const struct ink_ast_node *node)
{
    const size_t id = astgen->story->constants.count;
    struct ink_object *const obj = ink_astgen_number_new(astgen, node);

    if (!obj) {
        return;
    }

    ink_astgen_add_const(astgen, obj);
    ink_astgen_add_inst(astgen, INK_OP_LOAD_CONST, (uint8_t)id);
}

static void ink_astgen_string(struct ink_astgen *astgen,
                              const struct ink_ast_node *node)
{
    const size_t id = astgen->story->constants.count;
    struct ink_object *const obj = ink_astgen_string_new(astgen, node);

    if (!obj) {
        return;
    }

    ink_astgen_add_const(astgen, obj);
    ink_astgen_add_inst(astgen, INK_OP_LOAD_CONST, (uint8_t)id);
}

static void ink_astgen_identifier(struct ink_astgen *astgen,
                                  const struct ink_ast_node *node)
{
    const struct ink_ast *const tree = astgen->tree;
    const struct ink_symtab_key key = {
        .bytes = &tree->source_bytes[node->start_offset],
        .length = node->end_offset - node->start_offset,
    };
    struct ink_symbol symbol;

    if (ink_symtab_lookup(&astgen->symbol_table, key, &symbol) < 0) {
        ink_astgen_error(astgen, INK_AST_IDENT_UNKNOWN, node);
        return;
    }
}

static void ink_astgen_expr(struct ink_astgen *astgen,
                            const struct ink_ast_node *node);
static void ink_astgen_block_stmt(struct ink_astgen *astgen,
                                  const struct ink_ast_node *node);

static void ink_astgen_unary_op(struct ink_astgen *astgen,
                                const struct ink_ast_node *node,
                                enum ink_vm_opcode op)
{
    ink_astgen_expr(astgen, node->lhs);
    ink_astgen_add_inst(astgen, op, 0);
}

static void ink_astgen_binary_op(struct ink_astgen *astgen,
                                 const struct ink_ast_node *node,
                                 enum ink_vm_opcode op)
{
    ink_astgen_expr(astgen, node->lhs);
    ink_astgen_expr(astgen, node->rhs);
    ink_astgen_add_inst(astgen, op, 0);
}

static void ink_astgen_expr(struct ink_astgen *astgen,
                            const struct ink_ast_node *node)
{
    if (!node) {
        return;
    }
    switch (node->type) {
    case INK_NODE_NUMBER: {
        ink_astgen_number(astgen, node);
        break;
    }
    case INK_NODE_IDENTIFIER: {
        ink_astgen_identifier(astgen, node);
        break;
    }
    case INK_NODE_TRUE: {
        ink_astgen_add_inst(astgen, INK_OP_TRUE, 0);
        break;
    }
    case INK_NODE_FALSE: {
        ink_astgen_add_inst(astgen, INK_OP_FALSE, 0);
        break;
    }
    case INK_NODE_ADD_EXPR: {
        ink_astgen_binary_op(astgen, node, INK_OP_ADD);
        break;
    }
    case INK_NODE_SUB_EXPR: {
        ink_astgen_binary_op(astgen, node, INK_OP_SUB);
        break;
    }
    case INK_NODE_MUL_EXPR: {
        ink_astgen_binary_op(astgen, node, INK_OP_MUL);
        break;
    }
    case INK_NODE_DIV_EXPR: {
        ink_astgen_binary_op(astgen, node, INK_OP_DIV);
        break;
    }
    case INK_NODE_MOD_EXPR: {
        ink_astgen_binary_op(astgen, node, INK_OP_MOD);
        break;
    }
    case INK_NODE_NEGATE_EXPR: {
        ink_astgen_unary_op(astgen, node, INK_OP_NEG);
        break;
    }
    case INK_NODE_NOT_EXPR: {
        ink_astgen_unary_op(astgen, node, INK_OP_NOT);
        break;
    }
    default:
        assert(false);
        break;
    }
}

static void ink_astgen_inline_logic(struct ink_astgen *astgen,
                                    const struct ink_ast_node *node)
{
    ink_astgen_expr(astgen, node->lhs);
    ink_astgen_add_inst(astgen, INK_OP_CONTENT_PUSH, 0);
}

static int ink_check_conditional(struct ink_astgen *astgen,
                                 const struct ink_ast_node *node)
{
    bool has_block = false;
    const struct ink_ast_seq *const seq = node->rhs->seq;

    if (!node->lhs) {
        if (!seq || seq->count == 0) {
            ink_astgen_error(astgen, INK_AST_CONDITIONAL_EMPTY, node);
            return -1;
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
                return -1;
            }
            break;
        }
        case INK_NODE_CONDITIONAL_ELSE_BRANCH: {
            /* Only the last branch can be an else. */
            if (node != last) {
                if (last->type == node->type) {
                    ink_astgen_error(astgen, INK_AST_CONDITIONAL_MULTIPLE_ELSE,
                                     node);
                    return -1;
                } else {
                    ink_astgen_error(astgen, INK_AST_CONDITIONAL_FINAL_ELSE,
                                     node);
                    return -1;
                }
            }
            break;
        }
        default:
            break;
        }
    }
    return 0;
}

static void ink_astgen_conditional_inner(struct ink_astgen *astgen,
                                         const struct ink_ast_node *node)
{
    const struct ink_ast_seq *const seq = node->seq;

    for (size_t i = 0; i < seq->count; i++) {
        const struct ink_ast_node *const node = seq->nodes[i];

        switch (node->type) {
        case INK_NODE_BLOCK: {
            ink_astgen_block_stmt(astgen, node);
            break;
        }
        case INK_NODE_CONDITIONAL_BRANCH: {
            break;
        }
        case INK_NODE_CONDITIONAL_ELSE_BRANCH: {
            break;
        }
        default:
            assert(false);
            break;
        }
    }
}

static void ink_astgen_conditional_content(struct ink_astgen *astgen,
                                           const struct ink_ast_node *node)
{
    int rc;
    size_t offset;

    rc = ink_check_conditional(astgen, node);
    if (rc < 0) {
        return;
    }
    if (node->lhs) {
        ink_astgen_expr(astgen, node->lhs);
        offset = ink_astgen_add_inst(astgen, INK_OP_JMP_F, 0xff);
        ink_astgen_conditional_inner(astgen, node->rhs);
        ink_astgen_patch_jmp(astgen, offset);
    } else {
        ink_astgen_conditional_inner(astgen, node->rhs);
    }
}

static void ink_astgen_content_expr(struct ink_astgen *astgen,
                                    const struct ink_ast_node *node)
{
    const struct ink_ast_seq *const seq = node->seq;

    for (size_t i = 0; i < seq->count; i++) {
        const struct ink_ast_node *const node = seq->nodes[i];

        switch (node->type) {
        case INK_NODE_STRING: {
            ink_astgen_string(astgen, node);
            ink_astgen_add_inst(astgen, INK_OP_CONTENT_PUSH, 0);
            break;
        }
        case INK_NODE_INLINE_LOGIC: {
            ink_astgen_inline_logic(astgen, node);
            ink_astgen_add_inst(astgen, INK_OP_CONTENT_PUSH, 0);
            break;
        }
        case INK_NODE_CONDITIONAL_CONTENT: {
            ink_astgen_conditional_content(astgen, node);
            break;
        }
        default:
            assert(false);
            break;
        }
    }
}

static void ink_astgen_var_decl(struct ink_astgen *astgen,
                                const struct ink_ast_node *node)
{
    const struct ink_symbol symbol = {
        .node = node,
        .is_const = node->type == INK_NODE_CONST_DECL,
    };
    struct ink_symtab_key key;

    ink_astgen_identifier_token(astgen, node->lhs, &key);

    if (ink_symtab_insert(&astgen->symbol_table, key, symbol) < 0) {
        ink_astgen_error(astgen, INK_AST_IDENT_REDEFINED, node->lhs);
        return;
    }
}

static void ink_astgen_content_stmt(struct ink_astgen *astgen,
                                    const struct ink_ast_node *node)
{
    ink_astgen_content_expr(astgen, node->lhs);
    ink_astgen_add_inst(astgen, INK_OP_CONTENT_POST, 0);
}

static void ink_astgen_expr_stmt(struct ink_astgen *astgen,
                                 const struct ink_ast_node *node)
{
    ink_astgen_expr(astgen, node->lhs);
    ink_astgen_add_inst(astgen, INK_OP_POP, 0);
}

static void ink_astgen_block_stmt(struct ink_astgen *astgen,
                                  const struct ink_ast_node *node)
{
    const struct ink_ast_seq *const seq = node->seq;

    for (size_t i = 0; i < seq->count; i++) {
        const struct ink_ast_node *const node = seq->nodes[i];

        switch (node->type) {
        case INK_NODE_VAR_DECL:
        case INK_NODE_CONST_DECL:
        case INK_NODE_TEMP_DECL: {
            ink_astgen_var_decl(astgen, node);
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

static void ink_astgen_file(struct ink_astgen *astgen,
                            const struct ink_ast_node *node)
{
    const struct ink_ast_seq *const seq = node->seq;

    for (size_t i = 0; i < seq->count; i++) {
        const struct ink_ast_node *const node = seq->nodes[i];

        switch (node->type) {
        case INK_NODE_BLOCK: {
            ink_astgen_block_stmt(astgen, node);
            break;
        }
        default:
            assert(false);
            break;
        }
    }

    ink_astgen_add_inst(astgen, INK_OP_RET, 0);
}

int ink_astgen(struct ink_ast *tree, struct ink_story *story, int flags)
{
    struct ink_astgen astgen;

    if (!ink_ast_error_vec_is_empty(&tree->errors)) {
        return -1;
    }

    ink_astgen_init(&astgen, tree, story);
    ink_astgen_file(&astgen, tree->root);
    ink_astgen_deinit(&astgen);

    if (!ink_ast_error_vec_is_empty(&tree->errors)) {
        ink_ast_render_errors(tree);
        return -1;
    }
    return INK_E_OK;
}
