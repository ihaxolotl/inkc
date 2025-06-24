#include <time.h>

#include "gc.h"
#include "logging.h"
#include "object.h"
#include "story.h"

static void ink_gc_mark_object(struct ink_story *story, struct ink_object *obj)
{
    if (!obj || obj->is_marked) {
        return;
    }

    obj->is_marked = true;

    if (story->flags & INK_F_GC_TRACING) {
        ink_trace("Marked object %p, type=%s", (void *)obj,
                  ink_object_type_strz(obj->type));
    }

    ink_object_vec_push(&story->gc_gray, obj);
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

    if (story->flags & INK_F_GC_TRACING) {
        ink_trace("Blackened object %p, type=%s, size=%zu", (void *)obj,
                  ink_object_type_strz(obj->type), obj_size);
    }
}

void ink_gc_own(struct ink_story *story, struct ink_object *obj)
{
    const struct ink_object_set_key key = {
        .obj = obj,
    };

    ink_object_set_insert(&story->gc_owned, key, NULL);
}

void ink_gc_disown(struct ink_story *story, struct ink_object *obj)
{
    const struct ink_object_set_key key = {
        .obj = obj,
    };

    ink_object_set_remove(&story->gc_owned, key);
}

void ink_gc_collect(struct ink_story *story)
{
    double time_start, time_elapsed;
    size_t bytes_before, bytes_after;

    if (story->flags & INK_F_GC_TRACING) {
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
    for (size_t i = 0; i < story->gc_owned.capacity; i++) {
        struct ink_object_set_kv *const entry = &story->gc_owned.entries[i];

        if (entry->key.obj) {
            ink_gc_mark_object(story, INK_OBJ(entry->key.obj));
        }
    }

    ink_gc_mark_object(story, story->globals);
    ink_gc_mark_object(story, story->paths);
    ink_gc_mark_object(story, story->current_path);
    ink_gc_mark_object(story, story->current_choice_id);

    for (size_t i = 0; i < story->current_choices.count; i++) {
        struct ink_choice *const choice = &story->current_choices.entries[i];

        ink_gc_mark_object(story, choice->id);
    }
    while (story->gc_gray.count > 0) {
        struct ink_object *const obj = ink_object_vec_pop(&story->gc_gray);

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
    if (story->flags & INK_F_GC_TRACING) {
        time_elapsed = ((double)clock() / CLOCKS_PER_SEC) - time_start;
        bytes_after = story->gc_allocated;

        ink_trace("Collection completed in %.3fms, before=%zu, after=%zu, "
                  "collected=%zu, next at %zu",
                  time_elapsed * 1000.0, bytes_before, bytes_after,
                  bytes_before - bytes_after, story->gc_threshold);
    }
}
