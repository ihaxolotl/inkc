#ifndef INK_COMPILE_H
#define INK_COMPILE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct ink_story;
struct ink_load_opts;

extern int ink_compile(struct ink_story *story,
                       const struct ink_load_opts *opts);

#ifdef __cplusplus
}
#endif

#endif
