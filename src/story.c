#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "ast.h"
#include "astgen.h"
#include "codegen.h"
#include "ir.h"
#include "object.h"
#include "opcode.h"
#include "parse.h"
#include "story.h"

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
                                       const uint8_t *bytes, size_t offset,
                                       enum ink_vm_opcode opcode)
{
    const struct ink_object_vec *const consts = &story->constants;
    const uint8_t arg = bytes[offset + 1];

    if (opcode == INK_OP_CONST) {
        printf("%-16s %4d '", ink_opcode_strz(opcode), arg);
        ink_object_print(consts->entries[arg]);
        printf("'\n");
    } else {
        printf("%-16s %4d\n", ink_opcode_strz(opcode), arg);
    }
}

static void ink_disassemble_global_inst(const struct ink_story *story,
                                        const uint8_t *bytes, size_t offset,
                                        enum ink_vm_opcode opcode)
{
    const struct ink_object_vec *const consts = &story->constants;
    const uint8_t arg = bytes[offset + 1];
    const struct ink_string *global_name =
        INK_OBJ_AS_STRING(consts->entries[arg]);

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
                                    const uint8_t *bytes, size_t offset)
{
    const uint8_t op = bytes[offset];

    printf("%04zu  | ", offset);

    switch (op) {
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
    case INK_OP_CONTENT_POST:
    case INK_OP_CONTENT_PUSH: {
        ink_disassemble_simple_inst(story, bytes, offset, op);
        break;
    }
    case INK_OP_CONST:
    case INK_OP_LOAD:
    case INK_OP_STORE: {
        ink_disassemble_unary_inst(story, bytes, offset, op);
        break;
    }
    case INK_OP_LOAD_GLOBAL:
    case INK_OP_STORE_GLOBAL: {
        ink_disassemble_global_inst(story, bytes, offset, op);
        break;
    }
    case INK_OP_JMP:
    case INK_OP_JMP_T:
    case INK_OP_JMP_F: {
        ink_disassemble_jump_inst(story, bytes, offset, op);
        break;
    }
    default:
        printf("Unknown opcode 0x%x\n", op);
        break;
    }
    return offset + 2;
}

static void ink_story_dump(const struct ink_story *story)
{
    const uint8_t *bytes;
    size_t length;
    struct ink_string *path_name;
    struct ink_content_path *path;
    const struct ink_table *const paths_table = INK_OBJ_AS_TABLE(story->paths);
    const struct ink_byte_vec *const code_bytes = &story->code;

    for (size_t i = 0; i < paths_table->capacity; i++) {
        if (paths_table->entries[i].key) {
            path = INK_OBJ_AS_CONTENT_PATH(paths_table->entries[i].value);
            path_name = INK_OBJ_AS_STRING(path->name);
            bytes = &code_bytes->entries[path->code_offset];
            length = path->code_length;

            assert(path->code_length > 0);
            printf("=== %s(args: %u, locals: %u) ===\n", path_name->bytes,
                   path->args_count, path->locals_count);

            for (size_t offset = 0; offset < length;) {
                offset = ink_story_disassemble(story, bytes, offset);
            }
        }
    }
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
            fprintf(stdout, "[TRACE] Allocating memory (%p) %zu -> %zu\n", ptr,
                    size_old, size_new);
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
        fprintf(stdout, "[TRACE] Free memory region (%p)\n", ptr);
    }

    ink_story_mem_alloc(story, ptr, 0, 0);
}

void ink_story_mem_flush(struct ink_story *story)
{
    story->stack_top = 0;

    while (story->objects != NULL) {
        struct ink_object *obj = story->objects;

        story->objects = story->objects->next;
        ink_object_free(story, obj);
    }
}

int ink_story_load(struct ink_story *story, const char *text, int flags)
{
    int rc;
    struct ink_arena arena;
    struct ink_ast ast;
    struct ink_ir ircode;
    static const size_t arena_alignment = 8;
    static const size_t arena_block_size = 8192;

    ink_arena_init(&arena, arena_block_size, arena_alignment);
    ink_ast_init(&ast, "<STDIN>", (uint8_t *)text);

    rc = ink_parse(&ast, &arena, flags);
    if (rc < 0) {
        goto out;
    }
    if (flags & INK_F_DUMP_AST) {
        ink_ast_print(&ast, flags & INK_F_COLOR);
    }

    rc = ink_astgen(&ast, &ircode, flags);
    if (rc < 0) {
        goto out;
    }
    if (flags & INK_F_DUMP_IR) {
        ink_ir_dump(&ircode);
    }

    rc = ink_codegen(&ircode, story, flags);
    if (rc < 0) {
        goto out;
    }
    if (flags & INK_F_DUMP_CODE) {
        ink_story_dump(story);
    }
out:
    ink_ir_deinit(&ircode);
    ink_ast_deinit(&ast);
    ink_arena_release(&arena);
    return rc;
}

void ink_story_free(struct ink_story *story)
{
    ink_story_deinit(story);
}

static char *ink_story_content_copy(struct ink_story *story)
{
    char *str;
    const size_t size = story->content.count;

    str = ink_malloc(size + 1);
    if (!str) {
        return str;
    }

    memcpy(str, story->content.entries, size);
    str[size] = '\0';
    return str;
}

static int ink_story_content_push(struct ink_story *story,
                                  struct ink_object *obj)
{
    int rc;
    struct ink_string *const str = INK_OBJ_AS_STRING(obj);

    for (size_t i = 0; i < str->length; i++) {
        rc = ink_byte_vec_push(&story->content, str->bytes[i]);
        if (rc < 0) {
            return rc;
        }
    }
    return INK_E_OK;
}

static void ink_story_content_clear(struct ink_story *story)
{
    ink_byte_vec_shrink(&story->content, 0);

    if (story->content.capacity > 0) {
        story->content.entries[0] = '\0';
    }
}

char *ink_story_continue(struct ink_story *story)
{
    const int rc = ink_story_execute(story);

    if (rc < 0) {
        return NULL;
    }
    return ink_story_content_copy(story);
}

int ink_story_constant_get(struct ink_story *story, size_t index,
                           struct ink_object **obj)
{
    if (index > story->constants.count) {
        return -INK_STORY_ERR_INVALID_ARG;
    }

    *obj = story->constants.entries[index];
    return INK_STORY_OK;
}

int ink_story_stack_push(struct ink_story *story, struct ink_object *object)
{
    assert(object != NULL);

    if (story->stack_top >= INK_STORY_STACK_MAX) {
        return -INK_STORY_ERR_STACK_OVERFLOW;
    }

    story->stack[story->stack_top++] = object;
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
    return story->stack[story->stack_top - offset];
}

void ink_story_stack_print(struct ink_story *story)
{
    printf("          ");
    printf("[ ");

    for (size_t slot = 0; slot < story->stack_top; slot++) {
        ink_object_print(story->stack[slot]);
        printf(", ");
    }

    printf("]");
    printf("\n");
}

#define INK_STORY_BINARY_OP(story, proc)                                       \
    do {                                                                       \
        struct ink_object *res = NULL;                                         \
        const struct ink_object *const arg2 = ink_story_stack_pop(story);      \
        const struct ink_object *const arg1 = ink_story_stack_pop(story);      \
                                                                               \
        if (!INK_OBJ_IS_NUMBER(arg1) || !INK_OBJ_IS_NUMBER(arg2)) {            \
            return -INK_STORY_ERR_INVALID_ARG;                                 \
        }                                                                      \
                                                                               \
        res = proc(story, arg1, arg2);                                         \
        if (res == NULL) {                                                     \
            rc = -INK_STORY_ERR_MEMORY;                                        \
            goto exit_loop;                                                    \
        }                                                                      \
                                                                               \
        rc = ink_story_stack_push(story, res);                                 \
        if (rc < 0) {                                                          \
            goto exit_loop;                                                    \
        }                                                                      \
    } while (0)

int ink_story_execute(struct ink_story *story)
{
    int rc;
    uint8_t byte, arg;

    if (story->code.count == 0) {
        return INK_STORY_OK;
    }
    if (story->pc == NULL) {
        story->pc = &story->code.entries[0];
    }
    ink_story_content_clear(story);

    for (;;) {
        if (story->flags & INK_F_TRACING) {
            ink_story_stack_print(story);
            ink_story_disassemble(story, story->code.entries,
                                  (size_t)(story->pc - story->code.entries));
        }

        byte = *story->pc++;
        arg = *story->pc++;

        switch (byte) {
        case INK_OP_RET: {
            rc = INK_STORY_OK;
            story->can_continue = false;
            goto exit_loop;
        }
        case INK_OP_POP: {
            ink_story_stack_pop(story);
            break;
        }
        case INK_OP_CONST: {
            struct ink_object *constant;

            rc = ink_story_constant_get(story, arg, &constant);
            if (rc < 0) {
                goto exit_loop;
            }

            rc = ink_story_stack_push(story, constant);
            if (rc < 0) {
                goto exit_loop;
            }
            break;
        }
        case INK_OP_LOAD: {
            break;
        }
        case INK_OP_STORE: {
            break;
        }
        case INK_OP_LOAD_GLOBAL: {
            break;
        }
        case INK_OP_STORE_GLOBAL: {
            break;
        }
        case INK_OP_ADD: {
            INK_STORY_BINARY_OP(story, ink_number_add);
            break;
        }
        case INK_OP_SUB: {
            INK_STORY_BINARY_OP(story, ink_number_subtract);
            break;
        }
        case INK_OP_MUL: {
            INK_STORY_BINARY_OP(story, ink_number_multiply);
            break;
        }
        case INK_OP_DIV: {
            INK_STORY_BINARY_OP(story, ink_number_divide);
            break;
        }
        case INK_OP_MOD: {
            INK_STORY_BINARY_OP(story, ink_number_modulo);
            break;
        }
        case INK_OP_NEG: {
            struct ink_object *const arg = ink_story_stack_peek(story, 0);
            struct ink_object *const res = ink_number_negate(story, arg);

            ink_story_stack_pop(story);
            ink_story_stack_push(story, res);
            break;
        }
        case INK_OP_CONTENT_PUSH: {
            struct ink_object *const arg = ink_story_stack_pop(story);

            ink_story_content_push(story, arg);
            break;
        }
        case INK_OP_CONTENT_POST: {
            rc = INK_STORY_OK;
            goto exit_loop;
        }
        default:
            rc = -1;
            goto exit_loop;
        }
    }
exit_loop:
    return rc;
}

#undef INK_STORY_BINARY_OP

void ink_story_init(struct ink_story *story, int flags)
{
    story->can_continue = true;
    story->flags = flags;
    story->pc = NULL;
    story->stack_top = 0;
    story->objects = NULL;
    story->globals = NULL;
    story->paths = NULL;
    story->stack[0] = NULL;

    ink_byte_vec_init(&story->content);
    ink_byte_vec_init(&story->code);
    ink_object_vec_init(&story->constants);

    story->globals = ink_table_new(story);
    story->paths = ink_table_new(story);
}

void ink_story_deinit(struct ink_story *story)
{
    ink_byte_vec_deinit(&story->content);
    ink_byte_vec_deinit(&story->code);
    ink_object_vec_deinit(&story->constants);
    ink_story_mem_flush(story);
    memset(story, 0, sizeof(*story));
}
