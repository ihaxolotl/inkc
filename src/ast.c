#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "ast.h"
#include "vec.h"

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"
#define ANSI_BOLD_ON "\x1b[1m"
#define ANSI_BOLD_OFF "\x1b[22m"

struct ink_source_range {
    size_t bytes_start;
    size_t bytes_end;
};

struct ink_print_context {
    const char *filename;
    const char *node_type_strz;
    const uint8_t *lexeme;
    size_t lexeme_length;
    size_t line_start;
    size_t line_end;
    size_t column_start;
    size_t column_end;
};

struct ink_error_info {
    size_t line;
    size_t column;
    size_t snippet_start;
    size_t snippet_end;
    const char *filename;
    char *message;
};

/* TODO: Rename `ink_node_buffer` to `ink_node_vec`. */
INK_VEC_T(ink_node_buffer, struct ink_ast_node *)
INK_VEC_T(ink_line_buffer, struct ink_source_range)

#define T(name, description) description,
static const char *INK_AST_TYPE_STR[] = {INK_MAKE_AST_NODES(T)};
#undef T

static const char *INK_AST_FMT_EMPTY[] = {"", ""};
static const char *INK_AST_FMT_INNER[] = {"|--", "|  "};
static const char *INK_AST_FMT_FINAL[] = {"`--", "   "};

/**
 * Return a NULL-terminated string representing the type description of a
 * syntax tree node.
 */
const char *ink_ast_node_type_strz(enum ink_ast_node_type type)
{
    return INK_AST_TYPE_STR[type];
}

/* FIXME: Deal with positions for not-printable characters. */
static void ink_render_error_info(const uint8_t *source_bytes,
                                  const struct ink_error_info *info)
{
    const size_t line = info->line + 1;
    const size_t col = info->column + 1;

    printf("%s:%zu:%zu: error: %s\n", info->filename, line, col, info->message);
    printf("%4zu | %.*s\n", line,
           (int)(info->snippet_end - info->snippet_start),
           source_bytes + info->snippet_start);
    printf("     | %*s^\n\n", (int)info->column, "");
}

static int ink_ast_error_renderf(const struct ink_ast *tree,
                                 const struct ink_ast_error *error,
                                 struct ink_arena *arena, const char *fmt, ...)
{
    va_list ap;
    long msglen;
    size_t offset;
    struct ink_error_info info;
    const uint8_t *bytes;

    va_start(ap, fmt);
    msglen = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (msglen < 0) {
        return -INK_E_OOM;
    }

    info.message = (char *)ink_arena_allocate(arena, (size_t)msglen + 1);
    if (!info.message) {
        return -INK_E_OOM;
    }

    va_start(ap, fmt);
    msglen = vsnprintf(info.message, (size_t)msglen + 1, fmt, ap);
    va_end(ap);

    bytes = tree->source_bytes;
    offset = 0;
    info.column = 0;
    info.line = 0;
    info.filename = (char *)tree->filename;

    for (;;) {
        if (offset < error->source_start) {
            if (bytes[offset] == '\n') {
                info.column = 0;
                info.line++;
            } else if (bytes[offset] != '\0') {
                info.column++;
            } else {
                break;
            }

            offset++;
        } else {
            info.snippet_end = info.snippet_start = offset - info.column;
            break;
        }
    }
    for (;;) {
        const uint8_t c = bytes[info.snippet_end];

        if (c != '\0' && c != '\n') {
            info.snippet_end++;
        } else {
            break;
        }
    }

    ink_render_error_info(tree->source_bytes, &info);
    return INK_E_OK;
}

static int ink_ast_error_render(const struct ink_ast *tree,
                                const struct ink_ast_error *error,
                                struct ink_arena *arena)
{
    const size_t length = error->source_end - error->source_start;
    const uint8_t *const bytes = &tree->source_bytes[error->source_start];

    switch (error->type) {
    case INK_AST_E_PANIC:
        return ink_ast_error_renderf(tree, error, arena, "parser panicked");
    case INK_AST_E_UNEXPECTED_TOKEN:
        return ink_ast_error_renderf(tree, error, arena, "unexpected token");
    case INK_AST_E_EXPECTED_NEWLINE:
        return ink_ast_error_renderf(tree, error, arena, "expected newline");
    case INK_AST_E_EXPECTED_DQUOTE:
        return ink_ast_error_renderf(
            tree, error, arena, "unterminated string, expected closing quote");
    case INK_AST_E_EXPECTED_IDENTIFIER:
        return ink_ast_error_renderf(tree, error, arena, "expected identifier");
    case INK_AST_E_EXPECTED_EXPR:
        return ink_ast_error_renderf(tree, error, arena, "expected expression");
    case INK_AST_E_INVALID_EXPR:
        return ink_ast_error_renderf(tree, error, arena, "invalid expression");
    case INK_AST_E_INVALID_LVALUE:
        return ink_ast_error_renderf(tree, error, arena,
                                     "invalid lvalue for assignment");
    case INK_AST_E_UNKNOWN_IDENTIFIER:
        return ink_ast_error_renderf(tree, error, arena,
                                     "use of undeclared identifier '%.*s'",
                                     (int)length, bytes);
    case INK_AST_E_REDEFINED_IDENTIFIER:
        return ink_ast_error_renderf(
            tree, error, arena, "redefinition of '%.*s'", (int)length, bytes);
    case INK_AST_E_TOO_FEW_ARGS:
        return ink_ast_error_renderf(tree, error, arena,
                                     "too few arguments to '%.*s'", (int)length,
                                     bytes);
    case INK_AST_E_TOO_MANY_ARGS:
        return ink_ast_error_renderf(tree, error, arena,
                                     "too many arguments to '%.*s'",
                                     (int)length, bytes);
    case INK_AST_E_TOO_MANY_PARAMS:
        return ink_ast_error_renderf(tree, error, arena,
                                     "too many parameters defined for '%.*s'",
                                     (int)length, bytes);
    case INK_AST_E_CONDITIONAL_EMPTY:
        return ink_ast_error_renderf(tree, error, arena,
                                     "condition block with no conditions");
    case INK_AST_E_ELSE_EXPECTED:
        return ink_ast_error_renderf(
            tree, error, arena,
            "expected '- else:' clause rather than extra condition");
    case INK_AST_E_ELSE_MULTIPLE:
        return ink_ast_error_renderf(tree, error, arena,
                                     "multiple 'else' cases in conditional");
    case INK_AST_E_ELSE_FINAL:
        return ink_ast_error_renderf(
            tree, error, arena,
            "'else' case should always be the final case in conditional");
    case INK_AST_E_SWITCH_EXPR:
        return ink_ast_error_renderf(
            tree, error, arena,
            "expected switch case expression to be constant value");
    case INK_AST_E_CONST_ASSIGN:
        return ink_ast_error_renderf(tree, error, arena,
                                     "attempt to modify constant value");
    default:
        return ink_ast_error_renderf(tree, error, arena, "unknown error");
    }
}

void ink_ast_render_errors(const struct ink_ast *tree)
{
    struct ink_arena arena;
    const struct ink_ast_error_vec *const errors = &tree->errors;
    static const size_t arena_alignment = 8;
    static const size_t arena_block_size = 8192;

    ink_arena_init(&arena, arena_block_size, arena_alignment);

    for (size_t i = 0; i < errors->count; i++) {
        const struct ink_ast_error e = errors->entries[i];

        ink_ast_error_render(tree, &e, &arena);
    }

    ink_arena_release(&arena);
}

static void ink_build_lines(struct ink_line_buffer *lines, const uint8_t *bytes)
{
    struct ink_source_range range;
    size_t bytes_start = 0;
    size_t bytes_end = 0;

    while (bytes[bytes_end] != '\0') {
        if (bytes[bytes_end] == '\n') {
            range.bytes_start = bytes_start;
            range.bytes_end = bytes_end;

            ink_line_buffer_push(lines, range);
            bytes_start = bytes_end + 1;
        }
        bytes_end++;
    }
    if (bytes_start != bytes_end || !bytes_end) {
        range.bytes_start = bytes_start;
        range.bytes_end = bytes_end;

        ink_line_buffer_push(lines, range);
    }
}

static size_t ink_calculate_line(const struct ink_line_buffer *lines,
                                 size_t offset)
{
    size_t line_number = 0;

    for (size_t i = 0; i < lines->count; i++) {
        const struct ink_source_range range = lines->entries[i];

        if (offset >= range.bytes_start && offset <= range.bytes_end) {
            return line_number;
        }

        line_number++;
    }
    return lines->count - 1;
}

static void ink_ast_node_print_nocolors(const struct ink_ast_node *node,
                                        const struct ink_print_context *context,
                                        char *buffer, size_t length)
{
    switch (node->type) {
    case INK_AST_FILE:
        snprintf(buffer, length, "%s \"%s\"", context->node_type_strz,
                 context->filename);
        break;
    case INK_AST_BLOCK:
    case INK_AST_CHOICE_STMT:
    case INK_AST_FUNC_DECL:
    case INK_AST_GATHERED_STMT:
    case INK_AST_KNOT_DECL:
    case INK_AST_STITCH_DECL:
        snprintf(buffer, length, "%s <line:%zu, line:%zu>",
                 context->node_type_strz, context->line_start,
                 context->line_end);
        break;
    case INK_AST_ASSIGN_STMT:
    case INK_AST_CHOICE_PLUS_STMT:
    case INK_AST_CHOICE_STAR_STMT:
    case INK_AST_CONST_DECL:
    case INK_AST_CONTENT_STMT:
    case INK_AST_DIVERT_STMT:
    case INK_AST_EXPR_STMT:
    case INK_AST_GATHER_POINT_STMT:
    case INK_AST_LIST_DECL:
    case INK_AST_RETURN_STMT:
    case INK_AST_TEMP_DECL:
    case INK_AST_VAR_DECL:
        snprintf(buffer, length, "%s <line:%zu, col:%zu:%zu>",
                 context->node_type_strz, context->line_start,
                 context->column_start, context->column_end);
        break;
    case INK_AST_CHOICE_START_EXPR:
    case INK_AST_CHOICE_OPTION_EXPR:
    case INK_AST_CHOICE_INNER_EXPR:
    case INK_AST_IDENTIFIER:
    case INK_AST_INTEGER:
    case INK_AST_FLOAT:
    case INK_AST_PARAM_DECL:
    case INK_AST_REF_PARAM_DECL:
    case INK_AST_STRING:
    case INK_AST_STRING_EXPR:
        snprintf(buffer, length, "%s `%.*s` <col:%zu, col:%zu>",
                 context->node_type_strz, (int)context->lexeme_length,
                 context->lexeme, context->column_start, context->column_end);
        break;
    default:
        snprintf(buffer, length, "%s <col:%zu, col:%zu>",
                 context->node_type_strz, context->column_start,
                 context->column_end);
        break;
    }
}

static void ink_ast_node_print_colors(const struct ink_ast_node *node,
                                      const struct ink_print_context *context,
                                      char *buffer, size_t length)
{
    switch (node->type) {
    case INK_AST_FILE:
        snprintf(buffer, length,
                 ANSI_COLOR_BLUE ANSI_BOLD_ON
                 "%s " ANSI_BOLD_OFF ANSI_COLOR_RESET "\"%s\"",
                 context->node_type_strz, context->filename);
        break;
    case INK_AST_BLOCK:
    case INK_AST_CHOICE_STMT:
    case INK_AST_FUNC_DECL:
    case INK_AST_GATHERED_STMT:
    case INK_AST_KNOT_DECL:
    case INK_AST_STITCH_DECL:
        snprintf(buffer, length,
                 ANSI_COLOR_BLUE ANSI_BOLD_ON
                 "%s " ANSI_BOLD_OFF ANSI_COLOR_RESET "<" ANSI_COLOR_YELLOW
                 "line:%zu, line:%zu" ANSI_COLOR_RESET ">",
                 context->node_type_strz, context->line_start,
                 context->line_end);
        break;
    case INK_AST_ASSIGN_STMT:
    case INK_AST_CHOICE_PLUS_STMT:
    case INK_AST_CHOICE_STAR_STMT:
    case INK_AST_CONST_DECL:
    case INK_AST_CONTENT_STMT:
    case INK_AST_DIVERT_STMT:
    case INK_AST_EXPR_STMT:
    case INK_AST_GATHER_POINT_STMT:
    case INK_AST_LIST_DECL:
    case INK_AST_RETURN_STMT:
    case INK_AST_TEMP_DECL:
    case INK_AST_VAR_DECL:
        snprintf(buffer, length,
                 ANSI_COLOR_BLUE ANSI_BOLD_ON
                 "%s " ANSI_BOLD_OFF ANSI_COLOR_RESET "<" ANSI_COLOR_YELLOW
                 "line:%zu, col:%zu:%zu" ANSI_COLOR_RESET ">",
                 context->node_type_strz, context->line_start,
                 context->column_start, context->column_end);
        break;
    case INK_AST_CHOICE_START_EXPR:
    case INK_AST_CHOICE_OPTION_EXPR:
    case INK_AST_CHOICE_INNER_EXPR:
    case INK_AST_IDENTIFIER:
    case INK_AST_INTEGER:
    case INK_AST_FLOAT:
    case INK_AST_PARAM_DECL:
    case INK_AST_REF_PARAM_DECL:
    case INK_AST_STRING_EXPR:
    case INK_AST_STRING:
        snprintf(buffer, length,
                 ANSI_COLOR_BLUE ANSI_BOLD_ON
                 "%s " ANSI_BOLD_OFF ANSI_COLOR_RESET "`" ANSI_COLOR_GREEN
                 "%.*s" ANSI_COLOR_RESET "` "
                 "<" ANSI_COLOR_YELLOW "col:%zu, col:%zu" ANSI_COLOR_RESET ">",
                 context->node_type_strz, (int)context->lexeme_length,
                 context->lexeme, context->column_start, context->column_end);
        break;
    default:
        snprintf(buffer, length,
                 ANSI_COLOR_BLUE ANSI_BOLD_ON
                 "%s " ANSI_BOLD_OFF ANSI_COLOR_RESET "<" ANSI_COLOR_YELLOW
                 "col:%zu, col:%zu" ANSI_COLOR_RESET ">",
                 context->node_type_strz, context->column_start,
                 context->column_end);
        break;
    }
}

static void ink_ast_print_node(const struct ink_ast *tree,
                               const struct ink_line_buffer *lines,
                               const struct ink_ast_node *node,
                               const char *prefix, const char **pointers,
                               bool colors)
{
    char line[1024];
    const size_t line_start = ink_calculate_line(lines, node->bytes_start);
    const size_t line_end = ink_calculate_line(lines, node->bytes_end);
    const struct ink_source_range line_range = lines->entries[line_start];
    const struct ink_print_context context = {
        .filename = (char *)tree->filename,
        .node_type_strz = ink_ast_node_type_strz(node->type),
        .lexeme = tree->source_bytes + node->bytes_start,
        .lexeme_length = node->bytes_end - node->bytes_start,
        .line_start = line_start + 1,
        .line_end = line_end + 1,
        .column_start = (node->bytes_start - line_range.bytes_start) + 1,
        .column_end = (node->bytes_end - line_range.bytes_start) + 1,
    };

    if (colors) {
        ink_ast_node_print_colors(node, &context, line, sizeof(line));
    } else {
        ink_ast_node_print_nocolors(node, &context, line, sizeof(line));
    }

    printf("%s%s%s\n", prefix, pointers[0], line);
}

static void ink_ast_print_walk(const struct ink_ast *tree,
                               const struct ink_line_buffer *lines,
                               const struct ink_ast_node *node,
                               const char *prefix, const char **pointers,
                               bool colors)
{
    /**
     * TODO: Have this entire procedure write characters to a stream instead of
     * directly to STDOUT.
     *
     * TODO: `new_prefix` needs bounds checking.
     */
    char new_prefix[1024];
    struct ink_ast_node *lhs, *mhs, *rhs;
    struct ink_ast_node_list *list;
    struct ink_node_buffer nodes;

    ink_node_buffer_init(&nodes);

    switch (node->type) {
    case INK_AST_ARG_LIST:
    case INK_AST_BLOCK:
    case INK_AST_CHOICE_STMT:
    case INK_AST_CONTENT:
    case INK_AST_EMPTY_STRING:
    case INK_AST_FILE:
    case INK_AST_PARAM_LIST:
        list = node->data.many.list;
        if (list) {
            for (size_t i = 0; i < list->count; i++) {
                ink_node_buffer_push(&nodes, list->nodes[i]);
            }
        }
        break;
    case INK_AST_CHOICE_EXPR:
        lhs = node->data.choice_expr.start_expr;
        mhs = node->data.choice_expr.option_expr;
        rhs = node->data.choice_expr.inner_expr;

        if (lhs) {
            ink_node_buffer_push(&nodes, lhs);
        }
        if (mhs) {
            ink_node_buffer_push(&nodes, mhs);
        }
        if (rhs) {
            ink_node_buffer_push(&nodes, rhs);
        }
        break;
    case INK_AST_IF_STMT:
    case INK_AST_MULTI_IF_STMT:
    case INK_AST_SWITCH_STMT:
        lhs = node->data.switch_stmt.cond_expr;
        list = node->data.switch_stmt.cases;

        if (lhs) {
            ink_node_buffer_push(&nodes, lhs);
        }
        if (list) {
            for (size_t i = 0; i < list->count; i++) {
                ink_node_buffer_push(&nodes, list->nodes[i]);
            }
        }

        break;
    case INK_AST_KNOT_DECL:
        lhs = node->data.knot_decl.proto;
        list = node->data.knot_decl.children;

        if (lhs) {
            ink_node_buffer_push(&nodes, lhs);
        }
        if (list) {
            for (size_t i = 0; i < list->count; i++) {
                ink_node_buffer_push(&nodes, list->nodes[i]);
            }
        }
        break;
    default:
        lhs = node->data.bin.lhs;
        rhs = node->data.bin.rhs;

        if (lhs) {
            ink_node_buffer_push(&nodes, lhs);
        }
        if (rhs) {
            ink_node_buffer_push(&nodes, rhs);
        }
        break;
    }
    for (size_t i = 0; i < nodes.count; i++) {
        const char **ptrs =
            i == nodes.count - 1 ? INK_AST_FMT_FINAL : INK_AST_FMT_INNER;

        snprintf(new_prefix, sizeof(new_prefix), "%s%s", prefix, ptrs[1]);

        if (nodes.entries[i]) {
            ink_ast_print_node(tree, lines, nodes.entries[i], prefix, ptrs,
                               colors);
            ink_ast_print_walk(tree, lines, nodes.entries[i], new_prefix, ptrs,
                               colors);
        } else {
            printf("%s%sNullNode\n", prefix, ptrs[0]);
        }
    }

    ink_node_buffer_deinit(&nodes);
}

void ink_ast_print(const struct ink_ast *tree, bool colors)
{
    struct ink_line_buffer lines;

    ink_line_buffer_init(&lines);
    ink_build_lines(&lines, tree->source_bytes);

    if (tree->root) {
        ink_ast_print_node(tree, &lines, tree->root, "", INK_AST_FMT_EMPTY,
                           colors);
        ink_ast_print_walk(tree, &lines, tree->root, "", INK_AST_FMT_EMPTY,
                           colors);
    }

    ink_line_buffer_deinit(&lines);
}

static struct ink_ast_node *ink_ast_base_new(enum ink_ast_node_type type,
                                             size_t bytes_start,
                                             size_t bytes_end,
                                             struct ink_arena *arena)
{
    struct ink_ast_node *const n = ink_arena_allocate(arena, sizeof(*n));

    if (!n) {
        return n;
    }

    memset(n, 0, sizeof(*n));

    n->type = type;
    n->bytes_start = bytes_start;
    n->bytes_end = bytes_end;
    return n;
}

struct ink_ast_node *ink_ast_leaf_new(enum ink_ast_node_type type,
                                      size_t bytes_start, size_t bytes_end,
                                      struct ink_arena *arena)
{
    struct ink_ast_node *const n =
        ink_ast_base_new(type, bytes_start, bytes_end, arena);

    assert(n);
    return n;
}

struct ink_ast_node *ink_ast_binary_new(enum ink_ast_node_type type,
                                        size_t bytes_start, size_t bytes_end,
                                        struct ink_ast_node *lhs,
                                        struct ink_ast_node *rhs,
                                        struct ink_arena *arena)
{
    struct ink_ast_node *const n =
        ink_ast_base_new(type, bytes_start, bytes_end, arena);

    assert(n);
    n->data.bin.lhs = lhs;
    n->data.bin.rhs = rhs;
    return n;
}

struct ink_ast_node *ink_ast_many_new(enum ink_ast_node_type type,
                                      size_t bytes_start, size_t bytes_end,
                                      struct ink_ast_node_list *list,
                                      struct ink_arena *arena)
{
    struct ink_ast_node *const n =
        ink_ast_base_new(type, bytes_start, bytes_end, arena);

    n->data.many.list = list;
    return n;
}

struct ink_ast_node *ink_ast_choice_expr_new(
    enum ink_ast_node_type type, size_t bytes_start, size_t bytes_end,
    struct ink_ast_node *start_expr, struct ink_ast_node *option_expr,
    struct ink_ast_node *inner_expr, struct ink_arena *arena)
{
    struct ink_ast_node *const n =
        ink_ast_base_new(type, bytes_start, bytes_end, arena);

    assert(n);
    n->data.choice_expr.start_expr = start_expr;
    n->data.choice_expr.option_expr = option_expr;
    n->data.choice_expr.inner_expr = inner_expr;
    return n;
}

struct ink_ast_node *ink_ast_switch_stmt_new(enum ink_ast_node_type type,
                                             size_t bytes_start,
                                             size_t bytes_end,
                                             struct ink_ast_node *cond_expr,
                                             struct ink_ast_node_list *cases,
                                             struct ink_arena *arena)
{
    struct ink_ast_node *const n =
        ink_ast_base_new(type, bytes_start, bytes_end, arena);

    assert(n);
    n->data.switch_stmt.cond_expr = cond_expr;
    n->data.switch_stmt.cases = cases;
    return n;
}

struct ink_ast_node *ink_ast_knot_decl_new(enum ink_ast_node_type type,
                                           size_t bytes_start, size_t bytes_end,
                                           struct ink_ast_node *proto,
                                           struct ink_ast_node_list *children,
                                           struct ink_arena *arena)
{
    struct ink_ast_node *const n =
        ink_ast_base_new(type, bytes_start, bytes_end, arena);

    assert(n);
    n->data.knot_decl.proto = proto;
    n->data.knot_decl.children = children;
    return n;
}

void ink_ast_init(struct ink_ast *tree, const uint8_t *filename,
                  const uint8_t *source_bytes)
{
    tree->filename = filename;
    tree->source_bytes = source_bytes;
    tree->root = NULL;
    ink_ast_error_vec_init(&tree->errors);
}

void ink_ast_deinit(struct ink_ast *tree)
{
    /**
     * TODO: Maybe add the arena as a parameter?
     */
    tree->filename = NULL;
    tree->source_bytes = NULL;
    tree->root = NULL;
    ink_ast_error_vec_deinit(&tree->errors);
}
