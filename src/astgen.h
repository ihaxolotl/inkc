#ifndef INK_ASTGEN_H
#define INK_ASTGEN_H

#ifdef __cplusplus
extern "C" {
#endif

struct ink_ast;
struct ink_story;

extern int ink_astgen(struct ink_ast *tree, struct ink_story *story, int flags);

#ifdef __cplusplus
}
#endif

#endif
