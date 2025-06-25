#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ink/ink.h>

#include "compile.h"
#include "gc.h"
#include "logging.h"
#include "object.h"
#include "opcode.h"
#include "source.h"
#include "story.h"
#include "stream.h"

const char *INK_DEFAULT_PATH = "@main";

#define T(name, description) description,
static const char *INK_OPCODE_TYPE_STR[] = {INK_MAKE_OPCODE_LIST(T)};
#undef T

static uint32_t ink_object_set_key_hash(const void *key, size_t length)
{
    return ink_fnv32a((uint8_t *)key, length);
}

static bool ink_object_set_key_cmp(const void *lhs, const void *rhs)
{
    const struct ink_object_set_key *const key_lhs = lhs;
    const struct ink_object_set_key *const key_rhs = rhs;

    return (key_lhs->obj == key_rhs->obj);
}

/**
 * Return a printable string for an opcode type.
 */
static inline const char *ink_opcode_strz(enum ink_vm_opcode type)
{
    return INK_OPCODE_TYPE_STR[type];
}

/**
 * Throw a runtime error.
 *
 * Does not return.
 */
static void ink_runtime_error(struct ink_story *story, const char *fmt)
{
    (void)story;
    ink_error("%s!", fmt);
    exit(EXIT_FAILURE);
}

void *ink_story_mem_alloc(struct ink_story *story, void *ptr, size_t size_old,
                          size_t size_new)
{
    if (story->flags & INK_F_GC_TRACING) {
        if (size_new > size_old) {
            ink_trace("Allocating memory for %p, before=%zu, after=%zu", ptr,
                      size_old, size_new);
        }
    }

    story->gc_allocated += size_new - size_old;

    if (story->flags & INK_F_GC_ENABLE) {
        if (story->flags & INK_F_GC_STRESS) {
            if (size_new > 0) {
                ink_gc_collect(story);
            }
        }
        if (size_new > 0 && story->gc_allocated > story->gc_threshold) {
            ink_gc_collect(story);
        }
    }
    if (!size_new) {
        ink_free(ptr);
        return NULL;
    }
    return ink_realloc(ptr, size_new);
}

void ink_story_mem_free(struct ink_story *story, void *ptr)
{
    if (story->flags & INK_F_GC_TRACING) {
        ink_trace("Free memory %p", ptr);
    }

    ink_story_mem_alloc(story, ptr, 0, 0);
}

/**
 * Disassemble a single byte instruction.
 *
 * Returns the next instruction offset.
 */
static size_t ink_disassemble_simple_inst(const struct ink_story *story,
                                          const uint8_t *bytes, size_t offset,
                                          enum ink_vm_opcode opcode)
{
    fprintf(stderr, "%s\n", ink_opcode_strz(opcode));
    return offset + 1;
}

/**
 * Disassemble a two byte instruction.
 *
 * Returns the next instruction offset.
 */
static size_t ink_disassemble_byte_inst(const struct ink_story *story,
                                        const struct ink_object_vec *const_pool,
                                        const uint8_t *bytes, size_t offset,
                                        enum ink_vm_opcode opcode)
{
    const uint8_t arg = bytes[offset + 1];

    if (opcode == INK_OP_CONST) {
        fprintf(stderr, "%-16s 0x%x {", ink_opcode_strz(opcode), arg);
        ink_object_print(const_pool->entries[arg]);
        fprintf(stderr, "}\n");
    } else {
        fprintf(stderr, "%-16s 0x%x\n", ink_opcode_strz(opcode), arg);
    }
    return offset + 2;
}

/**
 * Disassemble an instruction that manipulates a global value.
 *
 * Returns the next instruction offset.
 */
static size_t ink_disassemble_global_inst(
    const struct ink_story *story, const struct ink_object_vec *const_pool,
    const uint8_t *bytes, size_t offset, enum ink_vm_opcode opcode)
{
    const uint8_t arg = bytes[offset + 1];
    const struct ink_string *global_name =
        INK_OBJ_AS_STRING(const_pool->entries[arg]);

    fprintf(stderr, "%-16s 0x%x '%s'\n", ink_opcode_strz(opcode), arg,
            global_name->bytes);
    return offset + 2;
}

/**
 * Disassemble a jump instruction.
 *
 * Returns the next instruction offset.
 */
static size_t ink_disassemble_jump_inst(const struct ink_story *story,
                                        const uint8_t *bytes, size_t offset,
                                        enum ink_vm_opcode opcode)
{
    uint16_t jump = (uint16_t)(bytes[offset + 1] << 8);

    jump |= bytes[offset + 2];

    fprintf(stderr, "%-16s 0x%04x (0x%04lx -> 0x%04lx)\n",
            ink_opcode_strz(opcode), jump, offset, offset + 3 + jump);
    return offset + 3;
}

/**
 * Decode and disassemble a bytecode instruction.
 *
 * Returns the next instruction offset.
 */
static size_t ink_story_disassemble(const struct ink_story *story,
                                    const struct ink_content_path *path,
                                    const uint8_t *bytes, size_t offset,
                                    bool should_prefix)
{
    const struct ink_string *const path_name = path->name;
    const struct ink_object_vec *const const_pool = &path->const_pool;
    const uint8_t op = bytes[offset];

    if (should_prefix) {
        fprintf(stderr, "<%s>:0x%04lx  | ", path_name->bytes, offset);
    } else {
        fprintf(stderr, "0x%04lx  | ", offset);
    }

    switch (op) {
    case INK_OP_EXIT:
    case INK_OP_RET:
    case INK_OP_POP:
    case INK_OP_TRUE:
    case INK_OP_FALSE:
    case INK_OP_ADD:
    case INK_OP_SUB:
    case INK_OP_MUL:
    case INK_OP_DIV:
    case INK_OP_MOD:
    case INK_OP_NEG:
    case INK_OP_NOT:
    case INK_OP_CMP_EQ:
    case INK_OP_CMP_LT:
    case INK_OP_CMP_LTE:
    case INK_OP_CMP_GT:
    case INK_OP_CMP_GTE:
    case INK_OP_FLUSH:
    case INK_OP_LOAD_CHOICE_ID:
    case INK_OP_CONTENT:
    case INK_OP_CHOICE:
    case INK_OP_LINE:
    case INK_OP_GLUE:
        return ink_disassemble_simple_inst(story, bytes, offset, op);
    case INK_OP_CONST:
    case INK_OP_LOAD:
    case INK_OP_STORE:
        return ink_disassemble_byte_inst(story, const_pool, bytes, offset, op);
    case INK_OP_LOAD_GLOBAL:
    case INK_OP_STORE_GLOBAL:
    case INK_OP_CALL:
    case INK_OP_DIVERT:
        return ink_disassemble_global_inst(story, const_pool, bytes, offset,
                                           op);
    case INK_OP_JMP:
    case INK_OP_JMP_T:
    case INK_OP_JMP_F:
        return ink_disassemble_jump_inst(story, bytes, offset, op);
    default:
        fprintf(stderr, "Unknown opcode 0x%x\n", op);
        return offset + 1;
    }
}

/**
 * Invoke a content path with LIFO discipline.
 */
static int ink_story_call(struct ink_story *story, struct ink_object *path_obj)
{
    struct ink_content_path *const current_path =
        INK_OBJ_AS_CONTENT_PATH(story->current_path);
    struct ink_content_path *const path = INK_OBJ_AS_CONTENT_PATH(path_obj);

    if (story->call_stack_top == INK_STORY_STACK_MAX) {
        ink_runtime_error(story, "Stack overflow.");
        return -1;
    }
    if (story->stack_top < path->arity) {
        ink_runtime_error(story, "Not enough arguments to path.");
        return -1;
    }

    struct ink_object **const stack_top = &story->stack[story->stack_top];
    struct ink_call_frame *const frame =
        &story->call_stack[story->call_stack_top++];

    frame->caller = current_path;
    frame->callee = path;
    frame->sp = stack_top - path->arity;
    frame->ip = &path->code.entries[0];
    story->current_path = INK_OBJ(path);
    story->stack_top += path->locals_count;
    return INK_E_OK;
}

/**
 * Divert execution to a content path.
 */
static int ink_story_divert(struct ink_story *story,
                            struct ink_object *path_obj)
{
    struct ink_content_path *const current_path =
        INK_OBJ_AS_CONTENT_PATH(story->current_path);
    struct ink_content_path *const path = INK_OBJ_AS_CONTENT_PATH(path_obj);

    if (story->stack_top < path->arity) {
        ink_runtime_error(story, "Not enough arguments to path.");
        return -1;
    }

    struct ink_call_frame *const frame = &story->call_stack[0];

    for (size_t i = 0; i < path->arity; i++) {
        story->stack[i] = story->stack[story->stack_top - path->arity + i];
    }

    frame->caller = current_path;
    frame->callee = path;
    frame->sp = story->stack;
    frame->ip = &path->code.entries[0];
    story->call_stack_top = 1;
    story->current_path = INK_OBJ(path);
    story->stack_top = path->arity + path->locals_count;
    return INK_E_OK;
}

/**
 * Trace execution.
 */
static void ink_trace_exec(struct ink_story *story,
                           struct ink_call_frame *frame)
{
    const struct ink_content_path *const path = frame->callee;
    const uint8_t *const code = path->code.entries;
    const uint8_t *const ip = frame->ip;
    struct ink_object **const sp = frame->sp;

    fprintf(stderr, "\tStack(%p): [ ", (void *)sp);

    if (story->stack_top > 0) {
        const size_t frame_offset = (size_t)(frame->sp - story->stack);

        for (size_t slot = frame_offset; slot < story->stack_top - 1; slot++) {
            ink_object_print(story->stack[slot]);
            fprintf(stderr, ", ");
        }

        ink_object_print(story->stack[story->stack_top - 1]);
    }
    fprintf(stderr, " ]\n");
    ink_story_disassemble(story, path, code, (size_t)(ip - code), true);
}

/**
 * Push an object onto the evaluation stack.
 */
static int ink_story_stack_push(struct ink_story *story, struct ink_object *obj)
{
    assert(obj != NULL);

    if (story->stack_top >= INK_STORY_STACK_MAX) {
        return -INK_E_STACK_OVERFLOW;
    }

    story->stack[story->stack_top++] = obj;
    return INK_E_OK;
}

/**
 * Pop an object from the evaluation stack.
 */
static struct ink_object *ink_story_stack_pop(struct ink_story *story)
{
    if (story->stack_top == 0) {
        return NULL;
    }
    return story->stack[--story->stack_top];
}

/**
 * Retrieve an object from the evaluation stack without removing it.
 */
static struct ink_object *ink_story_stack_peek(struct ink_story *story,
                                               size_t offset)
{
    if (story->stack_top == 0) {
        return NULL;
    }
    return story->stack[story->stack_top - offset - 1];
}

static ink_integer ink_vm_int_arith(enum ink_vm_opcode op, ink_integer lhs,
                                    ink_integer rhs)
{
    switch (op) {
    case INK_OP_ADD:
        return lhs + rhs;
    case INK_OP_SUB:
        return lhs - rhs;
    case INK_OP_MUL:
        return lhs * rhs;
    case INK_OP_DIV:
        return lhs / rhs;
    case INK_OP_MOD:
        return lhs % rhs;
    default:
        assert(false);
        return 0;
    }
}

static ink_float ink_vm_float_arith(enum ink_vm_opcode op, ink_float lhs,
                                    ink_float rhs)
{
    switch (op) {
    case INK_OP_ADD:
        return lhs + rhs;
    case INK_OP_SUB:
        return lhs - rhs;
    case INK_OP_MUL:
        return lhs * rhs;
    case INK_OP_DIV:
        return lhs / rhs;
    case INK_OP_MOD:
        return fmod(lhs, rhs);
    default:
        assert(false);
        return 0;
    }
}

static bool ink_vm_int_logic(enum ink_vm_opcode op, ink_integer lhs,
                             ink_integer rhs)
{
    switch (op) {
    case INK_OP_CMP_EQ:
        return lhs == rhs;
    case INK_OP_CMP_LT:
        return lhs < rhs;
    case INK_OP_CMP_GT:
        return lhs > rhs;
    case INK_OP_CMP_LTE:
        return lhs <= rhs;
    case INK_OP_CMP_GTE:
        return lhs >= rhs;
    default:
        assert(false);
        return false;
    }
}

/* TODO: Testing for equality of floating is error-prone and probably needs
 * something more sophisticated than this.
 */
static bool ink_vm_float_logic(enum ink_vm_opcode op, ink_float lhs,
                               ink_float rhs)
{
    switch (op) {
    case INK_OP_CMP_EQ:
        return lhs == rhs;
    case INK_OP_CMP_LT:
        return lhs < rhs;
    case INK_OP_CMP_GT:
        return lhs > rhs;
    case INK_OP_CMP_LTE:
        return lhs <= rhs;
    case INK_OP_CMP_GTE:
        return lhs >= rhs;
    default:
        assert(false);
        return false;
    }
}

static struct ink_object *ink_vm_to_number(struct ink_story *story,
                                           struct ink_object *obj)
{
    ink_integer value = 0;

    assert(obj);

    switch (obj->type) {
    case INK_OBJ_BOOL:
        value = (ink_integer)(INK_OBJ_AS_BOOL(obj)->value);
        break;
    case INK_OBJ_NUMBER:
        return obj;
    default:
        value = 1;
        break;
    }

    obj = ink_integer_new(story, value);
    ink_gc_own(story, obj);
    return obj;
}

static struct ink_object *ink_vm_to_string(struct ink_story *story,
                                           struct ink_object *obj)
{
    /* FIXME: Bad. */
#define INK_NUMBER_BUFLEN (20u)
    uint8_t buf[INK_NUMBER_BUFLEN];
    size_t buflen = 0;

    assert(obj);

    switch (obj->type) {
    case INK_OBJ_BOOL: {
        if (INK_OBJ_AS_BOOL(obj)->value) {
            buflen = strlen("true");
            memcpy(buf, "true", buflen);
        } else {
            buflen = strlen("false");
            memcpy(buf, "false", buflen);
        }
        break;
    }
    case INK_OBJ_NUMBER: {
        /* TODO: Check the behavior of snprintf here. The current code MAY cause
         * an overflow. */
        struct ink_number *nval = INK_OBJ_AS_NUMBER(obj);

        if (nval->is_int) {
            buflen = (size_t)snprintf(NULL, 0, "%ld", nval->as.integer);
            snprintf((char *)buf, INK_NUMBER_BUFLEN, "%ld", nval->as.integer);
        } else {
            buflen = (size_t)snprintf(NULL, 0, "%lf", nval->as.floating);
            snprintf((char *)buf, INK_NUMBER_BUFLEN, "%lf", nval->as.floating);
        }
        break;
    }
    case INK_OBJ_STRING: {
        return obj;
    }
    default:
        buflen = strlen("<object>");
        memcpy(buf, "<object>", buflen);
        break;
    }

    obj = ink_string_new(story, buf, buflen);
    ink_gc_own(story, obj);
    return obj;
#undef INK_NUMBER_BUFLEN
}

static inline ink_float ink_vm_to_float(const struct ink_object *value)
{
    assert(value && (INK_OBJ_IS_BOOL(value) || INK_OBJ_IS_NUMBER(value)));

    if (INK_OBJ_IS_NUMBER(value)) {
        if (INK_OBJ_AS_NUMBER(value)->is_int) {
            return (ink_float)(INK_OBJ_AS_NUMBER(value)->as.integer);
        } else {
            return (INK_OBJ_AS_NUMBER(value)->as.floating);
        }
    } else {
        return (ink_float)(INK_OBJ_AS_BOOL(value)->value);
    }
}

static struct ink_object *ink_vm_number_arith(struct ink_story *story,
                                              enum ink_vm_opcode op,
                                              struct ink_object *lhs,
                                              struct ink_object *rhs)
{
    if (INK_OBJ_AS_NUMBER(lhs)->is_int && INK_OBJ_AS_NUMBER(rhs)->is_int) {
        return ink_integer_new(
            story, ink_vm_int_arith(op, INK_OBJ_AS_NUMBER(lhs)->as.integer,
                                    INK_OBJ_AS_NUMBER(rhs)->as.integer));
    }
    return ink_float_new(story, ink_vm_float_arith(op, ink_vm_to_float(lhs),
                                                   ink_vm_to_float(rhs)));
}

static bool ink_vm_number_logic(enum ink_vm_opcode op, struct ink_object *lhs,
                                struct ink_object *rhs)
{
    if (INK_OBJ_AS_NUMBER(lhs)->is_int && INK_OBJ_AS_NUMBER(rhs)->is_int) {
        return ink_vm_int_logic(op, INK_OBJ_AS_NUMBER(lhs)->as.integer,
                                INK_OBJ_AS_NUMBER(rhs)->as.integer);
    }
    return ink_vm_float_logic(op, ink_vm_to_float(lhs), ink_vm_to_float(rhs));
}

static int ink_vm_arith(struct ink_story *story, enum ink_vm_opcode op)
{
    struct ink_object *v = NULL;
    struct ink_object *lhs = ink_story_stack_peek(story, 1);
    struct ink_object *rhs = ink_story_stack_peek(story, 0);

    if (!lhs || !rhs) {
        return -INK_E_INVALID_ARG;
    }

    assert(INK_OBJ_IS_NUMBER(lhs) || INK_OBJ_IS_BOOL(lhs));
    assert(INK_OBJ_IS_NUMBER(rhs) || INK_OBJ_IS_BOOL(rhs));

    if (INK_OBJ_IS_NUMBER(lhs) && INK_OBJ_IS_NUMBER(rhs)) {
        v = ink_vm_number_arith(story, op, lhs, rhs);
    } else if (INK_OBJ_IS_NUMBER(lhs)) {
        v = ink_vm_number_arith(story, op, lhs, ink_vm_to_number(story, rhs));
    } else if (INK_OBJ_IS_NUMBER(rhs)) {
        v = ink_vm_number_arith(story, op, ink_vm_to_number(story, lhs), rhs);
    }
    if (!v) {
        return -INK_E_OOM;
    }

    ink_story_stack_pop(story);
    ink_story_stack_pop(story);

    if (ink_story_stack_push(story, v) < 0) {
        return -INK_E_STACK_OVERFLOW;
    }
    return INK_E_OK;
}

static int ink_vm_add(struct ink_story *story)
{
    struct ink_object *v = NULL;
    struct ink_object *lhs = ink_story_stack_peek(story, 1);
    struct ink_object *rhs = ink_story_stack_peek(story, 0);

    if (!lhs || !rhs) {
        return -INK_E_INVALID_ARG;
    }
    if (INK_OBJ_IS_STRING(lhs) && INK_OBJ_IS_STRING(rhs)) {
        v = ink_string_concat(story, lhs, rhs);
    } else if (INK_OBJ_IS_STRING(lhs)) {
        v = ink_string_concat(story, lhs, ink_vm_to_string(story, rhs));
    } else if (INK_OBJ_IS_STRING(rhs)) {
        v = ink_string_concat(story, ink_vm_to_string(story, lhs), rhs);
    } else if (INK_OBJ_IS_NUMBER(lhs) && INK_OBJ_IS_NUMBER(rhs)) {
        v = ink_vm_number_arith(story, INK_OP_ADD, lhs, rhs);
    } else if (INK_OBJ_IS_NUMBER(lhs)) {
        v = ink_vm_number_arith(story, INK_OP_ADD, lhs,
                                ink_vm_to_number(story, rhs));
    } else if (INK_OBJ_IS_NUMBER(rhs)) {
        v = ink_vm_number_arith(story, INK_OP_ADD, ink_vm_to_number(story, lhs),
                                rhs);
    } else {
        return -INK_E_INVALID_ARG;
    }
    if (!v) {
        return -INK_E_OOM;
    }

    ink_story_stack_pop(story);
    ink_story_stack_pop(story);

    if (ink_story_stack_push(story, v) < 0) {
        return -INK_E_STACK_OVERFLOW;
    }
    return INK_E_OK;
}

static int ink_vm_cmp(struct ink_story *story, enum ink_vm_opcode op)
{
    bool cond = false;
    struct ink_object *v = NULL;
    struct ink_object *lhs = ink_story_stack_peek(story, 1);
    struct ink_object *rhs = ink_story_stack_peek(story, 0);

    if (!lhs || !rhs) {
        return -INK_E_INVALID_ARG;
    }
    if (op == INK_OP_CMP_EQ) {
        cond = ink_object_eq(lhs, rhs);
    } else {
        assert(INK_OBJ_IS_NUMBER(lhs) || INK_OBJ_IS_BOOL(lhs));
        assert(INK_OBJ_IS_NUMBER(rhs) || INK_OBJ_IS_BOOL(rhs));

        if (INK_OBJ_IS_NUMBER(lhs) && INK_OBJ_IS_NUMBER(rhs)) {
            cond = ink_vm_number_logic(op, lhs, rhs);
        } else if (INK_OBJ_IS_NUMBER(lhs)) {
            cond = ink_vm_number_logic(op, lhs, ink_vm_to_number(story, rhs));
        } else if (INK_OBJ_IS_NUMBER(rhs)) {
            cond = ink_vm_number_logic(op, ink_vm_to_number(story, lhs), rhs);
        } else {
            return -INK_E_INVALID_ARG;
        }
    }

    v = ink_bool_new(story, cond);
    if (!v) {
        return -INK_E_OOM;
    }

    ink_story_stack_pop(story);
    ink_story_stack_pop(story);

    if (ink_story_stack_push(story, v) < 0) {
        return -INK_E_STACK_OVERFLOW;
    }
    return INK_E_OK;
}

static int ink_vm_neg(struct ink_story *story)
{
    struct ink_number *v = NULL;
    struct ink_object *arg = ink_story_stack_peek(story, 0);

    if (!arg) {
        return -INK_E_STACK_OVERFLOW;
    }

    assert(arg->type == INK_OBJ_NUMBER);
    v = INK_OBJ_AS_NUMBER(arg);

    if (v->is_int) {
        v->as.integer = -v->as.integer;
    } else {
        v->as.floating = -v->as.floating;
    }
    return INK_E_OK;
}

static int ink_vm_not(struct ink_story *story)
{
    struct ink_object *v = NULL;
    struct ink_object *arg = ink_story_stack_peek(story, 0);

    if (!arg) {
        return -INK_E_STACK_OVERFLOW;
    }

    v = ink_bool_new(story, ink_object_is_falsey(arg));
    if (!v) {
        return -INK_E_OOM;
    }

    ink_story_stack_pop(story);
    ink_story_stack_push(story, v);
    return INK_E_OK;
}

static int ink_vm_load_const(struct ink_story *story,
                             struct ink_call_frame *frame, uint8_t offset)
{
    struct ink_object_vec *const const_pool = &frame->callee->const_pool;

    if (offset > const_pool->count) {
        return -INK_E_INVALID_ARG;
    }
    if (ink_story_stack_push(story, const_pool->entries[offset]) < 0) {
        return -INK_E_STACK_OVERFLOW;
    }
    return INK_E_OK;
}

/**
 * Main interpreter loop.
 *
 * Returns a non-zero value upon error.
 */
static int ink_story_exec(struct ink_story *story)
{
    int rc = -1;
    struct ink_object *const globals_pool = story->globals;
    struct ink_object *const paths_pool = story->paths;
    struct ink_call_frame *frame = NULL;

    if (story->call_stack_top > 0) {
        frame = &story->call_stack[story->call_stack_top - 1];
    } else {
        story->can_continue = false;
        return INK_E_OK;
    }

#define INK_READ_BYTE() (*frame->ip++)
#define INK_READ_ADDR()                                                        \
    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

    for (;;) {
        if (story->flags & INK_F_VM_TRACING) {
            ink_trace_exec(story, frame);
        }

        struct ink_object_vec *const const_pool = &frame->callee->const_pool;
        const uint8_t op = INK_READ_BYTE();

        switch (op) {
        case INK_OP_EXIT: {
            rc = INK_E_OK;
            story->is_exited = true;
            goto exit_loop;
        }
        case INK_OP_RET: {
            struct ink_object *value = ink_story_stack_pop(story);

            story->call_stack_top--;
            if (story->call_stack_top == 0) {
                ink_story_stack_pop(story);
                rc = INK_E_OK;
                goto exit_loop;
            }

            story->stack_top = (size_t)(frame->sp - story->stack);

            /* FIXME: This probably isn't a good way to handle this case. */
            if (!value) {
                value = ink_bool_new(story, false);
            }

            ink_story_stack_push(story, value);
            frame = &story->call_stack[story->call_stack_top - 1];
            break;
        }
        case INK_OP_POP: {
            if (!ink_story_stack_pop(story)) {
                rc = -INK_E_INVALID_ARG;
                goto exit_loop;
            }
            break;
        }
        case INK_OP_TRUE: {
            if (ink_story_stack_push(story, ink_bool_new(story, true)) < 0) {
                rc = -INK_E_STACK_OVERFLOW;
                goto exit_loop;
            }
            break;
        }
        case INK_OP_FALSE: {
            if (ink_story_stack_push(story, ink_bool_new(story, false)) < 0) {
                rc = -INK_E_STACK_OVERFLOW;
                goto exit_loop;
            }
            break;
        }
        case INK_OP_CONST: {
            if ((rc = ink_vm_load_const(story, frame, INK_READ_BYTE())) < 0) {
                goto exit_loop;
            }
            break;
        }
        case INK_OP_ADD: {
            rc = ink_vm_add(story);
            if (rc < 0) {
                goto exit_loop;
            }
            break;
        }
        case INK_OP_SUB:
        case INK_OP_MUL:
        case INK_OP_DIV:
        case INK_OP_MOD: {
            rc = ink_vm_arith(story, op);
            if (rc < 0) {
                goto exit_loop;
            }
            break;
        }
        case INK_OP_CMP_EQ:
        case INK_OP_CMP_LT:
        case INK_OP_CMP_GT:
        case INK_OP_CMP_LTE:
        case INK_OP_CMP_GTE: {
            rc = ink_vm_cmp(story, op);
            if (rc < 0) {
                goto exit_loop;
            }
            break;
        }
        case INK_OP_NEG: {
            rc = ink_vm_neg(story);
            if (rc < 0) {
                goto exit_loop;
            }
            break;
        }
        case INK_OP_NOT: {
            rc = ink_vm_not(story);
            if (rc < 0) {
                goto exit_loop;
            }
            break;
        }
        case INK_OP_JMP: {
            const uint16_t offset = INK_READ_ADDR();

            frame->ip += offset;
            break;
        }
        case INK_OP_JMP_T: {
            const uint16_t offset = INK_READ_ADDR();
            struct ink_object *const arg = ink_story_stack_peek(story, 0);

            if (!ink_object_is_falsey(arg)) {
                frame->ip += offset;
            }
            break;
        }
        case INK_OP_JMP_F: {
            const uint16_t offset = INK_READ_ADDR();
            struct ink_object *const arg = ink_story_stack_peek(story, 0);

            if (ink_object_is_falsey(arg)) {
                frame->ip += offset;
            }
            break;
        }
        case INK_OP_DIVERT: {
            const uint16_t offset = INK_READ_BYTE();
            struct ink_object *const arg = const_pool->entries[offset];
            struct ink_object *value = NULL;

            rc = ink_table_lookup(story, paths_pool, arg, &value);
            if (rc < 0) {
                goto exit_loop;
            }

            rc = ink_story_divert(story, value);
            if (rc < 0) {
                goto exit_loop;
            }

            frame = &story->call_stack[story->call_stack_top - 1];
            break;
        }
        case INK_OP_CALL: {
            const uint16_t offset = INK_READ_BYTE();
            struct ink_object *const arg = const_pool->entries[offset];
            struct ink_object *value = NULL;

            rc = ink_table_lookup(story, paths_pool, arg, &value);
            if (rc < 0) {
                goto exit_loop;
            }

            rc = ink_story_call(story, value);
            if (rc < 0) {
                goto exit_loop;
            }

            frame = &story->call_stack[story->call_stack_top - 1];
            break;
        }
        case INK_OP_LOAD: {
            const uint8_t offset = INK_READ_BYTE();
            struct ink_object *const value = frame->sp[offset];

            ink_story_stack_push(story, value);
            break;
        }
        case INK_OP_STORE: {
            const uint8_t offset = INK_READ_BYTE();
            struct ink_object *value = ink_story_stack_peek(story, 0);

            frame->sp[offset] = value;
            break;
        }
        case INK_OP_LOAD_GLOBAL: {
            const uint8_t offset = INK_READ_BYTE();
            struct ink_object *const arg = const_pool->entries[offset];
            struct ink_object *value = NULL;

            rc = ink_table_lookup(story, globals_pool, arg, &value);
            if (rc < 0) {
                goto exit_loop;
            }

            rc = ink_story_stack_push(story, value);
            if (rc < 0) {
                goto exit_loop;
            }
            break;
        }
        case INK_OP_STORE_GLOBAL: {
            const uint8_t offset = INK_READ_BYTE();
            struct ink_object *const arg = const_pool->entries[offset];
            struct ink_object *const value = ink_story_stack_peek(story, 0);

            rc = ink_table_insert(story, globals_pool, arg, value);
            if (rc < 0) {
                goto exit_loop;
            }

            ink_story_stack_pop(story);

            rc = ink_story_stack_push(story, value);
            if (rc < 0) {
                goto exit_loop;
            }
            break;
        }
        case INK_OP_CONTENT: {
            struct ink_object *const arg = ink_story_stack_pop(story);
            struct ink_object *const str_arg = ink_vm_to_string(story, arg);
            struct ink_string *const str = INK_OBJ_AS_STRING(str_arg);

            ink_stream_write(&story->stream, str->bytes, str->length);
            break;
        }
        case INK_OP_LINE: {
            ink_stream_writef(&story->stream, "\n");
            break;
        }
        case INK_OP_GLUE: {
            ink_stream_trim(&story->stream);
            break;
        }
        case INK_OP_CHOICE: {
            struct ink_choice choice = {
                .id = ink_story_stack_pop(story),
            };

            ink_stream_read_line(&story->stream, &choice.bytes, &choice.length);
            ink_choice_vec_push(&story->current_choices, choice);
            break;
        }
        case INK_OP_LOAD_CHOICE_ID: {
            rc = ink_story_stack_push(story, story->current_choice_id);
            if (rc < 0) {
                goto exit_loop;
            }
            break;
        }
        case INK_OP_FLUSH: {
            rc = INK_E_OK;
            goto exit_loop;
        }
        default:
            rc = -INK_E_INVALID_INST;
            goto exit_loop;
        }
    }
exit_loop:
    return rc;
#undef INK_READ_BYTE
#undef INK_READ_ADDR
}

bool ink_story_can_continue(struct ink_story *s)
{
    return s->can_continue;
}

int ink_story_continue(struct ink_story *s, uint8_t **line, size_t *linelen)
{
    int rc = -1;

    if (line) {
        *line = NULL;
    }
    if (linelen) {
        *linelen = 0;
    }
    for (;;) {
        if (!ink_stream_is_empty(&s->stream)) {
            ink_stream_read_line(&s->stream, line, linelen);

            if (ink_stream_is_empty(&s->stream)) {
                if (s->is_exited || s->current_choices.count > 0) {
                    s->can_continue = false;
                }
            }
            return INK_E_OK;
        }
        if (s->is_exited || s->current_choices.count > 0) {
            s->can_continue = false;
            return INK_E_OK;
        }

        rc = ink_story_exec(s);
        if (rc < 0) {
            return rc;
        }
    }
}

int ink_story_choose(struct ink_story *s, size_t index)
{
    struct ink_choice *ch;

    s->choice_index = 0;

    if (index > 0) {
        index--;
    }
    if (index < s->current_choices.count) {
        ch = &s->current_choices.entries[index];
        s->current_choice_id = ch->id;
        s->can_continue = true;
        ink_choice_vec_shrink(&s->current_choices, 0);
        return INK_E_OK;
    }
    return -INK_E_INVALID_ARG;
}

int ink_story_choice_next(struct ink_story *s, struct ink_choice *choice)
{
    if (s->choice_index < s->current_choices.count) {
        *choice = s->current_choices.entries[s->choice_index++];
        return 0;
    }
    return -1;
}

struct ink_object *ink_story_get_paths(struct ink_story *story)
{
    return story->paths;
}

void ink_story_dump(struct ink_story *story)
{
    const struct ink_table *const paths_table = INK_OBJ_AS_TABLE(story->paths);

    for (size_t i = 0; i < paths_table->capacity; i++) {
        struct ink_table_kv *const entry = &paths_table->entries[i];

        if (entry->key) {
            const struct ink_content_path *const path =
                INK_OBJ_AS_CONTENT_PATH(entry->value);
            const struct ink_string *const path_name =
                INK_OBJ_AS_STRING(path->name);

            assert(path->code.count > 0);
            fprintf(stderr, "=== %s(args: %u, locals: %u) ===\n",
                    path_name->bytes, path->arity, path->locals_count);

            for (size_t offset = 0; offset < path->code.count;) {
                offset = ink_story_disassemble(story, path, path->code.entries,
                                               offset, false);
            }
        }
    }
}

int ink_story_load_opts(struct ink_story *story,
                        const struct ink_load_opts *opts)
{
    int rc = -1;

    if (!opts->source_bytes) {
        return -INK_E_PANIC;
    }

    story->flags = opts->flags & ~INK_F_GC_ENABLE;
    story->globals = ink_table_new(story);
    story->paths = ink_table_new(story);

    rc = ink_compile(story, opts);
    if (rc < 0) {
        goto err;
    }

    struct ink_table *const paths_table = INK_OBJ_AS_TABLE(story->paths);

    for (size_t i = 0; i < paths_table->capacity; i++) {
        struct ink_table_kv *const entry = &paths_table->entries[i];

        if (entry->key) {
            const struct ink_content_path *const cpath =
                INK_OBJ_AS_CONTENT_PATH(entry->value);
            const struct ink_string *const path_name =
                INK_OBJ_AS_STRING(cpath->name);

            if (strcmp(INK_DEFAULT_PATH, (char *)path_name->bytes) == 0) {
                ink_story_divert(story, entry->value);
                break;
            }
        }
    }

    story->can_continue = true;

    if (opts->flags & INK_F_GC_ENABLE) {
        story->flags |= INK_F_GC_ENABLE;
    }
err:
    return rc;
}

int ink_story_load_string(struct ink_story *story, const char *source,
                          int flags)
{
    const struct ink_load_opts opts = {
        .flags = flags,
        .source_bytes = (uint8_t *)source,
        .source_length = strlen(source),
        .filename = (uint8_t *)"<STDIN>",
    };

    return ink_story_load_opts(story, &opts);
}

int ink_story_load_file(struct ink_story *story, const char *file_path,
                        int flags)
{
    int rc = -1;
    struct ink_source s;

    rc = ink_source_load(file_path, &s);
    if (rc < 0) {
        return rc;
    }

    const struct ink_load_opts opts = {
        .flags = flags,
        .source_bytes = (uint8_t *)s.bytes,
        .source_length = s.length,
        .filename = (uint8_t *)file_path,
    };

    rc = ink_story_load_opts(story, &opts);
    ink_source_free(&s);
    return rc;
}

struct ink_story *ink_open(void)
{
    struct ink_story *const story = ink_malloc(sizeof(*story));

    if (!story) {
        return NULL;
    }

    story->is_exited = false;
    story->can_continue = false;
    story->flags = 0;
    story->choice_index = 0;
    story->stack_top = 0;
    story->call_stack_top = 0;
    story->gc_allocated = 0;
    story->gc_threshold = INK_GC_HEAP_SIZE_MIN;
    story->gc_objects = NULL;
    story->globals = NULL;
    story->paths = NULL;
    story->current_path = NULL;
    story->current_choice_id = NULL;

    ink_stream_init(&story->stream);
    memset(story->stack, 0, sizeof(*story->stack) * INK_STORY_STACK_MAX);
    memset(story->call_stack, 0,
           sizeof(*story->call_stack) * INK_STORY_STACK_MAX);
    ink_object_vec_init(&story->gc_gray);
    ink_object_set_init(&story->gc_owned, INK_OBJECT_SET_LOAD_MAX,
                        ink_object_set_key_hash, ink_object_set_key_cmp);
    ink_choice_vec_init(&story->current_choices);
    return story;
}

void ink_close(struct ink_story *story)
{
    ink_choice_vec_deinit(&story->current_choices);
    ink_object_vec_deinit(&story->gc_gray);
    ink_object_set_deinit(&story->gc_owned);
    ink_stream_deinit(&story->stream);

    while (story->gc_objects) {
        struct ink_object *const obj = story->gc_objects;

        story->gc_objects = story->gc_objects->next;
        ink_object_free(story, obj);
    }

    memset(story, 0, sizeof(*story));
    ink_free(story);
}
