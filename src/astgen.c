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
        .source_start = node->start_offset,
        .source_end = node->end_offset,
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
        .bytes = &tree->source_bytes[node->start_offset],
        .length = node->end_offset - node->start_offset,
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
        rc = ink_astgen_lookup_expr_r(scope, lhs->lhs, lhs->rhs, sym);
        if (rc < 0) {
            return rc;
        }

        rc = ink_astgen_lookup_expr_r(scope, lhs->rhs, rhs, sym);
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
        rc = ink_astgen_lookup_expr_r(scope, node->lhs, node->rhs, sym);
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
    ink_astgen_expr(astgen, node->lhs);
    ink_astgen_emit_byte(astgen, (uint8_t)op);
}

static void ink_astgen_binary_op(struct ink_astgen *astgen,
                                 const struct ink_ast_node *node,
                                 enum ink_vm_opcode op)
{
    ink_astgen_expr(astgen, node->lhs);
    ink_astgen_expr(astgen, node->rhs);
    ink_astgen_emit_byte(astgen, (uint8_t)op);
}

static void ink_astgen_logical_op(struct ink_astgen *astgen,
                                  const struct ink_ast_node *node,
                                  bool binary_or)
{
    ink_astgen_expr(astgen, node->lhs);

    const size_t else_branch =
        ink_astgen_emit_jump(astgen, binary_or ? INK_OP_JMP_T : INK_OP_JMP_F);

    ink_astgen_emit_byte(astgen, INK_OP_POP);
    ink_astgen_expr(astgen, node->rhs);
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
    ink_astgen_string(astgen, node->lhs);
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
                                       struct ink_ast_seq *args_list,
                                       struct ink_symbol *symbol)
{
    const size_t knot_arity = symbol->as.knot.arity;

    if (args_list->count != knot_arity) {
        if (args_list->count > knot_arity) {
            ink_astgen_error(astgen, INK_AST_E_TOO_MANY_ARGS, node);
        } else {
            ink_astgen_error(astgen, INK_AST_E_TOO_FEW_ARGS, node);
        }
        return -1;
    }
    return 0;
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
    struct ink_ast_node *const name_node = node->lhs;
    struct ink_ast_node *const args_node = node->rhs;

    if (name_node->type == INK_AST_SELECTOR_EXPR) {
        rc = ink_astgen_lookup_qualified(astgen, name_node, &sym);
    } else if (name_node->type == INK_AST_IDENTIFIER) {
        rc = ink_astgen_lookup_name(astgen, name_node, &sym);
    } else {
        assert(false);
    }
    if (rc < 0) {
        ink_astgen_error(astgen, INK_AST_E_UNKNOWN_IDENTIFIER, name_node);
        return;
    }
    if (args_node) {
        struct ink_ast_seq *const args_list = args_node->seq;

        rc = ink_astgen_check_args_count(astgen, name_node, args_list, &sym);
        if (rc < 0) {
            return;
        }

        for (size_t i = 0; i < args_list->count; i++) {
            struct ink_ast_node *const arg_node = args_list->nodes[i];

            ink_astgen_expr(astgen, arg_node);
        }
    }

    const struct ink_string_ref str = ink_string_from_node(astgen, name_node);
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
    ink_astgen_expr(astgen, node->lhs);
}

/* TODO(Brett): There may be a bug here regarding fallthrough branches. */
static void ink_astgen_if_expr(struct ink_astgen *astgen,
                               const struct ink_ast_node *node)
{
    struct ink_ast_node *const expr_node = node->lhs;
    struct ink_ast_node *const body_node = node->rhs;

    ink_astgen_expr(astgen, expr_node);

    const size_t then_branch = ink_astgen_emit_jump(astgen, INK_OP_JMP_F);

    ink_astgen_emit_byte(astgen, INK_OP_POP);
    ink_astgen_content_expr(astgen, body_node);
    ink_astgen_patch_jump(astgen, then_branch);
    ink_astgen_emit_byte(astgen, INK_OP_POP);
}

static void ink_astgen_block_stmt(struct ink_astgen *parent_scope,
                                  const struct ink_ast_node *node)
{
    struct ink_astgen block_scope;

    ink_astgen_make(&block_scope, parent_scope, NULL);

    if (node) {
        struct ink_ast_seq *const node_list = node->seq;
        assert(node->type == INK_AST_BLOCK);

        for (size_t i = 0; i < node_list->count; i++) {
            ink_astgen_stmt(&block_scope, node_list->nodes[i]);
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
        ink_astgen_block_stmt(astgen, else_node->rhs);
    }

    ink_astgen_patch_jump(astgen, else_branch);
}

static void ink_astgen_multi_if_block(struct ink_astgen *astgen,
                                      struct ink_ast_seq *children,
                                      size_t node_index)
{
    if (node_index >= children->count) {
        return;
    }

    struct ink_ast_node *const then_node = children->nodes[node_index];

    ink_astgen_expr(astgen, then_node->lhs);

    const size_t then_branch = ink_astgen_emit_jump(astgen, INK_OP_JMP_F);

    ink_astgen_emit_byte(astgen, INK_OP_POP);
    ink_astgen_block_stmt(astgen, then_node->rhs);

    const size_t else_branch = ink_astgen_emit_jump(astgen, INK_OP_JMP);

    ink_astgen_patch_jump(astgen, then_branch);
    ink_astgen_emit_byte(astgen, INK_OP_POP);

    if (node_index + 1 < children->count) {
        struct ink_ast_node *const else_node = children->nodes[node_index + 1];

        if (else_node->type == INK_AST_ELSE_BRANCH) {
            ink_astgen_block_stmt(astgen, else_node->rhs);
        } else {
            ink_astgen_multi_if_block(astgen, children, node_index + 1);
        }
    }

    ink_astgen_patch_jump(astgen, else_branch);
}

static void ink_astgen_switch_stmt(struct ink_astgen *astgen,
                                   const struct ink_ast_node *node)
{
    struct ink_astgen_global *const global = astgen->global;
    struct ink_content_path *const path = global->current_path;
    struct ink_ast_seq *const node_list = node->seq;
    const size_t label_top = global->labels.count;
    const size_t stack_slot = path->arity + path->locals_count++;

    ink_astgen_expr(astgen, node->lhs);
    ink_astgen_emit_const(astgen, INK_OP_STORE, (uint8_t)stack_slot);
    ink_astgen_emit_byte(astgen, INK_OP_POP);

    for (size_t i = 0; i < node_list->count; i++) {
        struct ink_ast_node *const branch_node = node_list->nodes[i];

        if (branch_node->type == INK_AST_SWITCH_CASE) {
            switch (branch_node->lhs->type) {
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
        ink_astgen_expr(astgen, branch_node->lhs);
        ink_astgen_emit_byte(astgen, INK_OP_CMP_EQ);
        ink_astgen_add_jump(astgen, label_index,
                            ink_astgen_emit_jump(astgen, INK_OP_JMP_T));
        ink_astgen_emit_byte(astgen, INK_OP_POP);
    }
    for (size_t i = 0; i < node_list->count; i++) {
        struct ink_ast_node *const branch_node = node_list->nodes[i];

        ink_astgen_set_label(astgen, label_top + i);
        ink_astgen_emit_byte(astgen, INK_OP_POP);
        ink_astgen_block_stmt(astgen, branch_node->rhs);
    }
}

static void ink_astgen_conditional(struct ink_astgen *astgen,
                                   const struct ink_ast_node *node)

{
    bool has_block = false;
    bool has_else = false;
    bool has_initial = node->lhs != NULL;

    if (!node->seq || node->seq->count == 0) {
        ink_astgen_error(astgen, INK_AST_E_CONDITIONAL_EMPTY, node);
        return;
    }

    struct ink_ast_seq *const children = node->seq;
    struct ink_ast_node *const first = children->nodes[0];
    struct ink_ast_node *const last = children->nodes[children->count - 1];

    for (size_t i = 0; i < children->count; i++) {
        struct ink_ast_node *const child = children->nodes[i];

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
    if (has_initial && has_block) {
        ink_astgen_if_stmt(astgen, node->lhs, first,
                           first == last ? NULL : last);
    } else if (has_initial) {
        ink_astgen_switch_stmt(astgen, node);
    } else {
        ink_astgen_multi_if_block(astgen, node->seq, 0);
    }
}

static void ink_astgen_content_expr(struct ink_astgen *astgen,
                                    const struct ink_ast_node *node)
{
    struct ink_ast_seq *const expr_list = node->seq;

    if (!expr_list) {
        return;
    }
    for (size_t i = 0; i < expr_list->count; i++) {
        struct ink_ast_node *const expr_node = expr_list->nodes[i];

        switch (expr_node->type) {
        case INK_AST_STRING:
            ink_astgen_string(astgen, expr_node);
            ink_astgen_emit_byte(astgen, INK_OP_CONTENT_PUSH);
            break;
        case INK_AST_INLINE_LOGIC:
            ink_astgen_inline_logic(astgen, expr_node);
            ink_astgen_emit_byte(astgen, INK_OP_CONTENT_PUSH);
            break;
        case INK_AST_IF_STMT:
        case INK_AST_MULTI_IF_STMT:
        case INK_AST_SWITCH_STMT:
            ink_astgen_conditional(astgen, expr_node);
            ink_astgen_emit_byte(astgen, INK_OP_CONTENT_PUSH);
            break;
        case INK_AST_IF_EXPR:
            ink_astgen_if_expr(astgen, expr_node);
            ink_astgen_emit_byte(astgen, INK_OP_CONTENT_PUSH);
            break;
        case INK_AST_GLUE:
            INK_ASTGEN_TODO("GlueExpr");
            break;
        default:
            INK_ASTGEN_BUG(expr_node);
            break;
        }
    }
}

static void ink_astgen_var_decl(struct ink_astgen *astgen,
                                const struct ink_ast_node *node)
{
    struct ink_astgen_global *const global = astgen->global;
    struct ink_content_path *const path = global->current_path;
    struct ink_ast_node *const name_node = node->lhs;
    struct ink_ast_node *const expr_node = node->rhs;
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
    struct ink_ast_node *name_node = node->lhs;

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
    ink_astgen_content_expr(astgen, node->lhs);
    ink_astgen_emit_byte(astgen, INK_OP_FLUSH);
}

static void ink_astgen_divert_stmt(struct ink_astgen *astgen,
                                   const struct ink_ast_node *node)
{
    ink_astgen_divert_expr(astgen, node->lhs);
}

static void ink_astgen_expr_stmt(struct ink_astgen *astgen,
                                 const struct ink_ast_node *node)
{
    ink_astgen_expr(astgen, node->lhs);
    ink_astgen_emit_byte(astgen, INK_OP_POP);
}

static void ink_astgen_assign_stmt(struct ink_astgen *astgen,
                                   const struct ink_ast_node *node)
{
    int rc = INK_E_FAIL;
    struct ink_symbol sym;
    struct ink_ast_node *const name_node = node->lhs;
    struct ink_ast_node *const expr_node = node->rhs;

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
    ink_astgen_expr(astgen, node->lhs);
    ink_astgen_emit_byte(astgen, INK_OP_RET);
}

struct ink_astgen_choice {
    uint16_t constant;
    uint16_t label;
    struct ink_ast_node *start_expr;
    struct ink_ast_node *option_expr;
    struct ink_ast_node *inner_expr;
};

static void ink_astgen_choice_stmt(struct ink_astgen *astgen,
                                   const struct ink_ast_node *node)
{
    struct ink_ast_seq *const node_list = node->seq;
    struct ink_astgen_choice *const choice_data =
        ink_malloc(node_list->count * sizeof(*choice_data));

    if (!choice_data) {
        return;
    }
    for (size_t i = 0; i < node_list->count; i++) {
        struct ink_astgen_choice *const choice = &choice_data[i];
        struct ink_ast_node *const choice_node = node_list->nodes[i];

        assert(choice_node->type == INK_AST_CHOICE_STAR_STMT ||
               choice_node->type == INK_AST_CHOICE_PLUS_STMT);

        struct ink_ast_node *const hdr_node = choice_node->lhs;
        struct ink_ast_seq *const expr_list = hdr_node->seq;
        struct ink_object *const number =
            ink_number_new(astgen->global->story, (double)i);

        choice->start_expr = NULL;
        choice->option_expr = NULL;
        choice->inner_expr = NULL;

        if (expr_list) {
            for (size_t j = 0; j < expr_list->count; j++) {
                struct ink_ast_node *const expr_node = expr_list->nodes[j];

                if (expr_node->type == INK_AST_CHOICE_START_EXPR) {
                    choice->start_expr = expr_node;
                } else if (expr_node->type == INK_AST_CHOICE_OPTION_EXPR) {
                    choice->option_expr = expr_node;
                } else if (expr_node->type == INK_AST_CHOICE_INNER_EXPR) {
                    choice->inner_expr = expr_node;
                }
            }
        }
        if (choice->start_expr) {
            ink_astgen_string(astgen, choice->start_expr);
            ink_astgen_emit_byte(astgen, INK_OP_CONTENT_PUSH);
        }
        if (choice->option_expr) {
            ink_astgen_string(astgen, choice->option_expr);
            ink_astgen_emit_byte(astgen, INK_OP_CONTENT_PUSH);
        }

        choice->constant = (uint16_t)ink_astgen_add_const(astgen, number);

        ink_astgen_emit_const(astgen, INK_OP_CONST, choice->constant);
        ink_astgen_emit_byte(astgen, INK_OP_CHOICE_PUSH);
    }

    ink_astgen_emit_byte(astgen, INK_OP_FLUSH);

    for (size_t i = 0; i < node_list->count; i++) {
        struct ink_astgen_choice *const choice = &choice_data[i];

        ink_astgen_emit_byte(astgen, INK_OP_LOAD_CHOICE_ID);
        ink_astgen_emit_const(astgen, INK_OP_CONST, choice->constant);
        ink_astgen_emit_byte(astgen, INK_OP_CMP_EQ);
        choice->label = (uint16_t)ink_astgen_emit_jump(astgen, INK_OP_JMP_T);
        ink_astgen_emit_byte(astgen, INK_OP_POP);
    }

    /* TODO: Could possibly trap here instead. */
    ink_astgen_emit_byte(astgen, INK_OP_EXIT);

    for (size_t i = 0; i < node_list->count; i++) {
        struct ink_astgen_choice *const choice = &choice_data[i];
        struct ink_ast_node *const choice_node = node_list->nodes[i];
        struct ink_ast_node *const body_node = choice_node->rhs;

        ink_astgen_patch_jump(astgen, choice->label);
        ink_astgen_emit_byte(astgen, INK_OP_POP);

        if (choice->start_expr) {
            ink_astgen_string(astgen, choice->start_expr);
            ink_astgen_emit_byte(astgen, INK_OP_CONTENT_PUSH);
        }
        if (choice->inner_expr) {
            ink_astgen_string(astgen, choice->inner_expr);
            ink_astgen_emit_byte(astgen, INK_OP_CONTENT_PUSH);
        }

        ink_astgen_emit_byte(astgen, INK_OP_FLUSH);
        ink_astgen_block_stmt(astgen, body_node);
    }

    ink_free(choice_data);
}

static void ink_astgen_gather_stmt(struct ink_astgen *astgen,
                                   const struct ink_ast_node *node)
{
    if (node->lhs) {
        ink_astgen_stmt(astgen, node->lhs);
    }
}

static void ink_astgen_gathered_stmt(struct ink_astgen *parent_scope,
                                     const struct ink_ast_node *node)
{
    struct ink_astgen scope;

    ink_astgen_make(&scope, parent_scope, NULL);
    scope.exit_label = ink_astgen_add_label(&scope);

    ink_astgen_choice_stmt(&scope, node->lhs);
    ink_astgen_set_label(&scope, scope.exit_label);
    ink_astgen_gather_stmt(&scope, node->rhs);
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
    struct ink_ast_seq *const proto_list = node->seq;
    const size_t str_index = sym->as.knot.str_index;

    ink_astgen_add_knot(parent_scope, str_index);

    if (proto_list->count > 1) {
        struct ink_astgen_global *const global = parent_scope->global;
        struct ink_ast_node *const args_node = proto_list->nodes[1];
        struct ink_ast_seq *const args_list = args_node->seq;

        if (args_list) {
            for (size_t i = 0; i < args_list->count; i++) {
                struct ink_symbol param_sym;
                struct ink_ast_node *const param = args_list->nodes[i];

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
    struct ink_ast_node *const proto_node = node->lhs;
    struct ink_ast_node *const body_node = node->rhs;
    struct ink_ast_seq *const proto_list = proto_node->seq;
    struct ink_ast_node *const name_node = proto_list->nodes[0];

    if (ink_astgen_lookup_name(parent_scope, name_node, &sym) < 0) {
        ink_astgen_error(parent_scope, INK_AST_E_UNKNOWN_IDENTIFIER, node);
        return;
    }

    ink_astgen_make(&scope, parent_scope, sym.as.knot.local_names);
    scope.exit_label = ink_astgen_add_label(&scope);
    ink_astgen_knot_proto(&scope, proto_node, &sym);
    ink_astgen_block_stmt(&scope, body_node);
    ink_astgen_set_label(parent_scope, parent_scope->exit_label);
    ink_astgen_emit_byte(&scope, INK_OP_RET);
    ink_astgen_backpatch(&scope, scope.jumps_top, scope.global->branches.count);
}

static void ink_astgen_stitch_decl(struct ink_astgen *parent_scope,
                                   const struct ink_ast_node *node)
{
    struct ink_symbol sym;
    struct ink_astgen scope;
    struct ink_ast_node *const proto_node = node->lhs;
    struct ink_ast_node *const body_node = node->rhs;
    struct ink_ast_seq *const proto_list = proto_node->seq;
    struct ink_ast_node *const name_node = proto_list->nodes[0];

    if (ink_astgen_lookup_name(parent_scope, name_node, &sym) < 0) {
        ink_astgen_error(parent_scope, INK_AST_E_UNKNOWN_IDENTIFIER, node);
        return;
    }

    ink_astgen_make(&scope, parent_scope, sym.as.knot.local_names);
    scope.exit_label = ink_astgen_add_label(parent_scope);
    ink_astgen_knot_proto(&scope, proto_node, &sym);
    ink_astgen_block_stmt(&scope, body_node);
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
    struct ink_ast_node *const proto_node = node->lhs;
    struct ink_ast_seq *const child_list = node->seq;
    struct ink_ast_seq *const proto_list = proto_node->seq;
    struct ink_ast_node *const name_node = proto_list->nodes[0];

    if (ink_astgen_lookup_name(parent_scope, name_node, &sym) < 0) {
        ink_astgen_error(parent_scope, INK_AST_E_UNKNOWN_IDENTIFIER, node);
        return;
    }

    ink_astgen_make(&scope, parent_scope, sym.as.knot.local_names);
    scope.exit_label = ink_astgen_add_label(&scope);
    ink_astgen_knot_proto(&scope, proto_node, &sym);

    if (!child_list) {
        ink_astgen_set_label(&scope, scope.exit_label);
        ink_astgen_emit_byte(&scope, INK_OP_EXIT);
        return;
    }

    struct ink_ast_node *const first = child_list->nodes[0];

    if (first->type == INK_AST_BLOCK) {
        ink_astgen_block_stmt(&scope, first);
        i++;
    }

    ink_astgen_set_label(&scope, scope.exit_label);
    ink_astgen_emit_byte(&scope, INK_OP_EXIT);
    ink_astgen_backpatch(&scope, scope.jumps_top, scope.global->branches.count);

    for (; i < child_list->count; i++) {
        struct ink_ast_node *const child_node = child_list->nodes[i];

        assert(child_node && child_node->type == INK_AST_STITCH_DECL);
        ink_astgen_stitch_decl(&scope, child_node);
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
                                   const struct ink_ast_node *proto_node,
                                   struct ink_astgen *child_scope)
{
    int rc = INK_E_FAIL;
    struct ink_astgen_global *const global = parent_scope->global;
    struct ink_symtab_pool *const st_pool = &global->symtab_pool;
    struct ink_ast_seq *const proto_list = proto_node->seq;
    struct ink_ast_node *const name_node = proto_list->nodes[0];
    struct ink_string_ref knot_str =
        ink_string_from_node(parent_scope, name_node);
    struct ink_symtab *const local_symtab = ink_symtab_make(st_pool);

    if (!local_symtab) {
        return rc;
    }

    ink_astgen_make(child_scope, parent_scope, local_symtab);

    child_scope->namespace_index =
        ink_astgen_add_qualified_str(parent_scope, &knot_str);

    struct ink_symbol proto_sym = {
        .type = proto_node->type == INK_AST_FUNC_PROTO ? INK_SYMBOL_FUNC
                                                       : INK_SYMBOL_KNOT,
        .node = proto_node,
        .as.knot.local_names = local_symtab,
        .as.knot.str_index = child_scope->namespace_index,
    };

    rc = ink_astgen_insert_name(parent_scope, name_node, &proto_sym);
    if (rc < 0) {
        ink_astgen_error(parent_scope, INK_AST_E_REDEFINED_IDENTIFIER,
                         name_node);
        return rc;
    }
    if (proto_list->count > 1) {
        struct ink_ast_node *const args_node = proto_list->nodes[1];
        struct ink_ast_seq *const args_list = args_node->seq;

        if (args_list) {
            for (size_t i = 0; i < args_list->count; i++) {
                struct ink_ast_node *const param_node = args_list->nodes[i];
                struct ink_string_ref param_str =
                    ink_string_from_node(child_scope, param_node);
                struct ink_symbol param_sym = {
                    .type = INK_SYMBOL_PARAM,
                    .node = param_node,
                    .as.var.stack_slot = i,
                    .as.var.str_index = ink_astgen_add_str(
                        child_scope, param_str.bytes, param_str.length),
                };

                rc =
                    ink_astgen_insert_name(child_scope, param_node, &param_sym);
                if (rc < 0) {
                    ink_astgen_error(child_scope,
                                     INK_AST_E_REDEFINED_IDENTIFIER,
                                     param_node);
                    return rc;
                }
            }

            proto_sym.as.knot.arity = args_list->count;
        }

        rc = ink_astgen_update_name(parent_scope, name_node, &proto_sym);
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
                                    const struct ink_ast_node *root_node)
{
    struct ink_astgen stitch_scope;
    struct ink_ast_node *const proto_node = root_node->lhs;

    return ink_astgen_record_proto(parent_scope, proto_node, &stitch_scope);
}

/**
 * Collect information for a knot prototype.
 */
static int ink_astgen_intern_knot(struct ink_astgen *parent_scope,
                                  const struct ink_ast_node *root_node)
{
    int rc;
    struct ink_astgen knot_scope;
    struct ink_ast_node *const proto_node = root_node->lhs;
    struct ink_ast_seq *const body_list = root_node->seq;

    rc = ink_astgen_record_proto(parent_scope, proto_node, &knot_scope);
    if (rc < 0) {
        return rc;
    }
    if (!body_list) {
        return rc;
    }
    for (size_t i = 0; i < body_list->count; i++) {
        struct ink_ast_node *const body_node = body_list->nodes[i];

        if (body_node->type == INK_AST_STITCH_DECL) {
            rc = ink_astgen_intern_stitch(&knot_scope, body_node);
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
                                  const struct ink_ast_node *root_node)
{
    return ink_astgen_intern_stitch(parent_scope, root_node);
}

/**
 * Perform a pass over the AST, gathering prototype information.
 */
static int ink_astgen_intern_paths(struct ink_astgen *parent_scope,
                                   const struct ink_ast_node *root_node)
{
    int rc = 0;
    struct ink_ast_seq *const decl_list = root_node->seq;

    if (!decl_list) {
        return rc;
    }
    for (size_t i = 0; i < decl_list->count; i++) {
        struct ink_ast_node *const decl_node = decl_list->nodes[i];

        switch (decl_node->type) {
        case INK_AST_KNOT_DECL:
            rc = ink_astgen_intern_knot(parent_scope, decl_node);
            break;
        case INK_AST_STITCH_DECL:
            rc = ink_astgen_intern_stitch(parent_scope, decl_node);
            break;
        case INK_AST_FUNC_DECL:
            rc = ink_astgen_intern_func(parent_scope, decl_node);
            break;
        default:
            assert(decl_node->type == INK_AST_BLOCK);
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
    struct ink_ast_seq *const node_list = node->seq;
    struct ink_symtab_pool *const st_pool = &global->symtab_pool;
    struct ink_astgen file_scope = {
        .parent = NULL,
        .global = global,
        .symbol_table = ink_symtab_make(st_pool),
    };

    if (!node_list) {
        ink_astgen_default_body(&file_scope, NULL);
        ink_astgen_backpatch(&file_scope, file_scope.jumps_top,
                             file_scope.global->branches.count);
        return;
    }
    if (node_list->count > 0) {
        size_t i = 0;
        struct ink_ast_node *const first = node_list->nodes[0];

        ink_astgen_intern_paths(&file_scope, node);

        if (first->type == INK_AST_BLOCK) {
            ink_astgen_default_body(&file_scope, first);
            ink_astgen_backpatch(&file_scope, file_scope.jumps_top,
                                 file_scope.global->branches.count);
            i++;
        }
        for (; i < node_list->count; i++) {
            struct ink_ast_node *const node = node_list->nodes[i];

            assert(node->type != INK_AST_BLOCK);
            switch (node->type) {
            case INK_AST_KNOT_DECL:
                ink_astgen_knot_decl(&file_scope, node);
                break;
            case INK_AST_STITCH_DECL:
                ink_astgen_stitch_decl(&file_scope, node);
                break;
            case INK_AST_FUNC_DECL:
                ink_astgen_func_decl(&file_scope, node);
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
