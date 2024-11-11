#ifndef __INK_UTIL_H__
#define __INK_UTIL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

extern bool ink_is_alpha(unsigned char c);
extern bool ink_is_digit(unsigned char c);
extern bool ink_is_identifier(unsigned char c);

#ifdef __cplusplus
}
#endif

#endif
