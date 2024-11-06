#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "lex.h"
#include "logging.h"
#include "parse.h"
#include "platform.h"
#include "tree.h"
#include "util.h"

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
    INK_PARSE_CHOICE,
};

struct ink_parse_context {
    enum ink_parse_context_type type;
    const enum ink_token_type *delims;
    size_t token_index;
};

/**
 * Scratch buffer for syntax tree nodes.
 *
 * Used to assist in the creation of syntax tree node sequences, avoiding the
 * need for a dynamic array.
 */
struct ink_scratch_buffer {
    size_t count;
    size_t capacity;
    struct ink_syntax_node **entries;
};

/**
 * Parsing state.
 */
struct ink_parser {
    /* Memory arena for syntax tree nodes */
    struct ink_arena *arena;

    /* Tokenized buffer */
    struct ink_token_buffer *tokens;

    /* Tokenizer state */
    struct ink_lexer lexer;

    /* Scratch storage for intermediate parsing results. */
    struct ink_scratch_buffer scratch;

    /* Flag to control error handling */
    bool panic_mode;

    /* Flag to control tracing output */
    bool tracing;

    /* Stolen from CPython. May adjust. */
    int pending;

    /* Index of the current token within the tokenized buffer */
    size_t token_index;
    size_t context_depth;
    size_t level_depth;
    size_t level_stack[INK_PARSE_DEPTH];
    struct ink_parse_context context_stack[INK_PARSE_DEPTH];
};

/**
 * Table of parsing context type descriptions.
 */
static const char *INK_CONTEXT_TYPE_STR[] = {
    [INK_PARSE_CONTENT] = "Content",
    [INK_PARSE_EXPRESSION] = "Expression",
    [INK_PARSE_BRACE] = "Brace",
    [INK_PARSE_CHOICE] = "Choice",
};

/**
 * Delimiters for content strings within the default parsing context.
 */
static const enum ink_token_type INK_CONTENT_DELIMS[] = {
    INK_TT_LEFT_BRACE,
    INK_TT_RIGHT_BRACE,
    INK_TT_EOF,
};

/**
 * Delimiters for content strings within the parsing context for expressions.
 */
static const enum ink_token_type INK_EXPRESSION_DELIMS[] = {
    INK_TT_EOF,
};

/**
 * Delimiters for content strings within the parsing context for string
 * interpolations.
 */
static const enum ink_token_type INK_BRACE_DELIMS[] = {
    INK_TT_LEFT_BRACE,
    INK_TT_RIGHT_BRACE,
    INK_TT_PIPE,
    INK_TT_EOF,
};

/**
 * Delimiters for content strings within the parsing context for choice
 * branches.
 */
static const enum ink_token_type INK_CHOICE_DELIMS[] = {
    INK_TT_LEFT_BRACE,    INK_TT_RIGHT_BRACE, INK_TT_LEFT_BRACKET,
    INK_TT_RIGHT_BRACKET, INK_TT_EOF,
};

static void ink_scratch_initialize(struct ink_scratch_buffer *scratch)
{
    scratch->count = 0;
    scratch->capacity = 0;
    scratch->entries = NULL;
}

/**
 * Reserve scratch space for a specified number of items.
 */
static int ink_scratch_reserve(struct ink_scratch_buffer *scratch,
                               size_t item_count)
{
    struct ink_syntax_node **entries = scratch->entries;
    const size_t old_capacity = scratch->capacity * sizeof(entries);
    const size_t new_capacity = item_count * sizeof(entries);

    entries = platform_mem_realloc(entries, old_capacity, new_capacity);
    if (entries == NULL) {
        scratch->entries = NULL;
        return -1;
    }

    scratch->count = scratch->count;
    scratch->capacity = item_count;
    scratch->entries = entries;

    return 0;
}

/**
 * Shrink the parser's scratch storage down to a specified size.
 *
 * Re-allocation is not performed here. Therefore, subsequent allocations
 * are amortized.
 */
static void ink_scratch_shrink(struct ink_scratch_buffer *scratch, size_t count)
{
    assert(count <= scratch->capacity);

    scratch->count = count;
}

/**
 * Append a syntax tree node to the parser's scratch storage.
 *
 * These nodes can be retrieved later for creating sequences.
 */
static void ink_scratch_append(struct ink_scratch_buffer *scratch,
                               struct ink_syntax_node *node)
{
    size_t capacity, old_size, new_size;

    if (scratch->count + 1 > scratch->capacity) {
        if (scratch->capacity < INK_SCRATCH_MIN_COUNT) {
            capacity = INK_SCRATCH_MIN_COUNT;
        } else {
            capacity = scratch->capacity * INK_SCRATCH_GROWTH_FACTOR;
        }

        old_size = scratch->capacity * sizeof(scratch->entries);
        new_size = capacity * sizeof(scratch->entries);

        scratch->entries =
            platform_mem_realloc(scratch->entries, old_size, new_size);
        scratch->capacity = capacity;
    }

    scratch->entries[scratch->count++] = node;
}

/**
 * Release memory for scratch storage.
 */
static void ink_scratch_cleanup(struct ink_scratch_buffer *scratch)
{
    const size_t mem_size = sizeof(scratch->entries) * scratch->capacity;

    platform_mem_dealloc(scratch->entries, mem_size);
}

/**
 * Create a syntax tree node sequence.
 */
static struct ink_syntax_seq *
ink_syntax_seq_new(struct ink_arena *arena, struct ink_scratch_buffer *scratch,
                   size_t start_offset, size_t end_offset)
{
    struct ink_syntax_seq *seq;
    size_t seq_index = 0;
    const size_t span = end_offset - start_offset;

    assert(span > 0);

    seq = ink_arena_allocate(arena, sizeof(*seq) + span * sizeof(seq->nodes));
    if (seq == NULL)
        return NULL;

    seq->count = span;

    for (size_t i = start_offset; i < end_offset; i++) {
        seq->nodes[seq_index] = scratch->entries[i];
        seq_index++;
    }
    return seq;
}

static struct ink_syntax_seq *
ink_seq_from_scratch(struct ink_arena *arena,
                     struct ink_scratch_buffer *scratch, size_t start_offset,
                     size_t end_offset)
{
    struct ink_syntax_seq *seq = NULL;

    if (start_offset < end_offset) {
        seq = ink_syntax_seq_new(arena, scratch, start_offset, end_offset);
        ink_scratch_shrink(scratch, start_offset);
    }
    return seq;
}

/**
 * Return a description of the current parsing context.
 */
static inline const char *
ink_parse_context_type_strz(enum ink_parse_context_type type)
{
    return INK_CONTEXT_TYPE_STR[type];
}

/**
 * Return the syntax node type for a prefix expression.
 */
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

/**
 * Retun the syntax node type for an infix expression.
 */
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

/**
 * Return the binding power of an operator.
 */
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

/**
 * Determine if a token can be used to recover the parsing state during
 * error handling.
 */
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
 * Return the branch type for a choice branch.
 */
static enum ink_syntax_node_type ink_branch_type(enum ink_token_type type)
{
    switch (type) {
    case INK_TT_STAR:
        return INK_NODE_CHOICE_STAR_BRANCH;
    case INK_TT_PLUS:
        return INK_NODE_CHOICE_PLUS_BRANCH;
    default:
        return INK_NODE_INVALID;
    }
}

/**
 * Return the current token.
 */
static inline struct ink_token *
ink_parser_current_token(const struct ink_parser *parser)
{
    return &parser->tokens->entries[parser->token_index];
}

/**
 * Return the type of the current token.
 */
static inline enum ink_token_type
ink_parser_token_type(const struct ink_parser *parser)
{
    const struct ink_token *token = ink_parser_current_token(parser);

    return token->type;
}

/**
 * Retrieve the next token.
 */
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
    const struct ink_token *token = ink_parser_current_token(parser);

    return token->type == type;
}

/**
 * Return the current parsing context.
 */
static struct ink_parse_context *ink_parser_context(struct ink_parser *parser)
{
    return &parser->context_stack[parser->context_depth];
}

/**
 * Print parser tracing information to the console.
 */
static void ink_parser_trace(struct ink_parser *parser, const char *rule_name)
{
    const struct ink_parse_context *context = ink_parser_context(parser);
    const struct ink_token *token = ink_parser_current_token(parser);
    const char *context_type = ink_parse_context_type_strz(context->type);
    const char *token_type = ink_token_type_strz(token->type);

    if (parser->tracing) {
        ink_trace("Entering %s(Context=%s, TokenType=%s, TokenIndex: %zu, "
                  "Level: %zu)",
                  rule_name, context_type, token_type, parser->token_index,
                  parser->level_stack[parser->level_depth]);
    }
}

/**
 * Rewind the parser's state to a previous token index.
 */
static void ink_parser_rewind(struct ink_parser *parser, size_t token_index)
{
    assert(token_index <= parser->token_index);

    if (parser->tracing) {
        ink_trace("Rewinding parser to %zu", token_index);
    }

    parser->token_index = token_index;
}

/**
 * Rewind to the parser's previous state.
 */
static void ink_parser_rewind_context(struct ink_parser *parser)
{
    const struct ink_parse_context *context =
        &parser->context_stack[parser->context_depth];

    ink_parser_rewind(parser, context->token_index);
}

/**
 * Initialize a parsing context.
 */
static void ink_parser_set_context(struct ink_parse_context *context,
                                   enum ink_parse_context_type type,
                                   size_t token_index)
{
    context->type = type;
    context->token_index = token_index;

    switch (context->type) {
    case INK_PARSE_CONTENT:
        context->delims = INK_CONTENT_DELIMS;
        break;
    case INK_PARSE_EXPRESSION:
        context->delims = INK_EXPRESSION_DELIMS;
        break;
    case INK_PARSE_BRACE:
        context->delims = INK_BRACE_DELIMS;
        break;
    case INK_PARSE_CHOICE:
        context->delims = INK_CHOICE_DELIMS;
        break;
    }
}

/**
 * Push a parsing context into the parser's state.
 */
static void ink_parser_push_context(struct ink_parser *parser,
                                    enum ink_parse_context_type type)
{
    const struct ink_token *token = ink_parser_current_token(parser);
    const char *token_type = ink_token_type_strz(token->type);
    const char *context_type = ink_parse_context_type_strz(type);

    assert(parser->context_depth < INK_PARSE_DEPTH);

    parser->context_depth++;

    ink_trace("Pushing new %s context! (TokenType: %s, TokenIndex: %zu)",
              context_type, token_type, parser->token_index);

    ink_parser_set_context(ink_parser_context(parser), type,
                           parser->token_index);
}

/**
 * Pop a parsing context from the parser's state.
 */
static void ink_parser_pop_context(struct ink_parser *parser)
{
    const struct ink_parse_context *context = ink_parser_context(parser);
    const struct ink_token *token = ink_parser_current_token(parser);
    const char *token_type = ink_token_type_strz(token->type);
    const char *context_type = ink_parse_context_type_strz(context->type);

    assert(parser->context_depth != 0);

    parser->context_depth--;

    ink_trace("Popping old %s context! (TokenType: %s, TokenIndex: %zu)",
              context_type, token_type, context->token_index);
}

/**
 * Determine if the current token is a delimiter for content strings within
 * the current parsing context.
 */
static bool ink_context_delim(struct ink_parser *parser)
{
    const struct ink_parse_context *context = ink_parser_context(parser);

    for (size_t i = 0; context->delims[i] != INK_TT_EOF; i++) {
        if (ink_parser_check(parser, context->delims[i])) {
            return true;
        }
    }
    return false;
}

/**
 * Raise an error in the parser.
 */
static void *ink_parser_error(struct ink_parser *parser, const char *format,
                              ...)
{
    va_list vargs;

    va_start(vargs, format);
    ink_log(INK_LOG_LEVEL_ERROR, format, vargs);
    va_end(vargs);

    parser->panic_mode = true;

    ink_token_print(parser->lexer.source, ink_parser_current_token(parser));

    return NULL;
}

/**
 * Advance the parser by retrieving the next token from the lexer.
 */
static size_t ink_parser_advance(struct ink_parser *parser)
{
    while (!ink_parser_check(parser, INK_TT_EOF)) {
        const struct ink_parse_context *context = ink_parser_context(parser);
        const size_t token_index = parser->token_index++;

        if (parser->token_index + 1 > parser->tokens->count) {
            ink_parser_next_token(parser);
        }
        if (context->type == INK_PARSE_EXPRESSION) {
            if (ink_parser_check(parser, INK_TT_WHITESPACE)) {
                continue;
            }
        }
        return token_index;
    }
    return parser->token_index;
}

/**
 * Ignore the current token if it's type matches the specified type.
 */
static bool ink_parser_eat(struct ink_parser *parser, enum ink_token_type type)
{
    if (ink_parser_check(parser, type)) {
        ink_parser_advance(parser);
        return true;
    }
    return false;
}

/**
 * Expect the current token to have the specified type. If not, raise an error.
 */
static size_t ink_parser_expect(struct ink_parser *parser,
                                enum ink_token_type type)
{
    size_t token_index = parser->token_index;
    const struct ink_parse_context *context = ink_parser_context(parser);

    if (context->type == INK_PARSE_EXPRESSION) {
        if (ink_parser_check(parser, INK_TT_WHITESPACE)) {
            token_index = ink_parser_advance(parser);
        }
    }
    if (!ink_parser_check(parser, type)) {
        ink_parser_error(parser, "Unexpected token!");

        do {
            token_index = ink_parser_advance(parser);
            type = ink_parser_token_type(parser);
        } while (!ink_is_sync_token(type));

        return token_index;
    }

    ink_parser_advance(parser);

    return token_index;
}

/**
 * Return a token type representing a keyword that the specified token
 * represents. If the token's type does not correspond to a keyword,
 * the token's type will be returned instead.
 *
 * TODO(Brett): Perhaps we could add an early return to ignore any
 * UTF-8 encoded byte sequence, as no reserved words contain such things.
 */
static enum ink_token_type ink_parser_keyword(struct ink_parser *parser,
                                              const struct ink_token *token)
{
    const unsigned char *source = parser->lexer.source->bytes;
    const unsigned char *lexeme = source + token->start_offset;
    const size_t length = token->end_offset - token->start_offset;
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
        } else if (memcmp(lexeme, "VAR", length) == 0) {
            type = INK_TT_KEYWORD_VAR;
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
        } else if (memcmp(lexeme, "CONST", length) == 0) {
            type = INK_TT_KEYWORD_CONST;
        }
        break;
    }
    return type;
}

/**
 * Determine if the current token can be interpreted as an identifier.
 *
 * If true, the specified token will have its type modified.
 */
static bool ink_parser_identifier(const struct ink_parser *parser,
                                  const struct ink_token *token)
{
    const unsigned char *source = parser->lexer.source->bytes;
    const unsigned char *lexeme = source + token->start_offset;
    const size_t length = token->end_offset - token->start_offset;

    for (size_t i = 0; i < length; i++) {
        if (!ink_is_alpha(lexeme[i])) {
            return false;
        }
    }
    return true;
}

/**
 * Try to parse the current token as a keyword.
 *
 * If true, the specified token will have its type modified.
 */
static bool ink_parser_try_keyword(struct ink_parser *parser,
                                   enum ink_token_type type)
{
    struct ink_token *token = ink_parser_current_token(parser);
    const enum ink_token_type keyword_type = ink_parser_keyword(parser, token);

    if (keyword_type == type) {
        token->type = type;
        return true;
    }
    return false;
}

/**
 * Try to parse the current token as an identifier.
 *
 * If true, the specified token will have its type modified.
 */
static bool ink_parser_try_identifier(struct ink_parser *parser)
{
    struct ink_token *token = ink_parser_current_token(parser);

    if (ink_parser_identifier(parser, token)) {
        token->type = INK_TT_IDENTIFIER;
        return true;
    }
    return false;
}

/**
 * Consume tokens that indicate the level of nesting.
 *
 * Returns how many tokens (ignoring whitespace) were consumed.
 */
static size_t ink_parser_eat_nesting(struct ink_parser *parser,
                                     enum ink_token_type type)
{
    size_t branch_level = 0;

    assert(ink_parser_check(parser, INK_TT_STAR) ||
           ink_parser_check(parser, INK_TT_PLUS));

    while (ink_parser_check(parser, type)) {
        branch_level++;

        ink_parser_advance(parser);
        ink_parser_eat(parser, INK_TT_WHITESPACE);
    }
    return branch_level;
}

static void ink_syntax_node_trace(const struct ink_parser *parser,
                                  enum ink_syntax_node_type type,
                                  size_t token_start, size_t token_end)
{
    if (parser->tracing) {
        ink_trace("Creating new node: %s(LeadingToken: %zu, EndToken: %zu)",
                  ink_syntax_node_type_strz(type), token_start, token_end);
    }
}

static struct ink_syntax_node *
ink_syntax_node_leaf(struct ink_parser *parser, enum ink_syntax_node_type type,
                     size_t token_start, size_t token_end)
{
    ink_syntax_node_trace(parser, type, token_start, token_end);

    return ink_syntax_node_new(parser->arena, type, token_start, token_end,
                               NULL, NULL, NULL);
}

static struct ink_syntax_node *
ink_syntax_node_unary(struct ink_parser *parser, enum ink_syntax_node_type type,
                      size_t token_start, size_t token_end,
                      struct ink_syntax_node *lhs)
{
    ink_syntax_node_trace(parser, type, token_start, token_end);

    return ink_syntax_node_new(parser->arena, type, token_start, token_end, lhs,
                               NULL, NULL);
}

static struct ink_syntax_node *
ink_syntax_node_binary(struct ink_parser *parser,
                       enum ink_syntax_node_type type, size_t token_start,
                       size_t token_end, struct ink_syntax_node *lhs,
                       struct ink_syntax_node *rhs)
{
    ink_syntax_node_trace(parser, type, token_start, token_end);

    return ink_syntax_node_new(parser->arena, type, token_start, token_end, lhs,
                               rhs, NULL);
}

static struct ink_syntax_node *
ink_syntax_node_sequence(struct ink_parser *parser,
                         enum ink_syntax_node_type type, size_t token_start,
                         size_t token_end, size_t scratch_offset)
{
    struct ink_syntax_seq *seq = NULL;

    ink_syntax_node_trace(parser, type, token_start, token_end);

    if (parser->scratch.count != scratch_offset) {
        seq = ink_seq_from_scratch(parser->arena, &parser->scratch,
                                   scratch_offset, parser->scratch.count);
    }
    return ink_syntax_node_new(parser->arena, type, token_start, token_end,
                               NULL, NULL, seq);
}

/**
 * Initialize the state of the parser.
 */
static int ink_parser_initialize(struct ink_parser *parser,
                                 const struct ink_source *source,
                                 struct ink_syntax_tree *tree,
                                 struct ink_arena *arena)
{
    parser->arena = arena;
    parser->tokens = &tree->tokens;
    parser->lexer.source = source;
    parser->lexer.start_offset = 0;
    parser->lexer.cursor_offset = 0;
    parser->panic_mode = false;
    parser->tracing = false;
    parser->pending = 0;
    parser->token_index = 0;
    parser->context_depth = 0;
    parser->level_depth = 0;
    parser->level_stack[0] = 0;

    ink_parser_set_context(&parser->context_stack[0], INK_PARSE_CONTENT, 0);
    ink_scratch_initialize(&parser->scratch);
    ink_scratch_reserve(&parser->scratch, INK_SCRATCH_MIN_COUNT);

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
static struct ink_syntax_node *ink_parse_identifier(struct ink_parser *);
static struct ink_syntax_node *ink_parse_sequence_expr(struct ink_parser *);
static struct ink_syntax_node *ink_parse_brace_expr(struct ink_parser *);
static struct ink_syntax_node *ink_parse_content_string(struct ink_parser *);
static struct ink_syntax_node *ink_parse_content_expr(struct ink_parser *);
static struct ink_syntax_node *ink_parse_content(struct ink_parser *);
static struct ink_syntax_node *ink_parse_primary_expr(struct ink_parser *);
static struct ink_syntax_node *ink_parse_prefix_expr(struct ink_parser *);
static struct ink_syntax_node *ink_parse_infix_expr(struct ink_parser *,
                                                    struct ink_syntax_node *,
                                                    enum ink_precedence);
static struct ink_syntax_node *ink_parse_expr(struct ink_parser *);
static struct ink_syntax_node *ink_parse_choice(struct ink_parser *);
static struct ink_syntax_node *ink_parse_block_delimited(struct ink_parser *);
static struct ink_syntax_node *ink_parse_block(struct ink_parser *);
static struct ink_syntax_node *ink_parse_var(struct ink_parser *);
static struct ink_syntax_node *ink_parse_const(struct ink_parser *);

static struct ink_syntax_node *ink_parse_true(struct ink_parser *parser)
{
    ink_parser_trace(parser, __func__);

    const size_t token_start = ink_parser_expect(parser, INK_TT_KEYWORD_TRUE);

    return ink_syntax_node_leaf(parser, INK_NODE_TRUE_EXPR, token_start,
                                token_start);
}

static struct ink_syntax_node *ink_parse_false(struct ink_parser *parser)
{
    ink_parser_trace(parser, __func__);

    const size_t token_start = ink_parser_expect(parser, INK_TT_KEYWORD_FALSE);

    return ink_syntax_node_leaf(parser, INK_NODE_FALSE_EXPR, token_start,
                                token_start);
}

static struct ink_syntax_node *ink_parse_number(struct ink_parser *parser)
{
    ink_parser_trace(parser, __func__);

    const size_t token_start = ink_parser_expect(parser, INK_TT_NUMBER);

    return ink_syntax_node_leaf(parser, INK_NODE_NUMBER_EXPR, token_start,
                                token_start);
}

static struct ink_syntax_node *ink_parse_string(struct ink_parser *parser)
{
    ink_parser_trace(parser, __func__);

    const size_t token_start = ink_parser_expect(parser, INK_TT_STRING);

    return ink_syntax_node_leaf(parser, INK_NODE_STRING_EXPR, token_start,
                                token_start);
}

static struct ink_syntax_node *ink_parse_identifier(struct ink_parser *parser)
{
    ink_parser_trace(parser, __func__);

    const size_t token_start = ink_parser_expect(parser, INK_TT_IDENTIFIER);

    return ink_syntax_node_leaf(parser, INK_NODE_IDENTIFIER_EXPR, token_start,
                                token_start);
}

static struct ink_syntax_node *ink_parse_primary_expr(struct ink_parser *parser)
{
    ink_parser_trace(parser, __func__);

    struct ink_syntax_node *lhs = NULL;
    const size_t token_index = parser->token_index;
    const enum ink_token_type token_type = ink_parser_token_type(parser);

    switch (token_type) {
    case INK_TT_NUMBER:
        return ink_parse_number(parser);
    case INK_TT_STRING:
        if (ink_parser_try_keyword(parser, INK_TT_KEYWORD_TRUE)) {
            return ink_parse_true(parser);
        }
        if (ink_parser_try_keyword(parser, INK_TT_KEYWORD_FALSE)) {
            return ink_parse_false(parser);
        }
        if (ink_parser_try_identifier(parser)) {
            return ink_parse_identifier(parser);
        }
        return ink_parse_string(parser);
    case INK_TT_LEFT_PAREN:
        ink_parser_advance(parser);
        lhs = ink_parse_expr(parser);
        ink_parser_expect(parser, INK_TT_RIGHT_PAREN);

        return lhs;
    default:
        return ink_syntax_node_leaf(parser, INK_NODE_INVALID, token_index,
                                    token_index);
    }
}

static struct ink_syntax_node *ink_parse_prefix_expr(struct ink_parser *parser)
{
    ink_parser_trace(parser, __func__);

    if (ink_parser_try_keyword(parser, INK_TT_KEYWORD_NOT) ||
        ink_parser_check(parser, INK_TT_MINUS) ||
        ink_parser_check(parser, INK_TT_BANG)) {
        const enum ink_token_type type = ink_parser_token_type(parser);
        const size_t token_index = ink_parser_advance(parser);
        struct ink_syntax_node *lhs = ink_parse_prefix_expr(parser);

        return ink_syntax_node_unary(parser, ink_token_prefix_type(type),
                                     token_index, token_index, lhs);
    }
    return ink_parse_primary_expr(parser);
}

static struct ink_syntax_node *ink_parse_infix_expr(struct ink_parser *parser,
                                                    struct ink_syntax_node *lhs,
                                                    enum ink_precedence prec)
{
    ink_parser_trace(parser, __func__);

    enum ink_precedence token_prec;
    enum ink_token_type type;
    size_t token_index;
    struct ink_syntax_node *rhs = NULL;

    if (!lhs)
        lhs = ink_parse_prefix_expr(parser);

    ink_parser_try_keyword(parser, INK_TT_KEYWORD_AND) ||
        ink_parser_try_keyword(parser, INK_TT_KEYWORD_OR) ||
        ink_parser_try_keyword(parser, INK_TT_KEYWORD_MOD);

    type = ink_parser_token_type(parser);
    token_prec = ink_binding_power(type);

    while (token_prec > prec) {
        token_index = ink_parser_advance(parser);
        rhs = ink_parse_infix_expr(parser, 0, token_prec);
        lhs = ink_syntax_node_binary(parser, ink_token_infix_type(type),
                                     token_index, token_index, lhs, rhs);

        ink_parser_try_keyword(parser, INK_TT_KEYWORD_AND) ||
            ink_parser_try_keyword(parser, INK_TT_KEYWORD_OR) ||
            ink_parser_try_keyword(parser, INK_TT_KEYWORD_MOD);

        type = ink_parser_token_type(parser);
        token_prec = ink_binding_power(type);
    }
    return lhs;
}

static struct ink_syntax_node *ink_parse_expr(struct ink_parser *parser)
{
    ink_parser_trace(parser, __func__);

    return ink_parse_infix_expr(parser, 0, INK_PREC_NONE);
}

static struct ink_syntax_node *
ink_parse_content_string(struct ink_parser *parser)
{
    ink_parser_trace(parser, __func__);

    size_t token_end;
    const size_t token_start = parser->token_index;

    while (!ink_parser_check(parser, INK_TT_EOF) &&
           !ink_parser_check(parser, INK_TT_NL)) {
        if (ink_context_delim(parser))
            break;

        ink_parser_advance(parser);
    }

    token_end = parser->token_index;
    if (token_start != token_end) {
        token_end--;
    }
    return ink_syntax_node_leaf(parser, INK_NODE_STRING_EXPR, token_start,
                                token_end);
}

static struct ink_syntax_node *
ink_parse_sequence_expr(struct ink_parser *parser)
{
    ink_parser_trace(parser, __func__);

    const size_t scratch_offset = parser->scratch.count;
    const size_t token_start = parser->token_index;
    struct ink_syntax_node *node = ink_parse_content_expr(parser);

    if (!ink_parser_check(parser, INK_TT_PIPE)) {
        /* Backtrack, parse primary_expr */
        ink_parser_rewind_context(parser);
        ink_parser_push_context(parser, INK_PARSE_EXPRESSION);
        node = ink_parse_expr(parser);
        ink_parser_pop_context(parser);

        return node;
    } else {
        ink_scratch_append(&parser->scratch, node);

        while (!ink_parser_check(parser, INK_TT_EOF) &&
               !ink_parser_check(parser, INK_TT_NL) &&
               !ink_parser_check(parser, INK_TT_RIGHT_BRACE)) {
            if (ink_parser_check(parser, INK_TT_PIPE)) {
                ink_parser_advance(parser);
            }

            node = ink_parse_content_expr(parser);
            ink_scratch_append(&parser->scratch, node);
        }
    }
    return ink_syntax_node_sequence(parser, INK_NODE_SEQUENCE_EXPR, token_start,
                                    parser->token_index, scratch_offset);
}

static struct ink_syntax_node *ink_parse_brace_expr(struct ink_parser *parser)
{
    ink_parser_trace(parser, __func__);

    struct ink_syntax_node *lhs = NULL;
    const size_t token_start = parser->token_index;

    ink_parser_advance(parser);
    ink_parser_push_context(parser, INK_PARSE_BRACE);
    lhs = ink_parse_sequence_expr(parser);
    ink_parser_pop_context(parser);
    ink_parser_expect(parser, INK_TT_RIGHT_BRACE);

    return ink_syntax_node_unary(parser, INK_NODE_BRACE_EXPR, token_start,
                                 parser->token_index, lhs);
}

static struct ink_syntax_node *ink_parse_content_expr(struct ink_parser *parser)
{
    ink_parser_trace(parser, __func__);

    struct ink_syntax_node *node = NULL;
    const size_t scratch_offset = parser->scratch.count;
    const size_t token_start = parser->token_index;

    while (!ink_parser_check(parser, INK_TT_EOF) &&
           !ink_parser_check(parser, INK_TT_NL)) {
        if (ink_parser_check(parser, INK_TT_LEFT_BRACE)) {
            node = ink_parse_brace_expr(parser);
        } else if (!ink_context_delim(parser)) {
            node = ink_parse_content_string(parser);
        } else {
            break;
        }

        ink_scratch_append(&parser->scratch, node);
    }
    return ink_syntax_node_sequence(parser, INK_NODE_CONTENT_EXPR, token_start,
                                    parser->token_index, scratch_offset);
}

static struct ink_syntax_node *ink_parse_content(struct ink_parser *parser)
{
    ink_parser_trace(parser, __func__);

    struct ink_syntax_node *lhs = NULL;
    const size_t token_start = parser->token_index;

    lhs = ink_parse_content_expr(parser);

    if (!ink_parser_check(parser, INK_TT_EOF) &&
        !ink_parser_check(parser, INK_TT_NL)) {
        ink_parser_error(parser, "Expected new line!");
    }

    ink_parser_advance(parser);

    return ink_syntax_node_unary(parser, INK_NODE_CONTENT_STMT, token_start,
                                 parser->token_index, lhs);
}

static struct ink_syntax_node *
ink_parse_choice_content(struct ink_parser *parser)
{
    ink_parser_trace(parser, __func__);

    const size_t scratch_offset = parser->scratch.count;
    const size_t token_start = parser->token_index;
    struct ink_syntax_node *node = ink_parse_content_string(parser);

    ink_scratch_append(&parser->scratch, node);

    if (ink_parser_check(parser, INK_TT_LEFT_BRACKET)) {
        node = ink_parse_content_string(parser);
        ink_scratch_append(&parser->scratch, node);
        ink_parser_expect(parser, INK_TT_RIGHT_BRACKET);
    }

    node = ink_parse_content_string(parser);
    ink_scratch_append(&parser->scratch, node);

    return ink_syntax_node_sequence(parser, INK_NODE_CHOICE_CONTENT_EXPR,
                                    token_start, parser->token_index,
                                    scratch_offset);
}

static struct ink_syntax_node *
ink_parse_choice_branch(struct ink_parser *parser,
                        enum ink_syntax_node_type type)
{
    ink_parser_trace(parser, __func__);

    struct ink_syntax_node *expr = NULL;
    struct ink_syntax_node *body = NULL;
    const size_t token_start = parser->token_index;

    ink_parser_eat_nesting(parser, ink_parser_token_type(parser));

    /* ink_parser_push_context(parser, INK_PARSE_CHOICE); */
    expr = ink_parse_choice_content(parser);
    /* ink_parser_pop_context(parser); */

    if (ink_parser_check(parser, INK_TT_NL)) {
        ink_parser_advance(parser);
        body = ink_parse_block(parser);
    }
    return ink_syntax_node_binary(parser, type, token_start,
                                  parser->token_index, expr, body);
}

static struct ink_syntax_node *ink_parse_choice(struct ink_parser *parser)
{
    const size_t scratch_offset = parser->scratch.count;
    const size_t token_start = parser->token_index;

    while (!ink_parser_check(parser, INK_TT_EOF)) {
        const enum ink_token_type token_type = ink_parser_token_type(parser);
        const enum ink_syntax_node_type branch_type =
            ink_branch_type(token_type);
        struct ink_syntax_node *node =
            ink_parse_choice_branch(parser, branch_type);

        ink_scratch_append(&parser->scratch, node);

        if (parser->pending != 0)
            break;
    }

    parser->pending++;

    return ink_syntax_node_sequence(parser, INK_NODE_CHOICE_STMT, token_start,
                                    parser->token_index, scratch_offset);
}

static struct ink_syntax_node *ink_parse_var(struct ink_parser *parser)
{
    ink_parser_trace(parser, __func__);

    struct ink_syntax_node *lhs = NULL;
    struct ink_syntax_node *rhs = NULL;
    const size_t token_start = ink_parser_expect(parser, INK_TT_KEYWORD_VAR);

    ink_parser_push_context(parser, INK_PARSE_EXPRESSION);
    ink_parser_eat(parser, INK_TT_WHITESPACE);
    ink_parser_try_identifier(parser);
    lhs = ink_parse_identifier(parser);
    ink_parser_expect(parser, INK_TT_EQUAL);
    rhs = ink_parse_expr(parser);
    ink_parser_expect(parser, INK_TT_NL);
    ink_parser_pop_context(parser);

    return ink_syntax_node_binary(parser, INK_NODE_VAR_DECL, token_start,
                                  parser->token_index, lhs, rhs);
}

static struct ink_syntax_node *ink_parse_const(struct ink_parser *parser)
{
    ink_parser_trace(parser, __func__);

    struct ink_syntax_node *lhs = NULL;
    struct ink_syntax_node *rhs = NULL;
    const size_t token_start = ink_parser_expect(parser, INK_TT_KEYWORD_CONST);

    ink_parser_push_context(parser, INK_PARSE_EXPRESSION);
    ink_parser_eat(parser, INK_TT_WHITESPACE);
    ink_parser_try_identifier(parser);
    lhs = ink_parse_identifier(parser);
    ink_parser_expect(parser, INK_TT_EQUAL);
    rhs = ink_parse_expr(parser);
    ink_parser_expect(parser, INK_TT_NL);
    ink_parser_pop_context(parser);

    return ink_syntax_node_binary(parser, INK_NODE_CONST_DECL, token_start,
                                  parser->token_index, lhs, rhs);
}

/**
 * Block delimiters are context-sensitive.
 * Opening new blocks is simple. Closing them is more tricky.
 */
static struct ink_syntax_node *
ink_parse_block_delimited(struct ink_parser *parser)
{
    ink_parser_trace(parser, __func__);

    const size_t token_start = parser->token_index;
    const enum ink_token_type token_type = ink_parser_token_type(parser);

    if (ink_parser_check(parser, INK_TT_STAR) ||
        ink_parser_check(parser, INK_TT_PLUS)) {
        const size_t level = ink_parser_eat_nesting(parser, token_type);

        ink_parser_rewind(parser, token_start);

        if (level > parser->level_stack[parser->level_depth]) {
            if (parser->level_depth + 1 >= INK_PARSE_DEPTH) {
                ink_parser_error(parser, "Level nesting is too deep.");
                return NULL;
            }

            parser->pending++;
            parser->level_depth++;
            parser->level_stack[parser->level_depth] = level;
        } else if (level < parser->level_stack[parser->level_depth]) {
            while (parser->level_depth > 0 &&
                   level < parser->level_stack[parser->level_depth]) {
                parser->pending--;
                parser->level_depth--;
            }
        }
        if (parser->pending > 0) {
            parser->pending--;

            return ink_parse_choice(parser);
        }
        return NULL;
    }
    if (ink_parser_try_keyword(parser, INK_TT_KEYWORD_CONST)) {
        return ink_parse_const(parser);
    }
    if (ink_parser_try_keyword(parser, INK_TT_KEYWORD_VAR)) {
        return ink_parse_var(parser);
    }
    return ink_parse_content(parser);
}

static struct ink_syntax_node *ink_parse_block(struct ink_parser *parser)
{
    ink_parser_trace(parser, __func__);

    const size_t scratch_offset = parser->scratch.count;
    const size_t token_start = parser->token_index;

    while (!ink_parser_check(parser, INK_TT_EOF)) {
        struct ink_syntax_node *node = NULL;

        ink_parser_eat(parser, INK_TT_WHITESPACE);

        node = ink_parse_block_delimited(parser);
        if (node == NULL)
            break;

        ink_scratch_append(&parser->scratch, node);
    }
    return ink_syntax_node_sequence(parser, INK_NODE_BLOCK_STMT, token_start,
                                    parser->token_index, scratch_offset);
}

static struct ink_syntax_node *ink_parse_file(struct ink_parser *parser)
{
    ink_parser_trace(parser, __func__);

    struct ink_syntax_node *node = NULL;
    const size_t token_start = parser->token_index;

    if (ink_parser_check(parser, INK_TT_EOF))
        return node;

    node = ink_parse_block(parser);

    return ink_syntax_node_unary(parser, INK_NODE_FILE, token_start,
                                 parser->token_index, node);
}

/**
 * Parse a source file and output a syntax tree.
 */
int ink_parse(struct ink_arena *arena, const struct ink_source *source,
              struct ink_syntax_tree *syntax_tree)
{
    struct ink_parser parser;
    int rc = -1;

    if (ink_parser_initialize(&parser, source, syntax_tree, arena) < 0)
        return rc;

    ink_parser_next_token(&parser);

    syntax_tree->root = ink_parse_file(&parser);
    if (syntax_tree->root) {
        rc = 0;
    }

    ink_parser_cleanup(&parser);

    return 0;
}
