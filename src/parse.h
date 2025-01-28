#ifndef __INK_PARSER_H__
#define __INK_PARSER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#define INK_PARSE_DEPTH 128

struct ink_arena;
struct ink_ast;

extern int ink_parse(struct ink_ast *tree, struct ink_arena *arena, int flags);

#ifdef __cplusplus
}
#endif

#endif
