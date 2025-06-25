#include <assert.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ink/ink.h>

#include "ast.h"
#include "astgen.h"
#include "common.h"
#include "object.h"
#include "opcode.h"
#include "symtab.h"
#include "vec.h"

/**
 * TODO: Prevent knots / stitches from explicitly returning. That should be
 * reserved for functions.
 *
 * TODO: Stitches could be allowed to be functions. Might help with code
 * organization.
 */

#define INK_STRINGSET_LOAD_MAX (80u)

#define INK_ASTGEN_TODO(msg)                                                   \
    do {                                                                       \
        fprintf(stderr, "TODO(astgen): %s\n", (msg));                          \
    } while (0)

#define INK_ASTGEN_BUG(node)                                                   \
    do {                                                                       \
        fprintf(stderr, "BUG(astgen): %s in %s\n",                             \
                ink_ast_node_type_strz((node)->type), __func__);               \
    } while (0)

/**
 * Key comparison operation for string set entries.
 */
static bool ink_stringset_cmp(const void *lhs, const void *rhs)
{
    const struct ink_string_ref *const key_lhs = lhs;
    const struct ink_string_ref *const key_rhs = rhs;

    return key_lhs->length == key_rhs->length &&
           memcmp(key_lhs->bytes, key_rhs->bytes, key_lhs->length) == 0;
}

/**
 * Hasher for string set entries.
 */
static uint32_t ink_stringset_hasher(const void *bytes, size_t length)
{
    const struct ink_string_ref *const key = bytes;

    return ink_fnv32a(key->bytes, key->length);
}

struct ink_astgen_jump {
    size_t label;
    size_t code_offset;
};

struct ink_astgen_label {
    size_t code_offset;
};

INK_VEC_T(ink_astgen_jump_vec, struct ink_astgen_jump)
INK_VEC_T(ink_astgen_label_vec, struct ink_astgen_label)
INK_HASHMAP_T(ink_stringset, struct ink_string_ref, size_t)

/**
 * Global state for all Astgen contexts.
 */
struct ink_astgen_global {
    struct ink_ast *tree;
    struct ink_story *story;
    struct ink_content_path *current_path;
    struct ink_symtab_pool symtab_pool;
    struct ink_stringset string_table;
    struct ink_byte_vec string_bytes;
    struct ink_astgen_label_vec labels;
    struct ink_astgen_jump_vec branches;
    jmp_buf jmpbuf;
};

static void ink_astgen_global_init(struct ink_astgen_global *g,
                                   struct ink_ast *tree,
                                   struct ink_story *story)
{
    g->tree = tree;
    g->story = story;
    g->current_path = NULL;

    ink_symtab_pool_init(&g->symtab_pool);
    ink_stringset_init(&g->string_table, INK_STRINGSET_LOAD_MAX,
                       ink_stringset_hasher, ink_stringset_cmp);
    ink_byte_vec_init(&g->string_bytes);
    ink_astgen_label_vec_init(&g->labels);
    ink_astgen_jump_vec_init(&g->branches);
}

static void ink_astgen_global_deinit(struct ink_astgen_global *g)
{
    ink_symtab_pool_deinit(&g->symtab_pool);
    ink_stringset_deinit(&g->string_table);
    ink_byte_vec_deinit(&g->string_bytes);
    ink_astgen_label_vec_deinit(&g->labels);
    ink_astgen_jump_vec_deinit(&g->branches);
}

struct ink_astgen {
    struct ink_astgen *parent;
    struct ink_astgen_global *global;
    struct ink_symtab *symbol_table;
    size_t jumps_top;
    size_t labels_top;
    size_t exit_label;
    size_t namespace_index;
};

static int ink_astgen_make(struct ink_astgen *scope,
                           struct ink_astgen *parent_scope,
                           struct ink_symtab *symbol_table)
{
    struct ink_astgen_global *const g = parent_scope->global;
    struct ink_symtab_pool *const st_pool = &g->symtab_pool;

    scope->global = g;

    if (!symbol_table) {
        symbol_table = ink_symtab_make(st_pool);
        if (!symbol_table) {
            return -INK_E_FAIL;
        }
    }

    scope->parent = parent_scope;
    scope->global = g;
    scope->symbol_table = symbol_table;
    scope->jumps_top = g->branches.count;
    scope->labels_top = g->labels.count;
    scope->exit_label = parent_scope->exit_label;
    scope->namespace_index = parent_scope->namespace_index;
    return INK_E_OK;
}

static void ink_astgen_deinit(struct ink_astgen *scope)
{
    scope->parent = NULL;
    scope->global = NULL;
    scope->symbol_table = NULL;
}

static void ink_astgen_fail(struct ink_astgen *astgen, const char *msg)
{
    fprintf(stderr, "%s", msg);
    exit(EXIT_FAILURE);
}

static void ink_astgen_error(struct ink_astgen *astgen,
                             enum ink_ast_error_type type,
                             const struct ink_ast_node *node)
{
    struct ink_astgen_global *const g = astgen->global;
    const struct ink_ast_error err = {
        .type = type,
        .source_start = node->bytes_start,
        .source_end = node->bytes_end,
    };

    ink_ast_error_vec_push(&g->tree->errors, err);
}

static void ink_astgen_global_panic(struct ink_astgen_global *g)
{
    longjmp(g->jmpbuf, 1);
}

/**
 * Retrieve a string reference from an AST node.
 *
 * The string reference is backed with memory from the source bytes.
 */
static inline struct ink_string_ref
ink_string_from_node(const struct ink_astgen *astgen,
                     const struct ink_ast_node *node)
{
    const struct ink_ast *const tree = astgen->global->tree;
    const struct ink_string_ref ref = {
        .bytes = &tree->source_bytes[node->bytes_start],
        .length = node->bytes_end - node->bytes_start,
    };

    return ref;
}

/**
 * Retrieve a string reference by index.
 *
 * The string reference is backed with memory from the string table.
 */
static inline struct ink_string_ref
ink_string_from_index(const struct ink_astgen *astgen, const size_t index)
{
    struct ink_astgen_global *const g = astgen->global;
    struct ink_byte_vec *const bv = &g->string_bytes;
    const uint8_t *const bytes = &bv->entries[index];
    const struct ink_string_ref ref = {
        .bytes = bytes,
        .length = strlen((char *)bytes),
    };

    return ref;
}

/**
 * Insert a name relative to the current scope.
 *
 * Will fail if the name already exists or an internal error occurs.
 */
static int ink_astgen_insert_name(struct ink_astgen *scope,
                                  const struct ink_ast_node *node,
                                  const struct ink_symbol *sym)
{
    const struct ink_string_ref key = ink_string_from_node(scope, node);

    return ink_symtab_insert(scope->symbol_table, key, *sym);
}

/**
 * Perform a lookup for an identifier node.
 *
 * Lookup is performed relative to the current scope. The scope chain is
 * traversed recursively until a match is found or if the lookup fails.
 */
static int ink_astgen_lookup_name(struct ink_astgen *scope,
                                  const struct ink_ast_node *node,
                                  struct ink_symbol *sym)
{
    int rc = INK_E_FAIL;
    const struct ink_string_ref key = ink_string_from_node(scope, node);

    while (scope) {
        rc = ink_symtab_lookup(scope->symbol_table, key, sym);
        if (rc < 0) {
            scope = scope->parent;
        } else {
            return rc;
        }
    }
    return rc;
}

static int ink_astgen_update_name(struct ink_astgen *scope,
                                  const struct ink_ast_node *node,
                                  const struct ink_symbol *sym)
{
    int rc = ink_astgen_insert_name(scope, node, sym);

    if (rc == -INK_E_OVERWRITE) {
        return 0;
    }
    return rc;
}

/**
 * Recursive function to perform a qualified lookup on two nodes.
 */
static int ink_astgen_lookup_expr_r(struct ink_astgen *scope,
                                    const struct ink_ast_node *lhs,
                                    const struct ink_ast_node *rhs,
                                    struct ink_symbol *sym)

{
    int rc = INK_E_FAIL;

    if (!lhs) {
        return rc;
    }
    if (lhs->type == INK_AST_SELECTOR_EXPR) {
        rc = ink_astgen_lookup_expr_r(scope, lhs->data.bin.lhs,
                                      lhs->data.bin.rhs, sym);
        if (rc < 0) {
            return rc;
        }

        rc = ink_astgen_lookup_expr_r(scope, lhs->data.bin.rhs, rhs, sym);
    } else if (lhs->type == INK_AST_IDENTIFIER) {
        const struct ink_string_ref lhs_key = ink_string_from_node(scope, lhs);

        rc = ink_symtab_lookup(scope->symbol_table, lhs_key, sym);
        if (rc < 0) {
            return rc;
        }
        if (sym->as.knot.local_names) {
            const struct ink_string_ref rhs_key =
                ink_string_from_node(scope, rhs);

            return ink_symtab_lookup(sym->as.knot.local_names, rhs_key, sym);
        }
        rc = INK_E_FAIL;
    }
    return rc;
}

/**
 * Perform a qualified lookup for a selector expression node.
 *
 * Lookup is performed relative to the current scope. The scope chain is
 * traversed recursively until a match is found or if the lookup fails.
 */
static int ink_astgen_lookup_qualified(struct ink_astgen *scope,
                                       const struct ink_ast_node *node,
                                       struct ink_symbol *sym)
{
    int rc = INK_E_FAIL;
    struct ink_ast_node *lhs = NULL;
    struct ink_ast_node *rhs = NULL;

    if (!node) {
        return rc;
    }
    while (scope) {
        lhs = node->data.bin.lhs;
        rhs = node->data.bin.rhs;
        rc = ink_astgen_lookup_expr_r(scope, lhs, rhs, sym);
        if (rc < 0) {
            scope = scope->parent;
        } else {
            return rc;
        }
    }
    return rc;
}

/**
 * Emit an instruction byte to the current chunk.
 */
static void ink_astgen_emit_byte(struct ink_astgen *astgen, uint8_t byte)
{
    struct ink_astgen_global *const g = astgen->global;
    struct ink_byte_vec *const code = &g->current_path->code;

    ink_byte_vec_push(code, (uint8_t)byte);
}

/**
 * Emit a two-byte constant instruction to the current chunk.
 */
static void ink_astgen_emit_const(struct ink_astgen *astgen,
                                  enum ink_vm_opcode op, size_t arg)
{
    if (arg < UINT8_MAX) {
        ink_astgen_emit_byte(astgen, (uint8_t)op);
        ink_astgen_emit_byte(astgen, (uint8_t)arg);
    } else {
        ink_astgen_global_panic(astgen->global);
    }
}

/**
 * Emit a three-byte jump instruction to the current chunk.
 *
 * The address bytes will be contain a sentinel value until backpatching is
 * performed.
 *
 * Returns the instruction offset to the instruction argument.
 */
static size_t ink_astgen_emit_jump(struct ink_astgen *astgen,
                                   enum ink_vm_opcode op)
{
    struct ink_astgen_global *const g = astgen->global;
    struct ink_byte_vec *const code = &g->current_path->code;

    ink_byte_vec_push(code, (uint8_t)op);
    ink_byte_vec_push(code, 0xff);
    ink_byte_vec_push(code, 0xff);
    return code->count - 2;
}

static size_t ink_astgen_add_jump(struct ink_astgen *astgen, size_t label_index,
                                  size_t code_offset)
{
    struct ink_astgen_global *const g = astgen->global;
    const size_t index = g->branches.count;
    const struct ink_astgen_jump jump = {
        .label = label_index,
        .code_offset = code_offset,
    };

    ink_astgen_jump_vec_push(&g->branches, jump);
    return index;
}

static size_t ink_astgen_add_label(struct ink_astgen *astgen)
{
    struct ink_astgen_global *const g = astgen->global;
    const size_t index = g->labels.count;
    const struct ink_astgen_label label = {
        .code_offset = 0xffffffff,
    };

    ink_astgen_label_vec_push(&g->labels, label);
    return index;
}

static void ink_astgen_set_label(struct ink_astgen *astgen, size_t label_index)
{
    struct ink_astgen_global *const g = astgen->global;
    struct ink_astgen_label_vec *const labels = &g->labels;
    struct ink_byte_vec *const code = &g->current_path->code;

    labels->entries[label_index].code_offset = code->count;
}

/**
 * Add a constant value to the current chunk.
 */
static size_t ink_astgen_add_const(struct ink_astgen *astgen,
                                   struct ink_object *obj)
{
    struct ink_astgen_global *const g = astgen->global;
    struct ink_object_vec *const_pool = &g->current_path->const_pool;
    const size_t const_index = const_pool->count;

    ink_object_vec_push(const_pool, obj);
    return const_index;
}

/**
 * Intern a string from the source file.
 */
static size_t ink_astgen_add_str(struct ink_astgen *astgen,
                                 const uint8_t *chars, size_t length)
{
    int rc;
    struct ink_astgen_global *const g = astgen->global;
    struct ink_stringset *const s_tab = &g->string_table;
    struct ink_byte_vec *const bv = &g->string_bytes;
    const struct ink_string_ref key = {
        .bytes = chars,
        .length = length,
    };
    size_t str_index = bv->count;

    rc = ink_stringset_lookup(s_tab, key, &str_index);
    if (rc < 0) {
        for (size_t i = 0; i < length; i++) {
            ink_byte_vec_push(bv, chars[i]);
        }

        ink_byte_vec_push(bv, '\0');
        ink_stringset_insert(s_tab, key, str_index);
    }
    return str_index;
}

/* TODO(Brett): Refactor this garbage. */
static size_t ink_astgen_add_qualified_str_r(struct ink_astgen *astgen,
                                             struct ink_string_ref *str,
                                             size_t pos)
{
    struct ink_byte_vec *const bv = &astgen->global->string_bytes;

    if (astgen->parent) {
        const size_t str_index = astgen->namespace_index;
        const size_t length = strlen((char *)&bv->entries[str_index]);

        for (size_t i = 0; i < length; i++) {
            ink_byte_vec_push(bv, bv->entries[str_index + i]);
        }

        ink_byte_vec_push(bv, '.');
        return ink_astgen_add_qualified_str_r(astgen->parent, str, pos);
    }
    for (size_t i = 0; i < str->length; i++) {
        ink_byte_vec_push(bv, str->bytes[i]);
    }

    ink_byte_vec_push(bv, '\0');
    return pos;
}

static size_t ink_astgen_add_qualified_str(struct ink_astgen *astgen,
                                           struct ink_string_ref *str)
{
    struct ink_byte_vec *const sv = &astgen->global->string_bytes;
    const size_t pos = sv->count;

    ink_astgen_add_qualified_str_r(astgen, str, pos);
    return pos;
}

static void ink_astgen_add_knot(struct ink_astgen *astgen, size_t str_index)
{
    struct ink_astgen_global *const g = astgen->global;
    struct ink_byte_vec *const bv = &g->string_bytes;
    uint8_t *const str = &bv->entries[str_index];
    struct ink_object *const paths_table = ink_story_get_paths(g->story);
    struct ink_object *const path_name =
        ink_string_new(g->story, str, strlen((char *)str));

    if (!path_name) {
        return;
    }

    struct ink_object *const path_obj =
        ink_content_path_new(g->story, path_name);
    if (!path_obj) {
        return;
    }

    ink_table_insert(g->story, paths_table, path_name, path_obj);
    astgen->global->current_path = INK_OBJ_AS_CONTENT_PATH(path_obj);
}

static void ink_astgen_patch_jump(struct ink_astgen *astgen, size_t offset)
{
    struct ink_astgen_global *const g = astgen->global;
    struct ink_byte_vec *const code = &g->current_path->code;
    const size_t jump = code->count - offset - 2;

    if (jump > UINT16_MAX) {
        ink_astgen_fail(astgen, "Too much code to jump over.");
        return;
    }

    code->entries[offset] = (jump >> 8) & 0xff;
    code->entries[offset + 1] = jump & 0xff;
}

static void ink_astgen_backpatch(struct ink_astgen *scope, size_t start_offset,
                                 size_t end_offset)
{
    struct ink_astgen_global *const g = scope->global;
    struct ink_astgen_jump_vec *const jump_vec = &g->branches;
    struct ink_astgen_label_vec *const label_vec = &g->labels;
    struct ink_byte_vec *const code = &g->current_path->code;

    assert(start_offset <= end_offset);

    for (size_t i = start_offset; i < end_offset; i++) {
        struct ink_astgen_jump *const jump = &jump_vec->entries[i];
        struct ink_astgen_label *const label = &label_vec->entries[jump->label];
        const size_t test = label->code_offset - jump->code_offset - 2;

        if (test > UINT16_MAX) {
            ink_astgen_fail(scope, "Too much code to jump over.");
            return;
        }

        code->entries[jump->code_offset] = (test >> 8) & 0xff;
        code->entries[jump->code_offset + 1] = test & 0xff;
    }

    ink_astgen_jump_vec_shrink(jump_vec, 0);
    ink_astgen_label_vec_shrink(label_vec, 0);
}

static const uint8_t *ink_astgen_str_bytes(const struct ink_astgen *astgen,
                                           size_t str_index)
{
    struct ink_astgen_global *const g = astgen->global;

    return &g->string_bytes.entries[str_index];
}

static void ink_astgen_expr(struct ink_astgen *, const struct ink_ast_node *);
static void ink_astgen_stmt(struct ink_astgen *, const struct ink_ast_node *);
static void ink_astgen_content_expr(struct ink_astgen *,
                                    const struct ink_ast_node *);

static void ink_astgen_unary_op(struct ink_astgen *scope,
                                const struct ink_ast_node *n,
                                enum ink_vm_opcode op)
{
    ink_astgen_expr(scope, n->data.bin.lhs);
    ink_astgen_emit_byte(scope, (uint8_t)op);
}

static void ink_astgen_binary_op(struct ink_astgen *scope,
                                 const struct ink_ast_node *n,
                                 enum ink_vm_opcode op)
{
    ink_astgen_expr(scope, n->data.bin.lhs);
    ink_astgen_expr(scope, n->data.bin.rhs);
    ink_astgen_emit_byte(scope, (uint8_t)op);
}

static void ink_astgen_logical_op(struct ink_astgen *scope,
                                  const struct ink_ast_node *n, bool binary_or)
{
    struct ink_ast_node *const lhs = n->data.bin.lhs;
    struct ink_ast_node *const rhs = n->data.bin.rhs;

    ink_astgen_expr(scope, lhs);

    const size_t else_branch =
        ink_astgen_emit_jump(scope, binary_or ? INK_OP_JMP_T : INK_OP_JMP_F);

    ink_astgen_emit_byte(scope, INK_OP_POP);
    ink_astgen_expr(scope, rhs);
    ink_astgen_patch_jump(scope, else_branch);
}

static void ink_astgen_true(struct ink_astgen *scope)
{
    ink_astgen_emit_byte(scope, INK_OP_TRUE);
}

static void ink_astgen_false(struct ink_astgen *scope)
{
    ink_astgen_emit_byte(scope, INK_OP_FALSE);
}

static void ink_astgen_integer(struct ink_astgen *scope,
                               const struct ink_ast_node *expr)
{
    /* TODO: Error-handling. */
    const struct ink_string_ref str = ink_string_from_node(scope, expr);
    const size_t str_index = ink_astgen_add_str(scope, str.bytes, str.length);
    const uint8_t *const str_bytes = ink_astgen_str_bytes(scope, str_index);
    const ink_integer v = strtol((char *)str_bytes, NULL, 10);
    struct ink_object *const obj = ink_integer_new(scope->global->story, v);

    if (!obj) {
        ink_astgen_fail(scope, "Could not create runtime object for number.");
        return;
    }

    ink_astgen_emit_const(scope, INK_OP_CONST,
                          (uint8_t)ink_astgen_add_const(scope, obj));
}

static void ink_astgen_float(struct ink_astgen *scope,
                             const struct ink_ast_node *expr)
{
    /* TODO: Error-handling. */
    const struct ink_string_ref str = ink_string_from_node(scope, expr);
    const size_t str_index = ink_astgen_add_str(scope, str.bytes, str.length);
    const uint8_t *const str_bytes = ink_astgen_str_bytes(scope, str_index);
    const ink_float v = strtod((char *)str_bytes, NULL);
    struct ink_object *const obj = ink_float_new(scope->global->story, v);

    if (!obj) {
        ink_astgen_fail(scope, "Could not create runtime object for number.");
        return;
    }

    ink_astgen_emit_const(scope, INK_OP_CONST,
                          (uint8_t)ink_astgen_add_const(scope, obj));
}

static void ink_astgen_string(struct ink_astgen *scope,
                              const struct ink_ast_node *expr)
{
    const struct ink_string_ref str = ink_string_from_node(scope, expr);
    const size_t str_index = ink_astgen_add_str(scope, str.bytes, str.length);
    struct ink_object *const obj =
        ink_string_new(scope->global->story, str.bytes, str.length);

    (void)str_index;

    if (!obj) {
        ink_astgen_fail(scope, "Could not create runtime object for string.");
        return;
    }

    ink_astgen_emit_const(scope, INK_OP_CONST,
                          (uint8_t)ink_astgen_add_const(scope, obj));
}

static void ink_astgen_string_expr(struct ink_astgen *scope,
                                   const struct ink_ast_node *expr)
{
    struct ink_ast_node *const lhs = expr->data.bin.lhs;

    ink_astgen_string(scope, lhs);
}

static void ink_astgen_identifier(struct ink_astgen *scope,
                                  const struct ink_ast_node *expr)
{
    struct ink_symbol sym;

    if (ink_astgen_lookup_name(scope, expr, &sym) < 0) {
        ink_astgen_error(scope, INK_AST_E_UNKNOWN_IDENTIFIER, expr);
        return;
    }
    switch (sym.type) {
    case INK_SYMBOL_VAR_GLOBAL:
        ink_astgen_emit_const(scope, INK_OP_LOAD_GLOBAL,
                              (uint8_t)sym.as.var.const_slot);
        break;
    case INK_SYMBOL_VAR_LOCAL:
    case INK_SYMBOL_PARAM:
        ink_astgen_emit_const(scope, INK_OP_LOAD,
                              (uint8_t)sym.as.var.stack_slot);
        break;
    default:
        ink_astgen_error(scope, INK_AST_E_INVALID_EXPR, expr);
        break;
    }
}

static int ink_astgen_check_args_count(struct ink_astgen *scope,
                                       const struct ink_ast_node *node,
                                       struct ink_ast_node_list *args,
                                       struct ink_symbol *symbol)
{
    int rc = -1;
    const size_t knot_arity = symbol->as.knot.arity;

    if (!args) {
        if (knot_arity != 0) {
            ink_astgen_error(scope, INK_AST_E_TOO_FEW_ARGS, node);
            rc = -1;
        } else {
            rc = 0;
        }
    } else {
        if (args->count != knot_arity) {
            if (args->count > knot_arity) {
                ink_astgen_error(scope, INK_AST_E_TOO_MANY_ARGS, node);
            } else {
                ink_astgen_error(scope, INK_AST_E_TOO_FEW_ARGS, node);
            }
            rc = -1;
        } else {
            rc = 0;
        }
    }
    return rc;
}

/*
 * TODO(Brett): Check for divert params.
 * TODO(Brett): Check for ref params.
 */
static void ink_astgen_call_expr(struct ink_astgen *scope,
                                 const struct ink_ast_node *expr,
                                 enum ink_vm_opcode op)
{
    int rc = INK_E_FAIL;
    struct ink_symbol sym;
    struct ink_astgen_global *g = scope->global;
    struct ink_ast_node *const lhs = expr->data.bin.lhs;
    struct ink_ast_node *const rhs = expr->data.bin.rhs;

    if (lhs->type == INK_AST_SELECTOR_EXPR) {
        rc = ink_astgen_lookup_qualified(scope, lhs, &sym);
    } else if (lhs->type == INK_AST_IDENTIFIER) {
        rc = ink_astgen_lookup_name(scope, lhs, &sym);
    } else {
        assert(false);
    }
    if (rc < 0) {
        ink_astgen_error(scope, INK_AST_E_UNKNOWN_IDENTIFIER, lhs);
        return;
    }
    if (rhs) {
        struct ink_ast_node_list *const l = rhs->data.many.list;

        rc = ink_astgen_check_args_count(scope, lhs, l, &sym);
        if (rc < 0) {
            return;
        }
        if (l) {
            for (size_t i = 0; i < l->count; i++) {
                struct ink_ast_node *const arg = l->nodes[i];

                ink_astgen_expr(scope, arg);
            }
        }
    }

    const struct ink_string_ref str = ink_string_from_node(scope, lhs);
    struct ink_object *const obj =
        ink_string_new(g->story, str.bytes, str.length);

    ink_astgen_emit_const(scope, op, (uint8_t)ink_astgen_add_const(scope, obj));
}

static void ink_astgen_expr(struct ink_astgen *astgen,
                            const struct ink_ast_node *node)
{
    if (!node) {
        return;
    }
    switch (node->type) {
    case INK_AST_TRUE:
        ink_astgen_true(astgen);
        break;
    case INK_AST_FALSE:
        ink_astgen_false(astgen);
        break;
    case INK_AST_INTEGER:
        ink_astgen_integer(astgen, node);
        break;
    case INK_AST_FLOAT:
        ink_astgen_float(astgen, node);
        break;
    case INK_AST_IDENTIFIER:
        ink_astgen_identifier(astgen, node);
        break;
    case INK_AST_STRING_EXPR:
        ink_astgen_string_expr(astgen, node);
        break;
    case INK_AST_EMPTY_STRING:
    case INK_AST_STRING:
        ink_astgen_string(astgen, node);
        break;
    case INK_AST_ADD_EXPR:
        ink_astgen_binary_op(astgen, node, INK_OP_ADD);
        break;
    case INK_AST_SUB_EXPR:
        ink_astgen_binary_op(astgen, node, INK_OP_SUB);
        break;
    case INK_AST_MUL_EXPR:
        ink_astgen_binary_op(astgen, node, INK_OP_MUL);
        break;
    case INK_AST_DIV_EXPR:
        ink_astgen_binary_op(astgen, node, INK_OP_DIV);
        break;
    case INK_AST_MOD_EXPR:
        ink_astgen_binary_op(astgen, node, INK_OP_MOD);
        break;
    case INK_AST_EQUAL_EXPR:
        ink_astgen_binary_op(astgen, node, INK_OP_CMP_EQ);
        break;
    case INK_AST_NOT_EQUAL_EXPR:
        ink_astgen_binary_op(astgen, node, INK_OP_CMP_EQ);
        ink_astgen_emit_byte(astgen, INK_OP_NOT);
        break;
    case INK_AST_LESS_EXPR:
        ink_astgen_binary_op(astgen, node, INK_OP_CMP_LT);
        break;
    case INK_AST_LESS_EQUAL_EXPR:
        ink_astgen_binary_op(astgen, node, INK_OP_CMP_LTE);
        break;
    case INK_AST_GREATER_EXPR:
        ink_astgen_binary_op(astgen, node, INK_OP_CMP_GT);
        break;
    case INK_AST_GREATER_EQUAL_EXPR:
        ink_astgen_binary_op(astgen, node, INK_OP_CMP_GTE);
        break;
    case INK_AST_NEGATE_EXPR:
        ink_astgen_unary_op(astgen, node, INK_OP_NEG);
        break;
    case INK_AST_NOT_EXPR:
        ink_astgen_unary_op(astgen, node, INK_OP_NOT);
        break;
    case INK_AST_AND_EXPR:
        ink_astgen_logical_op(astgen, node, false);
        break;
    case INK_AST_OR_EXPR:
        ink_astgen_logical_op(astgen, node, false);
        break;
    case INK_AST_CALL_EXPR:
        ink_astgen_call_expr(astgen, node, INK_OP_CALL);
        break;
    case INK_AST_CONTAINS_EXPR:
        INK_ASTGEN_TODO("ContainsExpr");
        break;
    default:
        INK_ASTGEN_BUG(node);
        break;
    }
}

static void ink_astgen_inline_logic(struct ink_astgen *scope,
                                    const struct ink_ast_node *expr)
{
    struct ink_ast_node *const lhs = expr->data.bin.lhs;

    ink_astgen_expr(scope, lhs);
}

/* TODO(Brett): There may be a bug here regarding fallthrough branches. */
static void ink_astgen_if_expr(struct ink_astgen *scope,
                               const struct ink_ast_node *expr)
{
    struct ink_ast_node *const cond_expr = expr->data.bin.lhs;
    struct ink_ast_node *const body_stmt = expr->data.bin.rhs;

    ink_astgen_expr(scope, cond_expr);

    const size_t then_branch = ink_astgen_emit_jump(scope, INK_OP_JMP_F);

    ink_astgen_emit_byte(scope, INK_OP_POP);
    ink_astgen_content_expr(scope, body_stmt);
    ink_astgen_patch_jump(scope, then_branch);
    ink_astgen_emit_byte(scope, INK_OP_POP);
}

static void ink_astgen_block_stmt(struct ink_astgen *parent_scope,
                                  const struct ink_ast_node *stmt)
{
    struct ink_astgen scope;
    struct ink_ast_node_list *l = NULL;

    if (!stmt) {
        return;
    }

    assert(stmt->type == INK_AST_BLOCK);
    ink_astgen_make(&scope, parent_scope, NULL);
    l = stmt->data.many.list;

    for (size_t i = 0; i < l->count; i++) {
        ink_astgen_stmt(&scope, l->nodes[i]);
    }

    ink_astgen_add_jump(&scope, scope.exit_label,
                        ink_astgen_emit_jump(&scope, INK_OP_JMP));
}

static void ink_astgen_if_stmt(struct ink_astgen *scope,
                               const struct ink_ast_node *cond_expr,
                               const struct ink_ast_node *then_stmt,
                               const struct ink_ast_node *else_stmt)
{
    size_t then_br = 0;
    size_t else_br = 0;

    ink_astgen_expr(scope, cond_expr);

    then_br = ink_astgen_emit_jump(scope, INK_OP_JMP_F);
    ink_astgen_emit_byte(scope, INK_OP_POP);
    ink_astgen_block_stmt(scope, then_stmt);

    else_br = ink_astgen_emit_jump(scope, INK_OP_JMP);
    ink_astgen_patch_jump(scope, then_br);
    ink_astgen_emit_byte(scope, INK_OP_POP);

    if (else_stmt && else_stmt->type == INK_AST_ELSE_BRANCH) {
        ink_astgen_block_stmt(scope, else_stmt->data.bin.rhs);
    }

    ink_astgen_patch_jump(scope, else_br);
}

static void ink_astgen_multi_if_block(struct ink_astgen *scope,
                                      struct ink_ast_node_list *cases,
                                      size_t index)
{
    size_t then_br = 0;
    size_t else_br = 0;
    struct ink_ast_node *lhs = NULL;
    struct ink_ast_node *rhs = NULL;
    struct ink_ast_node *then_stmt = NULL;
    struct ink_ast_node *else_stmt = NULL;

    if (index >= cases->count) {
        return;
    }

    then_stmt = cases->nodes[index];
    else_stmt = index + 1 < cases->count ? cases->nodes[index + 1] : NULL;
    lhs = then_stmt->data.bin.lhs;
    rhs = then_stmt->data.bin.rhs;

    ink_astgen_expr(scope, lhs);
    then_br = ink_astgen_emit_jump(scope, INK_OP_JMP_F);
    ink_astgen_emit_byte(scope, INK_OP_POP);
    ink_astgen_block_stmt(scope, rhs);
    else_br = ink_astgen_emit_jump(scope, INK_OP_JMP);
    ink_astgen_patch_jump(scope, then_br);
    ink_astgen_emit_byte(scope, INK_OP_POP);

    if (else_stmt) {
        if (else_stmt->type == INK_AST_ELSE_BRANCH) {
            rhs = else_stmt->data.bin.rhs;
            ink_astgen_block_stmt(scope, rhs);
        } else {
            ink_astgen_multi_if_block(scope, cases, index + 1);
        }
    }

    ink_astgen_patch_jump(scope, else_br);
}

static void ink_astgen_switch_stmt(struct ink_astgen *scope,
                                   const struct ink_ast_node *stmt)
{
    struct ink_astgen_global *const g = scope->global;
    struct ink_content_path *const path = g->current_path;
    struct ink_ast_node *const cond_expr = stmt->data.switch_stmt.cond_expr;
    struct ink_ast_node_list *const cases = stmt->data.switch_stmt.cases;
    const size_t label_top = g->labels.count;
    const size_t stack_slot = path->arity + path->locals_count++;

    ink_astgen_expr(scope, cond_expr);
    ink_astgen_emit_const(scope, INK_OP_STORE, (uint8_t)stack_slot);
    ink_astgen_emit_byte(scope, INK_OP_POP);

    for (size_t i = 0; i < cases->count; i++) {
        struct ink_ast_node *const br = cases->nodes[i];
        struct ink_ast_node *lhs = NULL;

        if (br->type == INK_AST_SWITCH_CASE) {
            lhs = br->data.bin.lhs;

            switch (lhs->type) {
            case INK_AST_FLOAT:
            case INK_AST_INTEGER:
            case INK_AST_TRUE:
            case INK_AST_FALSE:
                break;
            default:
                ink_astgen_error(scope, INK_AST_E_SWITCH_EXPR, stmt);
                return;
            }
        }

        const size_t label_index = ink_astgen_add_label(scope);

        ink_astgen_emit_const(scope, INK_OP_LOAD, stack_slot);
        ink_astgen_expr(scope, lhs);
        ink_astgen_emit_byte(scope, INK_OP_CMP_EQ);
        ink_astgen_add_jump(scope, label_index,
                            ink_astgen_emit_jump(scope, INK_OP_JMP_T));
        ink_astgen_emit_byte(scope, INK_OP_POP);
    }
    for (size_t i = 0; i < cases->count; i++) {
        struct ink_ast_node *const br = cases->nodes[i];
        struct ink_ast_node *const rhs = br->data.bin.rhs;

        ink_astgen_set_label(scope, label_top + i);
        ink_astgen_emit_byte(scope, INK_OP_POP);
        ink_astgen_block_stmt(scope, rhs);
    }
}

static void ink_astgen_conditional(struct ink_astgen *scope,
                                   const struct ink_ast_node *stmt)

{
    bool has_block = false;
    bool has_else = false;
    struct ink_ast_node *const expr = stmt->data.switch_stmt.cond_expr;
    struct ink_ast_node_list *const l = stmt->data.switch_stmt.cases;

    if (!l || l->count == 0) {
        ink_astgen_error(scope, INK_AST_E_CONDITIONAL_EMPTY, stmt);
        return;
    }

    struct ink_ast_node *const first = l->nodes[0];
    struct ink_ast_node *const last = l->nodes[l->count - 1];

    for (size_t i = 0; i < l->count; i++) {
        struct ink_ast_node *const child = l->nodes[i];

        switch (child->type) {
        case INK_AST_BLOCK:
            has_block = true;
            break;
        case INK_AST_SWITCH_CASE:
        case INK_AST_IF_BRANCH:
            if (has_block) {
                ink_astgen_error(scope, INK_AST_E_ELSE_EXPECTED, child);
                return;
            }
            break;
        case INK_AST_ELSE_BRANCH:
            /* Only the last branch can be an else. */
            if (child != last) {
                ink_astgen_error(scope, INK_AST_E_ELSE_FINAL, child);
                return;
            }
            if (has_else) {
                ink_astgen_error(scope, INK_AST_E_ELSE_MULTIPLE, child);
                return;
            }

            has_else = true;
            break;
        default:
            INK_ASTGEN_BUG(child);
            break;
        }
    }
    switch (stmt->type) {
    case INK_AST_IF_STMT:
        if (!expr) {
            ink_astgen_error(scope, INK_AST_E_EXPECTED_EXPR, stmt);
            return;
        }
        ink_astgen_if_stmt(scope, expr, first, first == last ? NULL : last);
        break;
    case INK_AST_MULTI_IF_STMT:
        ink_astgen_multi_if_block(scope, l, 0);
        break;
    case INK_AST_SWITCH_STMT:
        ink_astgen_switch_stmt(scope, stmt);
        break;
    default:
        break;
    }
}

static void ink_astgen_content_expr(struct ink_astgen *astgen,
                                    const struct ink_ast_node *node)
{
    struct ink_ast_node_list *const l = node->data.many.list;
    struct ink_ast_node *expr = NULL;

    if (!l) {
        return;
    }
    for (size_t i = 0; i < l->count; i++) {
        expr = l->nodes[i];

        switch (expr->type) {
        case INK_AST_STRING:
            ink_astgen_string(astgen, expr);
            ink_astgen_emit_byte(astgen, INK_OP_CONTENT);
            break;
        case INK_AST_INLINE_LOGIC:
            ink_astgen_inline_logic(astgen, expr);
            ink_astgen_emit_byte(astgen, INK_OP_CONTENT);
            break;
        case INK_AST_IF_STMT:
        case INK_AST_MULTI_IF_STMT:
        case INK_AST_SWITCH_STMT:
            ink_astgen_conditional(astgen, expr);
            ink_astgen_emit_byte(astgen, INK_OP_CONTENT);
            break;
        case INK_AST_IF_EXPR:
            ink_astgen_if_expr(astgen, expr);
            ink_astgen_emit_byte(astgen, INK_OP_CONTENT);
            break;
        case INK_AST_GLUE:
            ink_astgen_emit_byte(astgen, INK_OP_GLUE);
            break;
        default:
            INK_ASTGEN_BUG(expr);
            break;
        }
    }
    if (!expr || expr->type != INK_AST_GLUE) {
        ink_astgen_emit_byte(astgen, INK_OP_LINE);
    }
}

static void ink_astgen_var_decl(struct ink_astgen *scope,
                                const struct ink_ast_node *decl)
{
    struct ink_astgen_global *const g = scope->global;
    struct ink_content_path *const path = g->current_path;
    struct ink_ast_node *const lhs = decl->data.bin.lhs;
    struct ink_ast_node *const rhs = decl->data.bin.rhs;
    const struct ink_string_ref str = ink_string_from_node(scope, lhs);
    const size_t str_index = ink_astgen_add_str(scope, str.bytes, str.length);

    ink_astgen_expr(scope, rhs);

    if (lhs->type == INK_AST_TEMP_DECL) {
        const size_t stack_slot = path->arity + path->locals_count++;
        const struct ink_symbol sym = {
            .type = INK_SYMBOL_VAR_LOCAL,
            .node = decl,
            .as.var.stack_slot = stack_slot,
            .as.var.str_index = str_index,
        };

        if (ink_astgen_insert_name(scope, lhs, &sym) < 0) {
            ink_astgen_error(scope, INK_AST_E_REDEFINED_IDENTIFIER, lhs);
            return;
        }

        ink_astgen_emit_const(scope, INK_OP_STORE, (uint8_t)stack_slot);
    } else {
        struct ink_object *const name_obj =
            ink_string_new(g->story, str.bytes, str.length);
        const size_t const_index = ink_astgen_add_const(scope, name_obj);
        const struct ink_symbol sym = {
            .type = INK_SYMBOL_VAR_GLOBAL,
            .node = decl,
            .as.var.is_const = decl->type == INK_AST_CONST_DECL,
            .as.var.const_slot = const_index,
            .as.var.str_index = str_index,
        };

        if (ink_astgen_insert_name(scope, lhs, &sym) < 0) {
            ink_astgen_error(scope, INK_AST_E_REDEFINED_IDENTIFIER, lhs);
            return;
        }

        ink_astgen_emit_const(scope, INK_OP_STORE_GLOBAL, (uint8_t)const_index);
    }

    ink_astgen_emit_byte(scope, INK_OP_POP);
}

static void ink_astgen_divert_expr(struct ink_astgen *scope,
                                   const struct ink_ast_node *expr)
{
    struct ink_symbol sym;
    struct ink_string_ref str;
    struct ink_object *obj = NULL;
    struct ink_ast_node *lhs = expr->data.bin.lhs;

    assert(lhs != NULL);

    switch (lhs->type) {
    case INK_AST_SELECTOR_EXPR:
        if (ink_astgen_lookup_qualified(scope, lhs, &sym) < 0) {
            ink_astgen_error(scope, INK_AST_E_UNKNOWN_IDENTIFIER, lhs);
            return;
        }
        break;
    case INK_AST_IDENTIFIER: {
        str = ink_string_from_node(scope, lhs);

        switch (str.length) {
        case 3:
            if (memcmp(str.bytes, "END", str.length) == 0) {
                ink_astgen_emit_byte(scope, INK_OP_EXIT);
                return;
            }
            break;
        case 4:
            if (memcmp(str.bytes, "DONE", str.length) == 0) {
                ink_astgen_emit_byte(scope, INK_OP_EXIT);
                return;
            }
            break;
        default:
            break;
        }
        if (ink_astgen_lookup_name(scope, lhs, &sym) < 0) {
            ink_astgen_error(scope, INK_AST_E_UNKNOWN_IDENTIFIER, lhs);
            return;
        }
        break;
    case INK_AST_CALL_EXPR:
        ink_astgen_call_expr(scope, lhs, INK_OP_DIVERT);
        return;
    }
    default:
        INK_ASTGEN_BUG(lhs);
        return;
    }

    str = ink_string_from_index(scope, sym.as.knot.str_index);
    obj = ink_string_new(scope->global->story, str.bytes, str.length);
    ink_astgen_emit_const(scope, INK_OP_DIVERT,
                          (uint8_t)ink_astgen_add_const(scope, obj));
}

static void ink_astgen_content_stmt(struct ink_astgen *scope,
                                    const struct ink_ast_node *stmt)
{
    struct ink_ast_node *const lhs = stmt->data.bin.lhs;

    ink_astgen_content_expr(scope, lhs);
}

static void ink_astgen_divert_stmt(struct ink_astgen *scope,
                                   const struct ink_ast_node *stmt)
{
    struct ink_ast_node *const lhs = stmt->data.bin.lhs;

    ink_astgen_divert_expr(scope, lhs);
}

static void ink_astgen_expr_stmt(struct ink_astgen *scope,
                                 const struct ink_ast_node *stmt)
{
    struct ink_ast_node *const lhs = stmt->data.bin.lhs;

    ink_astgen_expr(scope, lhs);
    ink_astgen_emit_byte(scope, INK_OP_POP);
}

static void ink_astgen_assign_stmt(struct ink_astgen *scope,
                                   const struct ink_ast_node *stmt)
{
    int rc = INK_E_FAIL;
    struct ink_symbol sym;
    struct ink_ast_node *const lhs = stmt->data.bin.lhs;
    struct ink_ast_node *const rhs = stmt->data.bin.rhs;

    rc = ink_astgen_lookup_name(scope, lhs, &sym);
    if (rc < 0) {
        ink_astgen_error(scope, INK_AST_E_UNKNOWN_IDENTIFIER, lhs);
        return;
    }
    if (sym.as.var.is_const) {
        ink_astgen_error(scope, INK_AST_E_CONST_ASSIGN, lhs);
        return;
    }

    ink_astgen_expr(scope, rhs);

    switch (sym.type) {
    case INK_SYMBOL_VAR_GLOBAL:
        ink_astgen_emit_const(scope, INK_OP_STORE_GLOBAL,
                              (uint8_t)sym.as.var.const_slot);
        ink_astgen_emit_byte(scope, INK_OP_POP);
        break;
    case INK_SYMBOL_VAR_LOCAL:
        ink_astgen_emit_const(scope, INK_OP_STORE,
                              (uint8_t)sym.as.var.stack_slot);
        ink_astgen_emit_byte(scope, INK_OP_POP);
        break;
    default:
        /* TODO: Give a more informative error message here. */
        ink_astgen_error(scope, INK_AST_E_INVALID_EXPR, lhs);
        break;
    }
}

static void ink_astgen_return_stmt(struct ink_astgen *scope,
                                   const struct ink_ast_node *stmt)
{
    struct ink_ast_node *const lhs = stmt->data.bin.lhs;

    ink_astgen_expr(scope, lhs);
    ink_astgen_emit_byte(scope, INK_OP_RET);
}

struct ink_astgen_choice {
    uint16_t constant;
    uint16_t label;
    struct ink_object *id;
};

static void ink_astgen_choice_stmt(struct ink_astgen *scope,
                                   const struct ink_ast_node *stmt)
{
    struct ink_astgen_choice *data = NULL;
    struct ink_ast_node_list *l = stmt->data.many.list;

    assert(l != NULL);

    /* FIXME: This leaks on panic. */
    data = ink_malloc(l->count * sizeof(*data));
    if (!data) {
        return;
    }

    ink_astgen_emit_byte(scope, INK_OP_FLUSH);

    for (size_t i = 0; i < l->count; i++) {
        struct ink_astgen_choice *choice = &data[i];
        struct ink_ast_node *br_stmt = l->nodes[i];

        assert(br_stmt->type == INK_AST_CHOICE_STAR_STMT ||
               br_stmt->type == INK_AST_CHOICE_PLUS_STMT);

        choice->id = ink_integer_new(scope->global->story, (ink_integer)i);
        if (!choice->id) {
            /* TODO: Probably panic and report an error here. */
            return;
        }

        struct ink_ast_node *br_expr = br_stmt->data.bin.lhs;
        struct ink_ast_node *lhs = br_expr->data.choice_expr.start_expr;
        struct ink_ast_node *rhs = br_expr->data.choice_expr.option_expr;

        if (lhs) {
            ink_astgen_string(scope, lhs);
            ink_astgen_emit_byte(scope, INK_OP_CONTENT);
        }
        if (rhs) {
            ink_astgen_string(scope, rhs);
            ink_astgen_emit_byte(scope, INK_OP_CONTENT);
        }

        choice->constant = (uint16_t)ink_astgen_add_const(scope, choice->id);
        ink_astgen_emit_const(scope, INK_OP_CONST, choice->constant);
        ink_astgen_emit_byte(scope, INK_OP_CHOICE);
    }

    ink_astgen_emit_byte(scope, INK_OP_FLUSH);

    for (size_t i = 0; i < l->count; i++) {
        struct ink_astgen_choice *choice = &data[i];

        ink_astgen_emit_byte(scope, INK_OP_LOAD_CHOICE_ID);
        ink_astgen_emit_const(scope, INK_OP_CONST, choice->constant);
        ink_astgen_emit_byte(scope, INK_OP_CMP_EQ);
        choice->label = (uint16_t)ink_astgen_emit_jump(scope, INK_OP_JMP_T);
        ink_astgen_emit_byte(scope, INK_OP_POP);
    }

    /* TODO: Could possibly trap here instead. */
    ink_astgen_emit_byte(scope, INK_OP_EXIT);

    for (size_t i = 0; i < l->count; i++) {
        struct ink_astgen_choice *choice = &data[i];
        struct ink_ast_node *br_stmt = l->nodes[i];
        struct ink_ast_node *br_expr = br_stmt->data.bin.lhs;
        struct ink_ast_node *br_body = br_stmt->data.bin.rhs;
        struct ink_ast_node *lhs = br_expr->data.choice_expr.start_expr;
        struct ink_ast_node *rhs = br_expr->data.choice_expr.inner_expr;

        ink_astgen_patch_jump(scope, choice->label);
        ink_astgen_emit_byte(scope, INK_OP_POP);

        if (lhs) {
            ink_astgen_string(scope, lhs);
            ink_astgen_emit_byte(scope, INK_OP_CONTENT);
        }
        if (rhs) {
            ink_astgen_string(scope, rhs);
            ink_astgen_emit_byte(scope, INK_OP_CONTENT);
        }

        ink_astgen_emit_byte(scope, INK_OP_FLUSH);
        ink_astgen_block_stmt(scope, br_body);
    }

    ink_free(data);
}

static void ink_astgen_gather_stmt(struct ink_astgen *astgen,
                                   const struct ink_ast_node *stmt)
{
    struct ink_ast_node *const lhs = stmt->data.bin.lhs;

    if (lhs) {
        ink_astgen_stmt(astgen, lhs);
    }
}

static void ink_astgen_gathered_stmt(struct ink_astgen *parent_scope,
                                     const struct ink_ast_node *stmt)
{
    struct ink_astgen scope;
    struct ink_ast_node *const lhs = stmt->data.bin.lhs;
    struct ink_ast_node *const rhs = stmt->data.bin.rhs;

    ink_astgen_make(&scope, parent_scope, NULL);
    scope.exit_label = ink_astgen_add_label(&scope);

    ink_astgen_choice_stmt(&scope, lhs);
    ink_astgen_set_label(&scope, scope.exit_label);
    ink_astgen_gather_stmt(&scope, rhs);
}

static void ink_astgen_stmt(struct ink_astgen *scope,
                            const struct ink_ast_node *stmt)
{
    assert(stmt);
    switch (stmt->type) {
    case INK_AST_VAR_DECL:
    case INK_AST_CONST_DECL:
    case INK_AST_TEMP_DECL:
        ink_astgen_var_decl(scope, stmt);
        break;
    case INK_AST_ASSIGN_STMT:
        ink_astgen_assign_stmt(scope, stmt);
        break;
    case INK_AST_CONTENT_STMT:
        ink_astgen_content_stmt(scope, stmt);
        break;
    case INK_AST_DIVERT_STMT:
        ink_astgen_divert_stmt(scope, stmt);
        break;
    case INK_AST_EXPR_STMT:
        ink_astgen_expr_stmt(scope, stmt);
        break;
    case INK_AST_RETURN_STMT:
        ink_astgen_return_stmt(scope, stmt);
        break;
    case INK_AST_CHOICE_STMT:
        ink_astgen_choice_stmt(scope, stmt);
        break;
    case INK_AST_GATHERED_STMT:
        ink_astgen_gathered_stmt(scope, stmt);
        break;
    case INK_AST_GATHER_POINT_STMT:
        ink_astgen_gather_stmt(scope, stmt);
        break;
    default:
        INK_ASTGEN_BUG(stmt);
        break;
    }
}

static void ink_astgen_knot_proto(struct ink_astgen *parent_scope,
                                  const struct ink_ast_node *proto,
                                  const struct ink_symbol *sym)
{
    int rc = INK_E_FAIL;
    struct ink_symbol param_sym;
    struct ink_astgen_global *const g = parent_scope->global;
    struct ink_ast_node *const rhs = proto->data.bin.rhs;
    const size_t str_index = sym->as.knot.str_index;

    ink_astgen_add_knot(parent_scope, str_index);

    if (rhs) {
        struct ink_ast_node_list *const args = rhs->data.many.list;

        if (args) {
            for (size_t i = 0; i < args->count; i++) {
                struct ink_ast_node *const param = args->nodes[i];

                rc = ink_astgen_lookup_name(parent_scope, param, &param_sym);
                if (rc < 0) {
                    return;
                }

                g->current_path->arity++;
            }
        }
    }
}

static void ink_astgen_func_decl(struct ink_astgen *parent_scope,
                                 const struct ink_ast_node *decl)
{
    struct ink_symbol sym;
    struct ink_astgen scope;
    struct ink_ast_node *const proto = decl->data.bin.lhs;
    struct ink_ast_node *const body = decl->data.bin.rhs;
    struct ink_ast_node *const name = proto->data.bin.lhs;

    assert(name != NULL);

    if (ink_astgen_lookup_name(parent_scope, name, &sym) < 0) {
        ink_astgen_error(parent_scope, INK_AST_E_UNKNOWN_IDENTIFIER, name);
        return;
    }

    ink_astgen_make(&scope, parent_scope, sym.as.knot.local_names);
    scope.exit_label = ink_astgen_add_label(&scope);
    ink_astgen_knot_proto(&scope, proto, &sym);
    ink_astgen_block_stmt(&scope, body);
    ink_astgen_set_label(parent_scope, parent_scope->exit_label);
    ink_astgen_emit_byte(&scope, INK_OP_RET);
    ink_astgen_backpatch(&scope, scope.jumps_top, scope.global->branches.count);
}

static void ink_astgen_stitch_decl(struct ink_astgen *parent_scope,
                                   const struct ink_ast_node *decl)
{
    struct ink_symbol sym;
    struct ink_astgen scope;
    struct ink_ast_node *const proto = decl->data.bin.lhs;
    struct ink_ast_node *const body = decl->data.bin.rhs;
    struct ink_ast_node *const name = proto->data.bin.lhs;

    assert(name != NULL);

    if (ink_astgen_lookup_name(parent_scope, name, &sym) < 0) {
        ink_astgen_error(parent_scope, INK_AST_E_UNKNOWN_IDENTIFIER, name);
        return;
    }

    ink_astgen_make(&scope, parent_scope, sym.as.knot.local_names);
    scope.exit_label = ink_astgen_add_label(parent_scope);
    ink_astgen_knot_proto(&scope, proto, &sym);
    ink_astgen_block_stmt(&scope, body);
    ink_astgen_set_label(&scope, scope.exit_label);
    ink_astgen_emit_byte(&scope, INK_OP_EXIT);
    ink_astgen_backpatch(&scope, scope.jumps_top, scope.global->branches.count);
}

static void ink_astgen_knot_decl(struct ink_astgen *parent_scope,
                                 const struct ink_ast_node *decl)
{
    size_t i = 0;
    struct ink_symbol sym;
    struct ink_astgen scope;
    struct ink_ast_node *const proto = decl->data.knot_decl.proto;
    struct ink_ast_node_list *const l = decl->data.knot_decl.children;
    struct ink_ast_node *const name = proto->data.bin.lhs;

    if (ink_astgen_lookup_name(parent_scope, name, &sym) < 0) {
        ink_astgen_error(parent_scope, INK_AST_E_UNKNOWN_IDENTIFIER, name);
        return;
    }

    ink_astgen_make(&scope, parent_scope, sym.as.knot.local_names);
    scope.exit_label = ink_astgen_add_label(&scope);
    ink_astgen_knot_proto(&scope, proto, &sym);

    if (!l) {
        ink_astgen_set_label(&scope, scope.exit_label);
        ink_astgen_emit_byte(&scope, INK_OP_EXIT);
        return;
    }

    struct ink_ast_node *const first = l->nodes[0];

    if (first->type == INK_AST_BLOCK) {
        ink_astgen_block_stmt(&scope, first);
        i++;
    }

    ink_astgen_set_label(&scope, scope.exit_label);
    ink_astgen_emit_byte(&scope, INK_OP_EXIT);
    ink_astgen_backpatch(&scope, scope.jumps_top, scope.global->branches.count);

    for (; i < l->count; i++) {
        struct ink_ast_node *const child = l->nodes[i];

        assert(child && child->type == INK_AST_STITCH_DECL);
        ink_astgen_stitch_decl(&scope, child);
    }
}

static void ink_astgen_default_body(struct ink_astgen *parent_scope,
                                    const struct ink_ast_node *body)
{
    const uint8_t *const b = (uint8_t *)INK_DEFAULT_PATH;
    const size_t bl = strlen(INK_DEFAULT_PATH);
    const size_t i = ink_astgen_add_str(parent_scope, b, bl);

    parent_scope->exit_label = ink_astgen_add_label(parent_scope);

    ink_astgen_add_knot(parent_scope, i);
    ink_astgen_block_stmt(parent_scope, body);
    ink_astgen_set_label(parent_scope, parent_scope->exit_label);
    ink_astgen_emit_byte(parent_scope, INK_OP_EXIT);
}

static int ink_astgen_record_proto(struct ink_astgen *parent_scope,
                                   const struct ink_ast_node *proto,
                                   struct ink_astgen *child_scope)
{
    int rc = INK_E_FAIL;
    struct ink_astgen_global *const g = parent_scope->global;
    struct ink_symtab_pool *const st_pool = &g->symtab_pool;
    struct ink_ast_node *const lhs = proto->data.bin.lhs;
    struct ink_ast_node *const rhs = proto->data.bin.rhs;
    struct ink_string_ref knot_str = ink_string_from_node(parent_scope, lhs);
    struct ink_symtab *const local_symtab = ink_symtab_make(st_pool);

    if (!local_symtab) {
        return rc;
    }

    ink_astgen_make(child_scope, parent_scope, local_symtab);

    child_scope->namespace_index =
        ink_astgen_add_qualified_str(parent_scope, &knot_str);

    struct ink_symbol proto_sym = {
        .type = proto->type == INK_AST_FUNC_PROTO ? INK_SYMBOL_FUNC
                                                  : INK_SYMBOL_KNOT,
        .node = proto,
        .as.knot.local_names = local_symtab,
        .as.knot.str_index = child_scope->namespace_index,
    };

    rc = ink_astgen_insert_name(parent_scope, lhs, &proto_sym);
    if (rc < 0) {
        ink_astgen_error(parent_scope, INK_AST_E_REDEFINED_IDENTIFIER, lhs);
        return rc;
    }
    if (rhs) {
        struct ink_ast_node_list *const args = rhs->data.many.list;

        if (args) {
            for (size_t i = 0; i < args->count; i++) {
                struct ink_ast_node *const param = args->nodes[i];
                struct ink_string_ref param_str =
                    ink_string_from_node(child_scope, param);
                struct ink_symbol param_sym = {
                    .type = INK_SYMBOL_PARAM,
                    .node = param,
                    .as.var.stack_slot = i,
                    .as.var.str_index = ink_astgen_add_str(
                        child_scope, param_str.bytes, param_str.length),
                };

                rc = ink_astgen_insert_name(child_scope, param, &param_sym);
                if (rc < 0) {
                    ink_astgen_error(child_scope,
                                     INK_AST_E_REDEFINED_IDENTIFIER, param);
                    return rc;
                }
            }

            proto_sym.as.knot.arity = args->count;
        }

        rc = ink_astgen_update_name(parent_scope, lhs, &proto_sym);
        if (rc < 0) {
            return rc;
        }
    }
    return rc;
}

/**
 * Collect information for a stitch prototype.
 */
static int ink_astgen_intern_stitch(struct ink_astgen *parent_scope,
                                    const struct ink_ast_node *decl)
{
    struct ink_astgen stitch_scope;
    struct ink_ast_node *const proto = decl->data.bin.lhs;

    return ink_astgen_record_proto(parent_scope, proto, &stitch_scope);
}

/**
 * Collect information for a knot prototype.
 */
static int ink_astgen_intern_knot(struct ink_astgen *parent_scope,
                                  const struct ink_ast_node *decl)
{
    int rc;
    struct ink_astgen knot_scope;
    struct ink_ast_node *const proto = decl->data.knot_decl.proto;
    struct ink_ast_node_list *const l = decl->data.knot_decl.children;

    rc = ink_astgen_record_proto(parent_scope, proto, &knot_scope);
    if (rc < 0) {
        return rc;
    }
    if (!l) {
        return rc;
    }
    for (size_t i = 0; i < l->count; i++) {
        struct ink_ast_node *const child = l->nodes[i];

        if (child->type == INK_AST_STITCH_DECL) {
            rc = ink_astgen_intern_stitch(&knot_scope, child);
            if (rc < 0) {
                break;
            }
        }
    }

    ink_astgen_deinit(&knot_scope);
    return rc;
}

/**
 * Collect information for a function prototype.
 */
static int ink_astgen_intern_func(struct ink_astgen *parent_scope,
                                  const struct ink_ast_node *decl)
{
    return ink_astgen_intern_stitch(parent_scope, decl);
}

/**
 * Perform a pass over the AST, gathering prototype information.
 */
static int ink_astgen_intern_paths(struct ink_astgen *parent_scope,
                                   const struct ink_ast_node *root)
{
    int rc = 0;
    struct ink_ast_node_list *const l = root->data.many.list;

    if (!l) {
        return rc;
    }
    for (size_t i = 0; i < l->count; i++) {
        struct ink_ast_node *const decl = l->nodes[i];

        switch (decl->type) {
        case INK_AST_KNOT_DECL:
            rc = ink_astgen_intern_knot(parent_scope, decl);
            break;
        case INK_AST_STITCH_DECL:
            rc = ink_astgen_intern_stitch(parent_scope, decl);
            break;
        case INK_AST_FUNC_DECL:
            rc = ink_astgen_intern_func(parent_scope, decl);
            break;
        default:
            assert(decl->type == INK_AST_BLOCK);
            break;
        }
        if (rc < 0) {
            break;
        }
    }
    return rc;
}

static void ink_astgen_file(struct ink_astgen_global *g,
                            const struct ink_ast_node *file)
{
    struct ink_ast_node_list *const l = file->data.many.list;
    struct ink_symtab_pool *const st_pool = &g->symtab_pool;
    struct ink_astgen file_scope = {
        .parent = NULL,
        .global = g,
        .symbol_table = ink_symtab_make(st_pool),
    };

    if (!l) {
        ink_astgen_default_body(&file_scope, NULL);
        ink_astgen_backpatch(&file_scope, file_scope.jumps_top,
                             g->branches.count);
        return;
    }
    if (l->count > 0) {
        struct ink_ast_node *const first = l->nodes[0];
        size_t i = 0;

        ink_astgen_intern_paths(&file_scope, file);

        if (first->type == INK_AST_BLOCK) {
            ink_astgen_default_body(&file_scope, first);
            ink_astgen_backpatch(&file_scope, file_scope.jumps_top,
                                 g->branches.count);
            i++;
        }
        for (; i < l->count; i++) {
            struct ink_ast_node *const n = l->nodes[i];

            assert(n->type != INK_AST_BLOCK);
            switch (n->type) {
            case INK_AST_KNOT_DECL:
                ink_astgen_knot_decl(&file_scope, n);
                break;
            case INK_AST_STITCH_DECL:
                ink_astgen_stitch_decl(&file_scope, n);
                break;
            case INK_AST_FUNC_DECL:
                ink_astgen_func_decl(&file_scope, n);
                break;
            default:
                break;
            }
        }
    }
}

/**
 * Perform code generation via tree-walk.
 */
int ink_astgen(struct ink_ast *tree, struct ink_story *story, int flags)
{
    int rc = -INK_E_FAIL;
    struct ink_astgen_global g;

    ink_astgen_global_init(&g, tree, story);

    if (!ink_ast_error_vec_is_empty(&tree->errors)) {
        ink_ast_render_errors(tree);
        goto out;
    }
    if (setjmp(g.jmpbuf) == 0) {
        ink_byte_vec_push(&g.string_bytes, '\0');
        ink_astgen_file(&g, tree->root);
    } else {
        rc = -INK_E_PANIC;
        goto out;
    }
    if (!ink_ast_error_vec_is_empty(&tree->errors)) {
        ink_ast_render_errors(tree);
        rc = -INK_E_FAIL;
    } else {
        rc = INK_E_OK;
    }
out:
    ink_astgen_global_deinit(&g);
    return rc;
}
