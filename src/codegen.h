#ifndef __INK_CODEGEN_H__
#define __INK_CODEGEN_H__

#ifdef __cplusplus
extern "C" {
#endif

struct ink_ir;
struct ink_story;
struct ink_symtab_pool;

extern int ink_codegen(const struct ink_ir *ircode, struct ink_story *story,
                       int flags);

#ifdef __cplusplus
}
#endif

#endif
