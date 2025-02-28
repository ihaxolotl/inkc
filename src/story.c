#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compile.h"
#include "object.h"
#include "opcode.h"
#include "story.h"

const char *INK_DEFAULT_PATH = "@main";

/* TODO(Brett): Add opcode stuff to new module. */
#define T(name, description) description,
static const char *INK_OPCODE_TYPE_STR[] = {INK_MAKE_OPCODE_LIST(T)};
#undef T

static const char *ink_opcode_strz(enum ink_vm_opcode type)
{
    return INK_OPCODE_TYPE_STR[type];
}

static void ink_disassemble_simple_inst(const struct ink_story *story,
                                        const uint8_t *bytes, size_t offset,
                                        enum ink_vm_opcode opcode)
{
    printf("%s\n", ink_opcode_strz(opcode));
}

static void ink_disassemble_unary_inst(const struct ink_story *story,
                                       const struct ink_object_vec *const_pool,
                                       const uint8_t *bytes, size_t offset,
                                       enum ink_vm_opcode opcode)
{
    const uint8_t arg = bytes[offset + 1];

    if (opcode == INK_OP_CONST) {
        printf("%-16s %4d {", ink_opcode_strz(opcode), arg);
        ink_object_print(const_pool->entries[arg]);
        printf("}\n");
    } else {
        printf("%-16s %4d\n", ink_opcode_strz(opcode), arg);
    }
}

static void ink_disassemble_global_inst(const struct ink_story *story,
                                        const struct ink_object_vec *const_pool,
                                        const uint8_t *bytes, size_t offset,
                                        enum ink_vm_opcode opcode)
{
    const uint8_t arg = bytes[offset + 1];
    const struct ink_string *global_name =
        INK_OBJ_AS_STRING(const_pool->entries[arg]);

    printf("%-16s %4d '%s'\n", ink_opcode_strz(opcode), arg,
           global_name->bytes);
}

static void ink_disassemble_jump_inst(const struct ink_story *story,
                                      const uint8_t *bytes, size_t offset,
                                      enum ink_vm_opcode opcode)
{
    const uint8_t arg = bytes[offset + 1];

    printf("%-16s %4d\n", ink_opcode_strz(opcode), arg);
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
        printf("<%s>:%04zu  | ", path_name->bytes, offset);
    } else {
        printf("%04zu  | ", offset);
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
        ink_disassemble_simple_inst(story, bytes, offset, op);
        break;
    case INK_OP_CONST:
    case INK_OP_LOAD:
    case INK_OP_STORE:
        ink_disassemble_unary_inst(story, const_pool, bytes, offset, op);
        break;
    case INK_OP_LOAD_GLOBAL:
    case INK_OP_STORE_GLOBAL:
    case INK_OP_CALL:
    case INK_OP_DIVERT:
        ink_disassemble_global_inst(story, const_pool, bytes, offset, op);
        break;
    case INK_OP_JMP:
    case INK_OP_JMP_T:
    case INK_OP_JMP_F:
        ink_disassemble_jump_inst(story, bytes, offset, op);
        break;
    default:
        printf("Unknown opcode 0x%x\n", op);
        break;
    }
    return offset + 2;
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
    fprintf(stdout, "[ERROR] %s!\n", fmt);
    exit(EXIT_FAILURE);
}

void ink_story_mem_panic(struct ink_story *story)
{
    (void)story;
    fprintf(stdout, "[ERROR] Out of memory!\n");
    exit(EXIT_FAILURE);
}

void *ink_story_mem_alloc(struct ink_story *story, void *ptr, size_t size_old,
                          size_t size_new)
{
    if (size_new > size_old) {
        if (story->flags & INK_F_TRACING) {
            /*
            fprintf(stdout, "[TRACE] Allocating memory (%p) %zu -> %zu\n", ptr,
                    size_old, size_new);
            */
        }
    }
    if (size_new == 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, size_new);
}

void ink_story_mem_free(struct ink_story *story, void *ptr)
{
    if (story->flags & INK_F_TRACING) {
        /*
        fprintf(stdout, "[TRACE] Free memory region (%p)\n", ptr);
        */
    }

    ink_story_mem_alloc(story, ptr, 0, 0);
}

int ink_story_stack_push(struct ink_story *story, struct ink_object *obj)
{
    assert(obj != NULL);

    if (story->stack_top >= INK_STORY_STACK_MAX) {
        return -INK_STORY_ERR_STACK_OVERFLOW;
    }

    story->stack[story->stack_top++] = obj;
    return INK_STORY_OK;
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

#define INK_STORY_LOGICAL_OP(story, proc)                                      \
    do {                                                                       \
        struct ink_object *value = NULL;                                       \
        const struct ink_object *const arg2 = ink_story_stack_pop(story);      \
        const struct ink_object *const arg1 = ink_story_stack_pop(story);      \
                                                                               \
        if (!INK_OBJ_IS_NUMBER(arg1) || !INK_OBJ_IS_NUMBER(arg2)) {            \
            rc = -INK_STORY_ERR_INVALID_ARG;                                   \
            goto exit_loop;                                                    \
        }                                                                      \
                                                                               \
        value = ink_bool_new(story, proc(story, arg1, arg2));                  \
        if (!value) {                                                          \
            rc = -INK_STORY_ERR_MEMORY;                                        \
            goto exit_loop;                                                    \
        }                                                                      \
                                                                               \
        rc = ink_story_stack_push(story, value);                               \
        if (rc < 0) {                                                          \
            goto exit_loop;                                                    \
        }                                                                      \
    } while (0)

#define INK_STORY_BINARY_OP(story, proc)                                       \
    do {                                                                       \
        struct ink_object *value = NULL;                                       \
        const struct ink_object *const arg2 = ink_story_stack_pop(story);      \
        const struct ink_object *const arg1 = ink_story_stack_pop(story);      \
                                                                               \
        if (!INK_OBJ_IS_NUMBER(arg1) || !INK_OBJ_IS_NUMBER(arg2)) {            \
            rc = -INK_STORY_ERR_INVALID_ARG;                                   \
            goto exit_loop;                                                    \
        }                                                                      \
                                                                               \
        value = proc(story, arg1, arg2);                                       \
        if (!value) {                                                          \
            rc = -INK_STORY_ERR_MEMORY;                                        \
            goto exit_loop;                                                    \
        }                                                                      \
                                                                               \
        rc = ink_story_stack_push(story, value);                               \
        if (rc < 0) {                                                          \
            goto exit_loop;                                                    \
        }                                                                      \
    } while (0)

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

static int ink_story_execute(struct ink_story *story)
{
    int rc = -1;
    struct ink_object *temp[4];
    struct ink_object *const globals_pool = story->globals;
    struct ink_object *const paths_pool = story->paths;
    struct ink_byte_vec pending_content;

    ink_byte_vec_init(&pending_content);
    ink_choice_vec_shrink(&story->current_choices, 0);
    story->current_content = NULL;

    for (;;) {
        struct ink_call_frame *const frame =
            &story->call_stack[story->call_stack_top - 1];
        struct ink_content_path *const path =
            INK_OBJ_AS_CONTENT_PATH(frame->callee);
        struct ink_object_vec *const const_pool = &path->const_pool;

        if (story->flags & INK_F_TRACING) {
            ink_trace_exec(story, frame);
        }

        const uint8_t byte = *frame->ip++;
        const uint8_t arg = *frame->ip++;

        switch (byte) {
        case INK_OP_EXIT:
            rc = INK_STORY_OK;
            story->can_continue = false;
            goto exit_loop;
        case INK_OP_RET:
            temp[0] = ink_story_stack_pop(story);

            story->call_stack_top--;
            if (story->call_stack_top == 0) {
                ink_story_stack_pop(story);
                rc = INK_E_OK;
                goto exit_loop;
            }

            story->stack_top = (size_t)(frame->sp - story->stack);
            ink_story_stack_push(story, temp[0]);
            break;
        case INK_OP_POP:
            if (!ink_story_stack_pop(story)) {
                rc = -INK_STORY_ERR_INVALID_ARG;
                goto exit_loop;
            }
            break;
        case INK_OP_TRUE:
            temp[0] = ink_bool_new(story, true);

            rc = ink_story_stack_push(story, temp[0]);
            if (rc < 0) {
                goto exit_loop;
            }
            break;
        case INK_OP_FALSE:
            temp[0] = ink_bool_new(story, false);

            rc = ink_story_stack_push(story, temp[0]);
            if (rc < 0) {
                goto exit_loop;
            }
            break;
        case INK_OP_CONST:
            if (arg > const_pool->count) {
                rc = -INK_STORY_ERR_INVALID_ARG;
                goto exit_loop;
            }

            rc = ink_story_stack_push(story, const_pool->entries[arg]);
            if (rc < 0) {
                goto exit_loop;
            }
            break;
        case INK_OP_ADD:
            INK_STORY_BINARY_OP(story, ink_number_add);
            break;
        case INK_OP_SUB:
            INK_STORY_BINARY_OP(story, ink_number_sub);
            break;
        case INK_OP_MUL:
            INK_STORY_BINARY_OP(story, ink_number_mul);
            break;
        case INK_OP_DIV:
            INK_STORY_BINARY_OP(story, ink_number_div);
            break;
        case INK_OP_MOD:
            INK_STORY_BINARY_OP(story, ink_number_mod);
            break;
        case INK_OP_NEG:
            temp[0] = ink_story_stack_peek(story, 0);
            temp[1] = ink_number_neg(story, temp[0]);

            ink_story_stack_pop(story);
            ink_story_stack_push(story, temp[1]);
            break;
        case INK_OP_NOT:
            break;
        case INK_OP_CMP_EQ:
            temp[0] = ink_story_stack_pop(story);
            temp[1] = ink_story_stack_pop(story);
            temp[2] = ink_object_eq(story, temp[0], temp[1]);

            ink_story_stack_push(story, temp[2]);
            break;
        case INK_OP_CMP_LT:
            INK_STORY_LOGICAL_OP(story, ink_number_lt);
            break;
        case INK_OP_CMP_GT:
            INK_STORY_LOGICAL_OP(story, ink_number_gt);
            break;
        case INK_OP_CMP_LTE:
            INK_STORY_LOGICAL_OP(story, ink_number_lte);
            break;
        case INK_OP_CMP_GTE:
            INK_STORY_LOGICAL_OP(story, ink_number_gte);
            break;
        case INK_OP_JMP:
            frame->ip += arg;
            break;
        case INK_OP_JMP_T:
            temp[0] = ink_story_stack_peek(story, 0);

            if (!ink_object_is_falsey(temp[0])) {
                frame->ip += arg;
            }
            break;
        case INK_OP_JMP_F:
            temp[0] = ink_story_stack_peek(story, 0);

            if (ink_object_is_falsey(temp[0])) {
                frame->ip += arg;
            }
            break;
        case INK_OP_DIVERT:
            temp[0] = const_pool->entries[arg];
            temp[1] = NULL;

            rc = ink_table_lookup(story, paths_pool, temp[0], &temp[1]);
            if (rc < 0) {
                goto exit_loop;
            }

            rc = ink_story_divert(story, temp[1]);
            if (rc < 0) {
                goto exit_loop;
            }
            break;
        case INK_OP_CALL:
            temp[0] = const_pool->entries[arg];
            temp[1] = NULL;

            rc = ink_table_lookup(story, paths_pool, temp[0], &temp[1]);
            if (rc < 0) {
                goto exit_loop;
            }

            rc = ink_story_call(story, temp[1]);
            if (rc < 0) {
                goto exit_loop;
            }
            break;
        case INK_OP_LOAD:
            temp[0] = frame->sp[arg];
            ink_story_stack_push(story, temp[0]);
            break;
        case INK_OP_STORE:
            temp[0] = ink_story_stack_peek(story, 0);
            frame->sp[arg] = temp[0];
            break;
        case INK_OP_LOAD_GLOBAL:
            temp[0] = const_pool->entries[arg];

            rc = ink_table_lookup(story, globals_pool, temp[0], &temp[1]);
            if (rc < 0) {
                goto exit_loop;
            }

            rc = ink_story_stack_push(story, temp[1]);
            if (rc < 0) {
                goto exit_loop;
            }
            break;
        case INK_OP_STORE_GLOBAL:
            temp[0] = const_pool->entries[arg];
            temp[1] = ink_story_stack_pop(story);

            rc = ink_table_insert(story, globals_pool, temp[0], temp[1]);
            if (rc < 0) {
                goto exit_loop;
            }

            rc = ink_story_stack_push(story, temp[1]);
            if (rc < 0) {
                goto exit_loop;
            }
            break;
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
        case INK_OP_LOAD_CHOICE_ID:
            rc = ink_story_stack_push(story, story->choice_id);
            if (rc < 0) {
                goto exit_loop;
            }
            break;
        case INK_OP_FLUSH:
            rc = ink_byte_vec_push(&pending_content, '\0');
            if (rc < 0) {
                return rc;
            }

            struct ink_object *const str = ink_string_new(
                story, pending_content.entries, pending_content.count);

            story->current_content = str;
            rc = INK_STORY_OK;
            goto exit_loop;
        default:
            rc = -INK_E_INVALID_INST;
            goto exit_loop;
        }
    }
exit_loop:
    ink_byte_vec_deinit(&pending_content);
    return rc;
}

#undef INK_STORY_BINARY_OP

static void ink_story_init(struct ink_story *story, int flags)
{
    memset(story, 0, sizeof(*story));

    ink_choice_vec_init(&story->current_choices);

    story->can_continue = true;
    story->flags = flags;
    story->globals = ink_table_new(story);
    story->paths = ink_table_new(story);
}

static void ink_story_deinit(struct ink_story *story)
{
    ink_choice_vec_deinit(&story->current_choices);

    while (story->objects) {
        struct ink_object *obj = story->objects;

        story->objects = story->objects->next;
        ink_object_free(story, obj);
    }

    memset(story, 0, sizeof(*story));
}

int ink_story_load_opts(struct ink_story *story,
                        const struct ink_load_opts *opts)
{
    int rc = -1;
    const int flags = opts->flags;
    const uint8_t *const filename = opts->filename;
    const uint8_t *const source_bytes = opts->source_text;

    ink_story_init(story, flags);

    rc = ink_compile(source_bytes, filename, story, flags);
    if (rc < 0) {
        return rc;
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
    return 0;
}

int ink_story_load(struct ink_story *story, const char *source, int flags)
{
    const struct ink_load_opts opts = {
        .flags = flags,
        .source_text = (uint8_t *)source,
        .filename = (uint8_t *)"<STDIN>",
    };

    return ink_story_load_opts(story, &opts);
}

void ink_story_free(struct ink_story *story)
{
    ink_story_deinit(story);
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
    return INK_STORY_OK;
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
