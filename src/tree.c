#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "arena.h"
#include "tree.h"
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
    const unsigned char *lexeme;
    size_t lexeme_length;
    size_t line_start;
    size_t line_end;
    size_t column_start;
    size_t column_end;
};

INK_VEC_DECLARE(ink_node_buffer, struct ink_syntax_node *)
INK_VEC_DECLARE(ink_line_buffer, struct ink_source_range)

#define T(name, description) description,
static const char *INK_NODE_TYPE_STR[] = {INK_NODE(T)};
#undef T

static const char *INK_SYNTAX_TREE_EMPTY[] = {"", ""};
static const char *INK_SYNTAX_TREE_INNER[] = {"|--", "|  "};
static const char *INK_SYNTAX_TREE_FINAL[] = {"`--", "   "};

/**
 * Return a NULL-terminated string representing the type description of a
 * syntax tree node.
 */
const char *ink_syntax_node_type_strz(enum ink_syntax_node_type type)
{
    return INK_NODE_TYPE_STR[type];
}

static void ink_build_lines(struct ink_line_buffer *lines,
                            const struct ink_source *source)
{
    struct ink_source_range range;
    size_t start_offset = 0;
    size_t end_offset = 0;

    while (source->bytes[end_offset] != '\0') {
        if (source->bytes[end_offset] == '\n') {
            range.start_offset = start_offset;
            range.end_offset = end_offset;

            ink_line_buffer_append(lines, range);
            start_offset = end_offset + 1;
        }
        end_offset++;
    }
    if (start_offset != end_offset || !end_offset) {
        range.start_offset = start_offset;
        range.end_offset = end_offset;

        ink_line_buffer_append(lines, range);
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

static void
ink_syntax_node_print_nocolors(const struct ink_syntax_node *node,
                               const struct ink_print_context *context,
                               char *buffer, size_t length)
{
    switch (node->type) {
    case INK_NODE_FILE: {
        snprintf(buffer, length, "%s \"%s\"", context->node_type_strz,
                 context->filename);
        break;
    }
    case INK_NODE_BLOCK_STMT:
    case INK_NODE_CHOICE_STMT: {
        snprintf(buffer, length, "%s <line:%zu, line:%zu>",
                 context->node_type_strz, context->line_start,
                 context->line_end);
        break;
    }
    case INK_NODE_CONTENT_STMT:
    case INK_NODE_CHOICE_STAR_STMT:
    case INK_NODE_CHOICE_PLUS_STMT:
    case INK_NODE_STRING_EXPR: {
        snprintf(buffer, length, "%s <line:%zu, col:%zu:%zu>",
                 context->node_type_strz, context->line_start,
                 context->column_start, context->column_end);
        break;
    }
    case INK_NODE_STRING_LITERAL:
    case INK_NODE_NUMBER_EXPR:
    case INK_NODE_IDENTIFIER_EXPR:
    case INK_NODE_CHOICE_START_EXPR:
    case INK_NODE_CHOICE_OPTION_EXPR:
    case INK_NODE_CHOICE_INNER_EXPR:
    case INK_NODE_PARAM_DECL:
    case INK_NODE_REF_PARAM_DECL: {
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

static void
ink_syntax_node_print_colors(const struct ink_syntax_node *node,
                             const struct ink_print_context *context,
                             char *buffer, size_t length)
{
    switch (node->type) {
    case INK_NODE_FILE: {
        snprintf(buffer, length,
                 ANSI_COLOR_BLUE ANSI_BOLD_ON
                 "%s " ANSI_BOLD_OFF ANSI_COLOR_RESET "\"%s\"",
                 context->node_type_strz, context->filename);
        break;
    }
    case INK_NODE_BLOCK_STMT:
    case INK_NODE_CHOICE_STMT: {
        snprintf(buffer, length,
                 ANSI_COLOR_BLUE ANSI_BOLD_ON
                 "%s " ANSI_BOLD_OFF ANSI_COLOR_RESET "<" ANSI_COLOR_YELLOW
                 "line:%zu, line:%zu" ANSI_COLOR_RESET ">",
                 context->node_type_strz, context->line_start,
                 context->line_end);
        break;
    }
    case INK_NODE_CONTENT_STMT:
    case INK_NODE_CHOICE_STAR_STMT:
    case INK_NODE_CHOICE_PLUS_STMT:
    case INK_NODE_STRING_EXPR: {
        snprintf(buffer, length,
                 ANSI_COLOR_BLUE ANSI_BOLD_ON
                 "%s " ANSI_BOLD_OFF ANSI_COLOR_RESET "<" ANSI_COLOR_YELLOW
                 "line:%zu, col:%zu:%zu" ANSI_COLOR_RESET ">",
                 context->node_type_strz, context->line_start,
                 context->column_start, context->column_end);
        break;
    }
    case INK_NODE_STRING_LITERAL:
    case INK_NODE_NUMBER_EXPR:
    case INK_NODE_IDENTIFIER_EXPR:
    case INK_NODE_CHOICE_START_EXPR:
    case INK_NODE_CHOICE_OPTION_EXPR:
    case INK_NODE_CHOICE_INNER_EXPR:
    case INK_NODE_PARAM_DECL:
    case INK_NODE_REF_PARAM_DECL: {
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

static void ink_syntax_tree_print_node(const struct ink_syntax_tree *tree,
                                       const struct ink_line_buffer *lines,
                                       const struct ink_syntax_node *node,
                                       const char *prefix,
                                       const char **pointers, bool colors)
{
    char line[1024];
    const size_t line_start = ink_calculate_line(lines, node->start_offset);
    const size_t line_end = ink_calculate_line(lines, node->end_offset);
    const struct ink_source_range line_range = lines->entries[line_start];
    const struct ink_print_context context = {
        .filename = tree->source->filename,
        .node_type_strz = ink_syntax_node_type_strz(node->type),
        .lexeme = tree->source->bytes + node->start_offset,
        .lexeme_length = node->end_offset - node->start_offset,
        .line_start = line_start + 1,
        .line_end = line_end + 1,
        .column_start = (node->start_offset - line_range.start_offset) + 1,
        .column_end = (node->end_offset - line_range.start_offset) + 1,
    };

    if (colors) {
        ink_syntax_node_print_colors(node, &context, line, sizeof(line));
    } else {
        ink_syntax_node_print_nocolors(node, &context, line, sizeof(line));
    }

    printf("%s%s%s\n", prefix, pointers[0], line);
}

static void ink_syntax_tree_print_walk(const struct ink_syntax_tree *tree,
                                       const struct ink_line_buffer *lines,
                                       const struct ink_syntax_node *node,
                                       const char *prefix,
                                       const char **pointers, bool colors)
{
    char new_prefix[1024];
    struct ink_node_buffer nodes;

    ink_node_buffer_create(&nodes);

    if (node->lhs) {
        ink_node_buffer_append(&nodes, node->lhs);
    }
    if (node->rhs) {
        ink_node_buffer_append(&nodes, node->rhs);
    }
    if (node->seq) {
        for (size_t i = 0; i < node->seq->count; i++) {
            ink_node_buffer_append(&nodes, node->seq->nodes[i]);
        }
    }
    for (size_t i = 0; i < nodes.count; i++) {
        const char **pointers = i == nodes.count - 1 ? INK_SYNTAX_TREE_FINAL
                                                     : INK_SYNTAX_TREE_INNER;

        snprintf(new_prefix, sizeof(new_prefix), "%s%s", prefix, pointers[1]);

        if (nodes.entries[i]) {
            ink_syntax_tree_print_node(tree, lines, nodes.entries[i], prefix,
                                       pointers, colors);
            ink_syntax_tree_print_walk(tree, lines, nodes.entries[i],
                                       new_prefix, pointers, colors);
        } else {
            printf("%s%sNullNode\n", prefix, pointers[0]);
        }
    }

    ink_node_buffer_destroy(&nodes);
}

/**
 * Print a syntax tree.
 */
void ink_syntax_tree_print(const struct ink_syntax_tree *tree, bool colors)
{
    struct ink_line_buffer lines;

    ink_line_buffer_create(&lines);
    ink_build_lines(&lines, tree->source);

    if (tree->root) {
        ink_syntax_tree_print_node(tree, &lines, tree->root, "",
                                   INK_SYNTAX_TREE_EMPTY, colors);
        ink_syntax_tree_print_walk(tree, &lines, tree->root, "",
                                   INK_SYNTAX_TREE_EMPTY, colors);
    }

    ink_line_buffer_destroy(&lines);
}

/**
 * Create a syntax tree node.
 */
struct ink_syntax_node *
ink_syntax_node_new(struct ink_arena *arena, enum ink_syntax_node_type type,
                    size_t start_offset, size_t end_offset,
                    struct ink_syntax_node *lhs, struct ink_syntax_node *rhs,
                    struct ink_syntax_seq *seq)
{
    struct ink_syntax_node *node;

    assert(start_offset <= end_offset);

    node = ink_arena_allocate(arena, sizeof(*node));
    if (!node) {
        return node;
    }

    node->type = type;
    node->start_offset = start_offset;
    node->end_offset = end_offset;
    node->lhs = lhs;
    node->rhs = rhs;
    node->seq = seq;
    return node;
}

/**
 * Initialize syntax tree.
 */
int ink_syntax_tree_initialize(const struct ink_source *source,
                               struct ink_syntax_tree *tree)
{
    tree->source = source;
    tree->root = NULL;
    return 0;
}

/**
 * Destroy syntax tree.
 */
void ink_syntax_tree_cleanup(struct ink_syntax_tree *tree)
{
}
