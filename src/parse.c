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
    struct ink_token_stream *tokens;
    struct ink_lexer lexer;
    struct ink_token current_token;
    struct ink_scratch_buffer scratch;
};

/**
 * Check if the current token matches a specified type.
 */
static bool ink_parser_check(struct ink_parser *parser,
                             enum ink_token_type type)
{
    return parser->current_token.type == type;
}

/**
 * Retrieve the next token from the lexer.
 */
static void ink_parser_next(struct ink_parser *parser)
{
    if (!ink_parser_check(parser, INK_TT_EOF)) {
        ink_token_next(&parser->lexer, &parser->current_token);
        ink_token_stream_append(parser->tokens, parser->current_token);
    }
}

/**
 * Retrieve the index of the current token.
 */
static size_t ink_parser_token_index(struct ink_parser *parser)
{
    size_t count = parser->tokens->count;

    if (count == 0)
        return 0;

    return count - 1;
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
    parser->tokens = &tree->tokens;
    parser->lexer.source = source;
    parser->lexer.start_offset = 0;
    parser->lexer.cursor_offset = 0;
    parser->current_token.type = INK_TT_ERROR;
    parser->current_token.start_offset = 0;
    parser->current_token.end_offset = 0;

    ink_parser_next(parser);
    return 0;
}

/**
 * Clean up the state of the parser.
 */
static void ink_parser_cleanup(struct ink_parser *parser)
{
    memset(parser, 0, sizeof(*parser));
}

static struct ink_syntax_node *ink_parse_content_expr(struct ink_parser *parser)
{
    size_t main_token = ink_parser_token_index(parser);

    ink_parser_next(parser);

    return ink_syntax_node_new(parser->arena, INK_NODE_CONTENT_EXPR, NULL, NULL,
                               main_token, NULL);
}

static struct ink_syntax_node *ink_parse_content_stmt(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    struct ink_syntax_seq *seq = NULL;
    struct ink_arena *arena = parser->arena;
    struct ink_scratch_buffer *scratch = &parser->scratch;
    size_t scratch_offset = scratch->count;
    size_t main_token = ink_parser_token_index(parser);

    while (!ink_parser_check(parser, INK_TT_EOF) &&
           !ink_parser_check(parser, INK_TT_NL)) {
        node = ink_parse_content_expr(parser);
        ink_scratch_append(scratch, node);
    }

    seq = ink_seq_from_scratch(arena, scratch, scratch_offset, scratch->count);

    return ink_syntax_node_new(arena, INK_NODE_CONTENT_STMT, NULL, NULL,
                               main_token, seq);
}

static struct ink_syntax_node *ink_parse_stmt(struct ink_parser *parser)
{
    struct ink_syntax_node *stmt = ink_parse_content_stmt(parser);

    while (ink_parser_check(parser, INK_TT_NL))
        ink_parser_next(parser);

    return stmt;
}

static struct ink_syntax_node *ink_parse_file(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    struct ink_syntax_seq *seq = NULL;
    struct ink_arena *arena = parser->arena;
    struct ink_scratch_buffer *scratch = &parser->scratch;
    size_t scratch_offset = scratch->count;
    size_t main_token = ink_parser_token_index(parser);

    while (!ink_parser_check(parser, INK_TT_EOF)) {
        node = ink_parse_stmt(parser);
        ink_scratch_append(scratch, node);
    }

    seq = ink_seq_from_scratch(arena, scratch, scratch_offset, scratch->count);

    return ink_syntax_node_new(arena, INK_NODE_FILE, NULL, NULL, main_token,
                               seq);
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
