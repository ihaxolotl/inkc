#include <stddef.h>
#include <stdint.h>

#include "common.h"
#include "symtab.h"

#define INK_SYMTAB_LOAD_MAX 80u

struct ink_symtab_node {
    struct ink_symtab_node *next;
    struct ink_symtab table;
};

/**
 * Hash function for symbol table maps.
 */
static uint32_t ink_symtab_hash(const void *bytes, size_t length)
{
    const struct ink_string_ref *const key = bytes;

    return ink_fnv32a(key->bytes, key->length);
}

/**
 * Key comparison function for symbol table maps.
 */
static bool ink_symtab_cmp(const void *lhs, const void *rhs)
{
    const struct ink_string_ref *const key_lhs = lhs;
    const struct ink_string_ref *const key_rhs = rhs;

    return key_lhs->length == key_rhs->length &&
           memcmp(key_lhs->bytes, key_rhs->bytes, key_lhs->length) == 0;
}

struct ink_symtab *ink_symtab_make(struct ink_symtab_pool *st_pool)
{
    struct ink_symtab_node *const node = ink_malloc(sizeof(*node));

    if (!node) {
        return NULL;
    }

    ink_symtab_init(&node->table, INK_SYMTAB_LOAD_MAX, ink_symtab_hash,
                    ink_symtab_cmp);

    node->next = st_pool->head;
    st_pool->head = node;
    return &node->table;
}

void ink_symtab_pool_init(struct ink_symtab_pool *st_pool)
{
    st_pool->head = NULL;
}

void ink_symtab_pool_deinit(struct ink_symtab_pool *st_pool)
{
    while (st_pool->head) {
        struct ink_symtab_node *tmp = st_pool->head;

        st_pool->head = tmp->next;
        ink_symtab_deinit(&tmp->table);
        ink_free(tmp);
    }
}
