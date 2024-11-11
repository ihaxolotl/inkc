#ifndef __INK_SOURCE_H__
#define __INK_SOURCE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

struct ink_source {
    char *filename;
    unsigned char *bytes;
    size_t length;
};

extern int ink_source_load(const char *filename, struct ink_source *source);
extern int ink_source_load_stdin(struct ink_source *source);
extern void ink_source_free(struct ink_source *source);

#ifdef __cplusplus
}
#endif

#endif
