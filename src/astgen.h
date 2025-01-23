#ifndef __INK_ASTGEN_H__
#define __INK_ASTGEN_H__

#ifdef __cplusplus
extern "C" {
#endif

struct ink_story;
struct ink_syntax_tree;

extern int ink_generate_from_ast(const struct ink_syntax_tree *tree,
                                 struct ink_story *story, int flags);

#ifdef __cplusplus
}
#endif

#endif
