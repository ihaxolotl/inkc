#ifndef __INK_LOGGING_H__
#define __INK_LOGGING_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

enum ink_log_level {
    INK_LOG_LEVEL_TRACE,
    INK_LOG_LEVEL_DEBUG,
    INK_LOG_LEVEL_ERROR,
};

extern void ink_log(enum ink_log_level log_level, const char *fmt,
                    va_list args);
extern void ink_trace(const char *format, ...);
extern void ink_error(const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
