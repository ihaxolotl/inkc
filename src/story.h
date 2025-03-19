#ifndef __INK_STORY_H__
#define __INK_STORY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include "common.h"
#include "vec.h"

struct ink_story;
struct ink_object;
struct ink_string;

enum ink_flags {
    INK_F_RESERVED_1 = (1 << 0),
    INK_F_RESERVED_2 = (1 << 1),
    INK_F_RESERVED_3 = (1 << 2),
    INK_F_CACHING = (1 << 3),
    INK_F_COLOR = (1 << 4),
    INK_F_DUMP_AST = (1 << 5),
    INK_F_RESERVED_4 = (1 << 6),
    INK_F_DUMP_CODE = (1 << 7),
    INK_F_GC_ENABLE = (1 << 8),
    INK_F_GC_STRESS = (1 << 9),
    INK_F_GC_TRACING = (1 << 10),
    INK_F_VM_TRACING = (1 << 11),
    INK_F_RESERVED_5 = (1 << 12),
    INK_F_RESERVED_6 = (1 << 13),
    INK_F_RESERVED_7 = (1 << 14),
    INK_F_RESERVED_8 = (1 << 15),
};

struct ink_choice {
    struct ink_object *id;
    uint8_t *bytes;
    size_t length;
};

INK_VEC_T(ink_choice_vec, struct ink_choice)

struct ink_load_opts {
    const uint8_t *filename;
    const uint8_t *source_bytes;
    size_t source_length;
    int flags;
};

extern struct ink_story *ink_open(void);
extern void ink_close(struct ink_story *story);
extern int ink_story_load_opts(struct ink_story *story,
                               const struct ink_load_opts *opts);
extern int ink_story_load_string(struct ink_story *story, const char *text,
                                 int flags);
extern int ink_story_load_file(struct ink_story *story, const char *file_path,
                               int flags);
extern void ink_story_dump(struct ink_story *story);
extern int ink_story_stack_push(struct ink_story *story,
                                struct ink_object *object);
extern struct ink_object *ink_story_stack_pop(struct ink_story *story);
extern struct ink_object *ink_story_stack_peek(struct ink_story *story,
                                               size_t offset);
extern bool ink_story_can_continue(struct ink_story *story);
extern int ink_story_continue(struct ink_story *story, uint8_t **line,
                              size_t *linelen);
extern void ink_story_get_choices(struct ink_story *story,
                                  struct ink_choice_vec *choices);
extern int ink_story_choose(struct ink_story *story, size_t index);

#ifdef __cplusplus
}
#endif

#endif
