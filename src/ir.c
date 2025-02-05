#include <stdio.h>
#include <stdlib.h>

#include "ir.h"

#define T(name, description) description,
static const char *INK_IR_TYPE_STR[] = {INK_IR_OP(T)};
#undef T

static const char *ink_ir_inst_op_strz(enum ink_ir_inst_op op)
{
    return INK_IR_TYPE_STR[op];
}

/* TODO(Brett): Make the IR rendering code better. */

static void ink_ir_dump_seq(const struct ink_ir *,
                            const struct ink_ir_inst_seq *, const char *);

static void ink_ir_dump_invalid(const struct ink_ir *ir,
                                const struct ink_ir_inst *inst, size_t index,
                                const char *prefix)
{
    printf("%s%%%zu = INVALID(0x%x)\n", prefix, index, inst->op);
}

static void ink_ir_dump_simple(const struct ink_ir *ir,
                               const struct ink_ir_inst *inst, size_t index,
                               const char *prefix)
{
    printf("%s%%%zu = %s(?)\n", prefix, index, ink_ir_inst_op_strz(inst->op));
}

static void ink_ir_dump_number(const struct ink_ir *ir,
                               const struct ink_ir_inst *inst, size_t index,
                               const char *prefix)
{
    printf("%s%%%zu = %s(%lf)\n", prefix, index, ink_ir_inst_op_strz(inst->op),
           inst->as.number);
}

static void ink_ir_dump_string(const struct ink_ir *ir,
                               const struct ink_ir_inst *inst, size_t index,
                               const char *prefix)
{
    const unsigned char *const bytes =
        &ir->string_bytes.entries[inst->as.string];

    printf("%s%%%zu = %s(`%s`)\n", prefix, index, ink_ir_inst_op_strz(inst->op),
           bytes);
}

static void ink_ir_dump_unary(const struct ink_ir *ir,
                              const struct ink_ir_inst *inst, size_t index,
                              const char *prefix)
{
    printf("%s%%%zu = %s(%%%zu)\n", prefix, index,
           ink_ir_inst_op_strz(inst->op), inst->as.unary.lhs);
}

static void ink_ir_dump_binary(const struct ink_ir *ir,
                               const struct ink_ir_inst *inst, size_t index,
                               const char *prefix)
{
    printf("%s%%%zu = %s(%%%zu, %%%zu)\n", prefix, index,
           ink_ir_inst_op_strz(inst->op), inst->as.binary.lhs,
           inst->as.binary.rhs);
}

static void ink_ir_dump_block(const struct ink_ir *ir,
                              const struct ink_ir_inst *inst, size_t index,
                              const char *prefix)
{
    printf("%s%%%zu = %s({\n", prefix, index, ink_ir_inst_op_strz(inst->op));
    ink_ir_dump_seq(ir, inst->as.block.seq, prefix);
    printf("%s})\n", prefix);
}

static void ink_ir_dump_condbr(const struct ink_ir *ir,
                               const struct ink_ir_inst *inst, size_t index,
                               const char *prefix)
{

    printf("%s%%%zu = %s(%%%zu, {\n", prefix, index,
           ink_ir_inst_op_strz(inst->op), inst->as.cond_br.payload_index);
    ink_ir_dump_seq(ir, inst->as.cond_br.then_, prefix);

    if (inst->as.cond_br.else_) {
        printf("%s}, {\n", prefix);
        ink_ir_dump_seq(ir, inst->as.cond_br.else_, prefix);
    }

    printf("%s})\n", prefix);
}

static void ink_ir_dump_seq(const struct ink_ir *ir,
                            const struct ink_ir_inst_seq *seq,
                            const char *prefix)
{
    char new_prefix[1024];
    const struct ink_ir_inst_vec *const all = &ir->instructions;

    if (prefix == NULL) {
        new_prefix[0] = '\0';
    } else {
        snprintf(new_prefix, sizeof(new_prefix), "%s%s", prefix, "  ");
    }

    for (size_t i = 0; i < seq->count; i++) {
        const size_t inst_index = seq->entries[i];
        struct ink_ir_inst *const inst = &all->entries[inst_index];

        switch (inst->op) {
        case INK_IR_INST_NUMBER: {
            ink_ir_dump_number(ir, inst, inst_index, new_prefix);
            break;
        }
        case INK_IR_INST_STRING: {
            ink_ir_dump_string(ir, inst, inst_index, new_prefix);
            break;
        }
        case INK_IR_INST_LOAD:
        case INK_IR_INST_CONTENT_PUSH:
        case INK_IR_INST_BR:
        case INK_IR_INST_NEG: {
            ink_ir_dump_unary(ir, inst, inst_index, new_prefix);
            break;
        }
        case INK_IR_INST_BLOCK: {
            ink_ir_dump_block(ir, inst, inst_index, new_prefix);
            break;
        }
        case INK_IR_INST_CONDBR: {
            ink_ir_dump_condbr(ir, inst, inst_index, new_prefix);
            break;
        }
        case INK_IR_INST_STORE:
        case INK_IR_INST_ADD:
        case INK_IR_INST_SUB:
        case INK_IR_INST_MUL:
        case INK_IR_INST_DIV:
        case INK_IR_INST_MOD:
        case INK_IR_INST_CMP_EQ:
        case INK_IR_INST_CMP_NEQ:
        case INK_IR_INST_CMP_LT:
        case INK_IR_INST_CMP_LTE:
        case INK_IR_INST_CMP_GT:
        case INK_IR_INST_CMP_GTE: {
            ink_ir_dump_binary(ir, inst, inst_index, new_prefix);
            break;
        }
        case INK_IR_INST_TRUE:
        case INK_IR_INST_FALSE:
        case INK_IR_INST_DONE:
        case INK_IR_INST_END:
        case INK_IR_INST_ALLOC:
        case INK_IR_INST_RET: {
            ink_ir_dump_simple(ir, inst, inst_index, new_prefix);
            break;
        }
        default:
            ink_ir_dump_invalid(ir, inst, inst_index, new_prefix);
            break;
        }
    }
}

void ink_ir_dump(const struct ink_ir *ir)
{
    ink_ir_dump_seq(ir, ir->sequence_list_tail, NULL);
}

void ink_ir_init(struct ink_ir *ir)
{
    ink_ir_byte_vec_init(&ir->string_bytes);
    ink_ir_inst_vec_init(&ir->instructions);
    ir->sequence_list_head = NULL;
    ir->sequence_list_tail = NULL;
}

void ink_ir_deinit(struct ink_ir *ir)
{
    ink_ir_byte_vec_deinit(&ir->string_bytes);
    ink_ir_inst_vec_deinit(&ir->instructions);
    ir->sequence_list_tail = NULL;

    while (ir->sequence_list_head != NULL) {
        struct ink_ir_inst_seq *const seq = ir->sequence_list_head;

        ir->sequence_list_head = ir->sequence_list_head->next;
        free(seq);
    }
}
