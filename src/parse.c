#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "lex.h"
#include "parse.h"
#include "tree.h"

enum ink_parse_context_type {
    INK_PARSE_CONTENT,
    INK_PARSE_BRACE,
};

struct ink_parse_context {
    enum ink_parse_context_type type;
    size_t token_index;
};

struct ink_parser {
    struct ink_arena *arena;
    struct ink_token_buffer *tokens;
    struct ink_lexer lexer;
    struct ink_scratch_buffer scratch;
    size_t current_token;
    size_t context_depth;
    struct ink_parse_context context_stack[INK_PARSE_DEPTH];
};

static void *ink_error_emit(struct ink_parser *parser, const char *fmt, ...)
{
    va_list vargs;

    va_start(vargs, fmt);
    vfprintf(stderr, fmt, vargs);
    va_end(vargs);

    exit(EXIT_FAILURE);

    return NULL;
}

/**
 * Retrieve the index of the current token.
 */
static size_t ink_parser_token_index(struct ink_parser *parser)
{
    return parser->current_token;
}

static struct ink_token *ink_parser_current_token(struct ink_parser *parser)
{
    return &parser->tokens->entries[parser->current_token];
}

static void ink_parser_next_token(struct ink_parser *parser)
{
    struct ink_token next;

    ink_token_next(&parser->lexer, &next);
    ink_token_buffer_append(parser->tokens, next);
}

/**
 * Check if the current token matches a specified type.
 */
static bool ink_parser_check(struct ink_parser *parser,
                             enum ink_token_type type)
{
    struct ink_token *token = ink_parser_current_token(parser);

    return token->type == type;
}

static struct ink_parse_context *ink_parser_context(struct ink_parser *parser)
{
    return &parser->context_stack[parser->context_depth];
}

/**
 * Rewind to the parser's previous state.
 */
static void ink_parser_rewind_context(struct ink_parser *parser)
{
    struct ink_parse_context *context =
        &parser->context_stack[parser->context_depth];

    parser->current_token = context->token_index;
}

static void ink_parser_push_context(struct ink_parser *parser,
                                    enum ink_parse_context_type type)
{
    struct ink_parse_context *context;

    assert(parser->context_depth < INK_PARSE_DEPTH);
    printf("%*s Pushing new context! (start: %zu)\n",
           (int)(parser->context_depth++ * 2), "", parser->current_token);

    context = &parser->context_stack[parser->context_depth];
    context->type = type;
    context->token_index = parser->current_token;
}

static void ink_parser_pop_context(struct ink_parser *parser)
{
    struct ink_parse_context *context = ink_parser_context(parser);

    assert(parser->context_depth != 0);

    parser->context_depth--;

    printf("%*s Popping old context! (start: %zu)\n",
           (int)(parser->context_depth * 2), "", context->token_index);
}

/**
 * Advance the parser by retrieving the next token from the lexer.
 */
static void ink_parser_advance(struct ink_parser *parser)
{
    if (!ink_parser_check(parser, INK_TT_EOF)) {
        parser->current_token++;

        if (parser->current_token < parser->tokens->count) {
            return;
        }
        ink_parser_next_token(parser);
    }
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
    parser->current_token = 0;
    parser->context_depth = 0;
    parser->context_stack[0].token_index = 0;
    parser->context_stack[0].type = INK_PARSE_CONTENT;

    return 0;
}

/**
 * Clean up the state of the parser.
 */
static void ink_parser_cleanup(struct ink_parser *parser)
{
    ink_scratch_cleanup(&parser->scratch);
    memset(parser, 0, sizeof(*parser));
}

static struct ink_syntax_node *ink_parse_number(struct ink_parser *);
static struct ink_syntax_node *ink_parse_primary_expr(struct ink_parser *);
static struct ink_syntax_node *ink_parse_sequence_expr(struct ink_parser *);
static struct ink_syntax_node *ink_parse_brace_expr(struct ink_parser *);
static struct ink_syntax_node *ink_parse_content_string(struct ink_parser *);
static struct ink_syntax_node *ink_parse_content_expr(struct ink_parser *);
static struct ink_syntax_node *ink_parse_content_stmt(struct ink_parser *);

static struct ink_syntax_node *ink_parse_number(struct ink_parser *parser)
{
    size_t token_start = ink_parser_token_index(parser);

    return ink_syntax_node_new(parser->arena, INK_NODE_NUMBER_LITERAL,
                               token_start, token_start, NULL, NULL, NULL);
}

static struct ink_syntax_node *ink_parse_primary_expr(struct ink_parser *parser)
{
    struct ink_syntax_node *lhs = ink_parse_number(parser);

    ink_parser_advance(parser);

    return lhs;
}

static struct ink_syntax_node *
ink_parse_content_string(struct ink_parser *parser)
{
    size_t token_start = ink_parser_token_index(parser);
    struct ink_parse_context *context = ink_parser_context(parser);

    while (!ink_parser_check(parser, INK_TT_EOF) &&
           !ink_parser_check(parser, INK_TT_NL)) {
        if (context->type == INK_PARSE_BRACE) {
            if (ink_parser_check(parser, INK_TT_PIPE) ||
                ink_parser_check(parser, INK_TT_RIGHT_BRACE)) {
                break;
            }
        }
        if (ink_parser_check(parser, INK_TT_LEFT_BRACE)) {
            break;
        }
        ink_parser_advance(parser);
    }
    return ink_syntax_node_new(parser->arena, INK_NODE_STRING_LITERAL,
                               token_start, ink_parser_token_index(parser) - 1,
                               NULL, NULL, NULL);
}

static struct ink_syntax_node *
ink_parse_sequence_expr(struct ink_parser *parser)
{
    struct ink_syntax_node *tmp_node = NULL;
    struct ink_syntax_seq *seq = NULL;
    struct ink_scratch_buffer *scratch = &parser->scratch;
    size_t scratch_offset = scratch->count;
    size_t token_start = ink_parser_token_index(parser);

    tmp_node = ink_parse_content_expr(parser);

    if (!ink_parser_check(parser, INK_TT_PIPE)) {
        /* Backtrack, parse primary_expr */
        ink_parser_rewind_context(parser);

        return ink_parse_primary_expr(parser);
    } else {
        ink_scratch_append(scratch, tmp_node);

        while (!ink_parser_check(parser, INK_TT_EOF) &&
               !ink_parser_check(parser, INK_TT_NL) &&
               !ink_parser_check(parser, INK_TT_RIGHT_BRACE)) {
            if (ink_parser_check(parser, INK_TT_PIPE)) {
                ink_parser_advance(parser);
            }

            tmp_node = ink_parse_content_expr(parser);
            ink_scratch_append(scratch, tmp_node);
        }
    }

    seq = ink_seq_from_scratch(parser->arena, scratch, scratch_offset,
                               scratch->count);

    return ink_syntax_node_new(parser->arena, INK_NODE_SEQUENCE_EXPR,
                               token_start, ink_parser_token_index(parser),
                               NULL, NULL, seq);
}

static struct ink_syntax_node *ink_parse_brace_expr(struct ink_parser *parser)
{
    struct ink_syntax_node *lhs = NULL;
    size_t token_start = ink_parser_token_index(parser);

    ink_parser_advance(parser);
    ink_parser_push_context(parser, INK_PARSE_BRACE);
    lhs = ink_parse_sequence_expr(parser);
    ink_parser_pop_context(parser);

    if (!ink_parser_check(parser, INK_TT_RIGHT_BRACE)) {
        ink_error_emit(parser, "Expected a closing curly brace!\n");
    }

    ink_parser_advance(parser);

    return ink_syntax_node_new(parser->arena, INK_NODE_BRACE_EXPR, token_start,
                               ink_parser_token_index(parser), lhs, NULL, NULL);
}

static struct ink_syntax_node *ink_parse_content_expr(struct ink_parser *parser)
{
    struct ink_syntax_node *tmp_node = NULL;
    struct ink_syntax_seq *seq = NULL;
    struct ink_parse_context *context = ink_parser_context(parser);
    struct ink_scratch_buffer *scratch = &parser->scratch;
    size_t scratch_offset = scratch->count;
    size_t token_start = ink_parser_token_index(parser);

    while (!ink_parser_check(parser, INK_TT_EOF) &&
           !ink_parser_check(parser, INK_TT_NL)) {
        if (context->type == INK_PARSE_BRACE) {
            if (ink_parser_check(parser, INK_TT_RIGHT_BRACE) ||
                ink_parser_check(parser, INK_TT_PIPE)) {
                break;
            }
        }
        if (ink_parser_check(parser, INK_TT_LEFT_BRACE)) {
            tmp_node = ink_parse_brace_expr(parser);
        } else {
            tmp_node = ink_parse_content_string(parser);
        }

        ink_scratch_append(scratch, tmp_node);
    }

    seq = ink_seq_from_scratch(parser->arena, scratch, scratch_offset,
                               scratch->count);

    return ink_syntax_node_new(parser->arena, INK_NODE_CONTENT_EXPR,
                               token_start, ink_parser_token_index(parser),
                               NULL, NULL, seq);
}

static struct ink_syntax_node *ink_parse_content_stmt(struct ink_parser *parser)
{
    struct ink_syntax_node *lhs = NULL;
    size_t token_start = ink_parser_token_index(parser);

    lhs = ink_parse_content_expr(parser);

    if (!ink_parser_check(parser, INK_TT_EOF) &&
        !ink_parser_check(parser, INK_TT_NL)) {
        ink_error_emit(parser, "Expected new line!\n");
    }

    ink_parser_advance(parser);

    return ink_syntax_node_new(parser->arena, INK_NODE_CONTENT_STMT,
                               token_start, ink_parser_token_index(parser), lhs,
                               NULL, NULL);
}

static struct ink_syntax_node *ink_parse_stmt(struct ink_parser *parser)
{
    struct ink_syntax_node *stmt = ink_parse_content_stmt(parser);

    while (ink_parser_check(parser, INK_TT_NL)) {
        ink_parser_advance(parser);
    }
    return stmt;
}

static struct ink_syntax_node *ink_parse_file(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    struct ink_syntax_seq *seq = NULL;
    struct ink_scratch_buffer *scratch = &parser->scratch;
    size_t scratch_offset = scratch->count;
    size_t token_start = ink_parser_token_index(parser);

    while (!ink_parser_check(parser, INK_TT_EOF)) {
        node = ink_parse_stmt(parser);
        ink_scratch_append(scratch, node);
    }

    seq = ink_seq_from_scratch(parser->arena, scratch, scratch_offset,
                               scratch->count);

    return ink_syntax_node_new(parser->arena, INK_NODE_FILE, token_start,
                               ink_parser_token_index(parser), NULL, NULL, seq);
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

    ink_parser_next_token(&parser);

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
