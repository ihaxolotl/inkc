#ifndef INK_STREAM_H
#define INK_STREAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <ink/ink.h>

/*
 * NOTE: Exporting functions from this module is probably a temporary thing.
 * Consumers likely do not need access to this stuff, though the tests are
 * breaking now that symbols are hidden by default.
 */

struct ink_stream {
    size_t cursor;
    size_t length;
    uint8_t *bytes;
};

/**
 * Initialize stream.
 */
INK_API void ink_stream_init(struct ink_stream *st);

/**
 * De-initialize and release stream.
 */
INK_API void ink_stream_deinit(struct ink_stream *st);

/**
 * Determine if stream is empty.
 */
INK_API bool ink_stream_is_empty(struct ink_stream *st);

/**
 * Write a formatted string to stream.
 */
INK_API int ink_stream_writef(struct ink_stream *st, const char *fmt, ...);

/**
 * Write a range of bytes to stream.
 */
INK_API int ink_stream_write(struct ink_stream *st, const uint8_t *bytes,
                             size_t length);

/**
 * Trim trailing new line characters from stream.
 */
INK_API void ink_stream_trim(struct ink_stream *st);

/**
 * Read line from stream.
 *
 * Returns the starting address in `line` and the length in `linelen`.
 */
INK_API int ink_stream_read_line(struct ink_stream *st, uint8_t **line,
                                 size_t *linelen);

#ifdef __cplusplus
}
#endif

#endif
