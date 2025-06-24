#ifndef INK_STORY_H
#define INK_STORY_H

/**
 * @defgroup ink_api API
 *
 * This module documents the exported functions that form the public API
 * of libink.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
#ifdef BUILDING_INKLIB
#define INK_API __declspec(dllexport)
#else
#define INK_API __declspec(dllimport)
#endif
#else
#if defined(__GNUC__) || defined(__clang__)
#define INK_API __attribute__((visibility("default")))
#else
#define INK_API
#endif
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @struct ink_story
 *
 * @brief Opaque type representing a story context.
 *
 * This forward declaration hides the internal structure of an `ink_story`.
 * Clients of the API should manipulate `ink_story` objects only through the
 * provided functions.
 */
typedef struct ink_story ink_story;

/**
 * @struct ink_object
 *
 * @brief Opaque type representing a runtime object.
 *
 * This forward declaration hides the internal structure of an `ink_object`.
 * Clients of the API should manipulate `ink_object` objects only through the
 * provided functions.
 */
typedef struct ink_object ink_object;

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
 * @brief Open a story context.
 *
 * @returns a new story context
 */
INK_API struct ink_story *ink_open(void);

/**
 * Close a story context.
 */
INK_API void ink_close(struct ink_story *story);

/**
 * Load an Ink story with extended options.
 *
 * @returns a non-zero value on error.
 */
INK_API int ink_story_load_opts(struct ink_story *story,
                                const struct ink_load_opts *opts);
/**
 * Load an Ink story from a NULL-terminated string of source bytes.
 *
 * @returns a non-zero value on error.
 */
INK_API int ink_story_load_string(struct ink_story *story, const char *text,
                                  int flags);

/**
 * Load an Ink story from the filesystem.
 *
 * @returns a non-zero value on error.
 */
INK_API int ink_story_load_file(struct ink_story *story, const char *file_path,
                                int flags);
/**
 * Dump a compiled Ink story.
 *
 * Disassemble bytecode instructions and print values, where available.
 */
INK_API void ink_story_dump(struct ink_story *story);

/**
 * Determine if the story can continue.
 *
 * @returns a boolean value, indicating if the story can continue.
 */
INK_API bool ink_story_can_continue(struct ink_story *story);

/**
 * Advance the story and output content, if available.
 *
 * @returns a non-zero value on error.
 */
INK_API int ink_story_continue(struct ink_story *story, uint8_t **line,
                               size_t *linelen);
/**
 * Select a choice by its index.
 *
 * @returns a non-zero value on error.
 */
INK_API int ink_story_choose(struct ink_story *story, size_t index);

/**
 * Iterator for choices.
 */
INK_API int ink_story_choice_next(ink_story *story, struct ink_choice *choice);

INK_API struct ink_object *ink_story_get_paths(struct ink_story *story);

#ifdef __cplusplus
}
#endif

/** @} */
#endif
