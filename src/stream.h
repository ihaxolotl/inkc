#ifndef INK_STREAM_H
#define INK_STREAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct ink_stream {
    size_t cursor;
    size_t length;
    uint8_t *bytes;
};

/**
 * Initialize stream.
 */
extern void ink_stream_init(struct ink_stream *st);

/**
 * De-initialize and release stream.
 */
extern void ink_stream_deinit(struct ink_stream *st);

/**
 * Determine if stream is empty.
 */
extern bool ink_stream_is_empty(struct ink_stream *st);

/**
 * Write a formatted string to stream.
 */
extern int ink_stream_writef(struct ink_stream *st, const char *fmt, ...);

/**
 * Write a range of bytes to stream.
 */
extern int ink_stream_write(struct ink_stream *st, const uint8_t *bytes,
                            size_t length);

/**
 * Trim trailing new line characters from stream.
 */
extern void ink_stream_trim(struct ink_stream *st);

/**
 * Read line from stream.
 *
 * Returns the starting address in `line` and the length in `linelen`.
 */
extern int ink_stream_read_line(struct ink_stream *st, uint8_t **line,
                                size_t *linelen);

#ifdef __cplusplus
}
#endif

#endif
