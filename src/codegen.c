#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen.h"
#include "hashmap.h"
#include "ir.h"
#include "object.h"
#include "opcode.h"
#include "story.h"

#define INK_CODEGEN_TODO(msg)                                                  \
    do {                                                                       \
        printf("TODO(codegen): %s\n", msg);                                    \
        exit(EXIT_FAILURE);                                                    \
    } while (0)

struct ink_inst_map_data {
    size_t stack_slot;
    size_t const_slot;
    struct ink_object *global_name;
};

struct ink_inst_map_key {
    size_t inst_index;
};

INK_HASHMAP_T(ink_inst_map, struct ink_inst_map_key, struct ink_inst_map_data)

static uint32_t ink_inst_map_hash(const void *bytes, size_t length)
{
    return ink_fnv32a(bytes, length);
}

static bool ink_inst_map_cmp(const void *a, size_t alen, const void *b,
                             size_t blen)
{
    const struct ink_inst_map_key *const key_1 = a;
    const struct ink_inst_map_key *const key_2 = b;

    return key_1->inst_index == key_2->inst_index;
}

struct ink_codegen {
    const struct ink_ir *ir;
    struct ink_story *story;
    struct ink_content_path *current_path;
    struct ink_inst_map inst_map;
};

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

    codegen->current_path->code_length += 2;
}

static void ink_codegen_add_const(struct ink_codegen *codegen,
                                  struct ink_object *obj)
{
    ink_object_vec_push(&codegen->story->constants, obj);
}

static struct ink_object *ink_codegen_add_str(struct ink_codegen *codegen,
                                              size_t str_index)
{
    struct ink_story *const story = codegen->story;
    const struct ink_ir *const ir = codegen->ir;
    const struct ink_ir_byte_vec *const string_bytes = &ir->string_bytes;
    const uint8_t *bytes = &string_bytes->entries[str_index];
    const size_t length = strlen((char *)bytes);

    return ink_string_new(story, bytes, length);
}

static void ink_codegen_patch_jump(struct ink_codegen *codegen,
                                   size_t inst_offset)
{
    struct ink_byte_vec *const code = &codegen->story->code;

    code->entries[inst_offset + 1] = (uint8_t)code->count;
}

static void ink_codegen_body(struct ink_codegen *,
                             const struct ink_ir_inst_seq *);

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
    ink_codegen_add_inst(codegen, INK_OP_CONST, (uint8_t)const_index);
}

static void ink_codegen_ir_string(struct ink_codegen *codegen,
                                  const struct ink_ir_inst *inst)
{
    struct ink_story *const story = codegen->story;
    const size_t const_index = story->constants.count;
    const size_t str_index = inst->as.string;
    struct ink_object *const str_obj = ink_codegen_add_str(codegen, str_index);

    if (!str_obj) {
        ink_codegen_fail(codegen,
                         "Could not create runtime object for string.");
        return;
    }

    ink_codegen_add_const(codegen, str_obj);
    ink_codegen_add_inst(codegen, INK_OP_CONST, (uint8_t)const_index);
}

static void ink_codegen_ir_arithmetic(struct ink_codegen *codegen,
                                      const struct ink_ir_inst *inst)
{
    switch (inst->op) {
    case INK_IR_INST_ADD:
        ink_codegen_add_inst(codegen, INK_OP_ADD, 0);
        break;
    case INK_IR_INST_SUB:
        ink_codegen_add_inst(codegen, INK_OP_SUB, 0);
        break;
    case INK_IR_INST_MUL:
        ink_codegen_add_inst(codegen, INK_OP_MUL, 0);
        break;
    case INK_IR_INST_DIV:
        ink_codegen_add_inst(codegen, INK_OP_DIV, 0);
        break;
    default:
        assert(false);
        break;
    }
}

static void ink_codegen_ir_cmp(struct ink_codegen *codegen,
                               const struct ink_ir_inst *inst)
{
    switch (inst->op) {
    case INK_IR_INST_CMP_EQ:
        ink_codegen_add_inst(codegen, INK_OP_CMP_EQ, 0);
        break;
    case INK_IR_INST_CMP_NEQ:
        ink_codegen_add_inst(codegen, INK_OP_CMP_EQ, 0);
        ink_codegen_add_inst(codegen, INK_OP_NOT, 0);
        break;
    case INK_IR_INST_CMP_LT:
        ink_codegen_add_inst(codegen, INK_OP_CMP_LT, 0);
        break;
    case INK_IR_INST_CMP_LTE:
        ink_codegen_add_inst(codegen, INK_OP_CMP_LTE, 0);
        break;
    case INK_IR_INST_CMP_GT:
        ink_codegen_add_inst(codegen, INK_OP_CMP_GT, 0);
        break;
    case INK_IR_INST_CMP_GTE:
        ink_codegen_add_inst(codegen, INK_OP_CMP_GTE, 0);
        break;
    default:
        assert(false);
        break;
    }
}

static void ink_codegen_ir_alloc(struct ink_codegen *codegen,
                                 const struct ink_ir_inst *inst, size_t index)
{
    const size_t stack_slot = codegen->current_path->locals_count;
    const struct ink_inst_map_key key = {index};
    const struct ink_inst_map_data value = {.stack_slot = stack_slot};

    ink_inst_map_insert(&codegen->inst_map, key, value);
    codegen->current_path->locals_count++;
}

static void ink_codegen_ir_load(struct ink_codegen *codegen,
                                const struct ink_ir_inst *inst)
{
    struct ink_inst_map_data value;
    const size_t payload_index = inst->as.unary.lhs;
    const struct ink_ir *const ir = codegen->ir;
    const struct ink_ir_inst *const source_inst =
        &ir->instructions.entries[payload_index];
    const struct ink_inst_map_key key = {payload_index};

    ink_inst_map_lookup(&codegen->inst_map, key, &value);

    switch (source_inst->op) {
    case INK_IR_INST_ALLOC:
        ink_codegen_add_inst(codegen, INK_OP_LOAD, (uint8_t)value.stack_slot);
        break;
    case INK_IR_INST_DECL_PARAM:
        ink_codegen_add_inst(codegen, INK_OP_LOAD, (uint8_t)value.stack_slot);
        break;
    case INK_IR_INST_DECL_VAR:
        ink_codegen_add_inst(codegen, INK_OP_LOAD_GLOBAL,
                             (uint8_t)value.const_slot);
        break;
    default:
        break;
    }
}

static void ink_codegen_ir_store(struct ink_codegen *codegen,
                                 const struct ink_ir_inst *inst)
{
    struct ink_inst_map_data value;
    const size_t payload_index = inst->as.unary.lhs;
    const struct ink_ir *const ir = codegen->ir;
    const struct ink_ir_inst *const arg =
        &ir->instructions.entries[payload_index];
    const struct ink_inst_map_key key = {payload_index};

    ink_inst_map_lookup(&codegen->inst_map, key, &value);

    if (arg->op == INK_IR_INST_ALLOC) {
        ink_codegen_add_inst(codegen, INK_OP_STORE, (uint8_t)value.stack_slot);
    } else {
        ink_codegen_add_inst(codegen, INK_OP_STORE_GLOBAL,
                             (uint8_t)value.const_slot);
    }
}

static void ink_codegen_ir_ret(struct ink_codegen *codegen,
                               const struct ink_ir_inst *inst)
{
    ink_codegen_add_inst(codegen, INK_OP_RET, 0);
}

static void ink_codegen_ir_block(struct ink_codegen *codegen,
                                 const struct ink_ir_inst *inst,
                                 size_t inst_index)
{
    struct ink_ir_inst_seq *const block_list = inst->as.block.seq;

    ink_codegen_body(codegen, block_list);
}

static void ink_codegen_ir_condbr(struct ink_codegen *codegen,
                                  const struct ink_ir_inst *inst,
                                  size_t inst_index)
{
    struct ink_content_path *path = codegen->current_path;
    struct ink_ir_inst_seq *const then_list = inst->as.cond_br.then_;
    struct ink_ir_inst_seq *const else_list = inst->as.cond_br.else_;
    const size_t then_offset = path->code_length;

    ink_codegen_add_inst(codegen, INK_OP_JMP_F, 0xFF);
    ink_codegen_add_inst(codegen, INK_OP_POP, 0x00);
    ink_codegen_body(codegen, then_list);

    const size_t else_offset = path->code_length;

    ink_codegen_add_inst(codegen, INK_OP_JMP, 0xFF);
    ink_codegen_patch_jump(codegen, then_offset);
    ink_codegen_add_inst(codegen, INK_OP_POP, 0x00);

    if (else_list) {
        ink_codegen_body(codegen, else_list);
    }

    ink_codegen_patch_jump(codegen, else_offset);
}

static void ink_codegen_ir_br(struct ink_codegen *codegen,
                              const struct ink_ir_inst *inst, size_t inst_index)
{
}

static void ink_codegen_ir_call(struct ink_codegen *codegen,
                                const struct ink_ir_inst *inst,
                                size_t inst_index)
{
    struct ink_story *const story = codegen->story;
    const size_t const_index = story->constants.count;
    const size_t name_index = inst->as.activation.callee_index;
    const struct ink_ir_inst_seq *const args = inst->as.activation.args;
    struct ink_object *const str_obj = ink_codegen_add_str(codegen, name_index);

    if (!str_obj) {
        ink_codegen_fail(codegen,
                         "Could not create runtime object for string.");
        return;
    }

    /* TODO(Brett): This emits a constant per invocation, which is not ideal. */
    ink_codegen_add_const(codegen, str_obj);
    ink_codegen_add_inst(codegen, INK_OP_CONST, (uint8_t)const_index);

    if (args) {
        ink_codegen_add_inst(codegen, INK_OP_CALL, (uint8_t)args->count);
    } else {
        ink_codegen_add_inst(codegen, INK_OP_CALL, 0);
    }
}

static void ink_codegen_ir_divert(struct ink_codegen *codegen,
                                  const struct ink_ir_inst *inst,
                                  size_t inst_index)
{
    struct ink_story *const story = codegen->story;
    const size_t const_index = story->constants.count;
    const size_t name_index = inst->as.activation.callee_index;
    const struct ink_ir_inst_seq *const args = inst->as.activation.args;
    struct ink_object *const str_obj = ink_codegen_add_str(codegen, name_index);

    if (!str_obj) {
        ink_codegen_fail(codegen,
                         "Could not create runtime object for string.");
        return;
    }

    /* TODO(Brett): This emits a constant per invocation, which is not ideal. */
    ink_codegen_add_const(codegen, str_obj);
    ink_codegen_add_inst(codegen, INK_OP_CONST, (uint8_t)const_index);

    if (args) {
        ink_codegen_add_inst(codegen, INK_OP_DIVERT, (uint8_t)args->count);
    } else {
        ink_codegen_add_inst(codegen, INK_OP_DIVERT, 0);
    }
}

static void ink_codegen_ir_param(struct ink_codegen *codegen,
                                 const struct ink_ir_inst *inst,
                                 size_t inst_index)
{
    const size_t stack_slot = codegen->current_path->locals_count;
    const struct ink_inst_map_key key = {inst_index};
    const struct ink_inst_map_data value = {.stack_slot = stack_slot};

    ink_inst_map_insert(&codegen->inst_map, key, value);
    codegen->current_path->locals_count++;
}

static void ink_codegen_ir_var(struct ink_codegen *codegen,
                               const struct ink_ir_inst *inst, size_t index)
{
    struct ink_story *const story = codegen->story;
    struct ink_object_vec *const const_list = &story->constants;
    struct ink_object *const globals_table = story->globals;
    const size_t name_index = inst->as.var_decl.name_offset;
    const size_t const_slot = const_list->count;
    struct ink_object *const var_name =
        ink_codegen_add_str(codegen, name_index);

    if (!var_name) {
        return;
    }

    ink_object_vec_push(const_list, var_name);
    ink_table_insert(story, globals_table, var_name, NULL);

    const struct ink_inst_map_key key = {index};
    const struct ink_inst_map_data value = {
        .global_name = var_name,
        .const_slot = const_slot,
    };

    ink_inst_map_insert(&codegen->inst_map, key, value);
}

static void ink_codegen_ir_knot(struct ink_codegen *codegen,
                                const struct ink_ir_inst *inst)
{
    const size_t name_index = inst->as.knot_decl.name_offset;
    const struct ink_ir_inst_seq *body = inst->as.knot_decl.body;
    struct ink_story *const story = codegen->story;
    struct ink_byte_vec *const code = &story->code;
    struct ink_object *const paths_table = story->paths;
    struct ink_object *const path_name =
        ink_codegen_add_str(codegen, name_index);

    if (!path_name) {
        return;
    }

    struct ink_object *const path_obj = ink_content_path_new(story, path_name);
    if (!path_obj) {
        return;
    }

    ink_table_insert(story, paths_table, path_name, path_obj);

    if (codegen->current_path) {
        codegen->current_path->code_length =
            (uint32_t)(code->count - codegen->current_path->code_offset);
    }

    codegen->current_path = INK_OBJ_AS_CONTENT_PATH(path_obj);
    codegen->current_path->code_offset = (uint32_t)(code->count);
    ink_codegen_body(codegen, body);
}

static void ink_codegen_ir_check_result(struct ink_codegen *codegen,
                                        const struct ink_ir_inst *inst)
{
    ink_codegen_add_inst(codegen, INK_OP_POP, 0);
}

static void ink_codegen_ir_content_push(struct ink_codegen *codegen,
                                        const struct ink_ir_inst *inst)
{
    ink_codegen_add_inst(codegen, INK_OP_CONTENT_PUSH, 0);
}

static void ink_codegen_ir_content_flush(struct ink_codegen *codegen,
                                         const struct ink_ir_inst *inst)
{
    ink_codegen_add_inst(codegen, INK_OP_CONTENT_FLUSH, 0);
}

static void ink_codegen_body(struct ink_codegen *codegen,
                             const struct ink_ir_inst_seq *body_list)
{
    const struct ink_ir *const ir = codegen->ir;
    const struct ink_ir_inst_vec *const inst_list = &ir->instructions;

    for (size_t i = 0; i < body_list->count; i++) {
        const size_t inst_index = body_list->entries[i];
        struct ink_ir_inst *const inst = &inst_list->entries[inst_index];

        switch (inst->op) {
        case INK_IR_INST_ALLOC:
            ink_codegen_ir_alloc(codegen, inst, inst_index);
            break;
        case INK_IR_INST_LOAD:
            ink_codegen_ir_load(codegen, inst);
            break;
        case INK_IR_INST_STORE:
            ink_codegen_ir_store(codegen, inst);
            break;
        case INK_IR_INST_NUMBER:
            ink_codegen_ir_number(codegen, inst);
            break;
        case INK_IR_INST_STRING:
            ink_codegen_ir_string(codegen, inst);
            break;
        case INK_IR_INST_TRUE:
            ink_codegen_ir_true(codegen, inst);
            break;
        case INK_IR_INST_FALSE:
            ink_codegen_ir_false(codegen, inst);
            break;
        case INK_IR_INST_ADD:
        case INK_IR_INST_SUB:
        case INK_IR_INST_MUL:
        case INK_IR_INST_DIV:
        case INK_IR_INST_MOD:
            ink_codegen_ir_arithmetic(codegen, inst);
            break;
        case INK_IR_INST_NEG:
            INK_CODEGEN_TODO("handle NEG");
            break;
        case INK_IR_INST_CMP_EQ:
        case INK_IR_INST_CMP_NEQ:
        case INK_IR_INST_CMP_LT:
        case INK_IR_INST_CMP_LTE:
        case INK_IR_INST_CMP_GT:
        case INK_IR_INST_CMP_GTE:
            ink_codegen_ir_cmp(codegen, inst);
            break;
        case INK_IR_INST_BOOL_NOT:
            INK_CODEGEN_TODO("handle BOOL_NOT");
            break;
        case INK_IR_INST_BLOCK:
            ink_codegen_ir_block(codegen, inst, inst_index);
            break;
        case INK_IR_INST_CONDBR:
            ink_codegen_ir_condbr(codegen, inst, inst_index);
            break;
        case INK_IR_INST_BR:
            ink_codegen_ir_br(codegen, inst, inst_index);
            break;
        case INK_IR_INST_SWITCH_BR:
            INK_CODEGEN_TODO("handle SWITCH_BR");
            break;
        case INK_IR_INST_SWITCH_CASE:
            INK_CODEGEN_TODO("handle SWITCH_CASE");
            break;
        case INK_IR_INST_CONTENT_PUSH:
            ink_codegen_ir_content_push(codegen, inst);
            break;
        case INK_IR_INST_CONTENT_FLUSH:
            ink_codegen_ir_content_flush(codegen, inst);
            break;
        case INK_IR_INST_CHECK_RESULT:
            ink_codegen_ir_check_result(codegen, inst);
            break;
        case INK_IR_INST_DONE:
            INK_CODEGEN_TODO("handle DONE");
            break;
        case INK_IR_INST_END:
            INK_CODEGEN_TODO("handle END");
            break;
        case INK_IR_INST_RET:
        case INK_IR_INST_RET_IMPLICIT:
            ink_codegen_ir_ret(codegen, inst);
            break;
        case INK_IR_INST_DECL_VAR:
            ink_codegen_ir_var(codegen, inst, inst_index);
            break;
        case INK_IR_INST_DECL_PARAM:
            ink_codegen_ir_param(codegen, inst, inst_index);
            break;
        case INK_IR_INST_DIVERT:
            ink_codegen_ir_divert(codegen, inst, inst_index);
            break;
        case INK_IR_INST_CALL:
            ink_codegen_ir_call(codegen, inst, inst_index);
            break;
        default:
            INK_CODEGEN_TODO("unhandled ir code");
            break;
        }
    }
}

static void ink_codegen_file(struct ink_codegen *codegen)
{
    const struct ink_ir *const file_ir = codegen->ir;
    const struct ink_ir_inst_seq *const decl_list = file_ir->sequence_list_tail;
    const struct ink_ir_inst_vec *const inst_list = &file_ir->instructions;

    for (size_t i = 0; i < decl_list->count; i++) {
        const size_t inst_index = decl_list->entries[i];
        struct ink_ir_inst *const inst = &inst_list->entries[inst_index];

        ink_codegen_ir_knot(codegen, inst);
    }
}

static void ink_codegen_init(struct ink_codegen *codegen,
                             const struct ink_ir *ir, struct ink_story *story)
{
    codegen->ir = ir;
    codegen->story = story;
    codegen->current_path = NULL;

    ink_inst_map_init(&codegen->inst_map, 80ul, ink_inst_map_hash,
                      ink_inst_map_cmp);
}

static void ink_codegen_deinit(struct ink_codegen *codegen)
{
    codegen->ir = NULL;
    codegen->story = NULL;
    codegen->current_path = NULL;
    ink_inst_map_deinit(&codegen->inst_map);
}

int ink_codegen(const struct ink_ir *ir, struct ink_story *story, int flags)
{
    struct ink_codegen codegen;

    ink_story_init(story, flags);
    ink_codegen_init(&codegen, ir, story);
    ink_codegen_file(&codegen);
    ink_codegen_deinit(&codegen);
    return INK_E_OK;
}
