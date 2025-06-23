#ifndef INK_SOURCE_H
#define INK_SOURCE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include <ink/ink.h>

struct ink_source {
    uint8_t *bytes;
    size_t length;
};

INK_API int ink_source_load(const char *filename, struct ink_source *source);
INK_API int ink_source_load_stdin(struct ink_source *source);
INK_API void ink_source_free(struct ink_source *source);

#ifdef __cplusplus
}
#endif

#endif
