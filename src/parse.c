#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "common.h"
#include "hashmap.h"
#include "logging.h"
#include "parse.h"
#include "scanner.h"
#include "token.h"
#include "tree.h"
#include "vec.h"

#define INK_HASHTABLE_SCALE_FACTOR 2u
#define INK_HASHTABLE_LOAD_MAX .75
#define INK_HASHTABLE_MIN_CAPACITY 16
#define INK_PARSER_ARGS_MAX 255
#define INK_PARSER_CACHE_SCALE_FACTOR INK_HASHTABLE_SCALE_FACTOR
#define INK_PARSER_CACHE_LOAD_MAX INK_HASHTABLE_LOAD_MAX
#define INK_PARSER_CACHE_MIN_CAPACITY INK_HASHTABLE_MIN_CAPACITY

#define INK_PARSER_TRACE(node, rule, ...)                                      \
    do {                                                                       \
        if (parser->flags & INK_PARSER_F_TRACING) {                            \
            ink_parser_trace(parser, #rule);                                   \
        }                                                                      \
    } while (0)

#define INK_PARSER_MEMOIZE(node, rule, ...)                                    \
    do {                                                                       \
        int rc;                                                                \
        const struct ink_parser_cache_key key = {                              \
            .source_offset = parser->source_offset,                            \
            .rule_address = (void *)rule,                                      \
        };                                                                     \
                                                                               \
        if (parser->flags & INK_PARSER_F_CACHING) {                            \
            rc = ink_parser_cache_lookup(&parser->cache, key, &node);          \
            if (rc < 0) {                                                      \
                node = INK_DISPATCH(rule, __VA_ARGS__);                        \
                ink_parser_cache_insert(&parser->cache, key, node);            \
            } else {                                                           \
                ink_trace("Parser cache hit!");                                \
                parser->source_offset = node->end_offset;                      \
                ink_parser_advance(parser);                                    \
            }                                                                  \
        } else {                                                               \
            node = INK_DISPATCH(rule, __VA_ARGS__);                            \
        }                                                                      \
    } while (0)

#define INK_PARSER_RULE(node, rule, ...)                                       \
    do {                                                                       \
        INK_PARSER_TRACE(node, rule, __VA_ARGS__);                             \
        INK_PARSER_MEMOIZE(node, rule, __VA_ARGS__);                           \
    } while (0)

struct ink_parser_state {
    size_t choice_level;
    size_t scratch_offset;
    size_t source_offset;
};

struct ink_parser_cache_key {
    size_t source_offset;
    void *rule_address;
};

INK_VEC_T(ink_parser_scratch, struct ink_syntax_node *)
INK_VEC_T(ink_parser_stack, struct ink_parser_state)
INK_HASHMAP_T(ink_parser_cache, struct ink_parser_cache_key,
              struct ink_syntax_node *)

struct ink_parser_node_context {
    bool is_conditional;
    size_t choice_level;
    size_t knot_offset;
    size_t scratch_start;
    /* NOTE: Probably temporary.
     * Could use ranges on a central stack, just like scratch */
    struct ink_parser_stack open_blocks;
    struct ink_parser_stack open_choices;
};

/**
 * Ink parsing state.
 *
 * To create nodes with a variable number of children, references to
 * intermediate parsing results are stored within a scratch buffer before a
 * node sequence is properly allocated. The size of this buffer grows and
 * shrinks dynamically as nodes are added to and removed from it.
 *
 * TODO(Brett): Describe the error recovery strategy.
 *
 * TODO(Brett): Describe expression parsing.
 *
 * TODO(Brett): Add a logger v-table to the parser.
 */
struct ink_parser {
    struct ink_arena *arena;
    struct ink_scanner scanner;
    struct ink_parser_scratch scratch;
    struct ink_parser_cache cache;
    struct ink_token token;
    bool panic_mode;
    int flags;
    size_t source_offset;
};

enum ink_precedence {
    INK_PREC_NONE = 0,
    INK_PREC_ASSIGN,
    INK_PREC_LOGICAL_OR,
    INK_PREC_LOGICAL_AND,
    INK_PREC_COMPARISON,
    INK_PREC_TERM,
    INK_PREC_FACTOR,
};

static struct ink_syntax_node *ink_parse_content(struct ink_parser *,
                                                 const enum ink_token_type *);
static struct ink_syntax_node *ink_parse_infix_expr(struct ink_parser *,
                                                    struct ink_syntax_node *,
                                                    enum ink_precedence);
static struct ink_syntax_node *ink_parse_inline_logic(struct ink_parser *);
static struct ink_syntax_node *ink_parse_argument_list(struct ink_parser *);
static struct ink_syntax_node *ink_parse_stmt(struct ink_parser *,
                                              struct ink_parser_node_context *);

static int ink_parser_stack_emplace(struct ink_parser_stack *stack,
                                    size_t choice_level, size_t scratch_offset,
                                    size_t source_offset)
{
    struct ink_parser_state entry = {
        .choice_level = choice_level,
        .scratch_offset = scratch_offset,
        .source_offset = source_offset,
    };
    return ink_parser_stack_push(stack, entry);
}

static unsigned int ink_parser_cache_key_hash(const void *key, size_t length)
{
    return ink_fnv32a((unsigned char *)key, length);
}

static bool ink_parser_cache_key_compare(const void *a, size_t a_length,
                                         const void *b, size_t b_length)
{
    struct ink_parser_cache_key *const key_a = (struct ink_parser_cache_key *)a;
    struct ink_parser_cache_key *const key_b = (struct ink_parser_cache_key *)b;

    return (key_a->source_offset == key_b->source_offset) &&
           (key_a->rule_address == key_b->rule_address);
}

/**
 * Create a syntax node sequence from a range of nodes from the scratch buffer.
 */
static struct ink_syntax_seq *
ink_seq_from_scratch(struct ink_parser_scratch *scratch, size_t start_offset,
                     size_t end_offset, struct ink_arena *arena)
{
    struct ink_syntax_seq *seq = NULL;
    size_t seq_index = 0;

    if (start_offset < end_offset) {
        const size_t span = end_offset - start_offset;
        const size_t seq_size = sizeof(*seq) + span * sizeof(seq->nodes);

        assert(span > 0);

        seq = ink_arena_allocate(arena, seq_size);
        if (seq == NULL) {
            /* TODO(Brett): Handle and log the error. */
            return NULL;
        }

        seq->count = span;

        for (size_t i = start_offset; i < end_offset; i++) {
            seq->nodes[seq_index] = scratch->entries[i];
            scratch->entries[i] = NULL;
            seq_index++;
        }

        ink_parser_scratch_shrink(scratch, start_offset);
    }
    return seq;
}

static inline enum ink_syntax_node_type
ink_token_prefix_type(enum ink_token_type type)
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

static inline enum ink_syntax_node_type
ink_token_infix_type(enum ink_token_type type)
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
    case INK_TT_QUESTION:
        return INK_NODE_CONTAINS_EXPR;
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

static inline enum ink_precedence ink_binding_power(enum ink_token_type type)
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
    case INK_TT_QUESTION:
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
static inline bool ink_is_sync_token(enum ink_token_type type)
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

static inline enum ink_syntax_node_type
ink_branch_type(enum ink_token_type type)
{
    switch (type) {
    case INK_TT_STAR:
        return INK_NODE_CHOICE_STAR_STMT;
    case INK_TT_PLUS:
        return INK_NODE_CHOICE_PLUS_STMT;
    default:
        return INK_NODE_INVALID;
    }
}

/**
 * Push a lexical analysis context onto the parser.
 */
static inline void ink_parser_push_scanner(struct ink_parser *parser,
                                           enum ink_grammar_type type)
{
    ink_scanner_push(&parser->scanner, type, parser->source_offset);
}

/**
 * Pop a lexical analysis context from the parser.
 */
static inline void ink_parser_pop_scanner(struct ink_parser *parser)
{
    ink_scanner_pop(&parser->scanner);
}

/**
 * Advance to the next token.
 */
static inline void ink_parser_next_token(struct ink_parser *parser)
{
    ink_scanner_next(&parser->scanner, &parser->token);
    parser->source_offset = parser->scanner.start_offset;
}

/**
 * Check if the current token matches a given token type.
 */
static inline bool ink_parser_check(struct ink_parser *parser,
                                    enum ink_token_type type)
{
    const struct ink_token *token = &parser->token;

    return token->type == type;
}

/**
 * Print parser tracing information to the console.
 */
static void ink_parser_trace(struct ink_parser *parser, const char *rule_name)
{
    const struct ink_token *token = &parser->token;

    ink_trace("Entering %s(TokenType=%s, SourceOffset: %zu)", rule_name,
              ink_token_type_strz(token->type), parser->source_offset);
}

static void ink_parser_rewind_scanner(struct ink_parser *parser)
{
    const struct ink_scanner_mode *mode = ink_scanner_current(&parser->scanner);

    if (parser->flags & INK_PARSER_F_TRACING) {
        ink_trace("Rewinding scanner to %zu", parser->source_offset);
    }

    ink_scanner_rewind(&parser->scanner, mode->source_offset);
    parser->source_offset = mode->source_offset;
}

static bool ink_parser_check_many(struct ink_parser *parser,
                                  const enum ink_token_type *token_set)
{
    if (ink_parser_check(parser, INK_TT_EOF)) {
        return true;
    }
    for (size_t i = 0; token_set[i] != INK_TT_EOF; i++) {
        if (ink_parser_check(parser, token_set[i])) {
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
    ink_token_print(parser->scanner.source, &parser->token);
    return NULL;
}

/**
 * Create a leaf node.
 */
static inline struct ink_syntax_node *
ink_syntax_node_leaf(enum ink_syntax_node_type type, size_t source_start,
                     size_t source_end, struct ink_arena *arena)
{
    return ink_syntax_node_new(type, source_start, source_end, NULL, NULL, NULL,
                               arena);
}

/**
 * Create a unary node.
 */
static inline struct ink_syntax_node *
ink_syntax_node_unary(enum ink_syntax_node_type type, size_t source_start,
                      size_t source_end, struct ink_syntax_node *lhs,
                      struct ink_arena *arena)
{
    return ink_syntax_node_new(type, source_start, source_end, lhs, NULL, NULL,
                               arena);
}

/**
 * Create a binary node.
 */
static inline struct ink_syntax_node *
ink_syntax_node_binary(enum ink_syntax_node_type type, size_t source_start,
                       size_t source_end, struct ink_syntax_node *lhs,
                       struct ink_syntax_node *rhs, struct ink_arena *arena)
{
    return ink_syntax_node_new(type, source_start, source_end, lhs, rhs, NULL,
                               arena);
}

/**
 * Create a node with a variable number of children.
 */
static inline struct ink_syntax_node *
ink_syntax_node_sequence(enum ink_syntax_node_type type, size_t source_start,
                         size_t source_end, size_t scratch_offset,
                         struct ink_parser_scratch *scratch,
                         struct ink_arena *arena)
{
    struct ink_syntax_seq *seq = NULL;

    if (scratch->count != scratch_offset) {
        seq = ink_seq_from_scratch(scratch, scratch_offset, scratch->count,
                                   arena);
        if (seq == NULL) {
            return NULL;
        }
    }
    return ink_syntax_node_new(type, source_start, source_end, NULL, NULL, seq,
                               arena);
}

/**
 * Advance the parser and return the source cursor offset.
 */
static size_t ink_parser_advance(struct ink_parser *parser)
{
    if (!ink_parser_check(parser, INK_TT_EOF)) {
        ink_parser_next_token(parser);
    }
    return parser->scanner.cursor_offset;
}

/**
 * Consume the current token if it matches a given token type.
 */
static bool ink_parser_eat(struct ink_parser *parser, enum ink_token_type type)
{
    if (ink_parser_check(parser, type)) {
        ink_parser_advance(parser);
        return true;
    }
    return false;
}

static size_t ink_parser_expect(struct ink_parser *parser,
                                enum ink_token_type type)
{
    size_t source_offset = parser->source_offset;
    const struct ink_scanner_mode *mode = ink_scanner_current(&parser->scanner);

    if (mode->type == INK_GRAMMAR_EXPRESSION) {
        if (ink_parser_check(parser, INK_TT_WHITESPACE)) {
            source_offset = ink_parser_advance(parser);
        }
    }
    if (!ink_parser_check(parser, type)) {
        ink_parser_error(parser, "Unexpected token! %s",
                         ink_token_type_strz(parser->token.type));

        do {
            source_offset = ink_parser_advance(parser);
            type = parser->token.type;
        } while (!ink_is_sync_token(type));

        return source_offset;
    }

    ink_parser_advance(parser);
    return source_offset;
}

static void ink_parser_expect_stmt_end(struct ink_parser *parser)
{
    if (!ink_parser_check(parser, INK_TT_EOF) &&
        !ink_parser_check(parser, INK_TT_NL)) {
        ink_parser_error(parser, "Expected new line!");
    }

    ink_parser_advance(parser);
}

static size_t ink_parser_eat_many(struct ink_parser *parser,
                                  enum ink_token_type type,
                                  bool ignore_whitespace)
{
    size_t count = 0;

    while (ink_parser_check(parser, type)) {
        count++;

        ink_parser_advance(parser);
        if (ignore_whitespace) {
            ink_parser_eat(parser, INK_TT_WHITESPACE);
        }
    }
    return count;
}

static void
ink_parser_node_context_init(struct ink_parser *parser,
                             struct ink_parser_node_context *context)
{
    context->is_conditional = false;
    context->choice_level = 0;
    context->knot_offset = 0;
    context->scratch_start = parser->scratch.count;

    ink_parser_stack_init(&context->open_blocks);
    ink_parser_stack_init(&context->open_choices);
}

static void
ink_parser_node_context_deinit(struct ink_parser_node_context *context)
{
    ink_parser_stack_deinit(&context->open_blocks);
    ink_parser_stack_deinit(&context->open_choices);
}

/**
 * Close the current block, if present.
 */
static struct ink_syntax_node *
ink_parser_collect_block(struct ink_parser *parser,
                         struct ink_parser_node_context *context)
{
    struct ink_parser_state block;
    struct ink_syntax_node *node = NULL;
    struct ink_syntax_node *tmp = NULL;
    struct ink_arena *const arena = parser->arena;
    struct ink_parser_scratch *const scratch = &parser->scratch;
    struct ink_parser_stack *const open_blocks = &context->open_blocks;

    if (!ink_parser_stack_is_empty(open_blocks)) {
        ink_parser_stack_pop(open_blocks, &block);
        ink_parser_scratch_last(scratch, &tmp);

        node = ink_syntax_node_sequence(INK_NODE_BLOCK, block.source_offset,
                                        tmp->end_offset, block.scratch_offset,
                                        scratch, arena);

        if (!ink_parser_scratch_is_empty(scratch) &&
            scratch->count > context->scratch_start) {
            ink_parser_scratch_last(scratch, &tmp);

            switch (tmp->type) {
            case INK_NODE_CHOICE_STAR_STMT:
            case INK_NODE_CHOICE_PLUS_STMT:
            case INK_NODE_CONDITIONAL_BRANCH:
            case INK_NODE_CONDITIONAL_ELSE_BRANCH: {
                tmp->rhs = node;
                node = tmp;

                ink_parser_scratch_pop(scratch, NULL);
                break;
            }
            default:
                break;
            }
        }
    }
    return node;
}

/**
 * Close an open choice statement for a given level, if present.
 */
static void ink_parser_collect_choices(struct ink_parser *parser,
                                       struct ink_parser_node_context *context,
                                       size_t choice_level)
{
    struct ink_parser_state block, choice, prev_choice;
    struct ink_syntax_node *tmp = NULL;
    struct ink_arena *const arena = parser->arena;
    struct ink_parser_scratch *const scratch = &parser->scratch;
    struct ink_parser_stack *const open_choices = &context->open_choices;
    struct ink_parser_stack *const open_blocks = &context->open_blocks;

    while (!ink_parser_stack_is_empty(open_choices)) {
        assert(!ink_parser_stack_is_empty(open_blocks));
        ink_parser_stack_last(open_choices, &choice);

        if (choice.choice_level > choice_level) {
            ink_parser_stack_pop(open_choices, &choice);

            if (!ink_parser_stack_is_empty(open_blocks)) {
                ink_parser_stack_last(open_blocks, &block);

                if (choice.choice_level == block.choice_level) {
                    tmp = ink_parser_collect_block(parser, context);
                    if (tmp != NULL) {
                        ink_parser_scratch_push(scratch, tmp);
                    }
                }
            }

            ink_parser_stack_last(open_choices, &prev_choice);

            if (choice.choice_level > choice_level &&
                choice_level > prev_choice.choice_level) {
                /* Handle non-sequentially increasing levels */
                ink_parser_stack_emplace(open_choices, choice_level,
                                         choice.scratch_offset,
                                         choice.source_offset);
                break;
            } else {
                tmp = ink_syntax_node_sequence(
                    INK_NODE_CHOICE_STMT, choice.source_offset,
                    parser->source_offset, choice.scratch_offset, scratch,
                    arena);

                ink_parser_scratch_push(scratch, tmp);
            }
        } else {
            break;
        }
    }
}

/**
 * Close an open stitch, if present.
 */
static struct ink_syntax_node *
ink_parser_collect_stitch(struct ink_parser *parser,
                          struct ink_parser_node_context *context)
{
    struct ink_syntax_node *node = NULL;
    struct ink_syntax_node *tmp = NULL;
    struct ink_arena *const arena = parser->arena;
    struct ink_parser_scratch *const scratch = &parser->scratch;

    ink_parser_collect_choices(parser, context, 0);
    node = ink_parser_collect_block(parser, context);

    if (!ink_parser_scratch_is_empty(scratch)) {
        ink_parser_scratch_last(scratch, &tmp);

        if (tmp->type == INK_NODE_STITCH_PROTO) {
            ink_parser_scratch_pop(scratch, NULL);
            node =
                ink_syntax_node_binary(INK_NODE_STITCH_DECL, tmp->start_offset,
                                       node->end_offset, tmp, node, arena);
        }
    }
    return node;
}

/**
 * Close an open knot, if present.
 */
static struct ink_syntax_node *
ink_parser_collect_knot(struct ink_parser *parser,
                        struct ink_parser_node_context *context)
{
    struct ink_syntax_node *node = NULL;
    struct ink_syntax_node *tmp = NULL;
    struct ink_syntax_seq *seq = NULL;
    struct ink_arena *const arena = parser->arena;
    struct ink_parser_scratch *const scratch = &parser->scratch;

    node = ink_parser_collect_stitch(parser, context);

    if (!ink_parser_scratch_is_empty(scratch)) {
        tmp = scratch->entries[context->knot_offset];

        if (tmp->type == INK_NODE_KNOT_PROTO) {
            if (node != NULL) {
                ink_parser_scratch_push(scratch, node);
            }

            seq = ink_seq_from_scratch(scratch, context->knot_offset + 1,
                                       scratch->count, arena);
            ink_parser_scratch_pop(scratch, &node);
            node = ink_syntax_node_unary(INK_NODE_KNOT_DECL, tmp->start_offset,
                                         parser->source_offset, node, arena);
            node->seq = seq;
        }
    }
    return node;
}

static void
ink_parser_handle_conditional_branch(struct ink_parser *parser,
                                     struct ink_parser_node_context *context,
                                     struct ink_syntax_node *node)
{
    struct ink_parser_scratch *const scratch = &parser->scratch;
    struct ink_syntax_node *tmp = NULL;

    ink_parser_collect_choices(parser, context, 0);

    tmp = ink_parser_collect_block(parser, context);
    if (tmp != NULL) {
        ink_parser_scratch_push(scratch, tmp);
    }
}

static void
ink_parser_handle_choice_branch(struct ink_parser *parser,
                                struct ink_parser_node_context *context,
                                struct ink_syntax_node *node)
{
    int rc;
    struct ink_parser_state block, choice;
    struct ink_parser_stack *const open_blocks = &context->open_blocks;
    struct ink_parser_stack *const open_choices = &context->open_choices;
    struct ink_parser_scratch *const scratch = &parser->scratch;
    struct ink_syntax_node *tmp = NULL;
    const size_t choice_level = context->choice_level;

    if (ink_parser_stack_is_empty(open_blocks)) {
        rc = ink_parser_stack_emplace(open_blocks, 0, scratch->count,
                                      node->start_offset);
        if (rc < 0) {
            return;
        }
    }
    if (ink_parser_stack_is_empty(open_choices)) {
        ink_parser_stack_emplace(open_choices, choice_level, scratch->count,
                                 node->start_offset);
    } else {
        ink_parser_stack_last(open_choices, &choice);
        ink_parser_stack_last(open_blocks, &block);

        if (choice_level > choice.choice_level) {
            if (block.choice_level < choice.choice_level) {
                ink_parser_stack_emplace(open_blocks, choice.choice_level,
                                         scratch->count, parser->source_offset);
            }

            ink_parser_stack_emplace(open_choices, choice_level, scratch->count,
                                     node->start_offset);
        } else if (choice_level == choice.choice_level) {
            if (choice_level == block.choice_level) {
                tmp = ink_parser_collect_block(parser, context);
                if (tmp != NULL) {
                    ink_parser_scratch_push(scratch, tmp);
                }
            }
        } else {
            ink_parser_collect_choices(parser, context, choice_level);

            if (!ink_parser_stack_is_empty(open_blocks)) {
                ink_parser_stack_last(open_blocks, &block);

                if (block.choice_level == choice_level) {
                    tmp = ink_parser_collect_block(parser, context);
                    if (tmp != NULL) {
                        ink_parser_scratch_push(scratch, tmp);
                    }
                }
            }
        }
    }
}

static void ink_parser_handle_gather(struct ink_parser *parser,
                                     struct ink_parser_node_context *context,
                                     struct ink_syntax_node **node)
{
    int rc;
    struct ink_parser_state block;
    struct ink_parser_stack *const open_blocks = &context->open_blocks;
    struct ink_parser_stack *const open_choices = &context->open_choices;
    struct ink_parser_scratch *const scratch = &parser->scratch;
    struct ink_syntax_node *tmp = NULL;
    const size_t choice_level = context->choice_level;
    const size_t source_start = parser->source_offset;

    if (ink_parser_stack_is_empty(open_blocks)) {
        rc = ink_parser_stack_emplace(open_blocks, 0, scratch->count,
                                      (*node)->start_offset);
        if (rc < 0) {
            return;
        }
    }
    if (!ink_parser_stack_is_empty(open_choices) &&
        !ink_parser_scratch_is_empty(scratch)) {
        ink_parser_collect_choices(parser, context, choice_level - 1);
        ink_parser_scratch_last(scratch, &tmp);

        if (tmp->type == INK_NODE_CHOICE_STMT) {
            ink_parser_scratch_pop(scratch, NULL);
            *node = ink_syntax_node_binary(INK_NODE_GATHERED_CHOICE_STMT,
                                           tmp->start_offset, source_start, tmp,
                                           *node, parser->arena);
        }
        if (!ink_parser_stack_is_empty(open_blocks)) {
            ink_parser_stack_last(open_blocks, &block);

            if (block.choice_level == choice_level) {
                tmp = ink_parser_collect_block(parser, context);
                if (tmp != NULL) {
                    ink_parser_scratch_push(scratch, tmp);
                }
            }
        }
    }
}

static void ink_parser_handle_content(struct ink_parser *parser,
                                      struct ink_parser_node_context *context,
                                      struct ink_syntax_node *node)
{
    int rc;
    struct ink_parser_state block, choice;
    struct ink_parser_scratch *const scratch = &parser->scratch;
    struct ink_parser_stack *const open_blocks = &context->open_blocks;
    struct ink_parser_stack *const open_choices = &context->open_choices;

    if (ink_parser_stack_is_empty(open_blocks)) {
        rc = ink_parser_stack_emplace(open_blocks, 0, scratch->count,
                                      node->start_offset);
        if (rc < 0) {
            return;
        }
    }
    if (!ink_parser_stack_is_empty(open_choices)) {
        ink_parser_stack_last(open_blocks, &block);
        ink_parser_stack_last(open_choices, &choice);

        if (block.choice_level != choice.choice_level) {
            ink_parser_stack_emplace(open_blocks, choice.choice_level,
                                     scratch->count, node->start_offset);
        }
    }
}

static void ink_parser_handle_knot(struct ink_parser *parser,
                                   struct ink_parser_node_context *context)
{
    struct ink_syntax_node *node = NULL;
    struct ink_parser_scratch *const scratch = &parser->scratch;

    node = ink_parser_collect_knot(parser, context);
    if (node != NULL) {
        ink_parser_scratch_push(scratch, node);
    }

    context->knot_offset = scratch->count;
}

static void ink_parser_handle_stitch(struct ink_parser *parser,
                                     struct ink_parser_node_context *context)
{
    struct ink_syntax_node *node = NULL;
    struct ink_parser_scratch *const scratch = &parser->scratch;

    node = ink_parser_collect_stitch(parser, context);
    if (node != NULL) {
        ink_parser_scratch_push(scratch, node);
    }
}

static int ink_parser_init(struct ink_parser *parser,
                           const struct ink_source *source,
                           struct ink_syntax_tree *tree,
                           struct ink_arena *arena, int flags)
{

    parser->arena = arena;
    parser->scanner.source = source;
    parser->scanner.is_line_start = true;
    parser->scanner.start_offset = 0;
    parser->scanner.cursor_offset = 0;
    parser->scanner.mode_stack[0].type = INK_GRAMMAR_CONTENT;
    parser->scanner.mode_stack[0].source_offset = 0;
    parser->scanner.mode_depth = 0;
    parser->token.type = 0;
    parser->token.start_offset = 0;
    parser->token.end_offset = 0;
    parser->panic_mode = false;
    parser->flags = flags;
    parser->source_offset = 0;

    ink_parser_scratch_init(&parser->scratch);
    ink_parser_cache_init(&parser->cache, 80, ink_parser_cache_key_hash,
                          ink_parser_cache_key_compare);
    return INK_E_OK;
}

static void ink_parser_deinit(struct ink_parser *parser)
{
    ink_parser_cache_deinit(&parser->cache);
    ink_parser_scratch_deinit(&parser->scratch);
}

static struct ink_syntax_node *
ink_parse_atom(struct ink_parser *parser, enum ink_syntax_node_type node_type)
{
    /* NOTE: Advancing the parser MUST only happen after the node is
     * created. This prevents trailing whitespace. */
    struct ink_syntax_node *node;
    const struct ink_token token = parser->token;

    node = ink_syntax_node_leaf(node_type, token.start_offset, token.end_offset,
                                parser->arena);
    ink_parser_advance(parser);
    return node;
}

static struct ink_syntax_node *ink_parse_true(struct ink_parser *parser)
{
    return ink_parse_atom(parser, INK_NODE_TRUE);
}

static struct ink_syntax_node *ink_parse_false(struct ink_parser *parser)
{
    return ink_parse_atom(parser, INK_NODE_FALSE);
}

static struct ink_syntax_node *ink_parse_number(struct ink_parser *parser)
{
    return ink_parse_atom(parser, INK_NODE_NUMBER);
}

static struct ink_syntax_node *ink_parse_identifier(struct ink_parser *parser)
{
    return ink_parse_atom(parser, INK_NODE_IDENTIFIER);
}

static struct ink_syntax_node *
ink_parse_string(struct ink_parser *parser,
                 const enum ink_token_type *token_set)
{
    const size_t source_start = parser->source_offset;

    while (!ink_parser_check_many(parser, token_set)) {
        ink_parser_advance(parser);
    }
    return ink_syntax_node_leaf(INK_NODE_STRING, source_start,
                                parser->source_offset, parser->arena);
}

static struct ink_syntax_node *ink_parse_name_expr(struct ink_parser *parser)
{
    struct ink_syntax_node *lhs = NULL;
    struct ink_syntax_node *rhs = NULL;
    const size_t source_start = parser->source_offset;

    INK_PARSER_RULE(lhs, ink_parse_identifier, parser);
    if (!ink_parser_check(parser, INK_TT_LEFT_PAREN)) {
        return lhs;
    }

    INK_PARSER_RULE(rhs, ink_parse_argument_list, parser);
    return ink_syntax_node_binary(INK_NODE_CALL_EXPR, source_start,
                                  parser->source_offset, lhs, rhs,
                                  parser->arena);
}

static struct ink_syntax_node *ink_parse_divert(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t source_start = parser->source_offset;

    ink_parser_advance(parser);
    INK_PARSER_RULE(node, ink_parse_identifier, parser);
    return ink_syntax_node_unary(INK_NODE_DIVERT, source_start,
                                 parser->source_offset, node, parser->arena);
}

static struct ink_syntax_node *
ink_parse_string_expr(struct ink_parser *parser,
                      const enum ink_token_type *token_set)
{
    struct ink_syntax_node *node = NULL;
    struct ink_parser_scratch *scratch = &parser->scratch;
    const size_t scratch_offset = scratch->count;
    const size_t source_start = ink_parser_expect(parser, INK_TT_DOUBLE_QUOTE);

    do {
        if (!ink_parser_check_many(parser, token_set)) {
            INK_PARSER_RULE(node, ink_parse_string, parser, token_set);
        } else if (ink_parser_check(parser, INK_TT_LEFT_BRACE)) {
            INK_PARSER_RULE(node, ink_parse_inline_logic, parser);
        } else {
            break;
        }

        ink_parser_scratch_push(scratch, node);
    } while (!ink_parser_check(parser, INK_TT_EOF) &&
             !ink_parser_check(parser, INK_TT_DOUBLE_QUOTE));

    ink_parser_expect(parser, INK_TT_DOUBLE_QUOTE);
    return ink_syntax_node_sequence(INK_NODE_STRING_EXPR, source_start,
                                    parser->source_offset, scratch_offset,
                                    scratch, parser->arena);
}

static struct ink_syntax_node *ink_parse_primary_expr(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    static const enum ink_token_type token_set[] = {
        INK_TT_DOUBLE_QUOTE, INK_TT_LEFT_BRACE, INK_TT_RIGHT_BRACE,
        INK_TT_NL,           INK_TT_EOF,
    };

    switch (parser->token.type) {
    case INK_TT_NUMBER: {
        INK_PARSER_RULE(node, ink_parse_number, parser);
        break;
    }
    case INK_TT_KEYWORD_TRUE: {
        INK_PARSER_RULE(node, ink_parse_true, parser);
        break;
    }
    case INK_TT_KEYWORD_FALSE: {
        INK_PARSER_RULE(node, ink_parse_false, parser);
        break;
    }
    case INK_TT_IDENTIFIER: {
        INK_PARSER_RULE(node, ink_parse_name_expr, parser);
        break;
    }
    case INK_TT_DOUBLE_QUOTE: {
        INK_PARSER_RULE(node, ink_parse_string_expr, parser, token_set);
        break;
    }
    case INK_TT_LEFT_PAREN: {
        ink_parser_advance(parser);
        INK_PARSER_RULE(node, ink_parse_infix_expr, parser, NULL,
                        INK_PREC_NONE);
        if (node == NULL) {
            return NULL;
        }
        if (!ink_parser_eat(parser, INK_TT_RIGHT_PAREN)) {
            return NULL;
        }
        break;
    }
    default:
        break;
    }
    return node;
}

static struct ink_syntax_node *ink_parse_prefix_expr(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const enum ink_token_type type = parser->token.type;

    switch (type) {
    case INK_TT_KEYWORD_NOT:
    case INK_TT_MINUS:
    case INK_TT_BANG: {
        const size_t source_start = ink_parser_advance(parser);

        INK_PARSER_RULE(node, ink_parse_prefix_expr, parser);
        if (node == NULL) {
            return NULL;
        }

        node =
            ink_syntax_node_unary(ink_token_prefix_type(type), source_start,
                                  parser->source_offset, node, parser->arena);
        break;
    }
    case INK_TT_RIGHT_ARROW: {
        INK_PARSER_RULE(node, ink_parse_divert, parser);
        break;
    }
    default:
        INK_PARSER_RULE(node, ink_parse_primary_expr, parser);
        break;
    }
    return node;
}

static struct ink_syntax_node *ink_parse_infix_expr(struct ink_parser *parser,
                                                    struct ink_syntax_node *lhs,
                                                    enum ink_precedence prec)
{
    enum ink_precedence token_prec;
    enum ink_token_type type;

    if (lhs == NULL) {
        INK_PARSER_RULE(lhs, ink_parse_prefix_expr, parser);
        if (lhs == NULL) {
            return NULL;
        }
    }

    type = parser->token.type;
    token_prec = ink_binding_power(type);

    while (token_prec > prec) {
        struct ink_syntax_node *rhs = NULL;
        const size_t source_start = ink_parser_advance(parser);

        INK_PARSER_RULE(rhs, ink_parse_infix_expr, parser, NULL, token_prec);
        if (rhs == NULL) {
            return NULL;
        }

        lhs = ink_syntax_node_binary(ink_token_infix_type(type), source_start,
                                     parser->source_offset, lhs, rhs,
                                     parser->arena);
        type = parser->token.type;
        token_prec = ink_binding_power(type);
    }
    return lhs;
}

static struct ink_syntax_node *
ink_parse_divert_or_tunnel(struct ink_parser *parser, bool *is_tunnel)
{
    struct ink_syntax_node *node = NULL;
    const size_t source_start = parser->source_offset;

    if (ink_parser_check(parser, INK_TT_RIGHT_ARROW)) {
        ink_parser_advance(parser);

        if (ink_parser_check(parser, INK_TT_RIGHT_ARROW)) {
            ink_parser_advance(parser);

            if (ink_parser_check(parser, INK_TT_IDENTIFIER)) {
                INK_PARSER_RULE(node, ink_parse_name_expr, parser);
            }
            return ink_syntax_node_unary(INK_NODE_TUNNEL_ONWARDS, source_start,
                                         parser->source_offset, node,
                                         parser->arena);
        } else if (ink_parser_check(parser, INK_TT_IDENTIFIER)) {
            INK_PARSER_RULE(node, ink_parse_name_expr, parser);
            return ink_syntax_node_unary(INK_NODE_DIVERT, source_start,
                                         parser->source_offset, node,
                                         parser->arena);
        } else {
            *is_tunnel = true;
            return NULL;
        }
    }
    return NULL;
}

static struct ink_syntax_node *ink_parse_thread_expr(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t source_start = parser->source_offset;

    ink_parser_push_scanner(parser, INK_GRAMMAR_EXPRESSION);
    ink_parser_advance(parser);

    if (ink_parser_check(parser, INK_TT_IDENTIFIER)) {
        node = ink_parse_name_expr(parser);
    }

    ink_parser_pop_scanner(parser);
    return ink_syntax_node_unary(INK_NODE_THREAD_EXPR, source_start,
                                 parser->source_offset, node, parser->arena);
}

static struct ink_syntax_node *ink_parse_expr(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;

    INK_PARSER_RULE(node, ink_parse_infix_expr, parser, NULL, INK_PREC_NONE);
    return node;
}

static struct ink_syntax_node *ink_parse_return_stmt(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t source_start = parser->source_offset;
    size_t source_end = 0;

    ink_parser_advance(parser);

    if (!ink_parser_check(parser, INK_TT_NL) &&
        !ink_parser_check(parser, INK_TT_EOF)) {
        node = ink_parse_expr(parser);
    }

    source_end = node ? node->end_offset : parser->source_offset;

    ink_parser_expect_stmt_end(parser);
    return ink_syntax_node_unary(INK_NODE_RETURN_STMT, source_start, source_end,
                                 node, parser->arena);
}

static struct ink_syntax_node *ink_parse_divert_stmt(struct ink_parser *parser)
{
    bool flag = 0;
    struct ink_syntax_node *node = NULL;
    struct ink_parser_scratch *scratch = &parser->scratch;
    const size_t scratch_offset = scratch->count;
    const size_t source_start = parser->source_offset;
    size_t source_end = 0;

    ink_parser_push_scanner(parser, INK_GRAMMAR_EXPRESSION);

    while (ink_parser_check(parser, INK_TT_RIGHT_ARROW)) {
        INK_PARSER_RULE(node, ink_parse_divert_or_tunnel, parser, &flag);
        ink_parser_scratch_push(scratch, node);
    }

    source_end = node ? node->end_offset : parser->source_offset;
    ink_parser_pop_scanner(parser);
    ink_parser_expect_stmt_end(parser);

    if (flag || scratch->count - scratch_offset > 1) {
        return ink_syntax_node_sequence(INK_NODE_TUNNEL_STMT, source_start,
                                        source_end, scratch_offset, scratch,
                                        parser->arena);
    }
    return ink_syntax_node_sequence(INK_NODE_DIVERT_STMT, source_start,
                                    source_end, scratch_offset, scratch,
                                    parser->arena);
}

static struct ink_syntax_node *ink_parse_glue(struct ink_parser *parser)
{
    const size_t source_start = parser->source_offset;

    ink_parser_advance(parser);
    return ink_syntax_node_leaf(INK_NODE_GLUE, source_start,
                                parser->source_offset, parser->arena);
}

static struct ink_syntax_node *ink_parse_temp_decl(struct ink_parser *parser)
{
    struct ink_syntax_node *lhs = NULL;
    struct ink_syntax_node *rhs = NULL;
    const size_t source_start = parser->source_offset;

    ink_parser_advance(parser);
    INK_PARSER_RULE(lhs, ink_parse_identifier, parser);
    ink_parser_expect(parser, INK_TT_EQUAL);
    INK_PARSER_RULE(rhs, ink_parse_expr, parser);
    ink_parser_expect_stmt_end(parser);
    return ink_syntax_node_binary(INK_NODE_TEMP_DECL, source_start,
                                  parser->source_offset, lhs, rhs,
                                  parser->arena);
}

static struct ink_syntax_node *ink_parse_thread_stmt(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t source_start = parser->source_offset;
    size_t source_end = 0;

    INK_PARSER_RULE(node, ink_parse_thread_expr, parser);
    source_end = node ? node->end_offset : parser->source_offset;

    ink_parser_expect_stmt_end(parser);
    return ink_syntax_node_unary(INK_NODE_THREAD_STMT, source_start, source_end,
                                 node, parser->arena);
}

static struct ink_syntax_node *ink_parse_expr_stmt(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t source_start = parser->source_offset;
    size_t source_end = 0;

    INK_PARSER_RULE(node, ink_parse_expr, parser);
    source_end = node ? node->end_offset : parser->source_offset;

    ink_parser_expect_stmt_end(parser);
    return ink_syntax_node_unary(INK_NODE_EXPR_STMT, source_start, source_end,
                                 node, parser->arena);
}

static struct ink_syntax_node *ink_parse_logic_stmt(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;

    ink_parser_push_scanner(parser, INK_GRAMMAR_EXPRESSION);
    ink_parser_advance(parser);

    if (ink_parser_check(parser, INK_TT_KEYWORD_TEMP)) {
        INK_PARSER_RULE(node, ink_parse_temp_decl, parser);
    } else if (ink_parser_check(parser, INK_TT_KEYWORD_RETURN)) {
        INK_PARSER_RULE(node, ink_parse_return_stmt, parser);
    } else {
        INK_PARSER_RULE(node, ink_parse_expr_stmt, parser);
    }

    ink_parser_pop_scanner(parser);
    return node;
}

static struct ink_syntax_node *ink_parse_sequence(struct ink_parser *parser,
                                                  struct ink_syntax_node *expr)
{
    struct ink_syntax_node *node = NULL;
    struct ink_parser_scratch *scratch = &parser->scratch;
    const size_t scratch_offset = scratch->count;
    const size_t source_start = parser->source_offset;
    static const enum ink_token_type token_set[] = {
        INK_TT_LEFT_BRACE,  INK_TT_LEFT_ARROW, INK_TT_RIGHT_BRACE,
        INK_TT_RIGHT_ARROW, INK_TT_PIPE,       INK_TT_NL,
        INK_TT_EOF,
    };

    if (expr) {
        INK_PARSER_RULE(node, ink_parse_content, parser, token_set);
        ink_parser_scratch_push(scratch, node);
    } else {
        INK_PARSER_RULE(node, ink_parse_content, parser, token_set);

        if (!ink_parser_check(parser, INK_TT_PIPE)) {
            return NULL;
        }

        ink_parser_advance(parser);
        ink_parser_scratch_push(scratch, node);
    }
    while (!ink_parser_check(parser, INK_TT_EOF) &&
           !ink_parser_check(parser, INK_TT_NL) &&
           !ink_parser_check(parser, INK_TT_RIGHT_BRACE)) {
        INK_PARSER_RULE(node, ink_parse_content, parser, token_set);
        ink_parser_scratch_push(scratch, node);

        if (ink_parser_check(parser, INK_TT_PIPE)) {
            ink_parser_advance(parser);
        }
    }
    return ink_syntax_node_sequence(INK_NODE_SEQUENCE_EXPR, source_start,
                                    parser->source_offset, scratch_offset,
                                    scratch, parser->arena);
}

static struct ink_syntax_node *
ink_parse_conditional_branch(struct ink_parser *parser)
{
    enum ink_syntax_node_type node_type;
    struct ink_syntax_node *node = NULL;
    const size_t source_start = parser->source_offset;

    if (ink_parser_eat(parser, INK_TT_KEYWORD_ELSE)) {
        node_type = INK_NODE_CONDITIONAL_ELSE_BRANCH;
    } else {
        node_type = INK_NODE_CONDITIONAL_BRANCH;

        INK_PARSER_RULE(node, ink_parse_expr, parser);
        if (node == NULL) {
            return NULL;
        }
    }
    if (!ink_parser_eat(parser, INK_TT_COLON)) {
        return NULL;
    }
    if (ink_parser_check(parser, INK_TT_NL)) {
        ink_parser_advance(parser);
    }
    return ink_syntax_node_binary(node_type, source_start,
                                  parser->source_offset, node, NULL,
                                  parser->arena);
}

static struct ink_syntax_node *
ink_parse_conditional(struct ink_parser *parser, struct ink_syntax_node *expr)
{
    struct ink_parser_node_context context;
    struct ink_parser_scratch *scratch = &parser->scratch;
    struct ink_syntax_node *node = NULL;
    const size_t scratch_offset = scratch->count;
    const size_t source_start = parser->source_offset;

    ink_parser_node_context_init(parser, &context);

    context.is_conditional = true;

    while (!ink_parser_check(parser, INK_TT_EOF) &&
           !ink_parser_check(parser, INK_TT_RIGHT_BRACE)) {
        INK_PARSER_RULE(node, ink_parse_stmt, parser, &context);
        ink_parser_scratch_push(scratch, node);
    }

    ink_parser_collect_choices(parser, &context, 0);
    node = ink_parser_collect_block(parser, &context);
    if (node != NULL) {
        ink_parser_scratch_push(scratch, node);
    }

    ink_parser_node_context_deinit(&context);
    return ink_syntax_node_sequence(INK_NODE_CONDITIONAL_CONTENT, source_start,
                                    parser->source_offset, scratch_offset,
                                    scratch, parser->arena);
}

static struct ink_syntax_node *ink_parse_inline_logic(struct ink_parser *parser)
{
    bool is_inline_seq = false;
    struct ink_syntax_node *node = NULL;
    struct ink_syntax_node *expr = NULL;
    const size_t source_start = parser->source_offset;

    ink_parser_push_scanner(parser, INK_GRAMMAR_EXPRESSION);
    ink_parser_advance(parser);

    if (ink_parser_check(parser, INK_TT_NL)) {
        ink_parser_advance(parser);
        INK_PARSER_RULE(node, ink_parse_conditional, parser, NULL);
        ink_parser_pop_scanner(parser);
        ink_parser_expect(parser, INK_TT_RIGHT_BRACE);
        return ink_syntax_node_binary(INK_NODE_INLINE_LOGIC, source_start,
                                      parser->source_offset, NULL, node,
                                      parser->arena);
    }

    INK_PARSER_RULE(expr, ink_parse_expr, parser);
    if (expr) {
        switch (parser->token.type) {
        case INK_TT_COLON: {
            ink_parser_advance(parser);
            ink_parser_push_scanner(parser, INK_GRAMMAR_CONTENT);

            if (ink_parser_check(parser, INK_TT_NL)) {
                ink_parser_advance(parser);
                INK_PARSER_RULE(node, ink_parse_conditional, parser, expr);
            } else {
                INK_PARSER_RULE(node, ink_parse_sequence, parser, expr);
            }

            ink_parser_pop_scanner(parser);
            break;
        }
        case INK_TT_RIGHT_BRACE: {
            node = expr;
            expr = NULL;
            break;
        }
        case INK_TT_PIPE: {
            is_inline_seq = true;
            break;
        }
        default:
            break;
        }
    } else {
        is_inline_seq = true;
    }
    if (is_inline_seq) {
        ink_parser_rewind_scanner(parser);
        ink_parser_push_scanner(parser, INK_GRAMMAR_CONTENT);
        ink_parser_advance(parser);
        ink_parser_advance(parser);
        INK_PARSER_RULE(node, ink_parse_sequence, parser, NULL);
        ink_parser_pop_scanner(parser);
    }

    ink_parser_pop_scanner(parser);
    ink_parser_expect(parser, INK_TT_RIGHT_BRACE);
    return ink_syntax_node_binary(INK_NODE_INLINE_LOGIC, source_start,
                                  parser->source_offset, expr, node,
                                  parser->arena);
}

static struct ink_syntax_node *
ink_parse_content(struct ink_parser *parser,
                  const enum ink_token_type *token_set)
{
    struct ink_syntax_node *node = NULL;
    struct ink_parser_scratch *scratch = &parser->scratch;
    const size_t scratch_offset = scratch->count;
    const size_t source_start = parser->source_offset;

    for (;;) {
        if (!ink_parser_check_many(parser, token_set)) {
            INK_PARSER_RULE(node, ink_parse_string, parser, token_set);
        } else {
            switch (parser->token.type) {
            case INK_TT_LEFT_BRACE: {
                INK_PARSER_RULE(node, ink_parse_inline_logic, parser);
                break;
            }
            case INK_TT_RIGHT_ARROW: {
                INK_PARSER_RULE(node, ink_parse_divert_stmt, parser);
                break;
            }
            case INK_TT_LEFT_ARROW: {
                INK_PARSER_RULE(node, ink_parse_thread_expr, parser);
                break;
            }
            case INK_TT_GLUE: {
                INK_PARSER_RULE(node, ink_parse_glue, parser);
                break;
            }
            default:
                goto exit_loop;
            }
        }

        ink_parser_scratch_push(scratch, node);
    }
exit_loop:
    if (source_start == parser->source_offset) {
        return ink_syntax_node_leaf(INK_NODE_EMPTY_CONTENT, source_start,
                                    source_start, parser->arena);
    }
    return ink_syntax_node_sequence(INK_NODE_CONTENT, source_start,
                                    parser->source_offset, scratch_offset,
                                    scratch, parser->arena);
}

static struct ink_syntax_node *ink_parse_content_stmt(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    static const enum ink_token_type token_set[] = {
        INK_TT_LEFT_BRACE,  INK_TT_LEFT_ARROW, INK_TT_RIGHT_BRACE,
        INK_TT_RIGHT_ARROW, INK_TT_GLUE,       INK_TT_NL,
        INK_TT_EOF,
    };
    const size_t source_start = parser->source_offset;
    size_t source_end = 0;

    INK_PARSER_RULE(node, ink_parse_content, parser, token_set);
    source_end = node ? node->end_offset : parser->source_offset;

    if (ink_parser_check(parser, INK_TT_NL)) {
        ink_parser_advance(parser);
    }
    return ink_syntax_node_unary(INK_NODE_CONTENT_STMT, source_start,
                                 source_end, node, parser->arena);
}

static struct ink_syntax_node *
ink_parse_choice_content(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    struct ink_parser_scratch *scratch = &parser->scratch;
    const size_t scratch_offset = scratch->count;
    const size_t source_start = parser->source_offset;
    static const enum ink_token_type token_set[] = {
        INK_TT_LEFT_BRACE,  INK_TT_LEFT_ARROW,    INK_TT_LEFT_BRACKET,
        INK_TT_RIGHT_BRACE, INK_TT_RIGHT_BRACKET, INK_TT_RIGHT_ARROW,
        INK_TT_NL,          INK_TT_EOF,
    };

    INK_PARSER_RULE(node, ink_parse_string, parser, token_set);
    if (node) {
        node->type = INK_NODE_CHOICE_START_EXPR;
        ink_parser_scratch_push(scratch, node);
    }
    if (ink_parser_check(parser, INK_TT_LEFT_BRACKET)) {
        ink_parser_advance(parser);
        ink_parser_eat(parser, INK_TT_WHITESPACE);

        if (!ink_parser_check(parser, INK_TT_RIGHT_BRACKET)) {
            INK_PARSER_RULE(node, ink_parse_string, parser, token_set);
            if (node) {
                node->type = INK_NODE_CHOICE_OPTION_EXPR;
                ink_parser_scratch_push(scratch, node);
            }
        }

        ink_parser_expect(parser, INK_TT_RIGHT_BRACKET);

        if (!ink_parser_check_many(parser, token_set)) {
            INK_PARSER_RULE(node, ink_parse_string, parser, token_set);
            if (node) {
                node->type = INK_NODE_CHOICE_INNER_EXPR;
                ink_parser_scratch_push(scratch, node);
            }
        }
    }
    return ink_syntax_node_sequence(INK_NODE_CHOICE_EXPR, source_start,
                                    parser->source_offset, scratch_offset,
                                    scratch, parser->arena);
}

static struct ink_syntax_node *
ink_parse_choice(struct ink_parser *parser,
                 struct ink_parser_node_context *context)
{
    struct ink_syntax_node *node = NULL;
    const enum ink_token_type token_type = parser->token.type;
    const size_t source_start = parser->source_offset;
    size_t source_end = 0;

    context->choice_level = ink_parser_eat_many(parser, token_type, true);
    INK_PARSER_RULE(node, ink_parse_choice_content, parser);

    source_end = node ? node->end_offset : parser->source_offset;
    if (ink_parser_check(parser, INK_TT_NL)) {
        ink_parser_advance(parser);
    }
    return ink_syntax_node_binary(ink_branch_type(token_type), source_start,
                                  source_end, node, NULL, parser->arena);
}

static struct ink_syntax_node *
ink_parse_gather(struct ink_parser *parser,
                 struct ink_parser_node_context *context)
{
    const enum ink_token_type token_type = parser->token.type;
    const size_t source_start = parser->source_offset;
    size_t source_end = 0;

    context->choice_level = ink_parser_eat_many(parser, token_type, true);

    while (!ink_parser_check(parser, INK_TT_EOF) &&
           !ink_parser_check(parser, INK_TT_NL)) {
        ink_parser_advance(parser);
    }

    source_end = parser->source_offset;
    if (ink_parser_check(parser, INK_TT_NL)) {
        ink_parser_advance(parser);
    }
    return ink_syntax_node_unary(INK_NODE_GATHER_STMT, source_start, source_end,
                                 NULL, parser->arena);
}

static struct ink_syntax_node *
ink_parse_list_element_def(struct ink_parser *parser)
{
    struct ink_syntax_node *lhs = NULL;
    struct ink_syntax_node *rhs = NULL;
    const size_t source_start = parser->source_offset;

    if (ink_parser_check(parser, INK_TT_LEFT_PAREN)) {
        ink_parser_advance(parser);
        INK_PARSER_RULE(lhs, ink_parse_identifier, parser);
        ink_parser_expect(parser, INK_TT_RIGHT_PAREN);
        return ink_syntax_node_unary(INK_NODE_SELECTED_LIST_ELEMENT,
                                     source_start, parser->source_offset, lhs,
                                     parser->arena);
    }

    INK_PARSER_RULE(lhs, ink_parse_identifier, parser);

    if (ink_parser_check(parser, INK_TT_EQUAL)) {
        ink_parser_advance(parser);
        INK_PARSER_RULE(rhs, ink_parse_expr, parser);
        return ink_syntax_node_binary(INK_NODE_ASSIGN_EXPR, source_start,
                                      parser->source_offset, lhs, rhs,
                                      parser->arena);
    }
    return lhs;
}

static struct ink_syntax_node *ink_parse_list(struct ink_parser *parser)
{
    int arg_count = 0;
    struct ink_syntax_node *node = NULL;
    struct ink_parser_scratch *scratch = &parser->scratch;
    const size_t scratch_offset = scratch->count;
    const size_t source_start = parser->source_offset;

    for (;;) {
        INK_PARSER_RULE(node, ink_parse_list_element_def, parser);
        if (arg_count == INK_PARSER_ARGS_MAX) {
            ink_parser_error(parser, "Too many arguments");
            break;
        }

        arg_count++;
        ink_parser_scratch_push(scratch, node);

        if (!ink_parser_check(parser, INK_TT_COMMA)) {
            break;
        }
        ink_parser_advance(parser);
    }
    return ink_syntax_node_sequence(INK_NODE_ARG_LIST, source_start,
                                    parser->source_offset, scratch_offset,
                                    scratch, parser->arena);
}

static struct ink_syntax_node *ink_parse_list_decl(struct ink_parser *parser)
{
    struct ink_syntax_node *lhs = NULL;
    struct ink_syntax_node *rhs = NULL;
    const size_t source_start = parser->source_offset;
    size_t source_end = 0;

    ink_parser_push_scanner(parser, INK_GRAMMAR_EXPRESSION);
    ink_parser_advance(parser);
    INK_PARSER_RULE(lhs, ink_parse_identifier, parser);
    ink_parser_expect(parser, INK_TT_EQUAL);
    INK_PARSER_RULE(rhs, ink_parse_list, parser);
    source_end = rhs ? rhs->end_offset : parser->source_offset;
    ink_parser_pop_scanner(parser);
    ink_parser_expect_stmt_end(parser);
    return ink_syntax_node_binary(INK_NODE_LIST_DECL, source_start, source_end,
                                  lhs, rhs, parser->arena);
}

static struct ink_syntax_node *
ink_parse_var(struct ink_parser *parser, enum ink_syntax_node_type node_type)
{
    struct ink_syntax_node *lhs = NULL;
    struct ink_syntax_node *rhs = NULL;
    const size_t source_start = parser->source_offset;
    size_t source_end = 0;

    ink_parser_push_scanner(parser, INK_GRAMMAR_EXPRESSION);
    ink_parser_advance(parser);
    INK_PARSER_RULE(lhs, ink_parse_identifier, parser);
    ink_parser_expect(parser, INK_TT_EQUAL);
    INK_PARSER_RULE(rhs, ink_parse_expr, parser);
    source_end = rhs ? rhs->end_offset : parser->source_offset;
    ink_parser_pop_scanner(parser);
    ink_parser_expect_stmt_end(parser);
    return ink_syntax_node_binary(node_type, source_start, source_end, lhs, rhs,
                                  parser->arena);
}

static struct ink_syntax_node *ink_parse_var_decl(struct ink_parser *parser)
{
    return ink_parse_var(parser, INK_NODE_VAR_DECL);
}

static struct ink_syntax_node *ink_parse_const_decl(struct ink_parser *parser)
{
    return ink_parse_var(parser, INK_NODE_CONST_DECL);
}

static struct ink_syntax_node *
ink_parse_argument_list(struct ink_parser *parser)
{
    size_t arg_count = 0;
    struct ink_syntax_node *node = NULL;
    struct ink_parser_scratch *scratch = &parser->scratch;
    const size_t scratch_offset = scratch->count;
    const size_t source_start = ink_parser_expect(parser, INK_TT_LEFT_PAREN);

    if (!ink_parser_check(parser, INK_TT_RIGHT_PAREN)) {
        for (;;) {
            node = ink_parse_expr(parser);
            if (arg_count == INK_PARSER_ARGS_MAX) {
                ink_parser_error(parser, "Too many arguments");
                break;
            }

            arg_count++;
            ink_parser_scratch_push(scratch, node);

            if (!ink_parser_check(parser, INK_TT_COMMA)) {
                break;
            }
            ink_parser_advance(parser);
        }
    }

    ink_parser_expect(parser, INK_TT_RIGHT_PAREN);
    return ink_syntax_node_sequence(INK_NODE_ARG_LIST, source_start,
                                    parser->source_offset, scratch_offset,
                                    scratch, parser->arena);
}

static struct ink_syntax_node *
ink_parse_parameter_decl(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;

    if (ink_parser_check(parser, INK_TT_KEYWORD_REF)) {
        ink_parser_advance(parser);
        node = ink_parse_identifier(parser);
        node->type = INK_NODE_REF_PARAM_DECL;
    } else {
        node = ink_parse_identifier(parser);
        node->type = INK_NODE_PARAM_DECL;
    }
    return node;
}

static struct ink_syntax_node *
ink_parse_parameter_list(struct ink_parser *parser)
{
    size_t arg_count = 0;
    struct ink_syntax_node *node = NULL;
    struct ink_parser_scratch *scratch = &parser->scratch;
    const size_t scratch_offset = scratch->count;
    const size_t source_start = ink_parser_expect(parser, INK_TT_LEFT_PAREN);

    if (!ink_parser_check(parser, INK_TT_RIGHT_PAREN)) {
        for (;;) {
            if (arg_count == INK_PARSER_ARGS_MAX) {
                ink_parser_error(parser, "Too many parameters");
                break;
            } else {
                arg_count++;
            }

            node = ink_parse_parameter_decl(parser);
            ink_parser_scratch_push(scratch, node);

            if (ink_parser_check(parser, INK_TT_COMMA)) {
                ink_parser_advance(parser);
            } else {
                break;
            }
        }
    }

    ink_parser_expect(parser, INK_TT_RIGHT_PAREN);
    return ink_syntax_node_sequence(INK_NODE_PARAM_LIST, source_start,
                                    parser->source_offset, scratch_offset,
                                    scratch, parser->arena);
}

static struct ink_syntax_node *ink_parse_knot_decl(struct ink_parser *parser)
{
    enum ink_syntax_node_type node_type;
    struct ink_syntax_node *node = NULL;
    struct ink_parser_scratch *scratch = &parser->scratch;
    const size_t scratch_offset = scratch->count;
    const size_t source_start = parser->source_offset;

    ink_parser_advance(parser);

    if (!ink_parser_check(parser, INK_TT_EQUAL)) {
        node_type = INK_NODE_STITCH_PROTO;
    } else {
        node_type = INK_NODE_KNOT_PROTO;

        while (ink_parser_check(parser, INK_TT_EQUAL)) {
            ink_parser_advance(parser);
        }
    }

    ink_parser_push_scanner(parser, INK_GRAMMAR_EXPRESSION);
    ink_parser_eat(parser, INK_TT_WHITESPACE);

    if (ink_scanner_try_keyword(&parser->scanner, &parser->token,
                                INK_TT_KEYWORD_FUNCTION)) {
        ink_parser_advance(parser);
    }

    INK_PARSER_RULE(node, ink_parse_identifier, parser);
    ink_parser_scratch_push(scratch, node);

    if (ink_parser_check(parser, INK_TT_LEFT_PAREN)) {
        INK_PARSER_RULE(node, ink_parse_parameter_list, parser);
        ink_parser_scratch_push(scratch, node);
    }
    while (ink_parser_check(parser, INK_TT_EQUAL) ||
           ink_parser_check(parser, INK_TT_EQUAL_EQUAL)) {
        ink_parser_advance(parser);
    }

    ink_parser_pop_scanner(parser);
    ink_parser_expect_stmt_end(parser);
    return ink_syntax_node_sequence(node_type, source_start,
                                    parser->source_offset, scratch_offset,
                                    scratch, parser->arena);
}

static struct ink_syntax_node *
ink_parse_stmt(struct ink_parser *parser,
               struct ink_parser_node_context *context)
{
    struct ink_syntax_node *node = NULL;

    ink_parser_eat(parser, INK_TT_WHITESPACE);

    switch (parser->token.type) {
    case INK_TT_EOF: {
        assert(node != NULL);
        break;
    }
    case INK_TT_STAR:
    case INK_TT_PLUS: {
        INK_PARSER_RULE(node, ink_parse_choice, parser, context);
        break;
    }
    case INK_TT_MINUS: {
        if (context->is_conditional) {
            ink_parser_push_scanner(parser, INK_GRAMMAR_EXPRESSION);
            ink_parser_advance(parser);
            INK_PARSER_RULE(node, ink_parse_conditional_branch, parser);

            if (node == NULL) {
                ink_parser_rewind_scanner(parser);
                ink_parser_push_scanner(parser, INK_GRAMMAR_CONTENT);
                ink_parser_advance(parser);
                INK_PARSER_RULE(node, ink_parse_gather, parser, context);
                ink_parser_pop_scanner(parser);
            }

            ink_parser_pop_scanner(parser);
        } else {
            INK_PARSER_RULE(node, ink_parse_gather, parser, context);
        }
        break;
    }
    case INK_TT_TILDE: {
        INK_PARSER_RULE(node, ink_parse_logic_stmt, parser);
        break;
    }
    case INK_TT_LEFT_ARROW: {
        INK_PARSER_RULE(node, ink_parse_thread_stmt, parser);
        break;
    }
    case INK_TT_RIGHT_ARROW: {
        INK_PARSER_RULE(node, ink_parse_divert_stmt, parser);
        break;
    }
    case INK_TT_EQUAL: {
        if (!context->is_conditional) {
            INK_PARSER_RULE(node, ink_parse_knot_decl, parser);
        } else {
            INK_PARSER_RULE(node, ink_parse_content_stmt, parser);
        }
        break;
    }
    default:
        if (ink_scanner_try_keyword(&parser->scanner, &parser->token,
                                    INK_TT_KEYWORD_CONST)) {
            INK_PARSER_RULE(node, ink_parse_const_decl, parser);
        } else if (ink_scanner_try_keyword(&parser->scanner, &parser->token,
                                           INK_TT_KEYWORD_VAR)) {
            INK_PARSER_RULE(node, ink_parse_var_decl, parser);
        } else if (ink_scanner_try_keyword(&parser->scanner, &parser->token,
                                           INK_TT_KEYWORD_LIST)) {
            INK_PARSER_RULE(node, ink_parse_list_decl, parser);
        } else {
            INK_PARSER_RULE(node, ink_parse_content_stmt, parser);
        }
        break;
    }
    switch (node->type) {
    case INK_NODE_CONDITIONAL_BRANCH:
    case INK_NODE_CONDITIONAL_ELSE_BRANCH: {
        ink_parser_handle_conditional_branch(parser, context, node);
        break;
    }
    case INK_NODE_CHOICE_STAR_STMT:
    case INK_NODE_CHOICE_PLUS_STMT: {
        ink_parser_handle_choice_branch(parser, context, node);
        break;
    }
    case INK_NODE_GATHER_STMT: {
        ink_parser_handle_gather(parser, context, &node);
        break;
    }
    case INK_NODE_KNOT_PROTO: {
        ink_parser_handle_knot(parser, context);
        break;
    }
    case INK_NODE_STITCH_PROTO: {
        ink_parser_handle_stitch(parser, context);
        break;
    }
    default:
        ink_parser_handle_content(parser, context, node);
        break;
    }
    return node;
}

static struct ink_syntax_node *ink_parse_file(struct ink_parser *parser)
{
    struct ink_parser_node_context context;
    struct ink_syntax_node *node = NULL;
    struct ink_parser_scratch *scratch = &parser->scratch;
    const size_t scratch_offset = scratch->count;
    const size_t source_offset = parser->source_offset;

    ink_parser_node_context_init(parser, &context);

    while (!ink_parser_check(parser, INK_TT_EOF)) {
        INK_PARSER_RULE(node, ink_parse_stmt, parser, &context);
        ink_parser_scratch_push(scratch, node);
    }

    node = ink_parser_collect_knot(parser, &context);
    if (node != NULL) {
        ink_parser_scratch_push(scratch, node);
    }

    ink_parser_node_context_deinit(&context);
    return ink_syntax_node_sequence(INK_NODE_FILE, source_offset,
                                    parser->source_offset, scratch_offset,
                                    scratch, parser->arena);
}

/**
 * Parse a source file and output a syntax tree.
 */
int ink_parse(const struct ink_source *source,
              struct ink_syntax_tree *syntax_tree, struct ink_arena *arena,
              int flags)
{
    int rc;
    struct ink_parser parser;

    rc = ink_parser_init(&parser, source, syntax_tree, arena, flags);
    if (rc < 0) {
        return rc;
    }

    ink_parser_next_token(&parser);

    syntax_tree->root = ink_parse_file(&parser);
    if (syntax_tree->root) {
        rc = INK_E_OK;
    } else {
        rc = -INK_E_PARSE_FAIL;
    }

    ink_parser_deinit(&parser);
    return rc;
}
