#ifndef __INK_PARSER_H__
#define __INK_PARSER_H__

#ifdef __cplusplus
extern "C" {
#endif

struct ink_arena;
struct ink_source;
struct ink_syntax_tree;

extern int ink_parse(struct ink_arena *arena, struct ink_source *source,
                     struct ink_syntax_tree *tree);

#ifdef __cplusplus
}
#endif

#endif
