#ifndef __INK_OBJECT_H__
#define __INK_OBJECT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

#include "story.h"

enum ink_object_type {
    INK_OBJECT_NUMBER,
};

struct ink_object {
    struct ink_object *next;
    enum ink_object_type type;
};

struct ink_number {
    struct ink_object obj;
    double value;
};

#define INK_OBJECT_IS_NUMBER(x) ((x)->type == INK_OBJECT_NUMBER)
#define INK_OBJECT_AS_NUMBER(x) ((struct ink_number *)(x))

extern struct ink_object *ink_story_object_new(struct ink_story *story,
                                               enum ink_object_type type,
                                               size_t size);
extern void ink_story_object_print(const struct ink_object *object);
extern void ink_number_print(const struct ink_object *object);
extern struct ink_object *ink_number_new(struct ink_story *story, double value);
extern struct ink_object *ink_number_add(struct ink_story *story,
                                         const struct ink_object *lhs,
                                         const struct ink_object *rhs);
extern struct ink_object *ink_number_subtract(struct ink_story *story,
                                              const struct ink_object *lhs,
                                              const struct ink_object *rhs);
extern struct ink_object *ink_number_multiply(struct ink_story *story,
                                              const struct ink_object *lhs,
                                              const struct ink_object *rhs);
extern struct ink_object *ink_number_divide(struct ink_story *story,
                                            const struct ink_object *lhs,
                                            const struct ink_object *rhs);
extern struct ink_object *ink_number_modulo(struct ink_story *story,
                                            const struct ink_object *lhs,
                                            const struct ink_object *rhs);
extern struct ink_object *ink_number_negate(struct ink_story *story,
                                            const struct ink_object *lhs);

#ifdef __cplusplus
}
#endif

#endif
