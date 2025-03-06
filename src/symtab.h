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
    INK_SYMBOL_VAR_LOCAL,
    INK_SYMBOL_VAR_GLOBAL,
    INK_SYMBOL_PARAM,
    INK_SYMBOL_KNOT,
    INK_SYMBOL_FUNC,
};

struct ink_symbol {
    enum ink_symbol_type type;
    const struct ink_ast_node *node;
    union {
        struct {
            bool is_const;
            size_t const_slot;
            size_t stack_slot;
            size_t str_index;
        } var;

        struct {
            size_t arity;
            size_t const_slot;
            size_t str_index;
            struct ink_symtab *local_names;
        } knot;
    } as;
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
