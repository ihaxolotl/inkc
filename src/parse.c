#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lex.h"
#include "parse.h"

#define INK_PARSE_DEPTH 128
#define INK_SCRATCH_MIN_COUNT 16
#define INK_SCRATCH_GROWTH_FACTOR 2

struct ink_scratch_buffer {
    size_t count;
    size_t capacity;
    struct ink_syntax_node **entries;
};

struct ink_parser {
    struct ink_lexer lexer;
    struct ink_token current_token;
    struct ink_scratch_buffer scratch;
};

#define T(name, description) description,
static const char *INK_NODE_TYPE_STR[] = {INK_NODE(T)};
#undef T

static void ink_syntax_node_print_walk(const struct ink_syntax_node *node,
                                       int level);

/**
 * Create a syntax tree node sequence.
 */
static struct ink_syntax_seq *
ink_syntax_seq_new(struct ink_scratch_buffer *scratch, size_t start_offset,
                   size_t end_offset)
{
    struct ink_syntax_seq *seq;
    size_t seq_index = 0;
    size_t span = end_offset - start_offset;

    assert(span > 0);

    seq = malloc(sizeof(*seq) + span * sizeof(seq->nodes));
    if (seq == NULL)
        return NULL;

    seq->count = span;

    for (size_t i = start_offset; i < end_offset; i++) {
        seq->nodes[seq_index] = scratch->entries[i];
        seq_index++;
    }
    return seq;
}

/**
 * Create a syntax tree node.
 */
static struct ink_syntax_node *
ink_syntax_node_new(enum ink_syntax_node_type type, struct ink_syntax_node *lhs,
                    struct ink_syntax_node *rhs, struct ink_syntax_seq *seq)
{
    struct ink_syntax_node *node;

    node = malloc(sizeof(*node));
    if (node == NULL)
        return NULL;

    node->type = type;
    node->lhs = lhs;
    node->rhs = rhs;
    node->seq = seq;

    return node;
}

static const char *ink_syntax_node_type_strz(enum ink_syntax_node_type type)
{
    return INK_NODE_TYPE_STR[type];
}

static void ink_syntax_seq_print(const struct ink_syntax_seq *seq, int level)
{
    for (size_t i = 0; i < seq->count; i++) {
        ink_syntax_node_print_walk(seq->nodes[i], level);
    }
}

static void ink_syntax_node_print_walk(const struct ink_syntax_node *node,
                                       int level)
{
    const char *type_str;

    if (node == NULL)
        return;

    type_str = ink_syntax_node_type_strz(node->type);
    printf("%*s%s\n", level * 2, "", type_str);

    level++;

    ink_syntax_node_print_walk(node->lhs, level);
    ink_syntax_node_print_walk(node->rhs, level);

    if (node->seq)
        ink_syntax_seq_print(node->seq, level);
}

/**
 * Print a syntax tree node
 */
void ink_syntax_node_print(const struct ink_syntax_node *node)
{
    if (node == NULL)
        return;

    ink_syntax_node_print_walk(node, 0);
}

/**
 * Append a syntax tree node to the parser's scratch storage.
 *
 * These nodes can be retrieved later for creating sequences.
 */
static void ink_scratch_append(struct ink_scratch_buffer *scratch,
                               struct ink_syntax_node *node)
{
    size_t capacity;

    if (scratch->count + 1 > scratch->capacity) {
        if (scratch->capacity < INK_SCRATCH_MIN_COUNT) {
            capacity = INK_SCRATCH_MIN_COUNT;
        } else {
            capacity = scratch->capacity * INK_SCRATCH_GROWTH_FACTOR;
        }

        scratch->entries =
            realloc(scratch->entries, capacity * sizeof(scratch->entries));
        scratch->capacity = capacity;
    }

    scratch->entries[scratch->count++] = node;
}

/**
 * Shrink the parser's scratch storage down to a previous size.
 */
static void ink_scratch_shrink(struct ink_scratch_buffer *scratch, size_t count)
{
    scratch->count = count;
}

static struct ink_syntax_seq *
ink_seq_from_scratch(struct ink_scratch_buffer *scratch, size_t start_offset,
                     size_t end_offset)
{
    struct ink_syntax_seq *seq = NULL;

    if (start_offset < end_offset) {
        seq = ink_syntax_seq_new(scratch, start_offset, end_offset);

        ink_scratch_shrink(scratch, start_offset);
    }
    return seq;
}

/**
 * Check if the current token matches a specified type.
 */
static bool ink_parse_check(struct ink_parser *parser, enum ink_token_type type)
{
    return parser->current_token.type == type;
}

/**
 * Retrieve the next token from the lexer.
 */
static void ink_parse_next(struct ink_parser *parser)
{
    ink_token_next(&parser->lexer, &parser->current_token);
}

static struct ink_syntax_node *ink_parse_content_expr(struct ink_parser *parser)
{
    ink_parse_next(parser);

    return ink_syntax_node_new(INK_NODE_CONTENT_EXPR, NULL, NULL, NULL);
}

static struct ink_syntax_node *ink_parse_content_stmt(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    struct ink_syntax_seq *seq = NULL;
    struct ink_scratch_buffer *scratch = &parser->scratch;
    size_t scratch_offset = scratch->count;

    while (!ink_parse_check(parser, INK_TT_EOF) &&
           !ink_parse_check(parser, INK_TT_NL)) {
        node = ink_parse_content_expr(parser);
        ink_scratch_append(scratch, node);
    }

    seq = ink_seq_from_scratch(scratch, scratch_offset, scratch->count);

    return ink_syntax_node_new(INK_NODE_CONTENT_STMT, NULL, NULL, seq);
}

static struct ink_syntax_node *ink_parse_stmt(struct ink_parser *parser)
{
    struct ink_syntax_node *stmt = ink_parse_content_stmt(parser);

    while (ink_parse_check(parser, INK_TT_NL))
        ink_parse_next(parser);

    return stmt;
}

static struct ink_syntax_node *ink_parse_file(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    struct ink_syntax_seq *seq = NULL;
    struct ink_scratch_buffer *scratch = &parser->scratch;
    size_t scratch_offset = scratch->count;

    while (!ink_parse_check(parser, INK_TT_EOF)) {
        node = ink_parse_stmt(parser);
        ink_scratch_append(scratch, node);
    }

    seq = ink_seq_from_scratch(scratch, scratch_offset, scratch->count);

    return ink_syntax_node_new(INK_NODE_FILE, NULL, NULL, seq);
}

/**
 * Initialize the state of the parser.
 */
static int ink_parser_initialize(struct ink_parser *parser,
                                 struct ink_source *source)
{
    struct ink_syntax_node **scratch;
    size_t scratch_capacity = INK_SCRATCH_MIN_COUNT * sizeof(scratch);

    scratch = malloc(scratch_capacity);
    if (scratch == NULL)
        return -1;

    parser->lexer.source = source;
    parser->lexer.start_offset = 0;
    parser->lexer.cursor_offset = 0;
    parser->scratch.count = 0;
    parser->scratch.capacity = INK_SCRATCH_MIN_COUNT;
    parser->scratch.entries = scratch;

    ink_parse_next(parser);
    return 0;
}

/**
 * Clean up the state of the parser.
 */
static void ink_parser_cleanup(struct ink_parser *parser)
{
    free(parser->scratch.entries);
    memset(parser, 0, sizeof(*parser));
}

/**
 * Parse a source file and output a syntax tree.
 */
int ink_parse(struct ink_source *source, struct ink_syntax_node **tree)
{
    struct ink_parser parser;

    if (ink_parser_initialize(&parser, source) < 0)
        return -1;

    *tree = ink_parse_file(&parser);

    ink_parser_cleanup(&parser);
    return 0;
}
