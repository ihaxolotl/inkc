#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "memory.h"
#include "stream.h"

void ink_stream_init(struct ink_stream *st)
{
    st->cursor = 0;
    st->length = 0;
    st->bytes = NULL;
}

void ink_stream_deinit(struct ink_stream *st)
{
    ink_free(st->bytes);
    st->cursor = 0;
    st->length = 0;
    st->bytes = NULL;
}

int ink_stream_writef(struct ink_stream *st, const char *fmt, ...)
{
    int n = 0;
    size_t bsz = 0;
    va_list ap;

    va_start(ap, fmt);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (n < 0) {
        return -INK_E_PANIC;
    }

    bsz = st->length + (size_t)n + 1;
    st->bytes = ink_realloc(st->bytes, bsz);
    if (!st->bytes) {
        return -INK_E_OOM;
    }

    va_start(ap, fmt);
    n = vsnprintf((char *)st->bytes + st->length, bsz - st->length, fmt, ap);
    va_end(ap);

    if (n < 0) {
        return -INK_E_PANIC;
    }

    st->length = bsz - 1;
    return INK_E_OK;
}

int ink_stream_write(struct ink_stream *st, const uint8_t *bytes, size_t length)
{
    const size_t bsz = st->length + (size_t)length + 1;

    st->bytes = ink_realloc(st->bytes, bsz);
    if (!st->bytes) {
        return -INK_E_OOM;
    }

    memcpy(st->bytes + st->length, bytes, bsz - st->length);
    st->bytes[bsz - 1] = '\0';
    st->length = bsz - 1;
    return INK_E_OK;
}

void ink_stream_trim(struct ink_stream *st)
{
    uint8_t *c;

    if (st->length == 0 || st->cursor >= st->length) {
        return;
    }

    c = st->bytes + st->length - 1;

    while (c >= st->bytes + st->cursor && *c == '\n') {
        *c = '\0';
        c--;
        st->length--;
    }
}

bool ink_stream_is_empty(struct ink_stream *st)
{
    if (!st->bytes || st->length == 0) {
        return true;
    }
    return (st->bytes + st->cursor) == (st->bytes + st->length);
}

int ink_stream_read_line(struct ink_stream *st, uint8_t **line, size_t *linelen)
{
    uint8_t *p_start, *p_end;
    size_t len = 0;

    if (ink_stream_is_empty(st)) {
        return -1;
    }

    p_start = st->bytes + st->cursor;
    p_end = p_start;

    while (!(*p_end == '\0' || *p_end == '\n')) {
        p_end++;
    }
    if (*p_end == '\n') {
        p_end++;
    }

    len = (size_t)(p_end - p_start);
    if (line) {
        *line = p_start;
    }
    if (linelen) {
        *linelen = len;
    }

    st->cursor += len;
    if (st->cursor > st->length) {
        st->cursor = st->length;
    }
    return 0;
}
