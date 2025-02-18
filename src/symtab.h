#ifndef __INK_SYMTAB_H__
#define __INK_SYMTAB_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hashmap.h"

struct ink_symtab_node;

struct ink_symtab_pool {
    struct ink_symtab_node *head;
};

enum ink_symbol_type {
    INK_SYMBOL_VAR,
    INK_SYMBOL_PARAM,
    INK_SYMBOL_KNOT,
    INK_SYMBOL_FUNC,
};

struct ink_symbol {
    enum ink_symbol_type type;
    bool is_const;
    size_t ir_inst_index;
    size_t ir_str_index;
    size_t arity;
    struct ink_symtab *local_names;
    const struct ink_ast_node *node;
};

struct ink_string_ref {
    const uint8_t *bytes;
    size_t length;
};

INK_HASHMAP_T(ink_symtab, struct ink_string_ref, struct ink_symbol)

extern void ink_symtab_pool_init(struct ink_symtab_pool *symtab_pool);
extern void ink_symtab_pool_deinit(struct ink_symtab_pool *symtab_pool);
extern struct ink_symtab *ink_symtab_make(struct ink_symtab_pool *symtab_pool);

#ifdef __cplusplus
}
#endif

#endif
