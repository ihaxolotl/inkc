#ifndef INK_LOGGING_H
#define INK_LOGGING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

#include <ink/ink.h>

enum ink_log_level {
    INK_LOG_LEVEL_TRACE,
    INK_LOG_LEVEL_DEBUG,
    INK_LOG_LEVEL_ERROR,
};

INK_API void ink_log(enum ink_log_level log_level, const char *fmt,
                     va_list args);
INK_API void ink_trace(const char *format, ...);
INK_API void ink_error(const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
