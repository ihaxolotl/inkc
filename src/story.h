#ifndef INK_STORY_H
#define INK_STORY_H

#include <stddef.h>

#include <ink/ink.h>

#include "hashmap.h"
#include "stream.h"
#include "vec.h"

#define INK_STORY_STACK_MAX (128ul)
#define INK_GC_GRAY_CAPACITY_MIN (16ul)
#define INK_GC_GRAY_GROWTH_FACTOR (2ul)
#define INK_GC_HEAP_SIZE_MIN (1024ul * 1024ul)
#define INK_GC_HEAP_GROWTH_PERCENT (50ul)
#define INK_OBJECT_SET_LOAD_MAX (80ul)

struct ink_object_set_key {
    struct ink_object *obj;
};

INK_VEC_T(ink_choice_vec, struct ink_choice)
INK_HASHMAP_T(ink_object_set, struct ink_object_set_key, void *)

struct ink_call_frame {
    struct ink_content_path *callee;
    struct ink_content_path *caller;
    uint8_t *ip;
    struct ink_object **sp;
};

struct ink_story {
    /* TODO: Could this be added to `flags`? */
    bool is_exited;
    /* TODO: Could this be added to `flags`? */
    bool can_continue;
    int flags;
    size_t choice_index;
    size_t stack_top;
    size_t call_stack_top;
    size_t gc_allocated;
    size_t gc_threshold;
    struct ink_object_vec gc_gray;
    struct ink_object_set gc_owned;
    struct ink_object *gc_objects;
    struct ink_object *globals;
    struct ink_object *paths;
    struct ink_object *current_path;
    struct ink_object *current_choice_id;
    struct ink_choice_vec current_choices;
    struct ink_stream stream;
    struct ink_object *stack[INK_STORY_STACK_MAX];
    struct ink_call_frame call_stack[INK_STORY_STACK_MAX];
};

/**
 * Allocate memory for runtime objects.
 */
extern void *ink_story_mem_alloc(struct ink_story *story, void *ptr,
                                 size_t size_old, size_t size_new);

/**
 * Free memory allocated for runtime objects.
 */
extern void ink_story_mem_free(struct ink_story *story, void *ptr);

/**
 * Record an object as a root, preventing collection.
 */
extern void ink_gc_own(struct ink_story *story, struct ink_object *obj);

/**
 * Release record for object, if present.
 */
extern void ink_gc_disown(struct ink_story *story, struct ink_object *obj);

#endif
