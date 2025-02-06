#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen.h"
#include "ir.h"
#include "object.h"
#include "opcode.h"
#include "story.h"

#define INK_CODEGEN_TODO(msg)                                                  \
    do {                                                                       \
        printf("TODO(codegen): %s\n", msg);                                    \
        exit(EXIT_FAILURE);                                                    \
    } while (0)

struct ink_codegen {
    const struct ink_ir *ir;
    struct ink_story *story;
};

static void ink_codegen_init(struct ink_codegen *codegen,
                             const struct ink_ir *ir, struct ink_story *story)
{
    codegen->ir = ir;
    codegen->story = story;
}

static void ink_codegen_deinit(struct ink_codegen *codegen)
{
}

static void ink_codegen_fail(struct ink_codegen *codegen, const char *msg)
{
    fprintf(stderr, "%s", msg);
    exit(EXIT_FAILURE);
}

static void ink_codegen_add_inst(struct ink_codegen *codegen,
                                 enum ink_vm_opcode opcode, uint8_t operand)
{
    struct ink_byte_vec *const code = &codegen->story->code;

    ink_byte_vec_push(code, (uint8_t)opcode);
    ink_byte_vec_push(code, operand);
}

static void ink_codegen_add_const(struct ink_codegen *codegen,
                                  struct ink_object *object)
{
    struct ink_story *const story = codegen->story;

    ink_story_constant_add(story, object);
}

static void ink_codegen_ir_number(struct ink_codegen *codegen,
                                  const struct ink_ir_inst *inst)
{
    struct ink_story *const story = codegen->story;
    const size_t const_index = story->constants.count;
    const double value = inst->as.number;
    struct ink_object *const obj = ink_number_new(story, value);

    if (!obj) {
        ink_codegen_fail(codegen,
                         "Could not create runtime object for number.");
        return;
    }

    ink_codegen_add_const(codegen, obj);
    ink_codegen_add_inst(codegen, INK_OP_LOAD_CONST, (uint8_t)const_index);
}

static void ink_codegen_ir_string(struct ink_codegen *codegen,
                                  const struct ink_ir_inst *inst)
{
    struct ink_story *const story = codegen->story;
    const struct ink_ir *const ir = codegen->ir;
    const struct ink_ir_byte_vec *const string_bytes = &ir->string_bytes;
    const size_t str_index = inst->as.string;
    const uint8_t *str_chars = &string_bytes->entries[str_index];
    const size_t const_index = story->constants.count;
    struct ink_object *const obj =
        ink_string_new(story, str_chars, strlen((char *)str_chars));

    if (!obj) {
        ink_codegen_fail(codegen,
                         "Could not create runtime object for string.");
        return;
    }

    ink_codegen_add_const(codegen, obj);
    ink_codegen_add_inst(codegen, INK_OP_LOAD_CONST, (uint8_t)const_index);
}

static void ink_codegen_ir_true(struct ink_codegen *codegen,
                                const struct ink_ir_inst *inst)
{
    ink_codegen_add_inst(codegen, INK_OP_TRUE, 0);
}

static void ink_codegen_ir_false(struct ink_codegen *codegen,
                                 const struct ink_ir_inst *inst)
{
    ink_codegen_add_inst(codegen, INK_OP_FALSE, 0);
}

static void ink_codegen_ir_arithmetic(struct ink_codegen *codegen,
                                      const struct ink_ir_inst *inst)
{
    switch (inst->op) {
    case INK_IR_INST_ADD: {
        ink_codegen_add_inst(codegen, INK_OP_ADD, 0);
        break;
    }
    case INK_IR_INST_SUB: {
        ink_codegen_add_inst(codegen, INK_OP_SUB, 0);
        break;
    }
    case INK_IR_INST_MUL: {
        ink_codegen_add_inst(codegen, INK_OP_MUL, 0);
        break;
    }
    case INK_IR_INST_DIV: {
        ink_codegen_add_inst(codegen, INK_OP_DIV, 0);
        break;
    }
    default:
        assert(false);
        break;
    }
}

static void ink_codegen_ir_cmp(struct ink_codegen *codegen,
                               const struct ink_ir_inst *inst)
{
    switch (inst->op) {
    case INK_IR_INST_CMP_EQ: {
        ink_codegen_add_inst(codegen, INK_OP_CMP_EQ, 0);
        break;
    }
    case INK_IR_INST_CMP_NEQ: {
        ink_codegen_add_inst(codegen, INK_OP_CMP_EQ, 0);
        ink_codegen_add_inst(codegen, INK_OP_NOT, 0);
        break;
    }
    case INK_IR_INST_CMP_LT: {
        ink_codegen_add_inst(codegen, INK_OP_CMP_LT, 0);
        break;
    }
    case INK_IR_INST_CMP_LTE: {
        ink_codegen_add_inst(codegen, INK_OP_CMP_LTE, 0);
        break;
    }
    case INK_IR_INST_CMP_GT: {
        ink_codegen_add_inst(codegen, INK_OP_CMP_GT, 0);
        break;
    }
    case INK_IR_INST_CMP_GTE: {
        ink_codegen_add_inst(codegen, INK_OP_CMP_GTE, 0);
        break;
    }
    default:
        assert(false);
        break;
    }
}

static void ink_codegen_body(struct ink_codegen *codegen)
{
    const struct ink_ir *const ir = codegen->ir;
    const struct ink_ir_inst_vec *const inst_list = &ir->instructions;

    for (size_t i = 0; i < inst_list->count; i++) {
        struct ink_ir_inst *const inst = &ir->instructions.entries[i];

        switch (inst->op) {
        case INK_IR_INST_ALLOC: {
            INK_CODEGEN_TODO("handle ALLOC");
            break;
        }
        case INK_IR_INST_LOAD: {
            INK_CODEGEN_TODO("handle LOAD");
            break;
        }
        case INK_IR_INST_STORE: {
            INK_CODEGEN_TODO("handle STORE");
            break;
        }
        case INK_IR_INST_NUMBER: {
            ink_codegen_ir_number(codegen, inst);
            break;
        }
        case INK_IR_INST_STRING: {
            ink_codegen_ir_string(codegen, inst);
            break;
        }
        case INK_IR_INST_TRUE: {
            ink_codegen_ir_true(codegen, inst);
            break;
        }
        case INK_IR_INST_FALSE: {
            ink_codegen_ir_false(codegen, inst);
            break;
        }
        case INK_IR_INST_ADD:
        case INK_IR_INST_SUB:
        case INK_IR_INST_MUL:
        case INK_IR_INST_DIV:
        case INK_IR_INST_MOD: {
            ink_codegen_ir_arithmetic(codegen, inst);
            break;
        }
        case INK_IR_INST_NEG: {
            INK_CODEGEN_TODO("handle NEG");
            break;
        }
        case INK_IR_INST_CMP_EQ:
        case INK_IR_INST_CMP_NEQ:
        case INK_IR_INST_CMP_LT:
        case INK_IR_INST_CMP_LTE:
        case INK_IR_INST_CMP_GT:
        case INK_IR_INST_CMP_GTE: {
            ink_codegen_ir_cmp(codegen, inst);
            break;
        }
        case INK_IR_INST_BOOL_NOT: {
            INK_CODEGEN_TODO("handle BOOL_NOT");
            break;
        }
        case INK_IR_INST_BLOCK: {
            INK_CODEGEN_TODO("handle BOOL_BLOCK");
            break;
        }
        case INK_IR_INST_CONDBR: {
            INK_CODEGEN_TODO("handle CONDBR");
            break;
        }
        case INK_IR_INST_BR: {
            INK_CODEGEN_TODO("handle BR");
            break;
        }
        case INK_IR_INST_SWITCH_BR: {
            INK_CODEGEN_TODO("handle SWITCH_BR");
            break;
        }
        case INK_IR_INST_SWITCH_CASE: {
            INK_CODEGEN_TODO("handle SWITCH_CASE");
            break;
        }
        case INK_IR_INST_CONTENT_PUSH: {
            INK_CODEGEN_TODO("handle CONTENT_PUSH");
            break;
        }
        case INK_IR_INST_DONE: {
            INK_CODEGEN_TODO("handle DONE");
            break;
        }
        case INK_IR_INST_END: {
            INK_CODEGEN_TODO("handle END");
            break;
        }
        case INK_IR_INST_RET: {
            INK_CODEGEN_TODO("handle RET");
            break;
        }
        }
    }
}

int ink_codegen(const struct ink_ir *ircode, struct ink_story *story, int flags)
{
    struct ink_codegen codegen;

    ink_codegen_init(&codegen, ircode, story);
    ink_codegen_body(&codegen);
    ink_codegen_deinit(&codegen);
    return INK_E_OK;
}
