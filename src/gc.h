#ifndef INK_GC_H
#define INK_GC_H

#ifdef __cplusplus
extern "C" {
#endif

struct ink_story;
struct ink_object;

/**
 * Collect garbage.
 *
 * Rudamentary tracing garbage collector.
 */
extern void ink_gc_collect(struct ink_story *story);

/**
 * Record an object as a root, preventing collection.
 */
extern void ink_gc_own(struct ink_story *story, struct ink_object *obj);

/**
 * Release record for object, if present, allowing for collection.
 */
extern void ink_gc_disown(struct ink_story *story, struct ink_object *obj);

#ifdef __cplusplus
}
#endif

#endif
