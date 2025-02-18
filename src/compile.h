#ifndef __INK_COMPILE_H__
#define __INK_COMPILE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct ink_story;

extern int ink_compile(const uint8_t *source_bytes, const uint8_t *filename,
                       struct ink_story *story, int flags);

#ifdef __cplusplus
}
#endif

#endif
