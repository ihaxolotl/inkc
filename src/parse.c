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

enum ink_precedence {
    INK_PREC_NONE = 0,
    INK_PREC_ASSIGN,
    INK_PREC_LOGICAL_OR,
    INK_PREC_LOGICAL_AND,
    INK_PREC_COMPARISON,
    INK_PREC_TERM,
    INK_PREC_FACTOR,
};

enum ink_parse_context_type {
    INK_PARSE_CONTENT,
    INK_PARSE_EXPRESSION,
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
    bool panic_mode;
    size_t current_token;
    size_t context_depth;
    struct ink_parse_context context_stack[INK_PARSE_DEPTH];
};

static enum ink_syntax_node_type ink_token_prefix_type(enum ink_token_type type)
{
    switch (type) {
    case INK_TT_KEYWORD_NOT:
    case INK_TT_BANG:
        return INK_NODE_NOT_EXPR;
    case INK_TT_MINUS:
        return INK_NODE_NEGATE_EXPR;
    default:
        return INK_NODE_INVALID;
    }
}

static enum ink_syntax_node_type ink_token_infix_type(enum ink_token_type type)
{
    switch (type) {
    case INK_TT_AMP_AMP:
    case INK_TT_KEYWORD_AND:
        return INK_NODE_AND_EXPR;
    case INK_TT_PIPE_PIPE:
    case INK_TT_KEYWORD_OR:
        return INK_NODE_OR_EXPR;
    case INK_TT_PERCENT:
    case INK_TT_KEYWORD_MOD:
        return INK_NODE_MOD_EXPR;
    case INK_TT_PLUS:
        return INK_NODE_ADD_EXPR;
    case INK_TT_MINUS:
        return INK_NODE_SUB_EXPR;
    case INK_TT_STAR:
        return INK_NODE_MUL_EXPR;
    case INK_TT_SLASH:
        return INK_NODE_DIV_EXPR;
    case INK_TT_EQUAL:
        return INK_NODE_ASSIGN_EXPR;
    case INK_TT_EQUAL_EQUAL:
        return INK_NODE_EQUAL_EXPR;
    case INK_TT_BANG_EQUAL:
        return INK_NODE_NOT_EQUAL_EXPR;
    case INK_TT_LESS_THAN:
        return INK_NODE_LESS_EXPR;
    case INK_TT_GREATER_THAN:
        return INK_NODE_GREATER_EXPR;
    case INK_TT_LESS_EQUAL:
        return INK_NODE_LESS_EQUAL_EXPR;
    case INK_TT_GREATER_EQUAL:
        return INK_NODE_GREATER_EQUAL_EXPR;
    default:
        return INK_NODE_INVALID;
    }
}

static enum ink_precedence ink_binding_power(enum ink_token_type type)
{
    switch (type) {
    case INK_TT_AMP_AMP:
    case INK_TT_KEYWORD_AND:
        return INK_PREC_LOGICAL_AND;
    case INK_TT_PIPE_PIPE:
    case INK_TT_KEYWORD_OR:
        return INK_PREC_LOGICAL_OR;
    case INK_TT_EQUAL_EQUAL:
    case INK_TT_BANG_EQUAL:
    case INK_TT_LESS_EQUAL:
    case INK_TT_LESS_THAN:
    case INK_TT_GREATER_EQUAL:
    case INK_TT_GREATER_THAN:
        return INK_PREC_COMPARISON;
    case INK_TT_PLUS:
    case INK_TT_MINUS:
        return INK_PREC_TERM;
    case INK_TT_STAR:
    case INK_TT_SLASH:
    case INK_TT_PERCENT:
    case INK_TT_KEYWORD_MOD:
        return INK_PREC_FACTOR;
    case INK_TT_EQUAL:
        return INK_PREC_ASSIGN;
    default:
        return INK_PREC_NONE;
    }
}

static bool ink_is_sync_token(enum ink_token_type type)
{
    switch (type) {
    case INK_TT_EOF:
    case INK_TT_NL:
    case INK_TT_RIGHT_BRACE:
    case INK_TT_RIGHT_PAREN:
        return true;
    default:
        return false;
    }
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

static enum ink_token_type ink_parser_token_type(struct ink_parser *parser)
{
    struct ink_token *token = ink_parser_current_token(parser);

    return token->type;
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

static const char *ink_parse_context_type_strz(enum ink_parse_context_type type)
{
    const char *str;

    switch (type) {
    case INK_PARSE_CONTENT:
        str = "Content";
        break;
    case INK_PARSE_EXPRESSION:
        str = "Expression";
        break;
    case INK_PARSE_BRACE:
        str = "Brace";
        break;
    default:
        str = "Invalid";
        break;
    }
    return str;
}

static void ink_parser_push_context(struct ink_parser *parser,
                                    enum ink_parse_context_type type)
{
    const char *context_type_str;
    struct ink_parse_context *context;

    assert(parser->context_depth < INK_PARSE_DEPTH);

    context_type_str = ink_parse_context_type_strz(type);

    printf("%*s Pushing new %s context! (start: %zu)\n",
           (int)(parser->context_depth++ * 2), "", context_type_str,
           parser->current_token);

    context = &parser->context_stack[parser->context_depth];
    context->type = type;
    context->token_index = parser->current_token;
}

static void ink_parser_pop_context(struct ink_parser *parser)
{
    const char *context_type_str;
    struct ink_parse_context *context = ink_parser_context(parser);

    assert(parser->context_depth != 0);

    context_type_str = ink_parse_context_type_strz(context->type);

    parser->context_depth--;

    printf("%*s Popping old %s context! (start: %zu)\n",
           (int)(parser->context_depth * 2), "", context_type_str,
           context->token_index);
}

static void *ink_emit_error(struct ink_parser *parser, const char *fmt, ...)
{
    va_list vargs;

    va_start(vargs, fmt);
    vfprintf(stderr, fmt, vargs);
    va_end(vargs);

    ink_token_print(parser->lexer.source, ink_parser_current_token(parser));
    printf("Tokens:\n");
    ink_token_buffer_print(parser->lexer.source, parser->tokens);

    exit(EXIT_FAILURE);

    return NULL;
}

/**
 * Advance the parser by retrieving the next token from the lexer.
 */
static size_t ink_parser_advance(struct ink_parser *parser)
{
    struct ink_parse_context *context;
    size_t token_index = parser->current_token;

    if (!ink_parser_check(parser, INK_TT_EOF)) {
        context = ink_parser_context(parser);
        token_index = ++parser->current_token;

        if (context->type == INK_PARSE_EXPRESSION) {
            while (ink_parser_check(parser, INK_TT_WHITESPACE)) {
                if (token_index < parser->tokens->count) {
                    token_index = ++parser->current_token;
                } else {
                    ink_parser_next_token(parser);
                }
            }
        } else {
            if (token_index >= parser->tokens->count) {
                ink_parser_next_token(parser);
            }
        }
    }
    return token_index;
}

static size_t ink_parser_expect(struct ink_parser *parser,
                                enum ink_token_type type)
{
    size_t token_index = ink_parser_token_index(parser);

    if (!ink_parser_check(parser, type)) {
        parser->panic_mode = true;
        ink_emit_error(parser, "Unexpected token!\n");

        while (!ink_is_sync_token(type)) {
            token_index = ink_parser_advance(parser);
        }
        return token_index;
    }

    ink_parser_advance(parser);
    return token_index;
}

/**
 * TODO(Brett): Perhaps we could add an early return to ignore any
 * UTF-8 encoded byte sequence, as no reserved words contain such things.
 */
static enum ink_token_type ink_parser_keyword(struct ink_parser *parser,
                                              const struct ink_token *token)
{
    const unsigned char *source = parser->lexer.source->bytes;
    const unsigned char *lexeme = source + token->start_offset;
    size_t length = token->end_offset - token->start_offset;
    enum ink_token_type type = token->type;

    switch (length) {
    case 2:
        if (memcmp(lexeme, "or", length) == 0) {
            type = INK_TT_KEYWORD_OR;
        }
        break;
    case 3:
        if (memcmp(lexeme, "and", length) == 0) {
            type = INK_TT_KEYWORD_AND;
        } else if (memcmp(lexeme, "mod", length) == 0) {
            type = INK_TT_KEYWORD_MOD;
        } else if (memcmp(lexeme, "not", length) == 0) {
            type = INK_TT_KEYWORD_NOT;
        }
        break;
    case 4:
        if (memcmp(lexeme, "true", length) == 0) {
            type = INK_TT_KEYWORD_TRUE;
        }
        break;
    case 5:
        if (memcmp(lexeme, "false", length) == 0) {
            type = INK_TT_KEYWORD_FALSE;
        }
        break;
    }
    return type;
}

static bool ink_parser_try_keyword(struct ink_parser *parser,
                                   enum ink_token_type type)
{
    struct ink_token *token = ink_parser_current_token(parser);
    enum ink_token_type keyword_type = ink_parser_keyword(parser, token);

    if (keyword_type == type) {
        token->type = type;
        return true;
    }
    return false;
}

static struct ink_syntax_node *
ink_syntax_node_leaf(struct ink_parser *parser, enum ink_syntax_node_type type,
                     size_t token_start, size_t token_end)
{
    return ink_syntax_node_new(parser->arena, type, token_start, token_end,
                               NULL, NULL, NULL);
}

static struct ink_syntax_node *
ink_syntax_node_unary(struct ink_parser *parser, enum ink_syntax_node_type type,
                      size_t token_start, size_t token_end,
                      struct ink_syntax_node *lhs)
{
    return ink_syntax_node_new(parser->arena, type, token_start, token_end, lhs,
                               NULL, NULL);
}

static struct ink_syntax_node *
ink_syntax_node_binary(struct ink_parser *parser,
                       enum ink_syntax_node_type type, size_t token_start,
                       size_t token_end, struct ink_syntax_node *lhs,
                       struct ink_syntax_node *rhs)
{
    return ink_syntax_node_new(parser->arena, type, token_start, token_end, lhs,
                               rhs, NULL);
}

static struct ink_syntax_node *
ink_syntax_node_sequence(struct ink_parser *parser,
                         enum ink_syntax_node_type type, size_t token_start,
                         size_t token_end, struct ink_syntax_seq *sequence)
{
    return ink_syntax_node_new(parser->arena, type, token_start, token_end,
                               NULL, NULL, sequence);
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

    ink_scratch_initialize(&parser->scratch);

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

static struct ink_syntax_node *ink_parse_true(struct ink_parser *);
static struct ink_syntax_node *ink_parse_false(struct ink_parser *);
static struct ink_syntax_node *ink_parse_number(struct ink_parser *);
static struct ink_syntax_node *ink_parse_string(struct ink_parser *);
static struct ink_syntax_node *ink_parse_sequence_expr(struct ink_parser *);
static struct ink_syntax_node *ink_parse_brace_expr(struct ink_parser *);
static struct ink_syntax_node *ink_parse_content_string(struct ink_parser *);
static struct ink_syntax_node *ink_parse_content_expr(struct ink_parser *);
static struct ink_syntax_node *ink_parse_content_stmt(struct ink_parser *);
static struct ink_syntax_node *ink_parse_primary_expr(struct ink_parser *);
static struct ink_syntax_node *ink_parse_prefix_expr(struct ink_parser *);
static struct ink_syntax_node *ink_parse_infix_expr(struct ink_parser *,
                                                    struct ink_syntax_node *lhs,
                                                    enum ink_precedence prec);
static struct ink_syntax_node *ink_parse_expr(struct ink_parser *);

static struct ink_syntax_node *ink_parse_true(struct ink_parser *parser)
{
    size_t token_start = ink_parser_expect(parser, INK_TT_KEYWORD_TRUE);

    return ink_syntax_node_leaf(parser, INK_NODE_TRUE_EXPR, token_start,
                                token_start);
}

static struct ink_syntax_node *ink_parse_false(struct ink_parser *parser)
{
    size_t token_start = ink_parser_expect(parser, INK_TT_KEYWORD_FALSE);

    return ink_syntax_node_leaf(parser, INK_NODE_FALSE_EXPR, token_start,
                                token_start);
}

static struct ink_syntax_node *ink_parse_number(struct ink_parser *parser)
{
    size_t token_start = ink_parser_expect(parser, INK_TT_NUMBER);

    return ink_syntax_node_leaf(parser, INK_NODE_NUMBER_EXPR, token_start,
                                token_start);
}

static struct ink_syntax_node *ink_parse_string(struct ink_parser *parser)
{
    size_t token_start = ink_parser_expect(parser, INK_TT_STRING);

    return ink_syntax_node_leaf(parser, INK_NODE_STRING_EXPR, token_start,
                                token_start);
}

static struct ink_syntax_node *ink_parse_primary_expr(struct ink_parser *parser)
{
    size_t token_index;
    struct ink_syntax_node *lhs;
    enum ink_token_type type = ink_parser_token_type(parser);

    switch (type) {
    /*
    case INK_TT_IDENTIFIER:
        return parse_name_expr(parser);
    */
    case INK_TT_NUMBER:
        return ink_parse_number(parser);
    case INK_TT_STRING:
        if (ink_parser_try_keyword(parser, INK_TT_KEYWORD_TRUE)) {
            return ink_parse_true(parser);
        }
        if (ink_parser_try_keyword(parser, INK_TT_KEYWORD_FALSE)) {
            return ink_parse_false(parser);
        }
        return ink_parse_string(parser);
    case INK_TT_LEFT_PAREN:
        ink_parser_advance(parser);
        lhs = ink_parse_expr(parser);
        ink_parser_expect(parser, INK_TT_RIGHT_PAREN);

        return lhs;
    default:
        token_index = ink_parser_token_index(parser);

        return ink_syntax_node_leaf(parser, INK_NODE_INVALID, token_index,
                                    token_index);
    }
}

static struct ink_syntax_node *ink_parse_prefix_expr(struct ink_parser *parser)
{
    size_t token_index;
    struct ink_syntax_node *lhs;
    enum ink_token_type type;

    if (ink_parser_try_keyword(parser, INK_TT_KEYWORD_NOT) ||
        ink_parser_check(parser, INK_TT_MINUS) ||
        ink_parser_check(parser, INK_TT_BANG)) {
        type = ink_parser_token_type(parser);
        token_index = ink_parser_advance(parser);
        lhs = ink_parse_prefix_expr(parser);

        return ink_syntax_node_unary(parser, ink_token_prefix_type(type),
                                     token_index, token_index, lhs);
    }
    return ink_parse_primary_expr(parser);
}

static struct ink_syntax_node *ink_parse_infix_expr(struct ink_parser *parser,
                                                    struct ink_syntax_node *lhs,
                                                    enum ink_precedence prec)
{
    size_t tokid;
    struct ink_syntax_node *rhs;
    enum ink_precedence tokprec;
    enum ink_token_type type;

    if (lhs == 0)
        lhs = ink_parse_prefix_expr(parser);

    ink_parser_try_keyword(parser, INK_TT_KEYWORD_AND) ||
        ink_parser_try_keyword(parser, INK_TT_KEYWORD_OR) ||
        ink_parser_try_keyword(parser, INK_TT_KEYWORD_MOD);

    type = ink_parser_token_type(parser);
    tokprec = ink_binding_power(type);

    while (tokprec > prec) {
        tokid = ink_parser_advance(parser);
        rhs = ink_parse_infix_expr(parser, 0, tokprec);
        lhs = ink_syntax_node_binary(parser, ink_token_infix_type(type), tokid,
                                     tokid, lhs, rhs);

        ink_parser_try_keyword(parser, INK_TT_KEYWORD_AND) ||
            ink_parser_try_keyword(parser, INK_TT_KEYWORD_OR) ||
            ink_parser_try_keyword(parser, INK_TT_KEYWORD_MOD);

        type = ink_parser_token_type(parser);
        tokprec = ink_binding_power(type);
    }
    return lhs;
}

static struct ink_syntax_node *ink_parse_expr(struct ink_parser *parser)
{
    return ink_parse_infix_expr(parser, 0, INK_PREC_NONE);
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
    return ink_syntax_node_leaf(parser, INK_NODE_STRING_EXPR, token_start,
                                ink_parser_token_index(parser) - 1);
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
        ink_parser_push_context(parser, INK_PARSE_EXPRESSION);
        tmp_node = ink_parse_expr(parser);
        ink_parser_pop_context(parser);

        return tmp_node;
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

    return ink_syntax_node_sequence(parser, INK_NODE_SEQUENCE_EXPR, token_start,
                                    ink_parser_token_index(parser), seq);
}

static struct ink_syntax_node *ink_parse_brace_expr(struct ink_parser *parser)
{
    struct ink_syntax_node *lhs = NULL;
    size_t token_start = ink_parser_token_index(parser);

    ink_parser_advance(parser);
    ink_parser_push_context(parser, INK_PARSE_BRACE);
    lhs = ink_parse_sequence_expr(parser);
    ink_parser_pop_context(parser);
    ink_parser_expect(parser, INK_TT_RIGHT_BRACE);

    return ink_syntax_node_unary(parser, INK_NODE_BRACE_EXPR, token_start,
                                 ink_parser_token_index(parser), lhs);
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

    return ink_syntax_node_sequence(parser, INK_NODE_CONTENT_EXPR, token_start,
                                    ink_parser_token_index(parser), seq);
}

static struct ink_syntax_node *ink_parse_content_stmt(struct ink_parser *parser)
{
    struct ink_syntax_node *lhs = NULL;
    size_t token_start = ink_parser_token_index(parser);

    lhs = ink_parse_content_expr(parser);

    if (!ink_parser_check(parser, INK_TT_EOF) &&
        !ink_parser_check(parser, INK_TT_NL)) {
        ink_emit_error(parser, "Expected new line!\n");
    }

    ink_parser_advance(parser);

    return ink_syntax_node_unary(parser, INK_NODE_CONTENT_STMT, token_start,
                                 ink_parser_token_index(parser), lhs);
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

    return ink_syntax_node_sequence(parser, INK_NODE_FILE, token_start,
                                    ink_parser_token_index(parser), seq);
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
