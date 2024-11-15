#ifndef __INK_PARSER_H__
#define __INK_PARSER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

#define INK_PARSE_DEPTH 128

struct ink_arena;
struct ink_source;
struct ink_syntax_tree;

enum ink_parser_flags {
    INK_PARSER_F_TRACING = (1 << 0),
    INK_PARSER_F_CACHING = (1 << 1),
};

extern int ink_parse(struct ink_arena *arena, const struct ink_source *source,
                     struct ink_syntax_tree *tree, int flags);

#ifdef __cplusplus
}
#endif

#endif
