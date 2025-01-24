#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "astgen.h"
#include "common.h"
#include "object.h"
#include "opcode.h"
#include "source.h"
#include "story.h"
#include "tree.h"

#define INK_NUMBER_BUFSZ 24

struct ink_astgen {
    struct ink_syntax_tree *tree;
    struct ink_story *story;
};

static void ink_astgen_init(struct ink_astgen *astgen,
                            struct ink_syntax_tree *tree,
                            struct ink_story *story)
{
    astgen->story = story;
    astgen->tree = tree;
}

static void ink_astgen_deinit(struct ink_astgen *astgen)
{
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
    return INK_E_OK;
}
