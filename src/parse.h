#ifndef INK_PARSER_H
#define INK_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

struct ink_arena;
struct ink_ast;

extern int ink_parse(const uint8_t *source_bytes, size_t source_length,
                     const uint8_t *filename, struct ink_arena *arena,
                     struct ink_ast *tree, int flags);

#ifdef __cplusplus
}
#endif

#endif
