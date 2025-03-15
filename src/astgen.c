#include <assert.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "astgen.h"
#include "common.h"
#include "object.h"
#include "opcode.h"
#include "story.h"
#include "symtab.h"
#include "vec.h"

/**
 * TODO: Rename `astgen` parameter to `scope` or `parent_scope`, where
 * appropriate.
 *
 * TODO: Rename `node` parameter to something more descriptive, such as
 * `stmt` / `expr`.
 *
 * TODO: Prevent knots / stitches from explicitly returning. That should be
 * reserved for functions.
 *
 * TODO: Stitches could be allowed to be functions. Might help with code
 * organization.
 */
struct ink_object *ink_story_get_paths(struct ink_story *);

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

static void ink_astgen_global_init(struct ink_astgen_global *global,
                                   struct ink_ast *tree,
                                   struct ink_story *story)
{
    global->tree = tree;
    global->story = story;
    global->current_path = NULL;

    ink_symtab_pool_init(&global->symtab_pool);
    ink_stringset_init(&global->string_table, INK_STRINGSET_LOAD_MAX,
                       ink_stringset_hasher, ink_stringset_cmp);
    ink_byte_vec_init(&global->string_bytes);
    ink_astgen_label_vec_init(&global->labels);
    ink_astgen_jump_vec_init(&global->branches);
}

static void ink_astgen_global_deinit(struct ink_astgen_global *global)
{
    ink_symtab_pool_deinit(&global->symtab_pool);
    ink_stringset_deinit(&global->string_table);
    ink_byte_vec_deinit(&global->string_bytes);
    ink_astgen_label_vec_deinit(&global->labels);
    ink_astgen_jump_vec_deinit(&global->branches);
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
    struct ink_astgen_global *const global = parent_scope->global;
    struct ink_symtab_pool *const st_pool = &global->symtab_pool;

    scope->global = parent_scope->global;

    if (!symbol_table) {
        symbol_table = ink_symtab_make(st_pool);
        if (!symbol_table) {
            return INK_E_FAIL;
        }
    }

    scope->parent = parent_scope;
    scope->global = parent_scope->global;
    scope->symbol_table = symbol_table;
    scope->jumps_top = global->branches.count;
    scope->labels_top = global->labels.count;
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
    struct ink_astgen_global *const global = astgen->global;
    const struct ink_ast_error err = {
        .type = type,
        .source_start = node->bytes_start,
        .source_end = node->bytes_end,
    };

    ink_ast_error_vec_push(&global->tree->errors, err);
}

static void ink_astgen_global_panic(struct ink_astgen_global *global)
{
    longjmp(global->jmpbuf, 1);
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
    struct ink_astgen_global *const global = astgen->global;
    const struct ink_ast *const tree = global->tree;
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
    struct ink_astgen_global *const global = astgen->global;
    struct ink_byte_vec *const string_bytes = &global->string_bytes;
    const uint8_t *const bytes = &string_bytes->entries[index];
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
int ink_astgen_lookup_expr_r(struct ink_astgen *scope,
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

    if (!node) {
        return rc;
    }
    while (scope) {
        rc = ink_astgen_lookup_expr_r(scope, node->data.bin.lhs,
                                      node->data.bin.rhs, sym);
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
    struct ink_astgen_global *const global = astgen->global;
    struct ink_byte_vec *const code = &global->current_path->code;

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
    struct ink_astgen_global *const global = astgen->global;
    struct ink_byte_vec *const code = &global->current_path->code;

    ink_byte_vec_push(code, (uint8_t)op);
    ink_byte_vec_push(code, 0xff);
    ink_byte_vec_push(code, 0xff);
    return code->count - 2;
}

static size_t ink_astgen_add_jump(struct ink_astgen *astgen, size_t label_index,
                                  size_t code_offset)
{
    struct ink_astgen_global *const global = astgen->global;
    const size_t index = global->branches.count;
    const struct ink_astgen_jump jump = {
        .label = label_index,
        .code_offset = code_offset,
    };

    ink_astgen_jump_vec_push(&global->branches, jump);
    return index;
}

static size_t ink_astgen_add_label(struct ink_astgen *astgen)
{
    struct ink_astgen_global *const global = astgen->global;
    const size_t index = global->labels.count;
    const struct ink_astgen_label label = {
        .code_offset = 0xffffffff,
    };

    ink_astgen_label_vec_push(&global->labels, label);
    return index;
}

static void ink_astgen_set_label(struct ink_astgen *astgen, size_t label_index)
{
    struct ink_astgen_global *const global = astgen->global;
    struct ink_astgen_label_vec *const labels = &global->labels;
    struct ink_byte_vec *const code = &global->current_path->code;

    labels->entries[label_index].code_offset = code->count;
}

/**
 * Add a constant value to the current chunk.
 */
static size_t ink_astgen_add_const(struct ink_astgen *astgen,
                                   struct ink_object *obj)
{
    struct ink_astgen_global *const global = astgen->global;
    struct ink_object_vec *const_pool = &global->current_path->const_pool;
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
    struct ink_astgen_global *const global = astgen->global;
    struct ink_stringset *const string_table = &global->string_table;
    struct ink_byte_vec *const strings = &global->string_bytes;
    const struct ink_string_ref key = {
        .bytes = chars,
        .length = length,
    };
    size_t str_index = strings->count;

    rc = ink_stringset_lookup(string_table, key, &str_index);
    if (rc < 0) {
        for (size_t i = 0; i < length; i++) {
            ink_byte_vec_push(strings, chars[i]);
        }

        ink_byte_vec_push(strings, '\0');
        ink_stringset_insert(string_table, key, str_index);
    }
    return str_index;
}

/* TODO(Brett): Refactor this garbage. */
static size_t ink_astgen_add_qualified_str_r(struct ink_astgen *astgen,
                                             struct ink_string_ref *str,
                                             size_t pos)
{
    struct ink_astgen_global *const global = astgen->global;
    struct ink_byte_vec *const strings = &global->string_bytes;

    if (astgen->parent) {
        const size_t str_index = astgen->namespace_index;
        const size_t length = strlen((char *)&strings->entries[str_index]);

        for (size_t i = 0; i < length; i++) {
            ink_byte_vec_push(strings, strings->entries[str_index + i]);
        }

        ink_byte_vec_push(strings, '.');
        return ink_astgen_add_qualified_str_r(astgen->parent, str, pos);
    }
    for (size_t i = 0; i < str->length; i++) {
        ink_byte_vec_push(strings, str->bytes[i]);
    }

    ink_byte_vec_push(strings, '\0');
    return pos;
}

static size_t ink_astgen_add_qualified_str(struct ink_astgen *astgen,
                                           struct ink_string_ref *str)
{
    struct ink_astgen_global *const global = astgen->global;
    struct ink_byte_vec *const strings = &global->string_bytes;
    const size_t pos = strings->count;

    ink_astgen_add_qualified_str_r(astgen, str, pos);
    return pos;
}

static void ink_astgen_add_knot(struct ink_astgen *astgen, size_t str_index)
{
    struct ink_astgen_global *const global = astgen->global;
    struct ink_byte_vec *const string_bytes = &global->string_bytes;
    struct ink_story *const story = global->story;
    uint8_t *const str = &string_bytes->entries[str_index];
    struct ink_object *const paths_table = ink_story_get_paths(story);
    struct ink_object *const path_name =
        ink_string_new(story, str, strlen((char *)str));

    if (!path_name) {
        return;
    }

    struct ink_object *const path_obj = ink_content_path_new(story, path_name);
    if (!path_obj) {
        return;
    }

    ink_table_insert(story, paths_table, path_name, path_obj);
    astgen->global->current_path = INK_OBJ_AS_CONTENT_PATH(path_obj);
}

static void ink_astgen_patch_jump(struct ink_astgen *astgen, size_t offset)
{
    struct ink_astgen_global *const global = astgen->global;
    struct ink_byte_vec *const code = &global->current_path->code;
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
    struct ink_astgen_jump_vec *const jump_vec = &scope->global->branches;
    struct ink_astgen_label_vec *const label_vec = &scope->global->labels;
    struct ink_astgen_global *const global = scope->global;
    struct ink_byte_vec *const code = &global->current_path->code;

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
    struct ink_astgen_global *const global = astgen->global;

    return &global->string_bytes.entries[str_index];
}

static void ink_astgen_expr(struct ink_astgen *, const struct ink_ast_node *);
static void ink_astgen_stmt(struct ink_astgen *, const struct ink_ast_node *);
static void ink_astgen_content_expr(struct ink_astgen *,
                                    const struct ink_ast_node *);

static void ink_astgen_unary_op(struct ink_astgen *astgen,
                                const struct ink_ast_node *node,
                                enum ink_vm_opcode op)
{
    ink_astgen_expr(astgen, node->data.bin.lhs);
    ink_astgen_emit_byte(astgen, (uint8_t)op);
}

static void ink_astgen_binary_op(struct ink_astgen *astgen,
                                 const struct ink_ast_node *node,
                                 enum ink_vm_opcode op)
{
    ink_astgen_expr(astgen, node->data.bin.lhs);
    ink_astgen_expr(astgen, node->data.bin.rhs);
    ink_astgen_emit_byte(astgen, (uint8_t)op);
}

static void ink_astgen_logical_op(struct ink_astgen *astgen,
                                  const struct ink_ast_node *node,
                                  bool binary_or)
{
    ink_astgen_expr(astgen, node->data.bin.lhs);

    const size_t else_branch =
        ink_astgen_emit_jump(astgen, binary_or ? INK_OP_JMP_T : INK_OP_JMP_F);

    ink_astgen_emit_byte(astgen, INK_OP_POP);
    ink_astgen_expr(astgen, node->data.bin.rhs);
    ink_astgen_patch_jump(astgen, else_branch);
}

static void ink_astgen_true(struct ink_astgen *astgen)
{
    ink_astgen_emit_byte(astgen, INK_OP_TRUE);
}

static void ink_astgen_false(struct ink_astgen *astgen)
{
    ink_astgen_emit_byte(astgen, INK_OP_FALSE);
}

static void ink_astgen_number(struct ink_astgen *astgen,
                              const struct ink_ast_node *node)
{
    const struct ink_string_ref str = ink_string_from_node(astgen, node);
    const size_t str_index = ink_astgen_add_str(astgen, str.bytes, str.length);
    const uint8_t *const str_bytes = ink_astgen_str_bytes(astgen, str_index);
    const double value = strtod((char *)str_bytes, NULL);
    struct ink_object *const obj = ink_number_new(astgen->global->story, value);

    if (!obj) {
        ink_astgen_fail(astgen, "Could not create runtime object for number.");
        return;
    }

    ink_astgen_emit_const(astgen, INK_OP_CONST,
                          (uint8_t)ink_astgen_add_const(astgen, obj));
}

static void ink_astgen_string(struct ink_astgen *astgen,
                              const struct ink_ast_node *node)
{
    const struct ink_string_ref str = ink_string_from_node(astgen, node);
    const size_t str_index = ink_astgen_add_str(astgen, str.bytes, str.length);
    struct ink_object *const obj =
        ink_string_new(astgen->global->story, str.bytes, str.length);

    (void)str_index;

    if (!obj) {
        ink_astgen_fail(astgen, "Could not create runtime object for string.");
        return;
    }

    ink_astgen_emit_const(astgen, INK_OP_CONST,
                          (uint8_t)ink_astgen_add_const(astgen, obj));
}

static void ink_astgen_string_expr(struct ink_astgen *astgen,
                                   const struct ink_ast_node *node)
{
    ink_astgen_string(astgen, node->data.bin.lhs);
}

static void ink_astgen_identifier(struct ink_astgen *astgen,
                                  const struct ink_ast_node *node)
{
    struct ink_symbol sym;

    if (ink_astgen_lookup_name(astgen, node, &sym) < 0) {
        ink_astgen_error(astgen, INK_AST_E_UNKNOWN_IDENTIFIER, node);
        return;
    }

    switch (sym.type) {
    case INK_SYMBOL_VAR_GLOBAL:
        ink_astgen_emit_const(astgen, INK_OP_LOAD_GLOBAL,
                              (uint8_t)sym.as.var.const_slot);
        break;
    case INK_SYMBOL_VAR_LOCAL:
    case INK_SYMBOL_PARAM:
        ink_astgen_emit_const(astgen, INK_OP_LOAD,
                              (uint8_t)sym.as.var.stack_slot);
        break;
    default:
        ink_astgen_error(astgen, INK_AST_E_INVALID_EXPR, node);
        break;
    }
}

static int ink_astgen_check_args_count(struct ink_astgen *astgen,
                                       const struct ink_ast_node *node,
                                       struct ink_ast_node_list *args_list,
                                       struct ink_symbol *symbol)
{
    int rc = -1;
    const size_t knot_arity = symbol->as.knot.arity;

    if (!args_list) {
        if (knot_arity != 0) {
            ink_astgen_error(astgen, INK_AST_E_TOO_FEW_ARGS, node);
            rc = -1;
        } else {
            rc = 0;
        }
    } else {
        if (args_list->count != knot_arity) {
            if (args_list->count > knot_arity) {
                ink_astgen_error(astgen, INK_AST_E_TOO_MANY_ARGS, node);
            } else {
                ink_astgen_error(astgen, INK_AST_E_TOO_FEW_ARGS, node);
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
static void ink_astgen_call_expr(struct ink_astgen *astgen,
                                 const struct ink_ast_node *node,
                                 enum ink_vm_opcode op)
{
    int rc = INK_E_FAIL;
    struct ink_symbol sym;
    struct ink_ast_node *const lhs = node->data.bin.lhs;
    struct ink_ast_node *const rhs = node->data.bin.rhs;

    if (lhs->type == INK_AST_SELECTOR_EXPR) {
        rc = ink_astgen_lookup_qualified(astgen, lhs, &sym);
    } else if (lhs->type == INK_AST_IDENTIFIER) {
        rc = ink_astgen_lookup_name(astgen, lhs, &sym);
    } else {
        assert(false);
    }
    if (rc < 0) {
        ink_astgen_error(astgen, INK_AST_E_UNKNOWN_IDENTIFIER, lhs);
        return;
    }
    if (rhs) {
        struct ink_ast_node_list *const l = rhs->data.many.list;

        rc = ink_astgen_check_args_count(astgen, lhs, l, &sym);
        if (rc < 0) {
            return;
        }
        if (l) {
            for (size_t i = 0; i < l->count; i++) {
                struct ink_ast_node *const arg = l->nodes[i];

                ink_astgen_expr(astgen, arg);
            }
        }
    }

    const struct ink_string_ref str = ink_string_from_node(astgen, lhs);
    struct ink_object *const obj =
        ink_string_new(astgen->global->story, str.bytes, str.length);

    ink_astgen_emit_const(astgen, op,
                          (uint8_t)ink_astgen_add_const(astgen, obj));
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
    case INK_AST_NUMBER:
        ink_astgen_number(astgen, node);
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

static void ink_astgen_inline_logic(struct ink_astgen *astgen,
                                    const struct ink_ast_node *node)
{
    ink_astgen_expr(astgen, node->data.bin.lhs);
}

/* TODO(Brett): There may be a bug here regarding fallthrough branches. */
static void ink_astgen_if_expr(struct ink_astgen *astgen,
                               const struct ink_ast_node *node)
{
    struct ink_ast_node *const expr_node = node->data.bin.lhs;
    struct ink_ast_node *const body_node = node->data.bin.rhs;

    ink_astgen_expr(astgen, expr_node);

    const size_t then_branch = ink_astgen_emit_jump(astgen, INK_OP_JMP_F);

    ink_astgen_emit_byte(astgen, INK_OP_POP);
    ink_astgen_content_expr(astgen, body_node);
    ink_astgen_patch_jump(astgen, then_branch);
    ink_astgen_emit_byte(astgen, INK_OP_POP);
}

static void ink_astgen_block_stmt(struct ink_astgen *parent_scope,
                                  const struct ink_ast_node *stmt)
{
    struct ink_astgen block_scope;

    assert(!stmt || stmt->type == INK_AST_BLOCK);

    ink_astgen_make(&block_scope, parent_scope, NULL);

    if (stmt) {
        struct ink_ast_node_list *const stmt_list = stmt->data.many.list;

        for (size_t i = 0; i < stmt_list->count; i++) {
            ink_astgen_stmt(&block_scope, stmt_list->nodes[i]);
        }
    }

    ink_astgen_add_jump(&block_scope, block_scope.exit_label,
                        ink_astgen_emit_jump(&block_scope, INK_OP_JMP));
}

static void ink_astgen_if_stmt(struct ink_astgen *astgen,
                               const struct ink_ast_node *expr_node,
                               const struct ink_ast_node *then_node,
                               const struct ink_ast_node *else_node)
{
    ink_astgen_expr(astgen, expr_node);

    const size_t then_branch = ink_astgen_emit_jump(astgen, INK_OP_JMP_F);

    ink_astgen_emit_byte(astgen, INK_OP_POP);
    ink_astgen_block_stmt(astgen, then_node);

    const size_t else_branch = ink_astgen_emit_jump(astgen, INK_OP_JMP);

    ink_astgen_patch_jump(astgen, then_branch);
    ink_astgen_emit_byte(astgen, INK_OP_POP);

    if (else_node && else_node->type == INK_AST_ELSE_BRANCH) {
        ink_astgen_block_stmt(astgen, else_node->data.bin.rhs);
    }

    ink_astgen_patch_jump(astgen, else_branch);
}

static void ink_astgen_multi_if_block(struct ink_astgen *astgen,
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

    ink_astgen_expr(astgen, lhs);
    then_br = ink_astgen_emit_jump(astgen, INK_OP_JMP_F);
    ink_astgen_emit_byte(astgen, INK_OP_POP);
    ink_astgen_block_stmt(astgen, rhs);
    else_br = ink_astgen_emit_jump(astgen, INK_OP_JMP);
    ink_astgen_patch_jump(astgen, then_br);
    ink_astgen_emit_byte(astgen, INK_OP_POP);

    if (else_stmt) {
        if (else_stmt->type == INK_AST_ELSE_BRANCH) {
            rhs = else_stmt->data.bin.rhs;
            ink_astgen_block_stmt(astgen, rhs);
        } else {
            ink_astgen_multi_if_block(astgen, cases, index + 1);
        }
    }

    ink_astgen_patch_jump(astgen, else_br);
}

static void ink_astgen_switch_stmt(struct ink_astgen *astgen,
                                   const struct ink_ast_node *node)
{
    struct ink_astgen_global *const global = astgen->global;
    struct ink_content_path *const path = global->current_path;
    struct ink_ast_node *const cond_expr = node->data.switch_stmt.cond_expr;
    struct ink_ast_node_list *const cases = node->data.switch_stmt.cases;
    const size_t label_top = global->labels.count;
    const size_t stack_slot = path->arity + path->locals_count++;

    ink_astgen_expr(astgen, cond_expr);
    ink_astgen_emit_const(astgen, INK_OP_STORE, (uint8_t)stack_slot);
    ink_astgen_emit_byte(astgen, INK_OP_POP);

    for (size_t i = 0; i < cases->count; i++) {
        struct ink_ast_node *const branch_node = cases->nodes[i];
        struct ink_ast_node *lhs = NULL;

        if (branch_node->type == INK_AST_SWITCH_CASE) {
            lhs = branch_node->data.bin.lhs;

            switch (lhs->type) {
            case INK_AST_NUMBER:
            case INK_AST_TRUE:
            case INK_AST_FALSE:
                break;
            default:
                ink_astgen_error(astgen, INK_AST_E_SWITCH_EXPR, node);
                return;
            }
        }

        const size_t label_index = ink_astgen_add_label(astgen);

        ink_astgen_emit_const(astgen, INK_OP_LOAD, stack_slot);
        ink_astgen_expr(astgen, lhs);
        ink_astgen_emit_byte(astgen, INK_OP_CMP_EQ);
        ink_astgen_add_jump(astgen, label_index,
                            ink_astgen_emit_jump(astgen, INK_OP_JMP_T));
        ink_astgen_emit_byte(astgen, INK_OP_POP);
    }
    for (size_t i = 0; i < cases->count; i++) {
        struct ink_ast_node *const branch_node = cases->nodes[i];
        struct ink_ast_node *const rhs = branch_node->data.bin.rhs;

        ink_astgen_set_label(astgen, label_top + i);
        ink_astgen_emit_byte(astgen, INK_OP_POP);
        ink_astgen_block_stmt(astgen, rhs);
    }
}

static void ink_astgen_conditional(struct ink_astgen *astgen,
                                   const struct ink_ast_node *node)

{
    bool has_block = false;
    bool has_else = false;
    struct ink_ast_node *const expr = node->data.switch_stmt.cond_expr;
    struct ink_ast_node_list *const l = node->data.switch_stmt.cases;

    if (!l || l->count == 0) {
        ink_astgen_error(astgen, INK_AST_E_CONDITIONAL_EMPTY, node);
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
                ink_astgen_error(astgen, INK_AST_E_ELSE_EXPECTED, child);
                return;
            }
            break;
        case INK_AST_ELSE_BRANCH:
            /* Only the last branch can be an else. */
            if (child != last) {
                ink_astgen_error(astgen, INK_AST_E_ELSE_FINAL, child);
                return;
            }
            if (has_else) {
                ink_astgen_error(astgen, INK_AST_E_ELSE_MULTIPLE, child);
                return;
            }

            has_else = true;
            break;
        default:
            INK_ASTGEN_BUG(node);
            break;
        }
    }
    switch (node->type) {
    case INK_AST_IF_STMT:
        if (!expr) {
            ink_astgen_error(astgen, INK_AST_E_EXPECTED_EXPR, node);
            return;
        }
        ink_astgen_if_stmt(astgen, expr, first, first == last ? NULL : last);
        break;
    case INK_AST_MULTI_IF_STMT:
        ink_astgen_multi_if_block(astgen, l, 0);
        break;
    case INK_AST_SWITCH_STMT:
        ink_astgen_switch_stmt(astgen, node);
        break;
    default:
        break;
    }
}

static void ink_astgen_content_expr(struct ink_astgen *astgen,
                                    const struct ink_ast_node *node)
{
    struct ink_ast_node_list *const l = node->data.many.list;

    if (!l) {
        return;
    }
    for (size_t i = 0; i < l->count; i++) {
        struct ink_ast_node *const expr = l->nodes[i];

        switch (expr->type) {
        case INK_AST_STRING:
            ink_astgen_string(astgen, expr);
            ink_astgen_emit_byte(astgen, INK_OP_CONTENT_PUSH);
            break;
        case INK_AST_INLINE_LOGIC:
            ink_astgen_inline_logic(astgen, expr);
            ink_astgen_emit_byte(astgen, INK_OP_CONTENT_PUSH);
            break;
        case INK_AST_IF_STMT:
        case INK_AST_MULTI_IF_STMT:
        case INK_AST_SWITCH_STMT:
            ink_astgen_conditional(astgen, expr);
            ink_astgen_emit_byte(astgen, INK_OP_CONTENT_PUSH);
            break;
        case INK_AST_IF_EXPR:
            ink_astgen_if_expr(astgen, expr);
            ink_astgen_emit_byte(astgen, INK_OP_CONTENT_PUSH);
            break;
        case INK_AST_GLUE:
            INK_ASTGEN_TODO("GlueExpr");
            break;
        default:
            INK_ASTGEN_BUG(expr);
            break;
        }
    }
}

static void ink_astgen_var_decl(struct ink_astgen *astgen,
                                const struct ink_ast_node *node)
{
    struct ink_astgen_global *const global = astgen->global;
    struct ink_content_path *const path = global->current_path;
    struct ink_ast_node *const name_node = node->data.bin.lhs;
    struct ink_ast_node *const expr_node = node->data.bin.rhs;
    const struct ink_string_ref str = ink_string_from_node(astgen, name_node);
    const size_t str_index = ink_astgen_add_str(astgen, str.bytes, str.length);

    ink_astgen_expr(astgen, expr_node);

    if (node->type == INK_AST_TEMP_DECL) {
        const size_t stack_slot = path->arity + path->locals_count++;
        const struct ink_symbol sym = {
            .type = INK_SYMBOL_VAR_LOCAL,
            .node = node,
            .as.var.stack_slot = stack_slot,
            .as.var.str_index = str_index,
        };

        if (ink_astgen_insert_name(astgen, name_node, &sym) < 0) {
            ink_astgen_error(astgen, INK_AST_E_REDEFINED_IDENTIFIER, name_node);
            return;
        }

        ink_astgen_emit_const(astgen, INK_OP_STORE, (uint8_t)stack_slot);
    } else {
        struct ink_object *const name_obj =
            ink_string_new(global->story, str.bytes, str.length);
        const size_t const_index = ink_astgen_add_const(astgen, name_obj);
        const struct ink_symbol sym = {
            .type = INK_SYMBOL_VAR_GLOBAL,
            .node = node,
            .as.var.is_const = node->type == INK_AST_CONST_DECL,
            .as.var.const_slot = const_index,
            .as.var.str_index = str_index,
        };

        if (ink_astgen_insert_name(astgen, name_node, &sym) < 0) {
            ink_astgen_error(astgen, INK_AST_E_REDEFINED_IDENTIFIER, name_node);
            return;
        }

        ink_astgen_emit_const(astgen, INK_OP_STORE_GLOBAL,
                              (uint8_t)const_index);
    }

    ink_astgen_emit_byte(astgen, INK_OP_POP);
}

static void ink_astgen_divert_expr(struct ink_astgen *astgen,
                                   const struct ink_ast_node *node)
{
    struct ink_symbol sym;
    struct ink_ast_node *name_node = node->data.bin.lhs;

    assert(name_node);

    switch (name_node->type) {
    case INK_AST_SELECTOR_EXPR:
        if (ink_astgen_lookup_qualified(astgen, name_node, &sym) < 0) {
            ink_astgen_error(astgen, INK_AST_E_UNKNOWN_IDENTIFIER, name_node);
            return;
        }
        break;
    case INK_AST_IDENTIFIER: {
        const struct ink_string_ref unqualified_name =
            ink_string_from_node(astgen, name_node);

        switch (unqualified_name.length) {
        case 3:
            if (memcmp(unqualified_name.bytes, "END",
                       unqualified_name.length) == 0) {
                ink_astgen_emit_byte(astgen, INK_OP_EXIT);
                return;
            }
            break;
        case 4:
            if (memcmp(unqualified_name.bytes, "DONE",
                       unqualified_name.length) == 0) {
                ink_astgen_emit_byte(astgen, INK_OP_EXIT);
                return;
            }
            break;
        default:
            break;
        }
        if (ink_astgen_lookup_name(astgen, name_node, &sym) < 0) {
            ink_astgen_error(astgen, INK_AST_E_UNKNOWN_IDENTIFIER, name_node);
            return;
        }
        break;
    case INK_AST_CALL_EXPR:
        ink_astgen_call_expr(astgen, name_node, INK_OP_DIVERT);
        return;
    }
    default:
        INK_ASTGEN_BUG(name_node);
        return;
    }

    const struct ink_string_ref qualified_str =
        ink_string_from_index(astgen, sym.as.knot.str_index);
    struct ink_object *const obj = ink_string_new(
        astgen->global->story, qualified_str.bytes, qualified_str.length);

    ink_astgen_emit_const(astgen, INK_OP_DIVERT,
                          (uint8_t)ink_astgen_add_const(astgen, obj));
}

static void ink_astgen_content_stmt(struct ink_astgen *astgen,
                                    const struct ink_ast_node *node)
{
    ink_astgen_content_expr(astgen, node->data.bin.lhs);
    ink_astgen_emit_byte(astgen, INK_OP_FLUSH);
}

static void ink_astgen_divert_stmt(struct ink_astgen *astgen,
                                   const struct ink_ast_node *node)
{
    ink_astgen_divert_expr(astgen, node->data.bin.lhs);
}

static void ink_astgen_expr_stmt(struct ink_astgen *astgen,
                                 const struct ink_ast_node *node)
{
    ink_astgen_expr(astgen, node->data.bin.lhs);
    ink_astgen_emit_byte(astgen, INK_OP_POP);
}

static void ink_astgen_assign_stmt(struct ink_astgen *astgen,
                                   const struct ink_ast_node *node)
{
    int rc = INK_E_FAIL;
    struct ink_symbol sym;
    struct ink_ast_node *const name_node = node->data.bin.lhs;
    struct ink_ast_node *const expr_node = node->data.bin.rhs;

    rc = ink_astgen_lookup_name(astgen, name_node, &sym);
    if (rc < 0) {
        ink_astgen_error(astgen, INK_AST_E_UNKNOWN_IDENTIFIER, name_node);
        return;
    }

    ink_astgen_expr(astgen, expr_node);

    switch (sym.type) {
    case INK_SYMBOL_VAR_GLOBAL:
        ink_astgen_emit_const(astgen, INK_OP_STORE_GLOBAL,
                              (uint8_t)sym.as.var.const_slot);
        ink_astgen_emit_byte(astgen, INK_OP_POP);
        break;
    case INK_SYMBOL_VAR_LOCAL:
        ink_astgen_emit_const(astgen, INK_OP_STORE,
                              (uint8_t)sym.as.var.stack_slot);
        ink_astgen_emit_byte(astgen, INK_OP_POP);
        break;
    default:
        /* TODO: Give a more informative error message here. */
        ink_astgen_error(astgen, INK_AST_E_INVALID_EXPR, name_node);
        break;
    }
}

static void ink_astgen_return_stmt(struct ink_astgen *astgen,
                                   const struct ink_ast_node *node)
{
    ink_astgen_expr(astgen, node->data.bin.lhs);
    ink_astgen_emit_byte(astgen, INK_OP_RET);
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

    data = ink_malloc(l->count * sizeof(*data));
    if (!data) {
        return;
    }
    for (size_t i = 0; i < l->count; i++) {
        struct ink_astgen_choice *choice = &data[i];
        struct ink_ast_node *br_stmt = l->nodes[i];

        assert(br_stmt->type == INK_AST_CHOICE_STAR_STMT ||
               br_stmt->type == INK_AST_CHOICE_PLUS_STMT);

        choice->id = ink_number_new(scope->global->story, (double)i);
        if (!choice->id) {
            /* TODO: Probably panic and report an error here. */
            return;
        }

        struct ink_ast_node *br_expr = br_stmt->data.bin.lhs;
        struct ink_ast_node *lhs = br_expr->data.choice_expr.start_expr;
        struct ink_ast_node *rhs = br_expr->data.choice_expr.option_expr;

        if (lhs) {
            ink_astgen_string(scope, lhs);
            ink_astgen_emit_byte(scope, INK_OP_CONTENT_PUSH);
        }
        if (rhs) {
            ink_astgen_string(scope, rhs);
            ink_astgen_emit_byte(scope, INK_OP_CONTENT_PUSH);
        }

        choice->constant = (uint16_t)ink_astgen_add_const(scope, choice->id);
        ink_astgen_emit_const(scope, INK_OP_CONST, choice->constant);
        ink_astgen_emit_byte(scope, INK_OP_CHOICE_PUSH);
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
            ink_astgen_emit_byte(scope, INK_OP_CONTENT_PUSH);
        }
        if (rhs) {
            ink_astgen_string(scope, rhs);
            ink_astgen_emit_byte(scope, INK_OP_CONTENT_PUSH);
        }

        ink_astgen_emit_byte(scope, INK_OP_FLUSH);
        ink_astgen_block_stmt(scope, br_body);
    }

    ink_free(data);
}

static void ink_astgen_gather_stmt(struct ink_astgen *astgen,
                                   const struct ink_ast_node *stmt)
{
    struct ink_ast_node *lhs = stmt->data.bin.lhs;

    if (lhs) {
        ink_astgen_stmt(astgen, lhs);
    }
}

static void ink_astgen_gathered_stmt(struct ink_astgen *parent_scope,
                                     const struct ink_ast_node *stmt)
{
    struct ink_astgen scope;
    struct ink_ast_node *lhs = stmt->data.bin.lhs;
    struct ink_ast_node *rhs = stmt->data.bin.rhs;

    ink_astgen_make(&scope, parent_scope, NULL);
    scope.exit_label = ink_astgen_add_label(&scope);

    ink_astgen_choice_stmt(&scope, lhs);
    ink_astgen_set_label(&scope, scope.exit_label);
    ink_astgen_gather_stmt(&scope, rhs);
}

static void ink_astgen_stmt(struct ink_astgen *astgen,
                            const struct ink_ast_node *node)
{
    assert(node);

    switch (node->type) {
    case INK_AST_VAR_DECL:
    case INK_AST_CONST_DECL:
    case INK_AST_TEMP_DECL:
        ink_astgen_var_decl(astgen, node);
        break;
    case INK_AST_ASSIGN_STMT:
        ink_astgen_assign_stmt(astgen, node);
        break;
    case INK_AST_CONTENT_STMT:
        ink_astgen_content_stmt(astgen, node);
        break;
    case INK_AST_DIVERT_STMT:
        ink_astgen_divert_stmt(astgen, node);
        break;
    case INK_AST_EXPR_STMT:
        ink_astgen_expr_stmt(astgen, node);
        break;
    case INK_AST_RETURN_STMT:
        ink_astgen_return_stmt(astgen, node);
        break;
    case INK_AST_CHOICE_STMT:
        ink_astgen_choice_stmt(astgen, node);
        break;
    case INK_AST_GATHERED_STMT:
        ink_astgen_gathered_stmt(astgen, node);
        break;
    case INK_AST_GATHER_POINT_STMT:
        ink_astgen_gather_stmt(astgen, node);
        break;
    default:
        INK_ASTGEN_BUG(node);
        break;
    }
}

static void ink_astgen_knot_proto(struct ink_astgen *parent_scope,
                                  const struct ink_ast_node *node,
                                  const struct ink_symbol *sym)
{
    int rc = INK_E_FAIL;
    struct ink_symbol param_sym;
    struct ink_astgen_global *const global = parent_scope->global;
    struct ink_ast_node *const rhs = node->data.bin.rhs;
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

                global->current_path->arity++;
            }
        }
    }
}

static void ink_astgen_func_decl(struct ink_astgen *parent_scope,
                                 const struct ink_ast_node *node)
{
    struct ink_symbol sym;
    struct ink_astgen scope;
    struct ink_ast_node *const proto = node->data.bin.lhs;
    struct ink_ast_node *const body = node->data.bin.rhs;
    struct ink_ast_node *const name = proto->data.bin.lhs;

    assert(name != NULL);

    if (ink_astgen_lookup_name(parent_scope, name, &sym) < 0) {
        ink_astgen_error(parent_scope, INK_AST_E_UNKNOWN_IDENTIFIER, node);
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
                                   const struct ink_ast_node *node)
{
    struct ink_symbol sym;
    struct ink_astgen scope;
    struct ink_ast_node *const proto = node->data.bin.lhs;
    struct ink_ast_node *const body = node->data.bin.rhs;
    struct ink_ast_node *const name = proto->data.bin.lhs;

    assert(name != NULL);

    if (ink_astgen_lookup_name(parent_scope, name, &sym) < 0) {
        ink_astgen_error(parent_scope, INK_AST_E_UNKNOWN_IDENTIFIER, node);
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
                                 const struct ink_ast_node *node)
{
    size_t i = 0;
    struct ink_symbol sym;
    struct ink_astgen scope;
    struct ink_ast_node *const proto = node->data.knot_decl.proto;
    struct ink_ast_node_list *const l = node->data.knot_decl.children;
    struct ink_ast_node *const name = proto->data.bin.lhs;

    if (ink_astgen_lookup_name(parent_scope, name, &sym) < 0) {
        ink_astgen_error(parent_scope, INK_AST_E_UNKNOWN_IDENTIFIER, node);
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
                                    const struct ink_ast_node *node)
{
    const uint8_t *const bytes = (uint8_t *)INK_DEFAULT_PATH;
    const size_t length = strlen(INK_DEFAULT_PATH);
    const size_t name_index = ink_astgen_add_str(parent_scope, bytes, length);

    parent_scope->exit_label = ink_astgen_add_label(parent_scope);

    ink_astgen_add_knot(parent_scope, name_index);
    ink_astgen_block_stmt(parent_scope, node);
    ink_astgen_set_label(parent_scope, parent_scope->exit_label);
    ink_astgen_emit_byte(parent_scope, INK_OP_EXIT);
}

static int ink_astgen_record_proto(struct ink_astgen *parent_scope,
                                   const struct ink_ast_node *proto,
                                   struct ink_astgen *child_scope)
{
    int rc = INK_E_FAIL;
    struct ink_astgen_global *const global = parent_scope->global;
    struct ink_symtab_pool *const st_pool = &global->symtab_pool;
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
                                    const struct ink_ast_node *root)
{
    struct ink_astgen stitch_scope;
    struct ink_ast_node *const proto = root->data.bin.lhs;

    return ink_astgen_record_proto(parent_scope, proto, &stitch_scope);
}

/**
 * Collect information for a knot prototype.
 */
static int ink_astgen_intern_knot(struct ink_astgen *parent_scope,
                                  const struct ink_ast_node *root)
{
    int rc;
    struct ink_astgen knot_scope;
    struct ink_ast_node *const proto = root->data.knot_decl.proto;
    struct ink_ast_node_list *const l = root->data.knot_decl.children;

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
                                  const struct ink_ast_node *root)
{
    return ink_astgen_intern_stitch(parent_scope, root);
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

static void ink_astgen_file(struct ink_astgen_global *global,
                            const struct ink_ast_node *node)
{
    struct ink_ast_node_list *const l = node->data.many.list;
    struct ink_symtab_pool *const st_pool = &global->symtab_pool;
    struct ink_astgen file_scope = {
        .parent = NULL,
        .global = global,
        .symbol_table = ink_symtab_make(st_pool),
    };

    if (!l) {
        ink_astgen_default_body(&file_scope, NULL);
        ink_astgen_backpatch(&file_scope, file_scope.jumps_top,
                             file_scope.global->branches.count);
        return;
    }
    if (l->count > 0) {
        struct ink_ast_node *const first = l->nodes[0];
        size_t i = 0;

        ink_astgen_intern_paths(&file_scope, node);

        if (first->type == INK_AST_BLOCK) {
            ink_astgen_default_body(&file_scope, first);
            ink_astgen_backpatch(&file_scope, file_scope.jumps_top,
                                 file_scope.global->branches.count);
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
    struct ink_astgen_global global_store;

    if (!ink_ast_error_vec_is_empty(&tree->errors)) {
        ink_ast_render_errors(tree);
        return -INK_E_FAIL;
    }
    if (setjmp(global_store.jmpbuf) == 0) {
        ink_astgen_global_init(&global_store, tree, story);
        ink_byte_vec_push(&global_store.string_bytes, '\0');
        ink_astgen_file(&global_store, tree->root);
        ink_astgen_global_deinit(&global_store);
    } else {
        return -INK_E_PANIC;
    }
    if (!ink_ast_error_vec_is_empty(&tree->errors)) {
        ink_ast_render_errors(tree);
        return -INK_E_FAIL;
    }
    return INK_E_OK;
}
