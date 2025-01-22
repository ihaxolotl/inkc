#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "opcode.h"
#include "story.h"

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
        fprintf(stdout, "[TRACE] Allocating memory (%p) %zu -> %zu\n", ptr,
                size_old, size_new);
    }
    if (size_new == 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, size_new);
}

void ink_story_mem_free(struct ink_story *story, void *ptr)
{
    fprintf(stdout, "[TRACE] Free memory region (%p)\n", ptr);
    ink_story_mem_alloc(story, ptr, 0, 0);
}

void ink_story_mem_flush(struct ink_story *story)
{
    story->stack_top = 0;

    while (story->objects != NULL) {
        struct ink_object *obj = story->objects;

        story->objects = story->objects->next;
        ink_story_mem_free(story, obj);
    }
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
        ink_story_object_print(story->stack[slot]);
        printf(", ");
    }

    printf("]");
    printf("\n");
}

int ink_story_constant_add(struct ink_story *story, struct ink_object *object)
{
    return ink_object_vec_push(&story->constants, object);
}

int ink_story_constant_get(struct ink_story *story, size_t index,
                           struct ink_object **object)
{
    if (index > story->constants.count) {
        return -INK_STORY_ERR_INVALID_ARG;
    }

    *object = story->constants.entries[index];
    return INK_STORY_OK;
}

#define INK_STORY_BINARY_OP(story, proc)                                       \
    do {                                                                       \
        struct ink_object *res = NULL;                                         \
        const struct ink_object *const arg2 = ink_story_stack_pop(story);      \
        const struct ink_object *const arg1 = ink_story_stack_pop(story);      \
                                                                               \
        if (!INK_OBJECT_IS_NUMBER(arg1) || !INK_OBJECT_IS_NUMBER(arg2)) {      \
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
    unsigned char byte;

    if (story->code.count == 0) {
        return INK_STORY_OK;
    }

    story->pc = &story->code.entries[0];

    for (;;) {
        ink_story_stack_print(story);
        byte = *story->pc++;

        switch (byte) {
        case INK_OP_RET: {
            rc = INK_STORY_OK;
            goto exit_loop;
        }
        case INK_OP_POP: {
            ink_story_stack_pop(story);
            break;
        }
        case INK_OP_LOAD_CONST: {
            struct ink_object *arg = NULL;
            const unsigned char operand = *story->pc++;

            rc = ink_story_constant_get(story, operand, &arg);
            if (rc < 0) {
                goto exit_loop;
            }

            rc = ink_story_stack_push(story, arg);
            if (rc < 0) {
                goto exit_loop;
            }
            break;
        };
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
        default:
            goto exit_loop;
        }
    }
exit_loop:
    return rc;
}

void ink_story_create(struct ink_story *story)
{
    story->pc = NULL;
    story->objects = NULL;
    story->stack_top = 0;
    story->stack[0] = NULL;

    ink_bytecode_vec_create(&story->code);
    ink_object_vec_create(&story->constants);
}

void ink_story_destroy(struct ink_story *story)
{
    ink_story_mem_flush(story);
    ink_bytecode_vec_destroy(&story->code);
    ink_object_vec_destroy(&story->constants);
}

#undef INK_STORY_BINARY_OP
