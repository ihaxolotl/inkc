#include <stdarg.h>
#include <stdio.h>

#include "logging.h"

static const char *INK_LOG_LEVEL_STR[] = {
    [INK_LOG_LEVEL_TRACE] = "TRACE",
    [INK_LOG_LEVEL_DEBUG] = "DEBUG",
    [INK_LOG_LEVEL_ERROR] = "ERROR",
};

/**
 * Format and output a string to the console.
 *
 * Automatically appends a newline.
 */
void ink_log(enum ink_log_level log_level, const char *fmt, va_list args)
{
    const char *level_str = INK_LOG_LEVEL_STR[log_level];

    fprintf(stderr, "[%s] ", level_str);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
}

void ink_error(const char *fmt, ...)
{
    va_list vargs;

    va_start(vargs, fmt);
    ink_log(INK_LOG_LEVEL_ERROR, fmt, vargs);
    va_end(vargs);
}

void ink_trace(const char *fmt, ...)
{
    va_list vargs;

    va_start(vargs, fmt);
    ink_log(INK_LOG_LEVEL_TRACE, fmt, vargs);
    va_end(vargs);
}
