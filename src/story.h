#ifndef __INK_STORY_H__
#define __INK_STORY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "vec.h"

struct ink_object;

#define INK_STORY_STACK_MAX 128

INK_VEC_DECLARE(ink_object_vec, struct ink_object *)
INK_VEC_DECLARE(ink_bytecode_vec, unsigned char)

enum ink_story_err {
    INK_STORY_OK = 0,
    INK_STORY_ERR_MEMORY,
    INK_STORY_ERR_STACK_OVERFLOW,
    INK_STORY_ERR_INVALID_OPCODE,
    INK_STORY_ERR_INVALID_ARG,
};

/**
 * Ink Story Context
 */
struct ink_story {
    /**
     * Program counter / instruction pointer.
     */
    unsigned char *pc;
    /**
     * Stack top offset.
     */
    size_t stack_top;
    /**
     * Constant value table.
     */
    struct ink_object_vec constants;
    /**
     * Bytecode instructions.
     */
    struct ink_bytecode_vec code;
    /**
     * Object chain for tracking.
     */
    struct ink_object *objects;
    /**
     * Evaluation stack.
     */
    struct ink_object *stack[INK_STORY_STACK_MAX];
};

extern void ink_story_create(struct ink_story *story);
extern void ink_story_destroy(struct ink_story *story);
extern int ink_story_execute(struct ink_story *story);
extern void ink_story_mem_panic(struct ink_story *story);
extern void *ink_story_mem_alloc(struct ink_story *story, void *ptr,
                                 size_t size_old, size_t size_new);
extern void ink_story_mem_free(struct ink_story *story, void *ptr);
extern void ink_story_mem_flush(struct ink_story *story);
extern int ink_story_stack_push(struct ink_story *story,
                                struct ink_object *object);
extern struct ink_object *ink_story_stack_pop(struct ink_story *story);
extern struct ink_object *ink_story_stack_peek(struct ink_story *story,
                                               size_t offset);
extern int ink_story_constant_add(struct ink_story *story,
                                  struct ink_object *object);
extern int ink_story_constant_get(struct ink_story *story, size_t index,
                                  struct ink_object **object);
extern void ink_story_stack_print(struct ink_story *story);

#ifdef __cplusplus
}
#endif

#endif
