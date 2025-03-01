#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "compile.h"
#include "logging.h"
#include "object.h"
#include "opcode.h"
#include "story.h"

#define INK_GC_GRAY_CAPACITY_MIN (16ul)
#define INK_GC_GRAY_GROWTH_FACTOR (2ul)
#define INK_GC_HEAP_SIZE_MIN (1024ul * 1024ul)
#define INK_GC_HEAP_GROWTH_PERCENT (50ul)

static const char *INK_OBJ_TYPE_STR[] = {
    [INK_OBJ_BOOL] = "Bool",
    [INK_OBJ_NUMBER] = "Number",
    [INK_OBJ_STRING] = "String",
    [INK_OBJ_TABLE] = "Table",
    [INK_OBJ_CONTENT_PATH] = "ContentPath",
};

const char *INK_DEFAULT_PATH = "@main";

/* TODO(Brett): Add opcode stuff to new module. */
#define T(name, description) description,
static const char *INK_OPCODE_TYPE_STR[] = {INK_MAKE_OPCODE_LIST(T)};
#undef T

static const char *ink_opcode_strz(enum ink_vm_opcode type)
{
    return INK_OPCODE_TYPE_STR[type];
}

static size_t ink_disassemble_simple_inst(const struct ink_story *story,
                                          const uint8_t *bytes, size_t offset,
                                          enum ink_vm_opcode opcode)
{
    printf("%s\n", ink_opcode_strz(opcode));
    return offset + 1;
}

static size_t ink_disassemble_byte_inst(const struct ink_story *story,
                                        const struct ink_object_vec *const_pool,
                                        const uint8_t *bytes, size_t offset,
                                        enum ink_vm_opcode opcode)
{
    const uint8_t arg = bytes[offset + 1];

    if (opcode == INK_OP_CONST) {
        printf("%-16s 0x%x {", ink_opcode_strz(opcode), arg);
        ink_object_print(const_pool->entries[arg]);
        printf("}\n");
    } else {
        printf("%-16s 0x%x\n", ink_opcode_strz(opcode), arg);
    }
    return offset + 2;
}

static size_t ink_disassemble_global_inst(
    const struct ink_story *story, const struct ink_object_vec *const_pool,
    const uint8_t *bytes, size_t offset, enum ink_vm_opcode opcode)
{
    const uint8_t arg = bytes[offset + 1];
    const struct ink_string *global_name =
        INK_OBJ_AS_STRING(const_pool->entries[arg]);

    printf("%-16s 0x%x '%s'\n", ink_opcode_strz(opcode), arg,
           global_name->bytes);
    return offset + 2;
}

static size_t ink_disassemble_jump_inst(const struct ink_story *story,
                                        const uint8_t *bytes, size_t offset,
                                        enum ink_vm_opcode opcode)
{
    uint16_t jump = (uint16_t)(bytes[offset + 1] << 8);

    jump |= bytes[offset + 2];

    printf("%-16s 0x%lx -> 0x%lx\n", ink_opcode_strz(opcode), offset,
           offset + 3 + jump);
    return offset + 3;
}

static size_t ink_story_disassemble(const struct ink_story *story,
                                    const struct ink_content_path *path,
                                    const uint8_t *bytes, size_t offset,
                                    bool should_prefix)
{
    const struct ink_string *const path_name = path->name;
    const struct ink_object_vec *const const_pool = &path->const_pool;
    const uint8_t op = bytes[offset];

    if (should_prefix) {
        printf("<%s>:0x%04lx  | ", path_name->bytes, offset);
    } else {
        printf("0x%04lx  | ", offset);
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
    case INK_OP_CONTENT_PUSH:
    case INK_OP_CHOICE_PUSH:
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
        printf("Unknown opcode 0x%x\n", op);
        return offset + 1;
    }
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
            printf("=== %s(args: %u, locals: %u) ===\n", path_name->bytes,
                   path->arity, path->locals_count);

            for (size_t offset = 0; offset < path->code.count;) {
                offset = ink_story_disassemble(story, path, path->code.entries,
                                               offset, false);
            }
        }
    }
}

static void ink_runtime_error(struct ink_story *story, const char *fmt)
{
    (void)story;
    ink_error("%s!", fmt);
    exit(EXIT_FAILURE);
}

static void ink_gc_mark_object(struct ink_story *story, struct ink_object *obj)
{
    if (!obj || obj->is_marked) {
        return;
    }

    obj->is_marked = true;
    ink_trace("Marked object %p, type=%s", (void *)obj,
              INK_OBJ_TYPE_STR[obj->type]);

    if (story->gc_gray_capacity < story->gc_gray_count + 1) {
        if (story->gc_gray_capacity == 0) {
            story->gc_gray_capacity = INK_GC_GRAY_CAPACITY_MIN;
        } else {
            story->gc_gray_capacity =
                story->gc_gray_capacity * INK_GC_GRAY_GROWTH_FACTOR;
        }

        story->gc_gray = (struct ink_object **)ink_realloc(
            story->gc_gray,
            sizeof(struct ink_object *) * story->gc_gray_capacity);

        if (!story->gc_gray) {
            ink_runtime_error(story, "Out of memory!");
        }
    }

    story->gc_gray[story->gc_gray_count++] = obj;
}

static void ink_gc_blacken_object(struct ink_story *story,
                                  struct ink_object *obj)
{
    size_t obj_size = 0;

    assert(obj);

    switch (obj->type) {
    case INK_OBJ_BOOL:
        obj_size = sizeof(struct ink_bool);
        break;
    case INK_OBJ_NUMBER:
        obj_size = sizeof(struct ink_number);
        break;
    case INK_OBJ_STRING: {
        struct ink_string *const str_obj = INK_OBJ_AS_STRING(obj);

        obj_size = sizeof(struct ink_string) + str_obj->length + 1;
        break;
    }
    case INK_OBJ_TABLE: {
        struct ink_table *const table_obj = INK_OBJ_AS_TABLE(obj);

        for (size_t i = 0; i < table_obj->capacity; i++) {
            struct ink_table_kv *const entry = &table_obj->entries[i];

            if (entry->key) {
                ink_gc_mark_object(story, INK_OBJ(entry->key));
                ink_gc_mark_object(story, entry->value);
            }
        }

        obj_size = sizeof(struct ink_table) +
                   table_obj->capacity * sizeof(struct ink_table_kv);
        break;
    }
    case INK_OBJ_CONTENT_PATH: {
        struct ink_content_path *const path_obj = INK_OBJ_AS_CONTENT_PATH(obj);

        ink_gc_mark_object(story, INK_OBJ(path_obj->name));

        for (size_t i = 0; i < path_obj->const_pool.count; i++) {
            struct ink_object *const entry = path_obj->const_pool.entries[i];

            ink_gc_mark_object(story, entry);
        }

        obj_size = sizeof(struct ink_content_path);
        break;
    }
    }

    story->gc_allocated += obj_size;

    ink_trace("Blackened object %p, type=%s, size=%zu", (void *)obj,
              INK_OBJ_TYPE_STR[obj->type], obj_size);
}

static void ink_gc_collect(struct ink_story *story)
{
    double time_start, time_elapsed;
    size_t bytes_before, bytes_after;

    if (story->flags & INK_F_TRACING) {
        time_start = (double)clock() / CLOCKS_PER_SEC;
        bytes_before = story->gc_allocated;
        ink_trace("Beginning collection");
    }

    story->gc_allocated = 0;

    for (size_t i = 0; i < story->stack_top; i++) {
        struct ink_object *const obj = story->stack[i];

        if (obj) {
            ink_gc_mark_object(story, obj);
        }
    }

    ink_gc_mark_object(story, story->globals);
    ink_gc_mark_object(story, story->paths);
    ink_gc_mark_object(story, story->current_path);
    ink_gc_mark_object(story, story->current_content);
    ink_gc_mark_object(story, story->choice_id);

    for (size_t i = 0; i < story->current_choices.count; i++) {
        struct ink_choice *const choice = &story->current_choices.entries[i];

        ink_gc_mark_object(story, choice->id);
        ink_gc_mark_object(story, INK_OBJ(choice->text));
    }
    while (story->gc_gray_count > 0) {
        struct ink_object *const obj = story->gc_gray[--story->gc_gray_count];

        ink_gc_blacken_object(story, obj);
    }

    struct ink_object **obj = &story->gc_objects;

    while (*obj != NULL) {
        if (!((*obj)->is_marked)) {
            struct ink_object *unreached = *obj;

            *obj = unreached->next;

            ink_object_free(story, unreached);
        } else {
            (*obj)->is_marked = false;
            obj = &(*obj)->next;
        }
    }

    story->gc_threshold =
        story->gc_allocated +
        ((story->gc_allocated * INK_GC_HEAP_GROWTH_PERCENT) / 100);
    if (story->gc_threshold < INK_GC_HEAP_SIZE_MIN) {
        story->gc_threshold = INK_GC_HEAP_SIZE_MIN;
    }
    if (story->flags & INK_F_TRACING) {
        time_elapsed = ((double)clock() / CLOCKS_PER_SEC) - time_start;
        bytes_after = story->gc_allocated;

        ink_trace("Collection completed in %.3fms, before=%zu, after=%zu, "
                  "collected=%zu, next at %zu",
                  time_elapsed * 1000.0, bytes_before, bytes_after,
                  bytes_before - bytes_after, story->gc_threshold);
    }
}

void *ink_story_mem_alloc(struct ink_story *story, void *ptr, size_t size_old,
                          size_t size_new)
{
    if (story->flags & INK_F_TRACING) {
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
    if (story->flags & INK_F_TRACING) {
        ink_trace("Free memory %p", ptr);
    }

    ink_story_mem_alloc(story, ptr, 0, 0);
}

int ink_story_stack_push(struct ink_story *story, struct ink_object *obj)
{
    assert(obj != NULL);

    if (story->stack_top >= INK_STORY_STACK_MAX) {
        return -INK_E_STACK_OVERFLOW;
    }

    story->stack[story->stack_top++] = obj;
    return INK_E_OK;
}

struct ink_object *ink_story_stack_pop(struct ink_story *story)
{
    if (story->stack_top == 0) {
        return NULL;
    }
    return story->stack[--story->stack_top];
}

struct ink_object *ink_story_stack_peek(struct ink_story *story, size_t offset)
{
    if (story->stack_top == 0) {
        return NULL;
    }
    return story->stack[story->stack_top - offset - 1];
}

static bool ink_object_is_falsey(const struct ink_object *obj)
{
    return (INK_OBJ_IS_BOOL(obj) && !INK_OBJ_AS_BOOL(obj)->value);
}

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

static void ink_trace_exec(struct ink_story *story,
                           struct ink_call_frame *frame)
{
    const struct ink_content_path *const path = frame->callee;
    const uint8_t *const code = path->code.entries;
    const uint8_t *const ip = frame->ip;
    struct ink_object *const *sp = frame->sp;

    printf("\tStack(%p): [ ", (void *)sp);

    if (story->stack_top > 0) {
        const size_t frame_offset = (size_t)(frame->sp - story->stack);

        for (size_t slot = frame_offset; slot < story->stack_top - 1; slot++) {
            ink_object_print(story->stack[slot]);
            printf(", ");
        }

        ink_object_print(story->stack[story->stack_top - 1]);
    }
    printf(" ]\n");
    ink_story_disassemble(story, path, code, (size_t)(ip - code), true);
}

#define INK_LOGICAL_OP(story, proc)                                            \
    do {                                                                       \
        struct ink_object *value = NULL;                                       \
        const struct ink_object *const arg2 = ink_story_stack_pop(story);      \
        const struct ink_object *const arg1 = ink_story_stack_pop(story);      \
                                                                               \
        if (!INK_OBJ_IS_NUMBER(arg1) || !INK_OBJ_IS_NUMBER(arg2)) {            \
            rc = -INK_E_INVALID_ARG;                                           \
            goto exit_loop;                                                    \
        }                                                                      \
                                                                               \
        value = ink_bool_new(story, proc(story, arg1, arg2));                  \
        if (!value) {                                                          \
            rc = -INK_E_OOM;                                                   \
            goto exit_loop;                                                    \
        }                                                                      \
                                                                               \
        rc = ink_story_stack_push(story, value);                               \
        if (rc < 0) {                                                          \
            goto exit_loop;                                                    \
        }                                                                      \
    } while (0)

#define INK_BINARY_OP(story, proc)                                             \
    do {                                                                       \
        struct ink_object *value = NULL;                                       \
        const struct ink_object *const arg2 = ink_story_stack_pop(story);      \
        const struct ink_object *const arg1 = ink_story_stack_pop(story);      \
                                                                               \
        if (!INK_OBJ_IS_NUMBER(arg1) || !INK_OBJ_IS_NUMBER(arg2)) {            \
            rc = -INK_E_INVALID_ARG;                                           \
            goto exit_loop;                                                    \
        }                                                                      \
                                                                               \
        value = proc(story, arg1, arg2);                                       \
        if (!value) {                                                          \
            rc = -INK_E_OOM;                                                   \
            goto exit_loop;                                                    \
        }                                                                      \
                                                                               \
        rc = ink_story_stack_push(story, value);                               \
        if (rc < 0) {                                                          \
            goto exit_loop;                                                    \
        }                                                                      \
    } while (0)

#define INK_READ_BYTE() (*frame->ip++)

#define INK_READ_ADDR()                                                        \
    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

static int ink_story_execute(struct ink_story *story)
{
    int rc = -1;
    struct ink_object *const globals_pool = story->globals;
    struct ink_object *const paths_pool = story->paths;
    struct ink_byte_vec pending_content;
    struct ink_call_frame *frame =
        &story->call_stack[story->call_stack_top - 1];

    ink_byte_vec_init(&pending_content);
    ink_choice_vec_shrink(&story->current_choices, 0);
    story->current_content = NULL;

    for (;;) {
        if (story->flags & INK_F_TRACING) {
            ink_trace_exec(story, frame);
        }

        struct ink_object_vec *const const_pool = &frame->callee->const_pool;
        const uint8_t byte = INK_READ_BYTE();

        switch (byte) {
        case INK_OP_EXIT: {
            rc = INK_E_OK;
            story->can_continue = false;
            goto exit_loop;
        }
        case INK_OP_RET: {
            struct ink_object *const value = ink_story_stack_pop(story);

            story->call_stack_top--;
            if (story->call_stack_top == 0) {
                ink_story_stack_pop(story);
                rc = INK_E_OK;
                goto exit_loop;
            }

            story->stack_top = (size_t)(frame->sp - story->stack);
            ink_story_stack_push(story, value);
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
            struct ink_object *const value = ink_bool_new(story, true);

            rc = ink_story_stack_push(story, value);
            if (rc < 0) {
                goto exit_loop;
            }
            break;
        }
        case INK_OP_FALSE: {
            struct ink_object *const value = ink_bool_new(story, false);

            rc = ink_story_stack_push(story, value);
            if (rc < 0) {
                goto exit_loop;
            }
            break;
        }
        case INK_OP_CONST: {
            const uint8_t offset = INK_READ_BYTE();

            if (offset > const_pool->count) {
                rc = -INK_E_INVALID_ARG;
                goto exit_loop;
            }

            rc = ink_story_stack_push(story, const_pool->entries[offset]);
            if (rc < 0) {
                goto exit_loop;
            }
            break;
        }
        case INK_OP_ADD: {
            INK_BINARY_OP(story, ink_number_add);
            break;
        }
        case INK_OP_SUB: {
            INK_BINARY_OP(story, ink_number_sub);
            break;
        }
        case INK_OP_MUL: {
            INK_BINARY_OP(story, ink_number_mul);
            break;
        }
        case INK_OP_DIV: {
            INK_BINARY_OP(story, ink_number_div);
            break;
        }
        case INK_OP_MOD: {
            INK_BINARY_OP(story, ink_number_mod);
            break;
        }
        case INK_OP_NEG: {
            struct ink_object *const arg = ink_story_stack_peek(story, 0);
            struct ink_object *const value = ink_number_neg(story, arg);

            ink_story_stack_pop(story);
            ink_story_stack_push(story, value);
            break;
        }
        case INK_OP_NOT: {
            struct ink_object *const arg = ink_story_stack_peek(story, 0);
            struct ink_object *const value =
                ink_bool_new(story, ink_object_is_falsey(arg));

            ink_story_stack_pop(story);
            ink_story_stack_push(story, value);
            break;
        }
        case INK_OP_CMP_EQ: {
            struct ink_object *const arg1 = ink_story_stack_pop(story);
            struct ink_object *const arg2 = ink_story_stack_pop(story);
            struct ink_object *result = NULL;

            if (!arg1 || !arg2) {
                rc = -INK_E_INVALID_ARG;
                goto exit_loop;
            }

            result = ink_object_eq(story, arg1, arg2);
            ink_story_stack_push(story, result);
            break;
        }
        case INK_OP_CMP_LT: {
            INK_LOGICAL_OP(story, ink_number_lt);
            break;
        }
        case INK_OP_CMP_GT: {
            INK_LOGICAL_OP(story, ink_number_gt);
            break;
        }
        case INK_OP_CMP_LTE: {
            INK_LOGICAL_OP(story, ink_number_lte);
            break;
        }
        case INK_OP_CMP_GTE: {
            INK_LOGICAL_OP(story, ink_number_gte);
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
            struct ink_object *value = ink_story_stack_pop(story);

            rc = ink_table_insert(story, globals_pool, arg, value);
            if (rc < 0) {
                goto exit_loop;
            }

            rc = ink_story_stack_push(story, value);
            if (rc < 0) {
                goto exit_loop;
            }
            break;
        }
        case INK_OP_CONTENT_PUSH: {
            struct ink_object *const arg = ink_story_stack_pop(story);
            struct ink_string *const str = INK_OBJ_AS_STRING(arg);

            for (size_t i = 0; i < str->length; i++) {
                rc = ink_byte_vec_push(&pending_content, str->bytes[i]);
                if (rc < 0) {
                    return rc;
                }
            }
            break;
        }
        case INK_OP_CHOICE_PUSH: {
            struct ink_object *const arg = ink_story_stack_pop(story);
            struct ink_object *const str = ink_string_new(
                story, pending_content.entries, pending_content.count);
            const struct ink_choice choice = {
                .id = arg,
                .text = INK_OBJ_AS_STRING(str),
            };

            ink_choice_vec_push(&story->current_choices, choice);
            ink_byte_vec_shrink(&pending_content, 0);
            break;
        }
        case INK_OP_LOAD_CHOICE_ID: {
            rc = ink_story_stack_push(story, story->choice_id);
            if (rc < 0) {
                goto exit_loop;
            }
            break;
        }
        case INK_OP_FLUSH: {
            if (pending_content.count > 0) {
                struct ink_object *str = ink_string_new(
                    story, pending_content.entries, pending_content.count);

                story->current_content = str;
            }
            rc = INK_E_OK;
            goto exit_loop;
        }
        default:
            rc = -INK_E_INVALID_INST;
            goto exit_loop;
        }
    }
exit_loop:
    ink_byte_vec_deinit(&pending_content);
    return rc;
}

#undef INK_BINARY_OP
#undef INK_LOGICAL_OP
#undef INK_READ_BYTE
#undef INK_READ_ADDR

int ink_story_load_opts(struct ink_story *story,
                        const struct ink_load_opts *opts)
{
    int rc = -1;

    if (!opts->source_bytes) {
        return -INK_E_PANIC;
    }

    story->can_continue = true;
    story->flags = opts->flags & ~INK_F_GC_ENABLE;
    story->stack_top = 0;
    story->call_stack_top = 0;
    story->gc_allocated = 0;
    story->gc_threshold = INK_GC_HEAP_SIZE_MIN;
    story->gc_gray_count = 0;
    story->gc_gray_capacity = 0;
    story->gc_gray = NULL;
    story->gc_objects = NULL;
    story->globals = ink_table_new(story);
    story->paths = ink_table_new(story);
    story->current_path = NULL;
    story->current_content = NULL;
    story->choice_id = NULL;

    memset(story->stack, 0, sizeof(struct ink_object *) * INK_STORY_STACK_MAX);
    memset(story->call_stack, 0,
           sizeof(struct ink_call_frame *) * INK_STORY_STACK_MAX);

    ink_choice_vec_init(&story->current_choices);

    rc = ink_compile(story, opts);
    if (rc < 0) {
        goto err;
    }
    if (opts->flags & INK_F_GC_ENABLE) {
        story->flags |= INK_F_GC_ENABLE;
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
err:
    return rc;
}

int ink_story_load(struct ink_story *story, const char *source, int flags)
{
    const struct ink_load_opts opts = {
        .flags = flags,
        .source_bytes = (uint8_t *)source,
        .source_length = strlen(source),
        .filename = (uint8_t *)"<STDIN>",
    };

    return ink_story_load_opts(story, &opts);
}

void ink_story_free(struct ink_story *story)
{
    ink_choice_vec_deinit(&story->current_choices);

    while (story->gc_objects) {
        struct ink_object *const obj = story->gc_objects;

        story->gc_objects = story->gc_objects->next;
        ink_object_free(story, obj);
    }

    ink_free(story->gc_gray);
    memset(story, 0, sizeof(*story));
}

int ink_story_continue(struct ink_story *story, struct ink_string **content)
{
    const int rc = ink_story_execute(story);

    if (rc < 0) {
        return rc;
    }
    if (content) {
        *content = INK_OBJ_AS_STRING(story->current_content);
    }
    return INK_E_OK;
}

int ink_story_choose(struct ink_story *story, size_t index)
{
    if (index < 0) {
        index = 0;
    } else if (index > 0) {
        index--;
    }
    if (index < story->current_choices.count) {
        struct ink_choice *const choice =
            &story->current_choices.entries[index];

        story->choice_id = choice->id;
        return INK_E_OK;
    }
    return -1;
}
