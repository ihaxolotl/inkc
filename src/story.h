#ifndef __INK_STORY_H__
#define __INK_STORY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include "common.h"
#include "vec.h"

struct ink_object;

#define INK_STORY_STACK_MAX 128

enum ink_flags {
    INK_F_TRACING = (1 << 0),
    INK_F_CACHING = (1 << 1),
    INK_F_COLOR = (1 << 2),
    INK_F_DUMP_AST = (1 << 3),
    INK_F_DUMP_IR = (1 << 4),
    INK_F_DUMP_CODE = (1 << 5),
    INK_F_GC_ENABLE = (1 << 6),
    INK_F_GC_STRESS = (1 << 7),
};

struct ink_choice {
    struct ink_object *id;
    struct ink_string *text;
};

struct ink_call_frame {
    struct ink_content_path *callee;
    struct ink_content_path *caller;
    uint8_t *ip;
    struct ink_object **sp;
};

INK_VEC_T(ink_object_vec, struct ink_object *)
INK_VEC_T(ink_choice_vec, struct ink_choice)
INK_VEC_T(ink_byte_vec, uint8_t)

/**
 * Ink Story Context
 */
struct ink_story {
    bool can_continue;
    int flags;
    size_t stack_top;
    size_t call_stack_top;
    size_t gc_allocated;
    size_t gc_threshold;
    size_t gc_gray_count;
    size_t gc_gray_capacity;
    struct ink_object **gc_gray;
    struct ink_object *gc_objects;
    struct ink_object *globals;
    struct ink_object *paths;
    struct ink_object *current_path;
    struct ink_object *current_content;
    struct ink_object *choice_id;
    struct ink_choice_vec current_choices;
    struct ink_object *stack[INK_STORY_STACK_MAX];
    struct ink_call_frame call_stack[INK_STORY_STACK_MAX];
};

struct ink_load_opts {
    const uint8_t *filename;
    const uint8_t *source_bytes;
    size_t source_length;
    int flags;
};

extern int ink_story_load_opts(struct ink_story *story,
                               const struct ink_load_opts *opts);
extern int ink_story_load(struct ink_story *story, const char *text, int flags);
extern void ink_story_free(struct ink_story *story);
extern void ink_story_dump(struct ink_story *story);
extern void *ink_story_mem_alloc(struct ink_story *story, void *ptr,
                                 size_t size_old, size_t size_new);
extern void ink_story_mem_free(struct ink_story *story, void *ptr);
extern int ink_story_stack_push(struct ink_story *story,
                                struct ink_object *object);
extern struct ink_object *ink_story_stack_pop(struct ink_story *story);
extern struct ink_object *ink_story_stack_peek(struct ink_story *story,
                                               size_t offset);
extern int ink_story_continue(struct ink_story *story,
                              struct ink_string **content);
extern int ink_story_choose(struct ink_story *story, size_t index);

#ifdef __cplusplus
}
#endif

#endif
