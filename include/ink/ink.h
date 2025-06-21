#ifndef INK_STORY_H
#define INK_STORY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct ink_story;
struct ink_object;

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

struct ink_load_opts {
    const uint8_t *filename;
    const uint8_t *source_bytes;
    size_t source_length;
    int flags;
};

/**
 * Open a new Ink story context.
 */
extern struct ink_story *ink_open(void);

/**
 * Free and deinitialize an Ink story context.
 */
extern void ink_close(struct ink_story *story);

/**
 * Load an Ink story with extended options.
 *
 * Returns a non-zero value on error.
 */
extern int ink_story_load_opts(struct ink_story *story,
                               const struct ink_load_opts *opts);
/**
 * Load an Ink story from a NULL-terminated string of source bytes.
 *
 * Returns a non-zero value on error.
 */
extern int ink_story_load_string(struct ink_story *story, const char *text,
                                 int flags);

/**
 * Load an Ink story from the filesystem.
 *
 * Returns a non-zero value on error.
 */
extern int ink_story_load_file(struct ink_story *story, const char *file_path,
                               int flags);
/**
 * Dump a compiled Ink story.
 *
 * Disassemble bytecode instructions and print values, where available.
 */
extern void ink_story_dump(struct ink_story *story);

/**
 * Determine if the story can continue.
 */
extern bool ink_story_can_continue(struct ink_story *story);

/**
 * Advance the story and output content, if available.
 *
 * Returns a non-zero value on error.
 */
extern int ink_story_continue(struct ink_story *story, uint8_t **line,
                              size_t *linelen);
/**
 * Select a choice by its index.
 *
 * Returns a non-zero value on error.
 */
extern int ink_story_choose(struct ink_story *story, size_t index);

/**
 * Iterator for choices.
 */
extern int ink_story_choice_next(struct ink_story *story,
                                 struct ink_choice *choice);

#ifdef __cplusplus
}
#endif

#endif
