#ifndef __INK_PARSER_H__
#define __INK_PARSER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#define INK_PARSE_DEPTH 128

struct ink_arena;
struct ink_ast;

extern int ink_parse(const uint8_t *source_bytes, const uint8_t *filename,
                     struct ink_arena *arena, struct ink_ast *tree, int flags);

#ifdef __cplusplus
}
#endif

#endif
