#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "astgen.h"
#include "common.h"
#include "hashmap.h"
#include "object.h"
#include "opcode.h"
#include "source.h"
#include "story.h"
#include "tree.h"
#include "vec.h"

#define INK_NUMBER_BUFSZ 24

struct ink_symbol_table_key {
    const unsigned char *bytes;
    size_t length;
};

struct ink_symbol {
    bool is_const;
    const struct ink_syntax_node *node;
};

INK_VEC_T(ink_byte_vec, unsigned char)
INK_HASHMAP_T(ink_symbol_table, struct ink_symbol_table_key, struct ink_symbol)

struct ink_astgen {
    struct ink_story *story;
    struct ink_syntax_tree *tree;
    struct ink_symbol_table symbol_table;
};

static unsigned int ink_symbol_table_hash(const void *bytes, size_t length)
{
    const struct ink_symbol_table_key *const key = bytes;

    return ink_fnv32a(key->bytes, key->length);
}

static bool ink_symbol_table_cmp(const void *a, size_t a_length, const void *b,
                                 size_t b_length)
{
    const struct ink_symbol_table_key *const key_1 = a;
    const struct ink_symbol_table_key *const key_2 = b;

    if (key_1->length != key_2->length) {
        return false;
    }
    return memcmp(key_1->bytes, key_2->bytes, key_1->length) == 0;
}

static void ink_astgen_init(struct ink_astgen *astgen,
                            struct ink_syntax_tree *tree,
                            struct ink_story *story)
{
    astgen->story = story;
    astgen->tree = tree;

    ink_symbol_table_init(&astgen->symbol_table, 80ul, ink_symbol_table_hash,
                          ink_symbol_table_cmp);
}

static void ink_astgen_deinit(struct ink_astgen *astgen)
{
    ink_symbol_table_deinit(&astgen->symbol_table);
}

static struct ink_object *
ink_astgen_create_number(struct ink_astgen *astgen,
                         const struct ink_syntax_node *node)
{
    char buf[INK_NUMBER_BUFSZ + 1];
    const struct ink_source *const source = astgen->tree->source;
    const unsigned char *const chars = &source->bytes[node->start_offset];
    const size_t length = node->end_offset - node->start_offset;

    if (length > INK_NUMBER_BUFSZ) {
        return NULL;
    }

    memcpy(buf, chars, length);
    buf[length] = '\0';
    return ink_number_new(astgen->story, strtod(buf, NULL));
}

static void ink_astgen_add_inst(struct ink_astgen *astgen,
                                enum ink_vm_opcode opcode, unsigned char arg)
{
    struct ink_bytecode_vec *const code = &astgen->story->code;

    ink_bytecode_vec_push(code, (unsigned char)opcode);
    ink_bytecode_vec_push(code, arg);
}

static void ink_astgen_add_const(struct ink_astgen *astgen,
                                 struct ink_object *object)
{
    struct ink_object_vec *const consts = &astgen->story->constants;

    ink_object_vec_push(consts, object);
}

static void ink_astgen_identifier_token(struct ink_astgen *astgen,
                                        const struct ink_syntax_node *node,
                                        struct ink_symbol_table_key *token)
{
    const struct ink_source *const source = astgen->tree->source;

    token->bytes = source->bytes + node->start_offset;
    token->length = node->end_offset - node->start_offset;
}

static void ink_astgen_number(struct ink_astgen *astgen,
                              const struct ink_syntax_node *node)
{
    const size_t id = astgen->story->constants.count;
    struct ink_object *const obj = ink_astgen_create_number(astgen, node);

    if (!obj) {
        return;
    }

    ink_astgen_add_const(astgen, obj);
    ink_astgen_add_inst(astgen, INK_OP_LOAD_CONST, (unsigned char)id);
}

static void ink_astgen_identifier(struct ink_astgen *astgen,
                                  const struct ink_syntax_node *node)
{
    const struct ink_source *source = astgen->tree->source;
    const struct ink_symbol_table_key key = {
        .bytes = source->bytes + node->start_offset,
        .length = node->end_offset - node->start_offset,
    };
    struct ink_symbol symbol;

    if (ink_symbol_table_lookup(&astgen->symbol_table, key, &symbol) < 0) {
        struct ink_syntax_error err = {
            .type = INK_SYNTAX_IDENT_UNKNOWN,
            .source_start = node->start_offset,
            .source_end = node->end_offset,
        };

        ink_syntax_error_vec_push(&astgen->tree->errors, err);
        return;
    }
}

static void ink_astgen_expr(struct ink_astgen *astgen,
                            const struct ink_syntax_node *node)
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
    case INK_NODE_ADD_EXPR: {
        ink_astgen_expr(astgen, node->lhs);
        ink_astgen_expr(astgen, node->rhs);
        ink_astgen_add_inst(astgen, INK_OP_ADD, 0);
        break;
    }
    case INK_NODE_SUB_EXPR: {
        ink_astgen_expr(astgen, node->lhs);
        ink_astgen_expr(astgen, node->rhs);
        ink_astgen_add_inst(astgen, INK_OP_SUB, 0);
        break;
    }
    case INK_NODE_MUL_EXPR: {
        ink_astgen_expr(astgen, node->lhs);
        ink_astgen_expr(astgen, node->rhs);
        ink_astgen_add_inst(astgen, INK_OP_MUL, 0);
        break;
    }
    case INK_NODE_DIV_EXPR: {
        ink_astgen_expr(astgen, node->lhs);
        ink_astgen_expr(astgen, node->rhs);
        ink_astgen_add_inst(astgen, INK_OP_DIV, 0);
        break;
    }
    case INK_NODE_MOD_EXPR: {
        ink_astgen_expr(astgen, node->lhs);
        ink_astgen_expr(astgen, node->rhs);
        ink_astgen_add_inst(astgen, INK_OP_MOD, 0);
        break;
    }
    case INK_NODE_NEGATE_EXPR: {
        ink_astgen_expr(astgen, node->lhs);
        ink_astgen_add_inst(astgen, INK_OP_NEG, 0);
        break;
    }
    default:
        assert(false);
        break;
    }
}

static void ink_astgen_var_decl(struct ink_astgen *astgen,
                                const struct ink_syntax_node *node)
{
    const struct ink_symbol symbol = {
        .node = node,
        .is_const = node->type == INK_NODE_CONST_DECL,
    };
    struct ink_symbol_table_key key;

    ink_astgen_identifier_token(astgen, node->lhs, &key);

    if (ink_symbol_table_insert(&astgen->symbol_table, key, symbol) < 0) {
        struct ink_syntax_error err = {
            .type = INK_SYNTAX_IDENT_REDEFINED,
            .source_start = node->lhs->start_offset,
            .source_end = node->lhs->end_offset,
        };

        ink_syntax_error_vec_push(&astgen->tree->errors, err);
    }
}

static void ink_astgen_expr_stmt(struct ink_astgen *astgen,
                                 const struct ink_syntax_node *node)
{
    ink_astgen_expr(astgen, node->lhs);
    ink_astgen_add_inst(astgen, INK_OP_POP, 0);
}

static void ink_astgen_block_stmt(struct ink_astgen *astgen,
                                  const struct ink_syntax_node *node)
{
    const struct ink_syntax_seq *const seq = node->seq;

    for (size_t i = 0; i < seq->count; i++) {
        const struct ink_syntax_node *const node = seq->nodes[i];

        switch (node->type) {
        case INK_NODE_VAR_DECL:
        case INK_NODE_CONST_DECL:
        case INK_NODE_TEMP_DECL: {
            ink_astgen_var_decl(astgen, node);
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
                            const struct ink_syntax_node *node)
{
    const struct ink_syntax_seq *const seq = node->seq;

    for (size_t i = 0; i < seq->count; i++) {
        const struct ink_syntax_node *const node = seq->nodes[i];

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

int ink_astgen(struct ink_syntax_tree *tree, struct ink_story *story, int flags)
{
    struct ink_astgen astgen;

    if (!ink_syntax_error_vec_is_empty(&tree->errors)) {
        return -1;
    }

    ink_astgen_init(&astgen, tree, story);
    ink_astgen_file(&astgen, tree->root);
    ink_astgen_deinit(&astgen);

    if (!ink_syntax_error_vec_is_empty(&tree->errors)) {
        ink_syntax_tree_render_errors(tree);
        return -1;
    }
    return INK_E_OK;
}
