#ifndef __INK_ASTGEN_H__
#define __INK_ASTGEN_H__

#ifdef __cplusplus
extern "C" {
#endif

struct ink_ast;
struct ink_ir;

extern int ink_astgen(struct ink_ast *tree, struct ink_ir *ircode, int flags);

#ifdef __cplusplus
}
#endif

#endif
