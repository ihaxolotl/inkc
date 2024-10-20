#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "lex.h"
#include "parse.h"
#include "tree.h"

#define INK_PARSE_DEPTH 128

struct ink_parser {
    struct ink_arena *arena;
    struct ink_syntax_tree *tree;
    struct ink_lexer lexer;
    struct ink_token current_token;
    struct ink_scratch_buffer scratch;
};

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

    return ink_syntax_node_new(parser->arena, INK_NODE_CONTENT_EXPR, NULL, NULL,
                               NULL);
}

static struct ink_syntax_node *ink_parse_content_stmt(struct ink_parser *parser)
{
    struct ink_arena *arena = parser->arena;
    struct ink_scratch_buffer *scratch = &parser->scratch;
    struct ink_syntax_node *node = NULL;
    struct ink_syntax_seq *seq = NULL;
    size_t scratch_offset = scratch->count;

    while (!ink_parse_check(parser, INK_TT_EOF) &&
           !ink_parse_check(parser, INK_TT_NL)) {
        node = ink_parse_content_expr(parser);
        ink_scratch_append(scratch, node);
    }

    seq = ink_seq_from_scratch(arena, scratch, scratch_offset, scratch->count);

    return ink_syntax_node_new(arena, INK_NODE_CONTENT_STMT, NULL, NULL, seq);
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
    struct ink_arena *arena = parser->arena;
    struct ink_scratch_buffer *scratch = &parser->scratch;
    struct ink_syntax_node *node = NULL;
    struct ink_syntax_seq *seq = NULL;
    size_t scratch_offset = scratch->count;

    while (!ink_parse_check(parser, INK_TT_EOF)) {
        node = ink_parse_stmt(parser);
        ink_scratch_append(scratch, node);
    }

    seq = ink_seq_from_scratch(arena, scratch, scratch_offset, scratch->count);

    return ink_syntax_node_new(arena, INK_NODE_FILE, NULL, NULL, seq);
}

/**
 * Initialize the state of the parser.
 */
static int ink_parser_initialize(struct ink_parser *parser,
                                 struct ink_source *source,
                                 struct ink_syntax_tree *tree,
                                 struct ink_arena *arena)
{
    parser->arena = arena;
    parser->tree = tree;
    parser->lexer.source = source;
    parser->lexer.start_offset = 0;
    parser->lexer.cursor_offset = 0;

    ink_parse_next(parser);
    return 0;
}

/**
 * Clean up the state of the parser.
 */
static void ink_parser_cleanup(struct ink_parser *parser)
{
    memset(parser, 0, sizeof(*parser));
}

/**
 * Parse a source file and output a syntax tree.
 */
int ink_parse(struct ink_arena *arena, struct ink_source *source,
              struct ink_syntax_tree *syntax_tree)
{
    struct ink_parser parser;
    struct ink_syntax_node *root;

    if (ink_syntax_tree_initialize(source, syntax_tree) < 0)
        return -1;
    if (ink_parser_initialize(&parser, source, syntax_tree, arena) < 0)
        goto err;

    root = ink_parse_file(&parser);
    if (root == NULL)
        goto err;

    syntax_tree->root = root;

    ink_parser_cleanup(&parser);
    return 0;

err:
    ink_syntax_tree_cleanup(syntax_tree);
    ink_parser_cleanup(&parser);
    return -1;
}
