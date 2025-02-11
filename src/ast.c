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
    size_t start_offset;
    size_t end_offset;
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

static void ink_ast_error_renderf(const struct ink_ast *tree,
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
        return;
    }

    info.message = (char *)ink_arena_allocate(arena, (size_t)msglen + 1);
    if (!info.message) {
        return;
    }

    va_start(ap, fmt);
    msglen = vsnprintf(info.message, (size_t)msglen + 1, fmt, ap);
    va_end(ap);

    bytes = tree->source_bytes;
    offset = 0;
    info.column = 0;
    info.line = 0;
    info.filename = tree->filename;

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
}

static void ink_ast_error_render(const struct ink_ast *tree,
                                 const struct ink_ast_error *error,
                                 struct ink_arena *arena)
{
    const size_t length = error->source_end - error->source_start;
    const uint8_t *const bytes = &tree->source_bytes[error->source_start];

    switch (error->type) {
    case INK_AST_IDENT_UNKNOWN: {
        ink_ast_error_renderf(tree, error, arena,
                              "use of undeclared identifier '%.*s'",
                              (int)length, bytes);
        break;
    }
    case INK_AST_IDENT_REDEFINED: {
        ink_ast_error_renderf(tree, error, arena, "redefinition of '%.*s'",
                              (int)length, bytes);
        break;
    }
    case INK_AST_CONDITIONAL_EMPTY: {
        ink_ast_error_renderf(tree, error, arena,
                              "condition block with no conditions");
        break;
    }
    case INK_AST_CONDITIONAL_EXPECTED_ELSE: {
        ink_ast_error_renderf(
            tree, error, arena,
            "expected '- else:' clause rather than extra condition");
        break;
    }
    case INK_AST_CONDITIONAL_MULTIPLE_ELSE: {
        ink_ast_error_renderf(tree, error, arena,
                              "multiple 'else' cases in conditional");
        break;
    }
    case INK_AST_CONDITIONAL_FINAL_ELSE: {
        ink_ast_error_renderf(
            tree, error, arena,
            "'else' case should always be the final case in conditional");
        break;
    }
    default:
        assert(false);
        return;
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
    size_t start_offset = 0;
    size_t end_offset = 0;

    while (bytes[end_offset] != '\0') {
        if (bytes[end_offset] == '\n') {
            range.start_offset = start_offset;
            range.end_offset = end_offset;

            ink_line_buffer_push(lines, range);
            start_offset = end_offset + 1;
        }
        end_offset++;
    }
    if (start_offset != end_offset || !end_offset) {
        range.start_offset = start_offset;
        range.end_offset = end_offset;

        ink_line_buffer_push(lines, range);
    }
}

static size_t ink_calculate_line(const struct ink_line_buffer *lines,
                                 size_t offset)
{
    size_t line_number = 0;

    for (size_t i = 0; i < lines->count; i++) {
        const struct ink_source_range range = lines->entries[i];

        if (offset >= range.start_offset && offset <= range.end_offset) {
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
    case INK_AST_FILE: {
        snprintf(buffer, length, "%s \"%s\"", context->node_type_strz,
                 context->filename);
        break;
    }
    case INK_AST_BLOCK:
    case INK_AST_CHOICE_STMT:
    case INK_AST_KNOT_DECL:
    case INK_AST_STITCH_DECL:
    case INK_AST_GATHERED_CHOICE_STMT: {
        snprintf(buffer, length, "%s <line:%zu, line:%zu>",
                 context->node_type_strz, context->line_start,
                 context->line_end);
        break;
    }
    case INK_AST_CHOICE_PLUS_STMT:
    case INK_AST_CHOICE_STAR_STMT:
    case INK_AST_CONST_DECL:
    case INK_AST_CONTENT_STMT:
    case INK_AST_DIVERT_STMT:
    case INK_AST_EXPR_STMT:
    case INK_AST_GATHER_STMT:
    case INK_AST_LIST_DECL:
    case INK_AST_RETURN_STMT:
    case INK_AST_TEMP_DECL:
    case INK_AST_VAR_DECL:
    case INK_AST_STRING_EXPR: {
        snprintf(buffer, length, "%s <line:%zu, col:%zu:%zu>",
                 context->node_type_strz, context->line_start,
                 context->column_start, context->column_end);
        break;
    }
    case INK_AST_STRING:
    case INK_AST_NUMBER:
    case INK_AST_IDENTIFIER:
    case INK_AST_CHOICE_START_EXPR:
    case INK_AST_CHOICE_OPTION_EXPR:
    case INK_AST_CHOICE_INNER_EXPR:
    case INK_AST_PARAM_DECL:
    case INK_AST_REF_PARAM_DECL: {
        snprintf(buffer, length, "%s `%.*s` <col:%zu, col:%zu>",
                 context->node_type_strz, (int)context->lexeme_length,
                 context->lexeme, context->column_start, context->column_end);
        break;
    }
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
    case INK_AST_FILE: {
        snprintf(buffer, length,
                 ANSI_COLOR_BLUE ANSI_BOLD_ON
                 "%s " ANSI_BOLD_OFF ANSI_COLOR_RESET "\"%s\"",
                 context->node_type_strz, context->filename);
        break;
    }
    case INK_AST_BLOCK:
    case INK_AST_CHOICE_STMT:
    case INK_AST_KNOT_DECL:
    case INK_AST_STITCH_DECL:
    case INK_AST_GATHERED_CHOICE_STMT: {
        snprintf(buffer, length,
                 ANSI_COLOR_BLUE ANSI_BOLD_ON
                 "%s " ANSI_BOLD_OFF ANSI_COLOR_RESET "<" ANSI_COLOR_YELLOW
                 "line:%zu, line:%zu" ANSI_COLOR_RESET ">",
                 context->node_type_strz, context->line_start,
                 context->line_end);
        break;
    }
    case INK_AST_CHOICE_PLUS_STMT:
    case INK_AST_CHOICE_STAR_STMT:
    case INK_AST_CONST_DECL:
    case INK_AST_CONTENT_STMT:
    case INK_AST_DIVERT_STMT:
    case INK_AST_EXPR_STMT:
    case INK_AST_GATHER_STMT:
    case INK_AST_LIST_DECL:
    case INK_AST_RETURN_STMT:
    case INK_AST_TEMP_DECL:
    case INK_AST_VAR_DECL:
    case INK_AST_STRING_EXPR: {
        snprintf(buffer, length,
                 ANSI_COLOR_BLUE ANSI_BOLD_ON
                 "%s " ANSI_BOLD_OFF ANSI_COLOR_RESET "<" ANSI_COLOR_YELLOW
                 "line:%zu, col:%zu:%zu" ANSI_COLOR_RESET ">",
                 context->node_type_strz, context->line_start,
                 context->column_start, context->column_end);
        break;
    }
    case INK_AST_STRING:
    case INK_AST_NUMBER:
    case INK_AST_IDENTIFIER:
    case INK_AST_CHOICE_START_EXPR:
    case INK_AST_CHOICE_OPTION_EXPR:
    case INK_AST_CHOICE_INNER_EXPR:
    case INK_AST_PARAM_DECL:
    case INK_AST_REF_PARAM_DECL: {
        snprintf(buffer, length,
                 ANSI_COLOR_BLUE ANSI_BOLD_ON
                 "%s " ANSI_BOLD_OFF ANSI_COLOR_RESET "`" ANSI_COLOR_GREEN
                 "%.*s" ANSI_COLOR_RESET "` "
                 "<" ANSI_COLOR_YELLOW "col:%zu, col:%zu" ANSI_COLOR_RESET ">",
                 context->node_type_strz, (int)context->lexeme_length,
                 context->lexeme, context->column_start, context->column_end);
        break;
    }
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
    const size_t line_start = ink_calculate_line(lines, node->start_offset);
    const size_t line_end = ink_calculate_line(lines, node->end_offset);
    const struct ink_source_range line_range = lines->entries[line_start];
    const struct ink_print_context context = {
        .filename = tree->filename,
        .node_type_strz = ink_ast_node_type_strz(node->type),
        .lexeme = tree->source_bytes + node->start_offset,
        .lexeme_length = node->end_offset - node->start_offset,
        .line_start = line_start + 1,
        .line_end = line_end + 1,
        .column_start = (node->start_offset - line_range.start_offset) + 1,
        .column_end = (node->end_offset - line_range.start_offset) + 1,
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
    char new_prefix[1024];
    struct ink_node_buffer nodes;

    ink_node_buffer_init(&nodes);

    if (node->lhs) {
        ink_node_buffer_push(&nodes, node->lhs);
    }
    if (node->rhs) {
        ink_node_buffer_push(&nodes, node->rhs);
    }
    if (node->seq) {
        for (size_t i = 0; i < node->seq->count; i++) {
            ink_node_buffer_push(&nodes, node->seq->nodes[i]);
        }
    }
    for (size_t i = 0; i < nodes.count; i++) {
        const char **pointers =
            i == nodes.count - 1 ? INK_AST_FMT_FINAL : INK_AST_FMT_INNER;

        snprintf(new_prefix, sizeof(new_prefix), "%s%s", prefix, pointers[1]);

        if (nodes.entries[i]) {
            ink_ast_print_node(tree, lines, nodes.entries[i], prefix, pointers,
                               colors);
            ink_ast_print_walk(tree, lines, nodes.entries[i], new_prefix,
                               pointers, colors);
        } else {
            printf("%s%sNullNode\n", prefix, pointers[0]);
        }
    }

    ink_node_buffer_deinit(&nodes);
}

/**
 * Print AST.
 */
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

/**
 * Create an AST node.
 */
struct ink_ast_node *ink_ast_node_new(enum ink_ast_node_type type,
                                      size_t start_offset, size_t end_offset,
                                      struct ink_ast_node *lhs,
                                      struct ink_ast_node *rhs,
                                      struct ink_ast_seq *seq,
                                      struct ink_arena *arena)
{
    struct ink_ast_node *node;

    assert(start_offset <= end_offset);

    node = ink_arena_allocate(arena, sizeof(*node));
    if (!node) {
        return node;
    }

    node->type = type;
    node->flags = 0;
    node->start_offset = start_offset;
    node->end_offset = end_offset;
    node->lhs = lhs;
    node->rhs = rhs;
    node->seq = seq;
    return node;
}

/**
 * Initialize AST.
 */
void ink_ast_init(struct ink_ast *tree, const char *filename,
                  const uint8_t *source_bytes)
{
    tree->filename = filename;
    tree->source_bytes = source_bytes;
    tree->root = NULL;

    ink_ast_error_vec_init(&tree->errors);
}

/**
 * Free AST.
 */
void ink_ast_deinit(struct ink_ast *tree)
{
    ink_ast_error_vec_deinit(&tree->errors);
}
