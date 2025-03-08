#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "ast.h"
#include "common.h"
#include "hashmap.h"
#include "parse.h"
#include "scanner.h"
#include "token.h"
#include "vec.h"

#define INK_PARSER_ARGS_MAX (255)
#define INK_PARSE_DEPTH (128)
#define INK_PARSER_CACHE_LOAD_MAX (80ul)

struct ink_parser_state {
    size_t choice_level;
    size_t scratch_offset;
    size_t source_offset;
};

struct ink_parser_cache_key {
    size_t source_offset;
    void *rule_address;
};

INK_HASHMAP_T(ink_parser_cache, struct ink_parser_cache_key,
              struct ink_ast_node *)
INK_VEC_T(ink_parser_scratch, struct ink_ast_node *)
INK_VEC_T(ink_parser_stack, struct ink_parser_state)

static uint32_t ink_parser_cache_key_hash(const void *key, size_t length)
{
    return ink_fnv32a((uint8_t *)key, length);
}

static bool ink_parser_cache_key_cmp(const void *lhs, const void *rhs)
{
    const struct ink_parser_cache_key *const key_lhs = lhs;
    const struct ink_parser_cache_key *const key_rhs = rhs;

    return (key_lhs->source_offset == key_rhs->source_offset) &&
           (key_lhs->rule_address == key_rhs->rule_address);
}

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

/**
 * Memoize an AST node.
 *
 * Reserved for another day.
 */
#define INK_PARSER_MEMOIZE(node, rule, ...)                                    \
    do {                                                                       \
        const struct ink_parser_cache_key key = {                              \
            .source_offset = parser->token.start_offset,                       \
            .rule_address = (void *)rule,                                      \
        };                                                                     \
                                                                               \
        if (parser->flags & INK_F_CACHING) {                                   \
            int rc = ink_parser_cache_lookup(&parser->cache, key, &node);      \
            if (rc < 0) {                                                      \
                node = INK_DISPATCH(rule, __VA_ARGS__);                        \
                ink_parser_cache_insert(&parser->cache, key, node);            \
            } else {                                                           \
                ink_trace("Parser cache hit!");                                \
                parser->token.start_offset = node->end_offset;                 \
                ink_parser_advance(parser);                                    \
            }                                                                  \
        } else {                                                               \
            node = INK_DISPATCH(rule, __VA_ARGS__);                            \
        }                                                                      \
    } while (0)

struct ink_parser_node_context {
    struct ink_ast_node *node;
    bool is_conditional;
    bool is_block_created;
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
 */
struct ink_parser {
    struct ink_arena *arena;
    struct ink_scanner scanner;
    struct ink_parser_scratch scratch;
    struct ink_parser_cache cache;
    struct ink_token token;
    struct ink_ast *tree;
    bool panic_mode;
    int flags;
};

/**
 * Precedence level for expressions.
 */
enum ink_precedence {
    INK_PREC_NONE = 0,
    INK_PREC_ASSIGN,
    INK_PREC_LOGICAL_OR,
    INK_PREC_LOGICAL_AND,
    INK_PREC_COMPARISON,
    INK_PREC_TERM,
    INK_PREC_FACTOR,
};

/**
 * Create a syntax node sequence from a range of nodes from the scratch buffer.
 */
static struct ink_ast_seq *
ink_ast_seq_from_scratch(struct ink_parser_scratch *scratch,
                         size_t start_offset, size_t end_offset,
                         struct ink_arena *arena)
{
    struct ink_ast_seq *seq = NULL;
    size_t seq_index = 0;

    if (start_offset < end_offset) {
        const size_t span = end_offset - start_offset;
        const size_t seq_size = sizeof(*seq) + span * sizeof(seq->nodes);

        assert(span > 0);

        seq = ink_arena_allocate(arena, seq_size);
        if (!seq) {
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

static inline enum ink_ast_node_type
ink_token_prefix_type(enum ink_token_type type)
{
    switch (type) {
    case INK_TT_KEYWORD_NOT:
    case INK_TT_BANG:
        return INK_AST_NOT_EXPR;
    case INK_TT_MINUS:
        return INK_AST_NEGATE_EXPR;
    default:
        return INK_AST_INVALID;
    }
}

static inline enum ink_ast_node_type
ink_token_infix_type(enum ink_token_type type)
{
    switch (type) {
    case INK_TT_AMP_AMP:
    case INK_TT_KEYWORD_AND:
        return INK_AST_AND_EXPR;
    case INK_TT_PIPE_PIPE:
    case INK_TT_KEYWORD_OR:
        return INK_AST_OR_EXPR;
    case INK_TT_PERCENT:
    case INK_TT_KEYWORD_MOD:
        return INK_AST_MOD_EXPR;
    case INK_TT_PLUS:
        return INK_AST_ADD_EXPR;
    case INK_TT_MINUS:
        return INK_AST_SUB_EXPR;
    case INK_TT_STAR:
        return INK_AST_MUL_EXPR;
    case INK_TT_SLASH:
        return INK_AST_DIV_EXPR;
    case INK_TT_QUESTION:
        return INK_AST_CONTAINS_EXPR;
    case INK_TT_EQUAL:
        return INK_AST_ASSIGN_STMT;
    case INK_TT_EQUAL_EQUAL:
        return INK_AST_EQUAL_EXPR;
    case INK_TT_BANG_EQUAL:
        return INK_AST_NOT_EQUAL_EXPR;
    case INK_TT_LESS_THAN:
        return INK_AST_LESS_EXPR;
    case INK_TT_GREATER_THAN:
        return INK_AST_GREATER_EXPR;
    case INK_TT_LESS_EQUAL:
        return INK_AST_LESS_EQUAL_EXPR;
    case INK_TT_GREATER_EQUAL:
        return INK_AST_GREATER_EQUAL_EXPR;
    default:
        return INK_AST_INVALID;
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

static inline enum ink_ast_node_type ink_branch_type(enum ink_token_type type)
{
    switch (type) {
    case INK_TT_STAR:
        return INK_AST_CHOICE_STAR_STMT;
    case INK_TT_PLUS:
        return INK_AST_CHOICE_PLUS_STMT;
    default:
        return INK_AST_INVALID;
    }
}

/**
 * Create a leaf AST node.
 */
static inline struct ink_ast_node *
ink_ast_node_leaf(enum ink_ast_node_type type, size_t source_start,
                  size_t source_end, struct ink_arena *arena)
{
    return ink_ast_node_new(type, source_start, source_end, NULL, NULL, NULL,
                            arena);
}

/**
 * Create a unary AST node.
 */
static inline struct ink_ast_node *
ink_ast_node_unary(enum ink_ast_node_type type, size_t source_start,
                   size_t source_end, struct ink_ast_node *lhs,
                   struct ink_arena *arena)
{
    return ink_ast_node_new(type, source_start, source_end, lhs, NULL, NULL,
                            arena);
}

/**
 * Create a binary AST node.
 */
static inline struct ink_ast_node *
ink_ast_node_binary(enum ink_ast_node_type type, size_t source_start,
                    size_t source_end, struct ink_ast_node *lhs,
                    struct ink_ast_node *rhs, struct ink_arena *arena)
{
    return ink_ast_node_new(type, source_start, source_end, lhs, rhs, NULL,
                            arena);
}

/**
 * Create an AST node with a variable number of children.
 */
static inline struct ink_ast_node *
ink_ast_node_sequence(enum ink_ast_node_type type, size_t source_start,
                      size_t source_end, size_t scratch_offset,
                      struct ink_parser_scratch *scratch,
                      struct ink_arena *arena)
{
    struct ink_ast_seq *seq = NULL;

    if (scratch->count != scratch_offset) {
        seq = ink_ast_seq_from_scratch(scratch, scratch_offset, scratch->count,
                                       arena);
        if (!seq) {
            return NULL;
        }
    }
    return ink_ast_node_new(type, source_start, source_end, NULL, NULL, seq,
                            arena);
}

static void
ink_parser_node_context_init(struct ink_parser *parser,
                             struct ink_parser_node_context *context)
{
    context->node = NULL;
    context->is_conditional = false;
    context->is_block_created = false;
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
 * Initialize the parser state.
 *
 * No memory is allocated here, as it is performed lazily.
 */
static void ink_parser_init(struct ink_parser *parser, struct ink_ast *tree,
                            struct ink_arena *arena, int flags)
{
    const struct ink_scanner scanner = {
        .source_bytes = tree->source_bytes,
        .is_line_start = true,
    };

    parser->arena = arena;
    parser->scanner = scanner;
    parser->token.type = INK_TT_ERROR;
    parser->token.start_offset = 0;
    parser->token.end_offset = 0;
    parser->panic_mode = false;
    parser->flags = flags;
    parser->tree = tree;

    ink_parser_scratch_init(&parser->scratch);
    ink_parser_cache_init(&parser->cache, INK_PARSER_CACHE_LOAD_MAX,
                          ink_parser_cache_key_hash, ink_parser_cache_key_cmp);
}

/**
 * Cleanup the parser state.
 */
static void ink_parser_deinit(struct ink_parser *parser)
{
    ink_parser_cache_deinit(&parser->cache);
    ink_parser_scratch_deinit(&parser->scratch);
}

/**
 * Push a lexical analysis context onto the parser.
 */
static inline void ink_parser_push_scanner(struct ink_parser *parser,
                                           enum ink_grammar_type type)
{
    ink_scanner_push(&parser->scanner, type, parser->token.start_offset);
}

/**
 * Pop a lexical analysis context from the parser.
 */
static inline void ink_parser_pop_scanner(struct ink_parser *parser)
{
    ink_scanner_pop(&parser->scanner);
}

/**
 * Rewind the scanner to the starting position of the current scanner mode.
 */
static inline void ink_parser_rewind_scanner(struct ink_parser *parser)
{
    struct ink_scanner_mode *const mode = ink_scanner_current(&parser->scanner);

    ink_scanner_rewind(&parser->scanner, mode->source_offset);
}

/**
 * Raise an error in the parser.
 */
static void *ink_parser_error(struct ink_parser *parser,
                              enum ink_ast_error_type type,
                              const struct ink_token *token)

{
    if (parser->panic_mode) {
        return NULL;
    }

    parser->panic_mode = true;

    const struct ink_ast_error err = {
        .type = type,
        .source_start = token->start_offset,
        .source_end = token->end_offset,
    };

    ink_ast_error_vec_push(&parser->tree->errors, err);
    return NULL;
}

/**
 * Advance the parser.
 *
 * Returns the previous token's start offset.
 */
static size_t ink_parser_advance(struct ink_parser *parser)
{
    struct ink_token *const token = &parser->token;
    const size_t offset = token->start_offset;

    for (;;) {
        ink_scanner_next(&parser->scanner, token);

        if (token->type == INK_TT_ERROR) {
            ink_parser_error(parser, INK_AST_E_UNEXPECTED_TOKEN, token);
        } else {
            break;
        }
    }
    return offset;
}

/**
 * Check if the current token matches a given token type.
 *
 * Returns a boolean indicating whether the current token's type matches.
 */
static inline bool ink_parser_check(struct ink_parser *parser,
                                    enum ink_token_type type)
{
    return parser->token.type == type;
}

static bool ink_parser_check_many(struct ink_parser *parser,
                                  const enum ink_token_type *token_set)
{
    /* TODO: Look into if this causes bugs. */
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
 * Consume the current token if it matches a given token type.
 *
 * Returns a boolean indicating whether the current token's type matches.
 */
static bool ink_parser_match(struct ink_parser *parser,
                             enum ink_token_type type)
{
    if (ink_parser_check(parser, type)) {
        ink_parser_advance(parser);
        return true;
    }
    return false;
}

/**
 * Synchronize the state of the parser after a panic.
 */
static void ink_parser_sync(struct ink_parser *parser)
{
    parser->panic_mode = false;

    while (!ink_is_sync_token(parser->token.type)) {
        ink_parser_advance(parser);
    }
}

/**
 * Expect a specific token type.
 *
 * A parse error will be added if the current token type does not match the
 * expected type.
 */
static size_t ink_parser_expect_token(struct ink_parser *parser,
                                      enum ink_token_type type)
{
    struct ink_scanner_mode *const mode = ink_scanner_current(&parser->scanner);
    size_t source_offset = parser->token.start_offset;

    if (mode->type == INK_GRAMMAR_EXPRESSION) {
        if (ink_parser_check(parser, INK_TT_WHITESPACE)) {
            source_offset = ink_parser_advance(parser);
        }
    }
    if (!ink_parser_check(parser, type)) {
        ink_parser_error(parser, INK_AST_E_UNEXPECTED_TOKEN, &parser->token);
        return source_offset;
    }

    ink_parser_advance(parser);
    return source_offset;
}

static size_t ink_parser_match_many(struct ink_parser *parser,
                                    enum ink_token_type type,
                                    bool ignore_whitespace)
{
    size_t count = 0;

    while (ink_parser_check(parser, type)) {
        count++;

        ink_parser_advance(parser);
        if (ignore_whitespace) {
            ink_parser_match(parser, INK_TT_WHITESPACE);
        }
    }
    return count;
}

/**
 * Close the current block, if present.
 */
static struct ink_ast_node *
ink_parser_collect_block(struct ink_parser *parser,
                         struct ink_parser_node_context *context)
{
    struct ink_parser_scratch *const scratch = &parser->scratch;
    struct ink_parser_stack *const open_blocks = &context->open_blocks;
    struct ink_ast_node *node = NULL;

    if (!ink_parser_stack_is_empty(open_blocks)) {
        const struct ink_parser_state block = ink_parser_stack_pop(open_blocks);
        struct ink_ast_node *const tmp = ink_parser_scratch_last(scratch);

        /** FIXME: Bounds checking for scratch. */
        node = ink_ast_node_sequence(INK_AST_BLOCK, block.source_offset,
                                     tmp->end_offset, block.scratch_offset,
                                     scratch, parser->arena);

        if (!ink_parser_scratch_is_empty(scratch) &&
            scratch->count > context->scratch_start) {
            struct ink_ast_node *const tmp = ink_parser_scratch_last(scratch);

            switch (tmp->type) {
            case INK_AST_CHOICE_STAR_STMT:
            case INK_AST_CHOICE_PLUS_STMT:
            case INK_AST_SWITCH_CASE:
            case INK_AST_IF_BRANCH:
            case INK_AST_ELSE_BRANCH: {
                tmp->rhs = node;
                node = tmp;

                ink_parser_scratch_pop(scratch);
                break;
            }
            default:
                context->is_block_created = true;
                break;
            }
        } else {
            context->is_block_created = true;
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
    struct ink_parser_scratch *const scratch = &parser->scratch;
    struct ink_parser_stack *const open_choices = &context->open_choices;
    struct ink_parser_stack *const open_blocks = &context->open_blocks;

    while (!ink_parser_stack_is_empty(open_choices)) {
        const struct ink_parser_state choice =
            ink_parser_stack_last(open_choices);
        struct ink_ast_node *tmp = NULL;

        assert(!ink_parser_stack_is_empty(open_blocks));

        if (choice.choice_level > choice_level) {
            ink_parser_stack_pop(open_choices);

            if (!ink_parser_stack_is_empty(open_blocks)) {
                const struct ink_parser_state block =
                    ink_parser_stack_last(open_blocks);

                if (choice.choice_level <= block.choice_level) {
                    tmp = ink_parser_collect_block(parser, context);
                    if (tmp != NULL) {
                        ink_parser_scratch_push(scratch, tmp);
                    }
                }
            }
            if (!ink_parser_stack_is_empty(open_choices)) {
                const struct ink_parser_state prev_choice =
                    ink_parser_stack_last(open_choices);

                if (choice.choice_level > choice_level &&
                    choice_level > prev_choice.choice_level) {
                    /* Handle non-sequentially increasing levels */
                    ink_parser_stack_emplace(open_choices, choice_level,
                                             choice.scratch_offset,
                                             choice.source_offset);
                    break;
                }
            }

            tmp = ink_ast_node_sequence(
                INK_AST_CHOICE_STMT, choice.source_offset,
                parser->token.start_offset, choice.scratch_offset, scratch,
                parser->arena);

            ink_parser_scratch_push(scratch, tmp);
        } else {
            break;
        }
    }
}

/**
 * Close an open stitch, if present.
 */
static struct ink_ast_node *
ink_parser_collect_stitch(struct ink_parser *parser,
                          struct ink_parser_node_context *context)
{
    ink_parser_collect_choices(parser, context, 0);

    struct ink_parser_scratch *const scratch = &parser->scratch;
    struct ink_ast_node *const body_node =
        ink_parser_collect_block(parser, context);

    if (!ink_parser_scratch_is_empty(scratch)) {
        struct ink_ast_node *const proto_node =
            ink_parser_scratch_last(scratch);

        if (proto_node->type == INK_AST_STITCH_PROTO) {
            const size_t end_offset =
                body_node ? body_node->end_offset : proto_node->end_offset;

            ink_parser_scratch_pop(scratch);
            return ink_ast_node_binary(INK_AST_STITCH_DECL,
                                       proto_node->start_offset, end_offset,
                                       proto_node, body_node, parser->arena);
        } else if (proto_node->type == INK_AST_FUNC_PROTO) {
            const size_t end_offset =
                body_node ? body_node->end_offset : proto_node->end_offset;

            ink_parser_scratch_pop(scratch);
            return ink_ast_node_binary(INK_AST_FUNC_DECL,
                                       proto_node->start_offset, end_offset,
                                       proto_node, body_node, parser->arena);
        }
    }
    return body_node;
}

/**
 * Close an open knot, if present.
 */
static struct ink_ast_node *
ink_parser_collect_knot(struct ink_parser *parser,
                        struct ink_parser_node_context *context)
{
    struct ink_parser_scratch *const scratch = &parser->scratch;
    struct ink_ast_node *node = NULL;

    if (!ink_parser_scratch_is_empty(scratch)) {
        struct ink_ast_node *const child_node =
            ink_parser_collect_stitch(parser, context);

        if (child_node) {
            ink_parser_scratch_push(scratch, child_node);
        }

        struct ink_ast_node *const proto_node =
            scratch->entries[context->knot_offset];

        if (proto_node->type == INK_AST_KNOT_PROTO) {
            const size_t source_start = proto_node->start_offset;
            const size_t source_end =
                child_node ? child_node->end_offset : proto_node->end_offset;
            struct ink_ast_seq *const seq =
                ink_ast_seq_from_scratch(scratch, context->knot_offset + 1,
                                         scratch->count, parser->arena);

            ink_parser_scratch_pop(scratch);
            node = ink_ast_node_unary(INK_AST_KNOT_DECL, source_start,
                                      source_end, proto_node, parser->arena);
            node->seq = seq;
        }
    }
    return node;
}

static void
ink_parser_handle_conditional_branch(struct ink_parser *parser,
                                     struct ink_parser_node_context *context,
                                     struct ink_ast_node *node)
{
    ink_parser_collect_choices(parser, context, 0);

    struct ink_parser_scratch *const scratch = &parser->scratch;
    struct ink_ast_node *const tmp = ink_parser_collect_block(parser, context);

    if (!tmp) {
        return;
    }

    ink_parser_scratch_push(scratch, tmp);
}

static void
ink_parser_handle_choice_branch(struct ink_parser *parser,
                                struct ink_parser_node_context *context,
                                struct ink_ast_node *node)
{
    int rc = -1;
    struct ink_parser_stack *const open_blocks = &context->open_blocks;
    struct ink_parser_stack *const open_choices = &context->open_choices;
    struct ink_parser_scratch *const scratch = &parser->scratch;
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
        const struct ink_parser_state choice =
            ink_parser_stack_last(open_choices);
        const struct ink_parser_state block =
            ink_parser_stack_last(open_blocks);
        struct ink_ast_node *tmp = NULL;

        if (choice_level > choice.choice_level) {
            if (block.choice_level < choice.choice_level) {
                ink_parser_stack_emplace(open_blocks, choice.choice_level,
                                         scratch->count,
                                         parser->token.start_offset);
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
                const struct ink_parser_state prev_block =
                    ink_parser_stack_last(open_blocks);

                if (prev_block.choice_level == choice_level) {
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
                                     struct ink_ast_node **node)
{
    int rc = -1;
    struct ink_parser_stack *const open_blocks = &context->open_blocks;
    struct ink_parser_stack *const open_choices = &context->open_choices;
    struct ink_parser_scratch *const scratch = &parser->scratch;
    const size_t gather_level = context->choice_level;
    const size_t source_start = parser->token.start_offset;

    if (ink_parser_stack_is_empty(open_blocks)) {
        rc = ink_parser_stack_emplace(open_blocks, gather_level - 1,
                                      scratch->count, (*node)->start_offset);
        if (rc < 0) {
            return;
        }
    } else if (!ink_parser_stack_is_empty(open_choices)) {
        const struct ink_parser_state choice =
            ink_parser_stack_last(open_choices);
        const struct ink_parser_state block =
            ink_parser_stack_last(open_blocks);

        if (gather_level > choice.choice_level &&
            block.choice_level < gather_level) {
            rc =
                ink_parser_stack_emplace(open_blocks, gather_level - 1,
                                         scratch->count, (*node)->start_offset);
            if (rc < 0) {
                return;
            }
        }
    }
    if (!ink_parser_stack_is_empty(open_choices) &&
        !ink_parser_scratch_is_empty(scratch)) {
        struct ink_ast_node *tmp = NULL;

        ink_parser_collect_choices(parser, context, gather_level - 1);

        tmp = ink_parser_scratch_last(scratch);
        if (tmp->type == INK_AST_CHOICE_STMT) {
            ink_parser_scratch_pop(scratch);
            *node = ink_ast_node_binary(INK_AST_GATHERED_CHOICE_STMT,
                                        tmp->start_offset, source_start, tmp,
                                        *node, parser->arena);
        }
        if (!ink_parser_stack_is_empty(open_blocks)) {
            const struct ink_parser_state block =
                ink_parser_stack_last(open_blocks);

            if (block.choice_level == gather_level) {
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
                                      struct ink_ast_node *node)
{
    int rc = -1;
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
        const struct ink_parser_state block =
            ink_parser_stack_last(open_blocks);
        const struct ink_parser_state choice =
            ink_parser_stack_last(open_choices);

        if (block.choice_level != choice.choice_level) {
            ink_parser_stack_emplace(open_blocks, choice.choice_level,
                                     scratch->count, node->start_offset);
        }
    }
}

static void ink_parser_handle_knot(struct ink_parser *parser,
                                   struct ink_parser_node_context *context)
{
    struct ink_parser_scratch *const scratch = &parser->scratch;
    struct ink_ast_node *const node = ink_parser_collect_knot(parser, context);

    if (node) {
        ink_parser_scratch_push(scratch, node);
    }

    context->knot_offset = scratch->count;
}

static void ink_parser_handle_stitch(struct ink_parser *parser,
                                     struct ink_parser_node_context *context)
{
    struct ink_parser_scratch *const scratch = &parser->scratch;
    struct ink_ast_node *const node =
        ink_parser_collect_stitch(parser, context);

    if (node) {
        ink_parser_scratch_push(scratch, node);
    }
}

static void ink_parser_handle_func(struct ink_parser *parser,
                                   struct ink_parser_node_context *context)
{
    ink_parser_handle_stitch(parser, context);
}

static struct ink_ast_node *ink_parse_content(struct ink_parser *,
                                              const enum ink_token_type *);
static struct ink_ast_node *ink_parse_arglist(struct ink_parser *);
static struct ink_ast_node *ink_parse_stmt(struct ink_parser *,
                                           struct ink_parser_node_context *);
static struct ink_ast_node *ink_parse_expr(struct ink_parser *);
static struct ink_ast_node *ink_parse_infix_expr(struct ink_parser *,
                                                 struct ink_ast_node *,
                                                 enum ink_precedence);
static struct ink_ast_node *ink_parse_lbrace_expr(struct ink_parser *);

static struct ink_ast_node *ink_parse_atom(struct ink_parser *parser,
                                           enum ink_ast_node_type node_type)
{
    /* NOTE: Advancing the parser MUST only happen after the node is
     * created. This prevents trailing whitespace. */
    const struct ink_token token = parser->token;
    struct ink_ast_node *const node = ink_ast_node_leaf(
        node_type, token.start_offset, token.end_offset, parser->arena);

    ink_parser_advance(parser);
    return node;
}

static struct ink_ast_node *ink_parse_true(struct ink_parser *parser)
{
    return ink_parse_atom(parser, INK_AST_TRUE);
}

static struct ink_ast_node *ink_parse_false(struct ink_parser *parser)
{
    return ink_parse_atom(parser, INK_AST_FALSE);
}

static struct ink_ast_node *ink_parse_number(struct ink_parser *parser)
{
    return ink_parse_atom(parser, INK_AST_NUMBER);
}

static struct ink_ast_node *ink_parse_identifier(struct ink_parser *parser)
{
    return ink_parse_atom(parser, INK_AST_IDENTIFIER);
}

static struct ink_ast_node *
ink_parse_expect_identifier(struct ink_parser *parser)
{
    if (!ink_parser_check(parser, INK_TT_IDENTIFIER)) {
        return ink_parser_error(parser, INK_AST_E_EXPECTED_IDENTIFIER,
                                &parser->token);
    }
    return ink_parse_identifier(parser);
}

static struct ink_ast_node *ink_parse_expect_expr(struct ink_parser *parser)
{
    const struct ink_token token = parser->token;
    struct ink_ast_node *const lhs = ink_parse_expr(parser);

    if (!lhs) {
        ink_parser_error(parser, INK_AST_E_EXPECTED_EXPR, &token);
    }
    return lhs;
}

static size_t ink_parse_expect_stmt_end(struct ink_parser *parser)
{
    if (!ink_parser_check(parser, INK_TT_EOF) &&
        !ink_parser_check(parser, INK_TT_NL)) {
        ink_parser_error(parser, INK_AST_E_EXPECTED_NEWLINE, &parser->token);
        return parser->token.start_offset;
    }
    return ink_parser_advance(parser);
}

static struct ink_ast_node *
ink_parse_string(struct ink_parser *parser,
                 const enum ink_token_type *token_set)
{
    const size_t source_start = parser->token.start_offset;

    while (!ink_parser_check_many(parser, token_set)) {
        ink_parser_advance(parser);
    }
    return ink_ast_node_leaf(
        source_start == parser->token.start_offset ? INK_AST_EMPTY_STRING
                                                   : INK_AST_STRING,
        source_start, parser->token.start_offset, parser->arena);
}

static struct ink_ast_node *ink_parse_arglist(struct ink_parser *parser)
{
    const size_t scratch_offset = parser->scratch.count;
    const size_t source_start =
        ink_parser_expect_token(parser, INK_TT_LEFT_PAREN);

    if (!ink_parser_check(parser, INK_TT_RIGHT_PAREN)) {
        size_t arg_count = 0;

        for (;;) {
            struct ink_ast_node *node = ink_parse_expr(parser);

            if (arg_count == INK_PARSER_ARGS_MAX) {
                ink_parser_error(parser, INK_AST_E_TOO_MANY_PARAMS,
                                 &parser->token);
                break;
            }

            arg_count++;
            ink_parser_scratch_push(&parser->scratch, node);

            if (ink_parser_check(parser, INK_TT_COMMA)) {
                ink_parser_advance(parser);
            } else {
                break;
            }
        }
    }

    ink_parser_expect_token(parser, INK_TT_RIGHT_PAREN);
    return ink_ast_node_sequence(INK_AST_ARG_LIST, source_start,
                                 parser->token.start_offset, scratch_offset,
                                 &parser->scratch, parser->arena);
}

static struct ink_ast_node *ink_parse_identifier_expr(struct ink_parser *parser)
{
    struct ink_ast_node *lhs = ink_parse_expect_identifier(parser);

    for (;;) {
        struct ink_ast_node *rhs = NULL;

        switch (parser->token.type) {
        case INK_TT_DOT:
            ink_parser_advance(parser);

            rhs = ink_parse_expect_identifier(parser);
            if (!rhs) {
                return rhs;
            }

            lhs = ink_ast_node_binary(INK_AST_SELECTOR_EXPR, lhs->start_offset,
                                      parser->token.start_offset, lhs, rhs,
                                      parser->arena);
            break;
        case INK_TT_LEFT_PAREN:
            rhs = ink_parse_arglist(parser);
            return ink_ast_node_binary(INK_AST_CALL_EXPR, lhs->start_offset,
                                       parser->token.start_offset, lhs, rhs,
                                       parser->arena);
        default:
            return lhs;
        }
    }
}

static struct ink_ast_node *ink_parse_divert(struct ink_parser *parser)
{
    const size_t source_start = parser->token.start_offset;
    struct ink_ast_node *node = NULL;

    ink_parser_advance(parser);
    node = ink_parse_identifier_expr(parser);
    return ink_ast_node_unary(INK_AST_DIVERT, source_start,
                              parser->token.start_offset, node, parser->arena);
}

static struct ink_ast_node *ink_parse_string_expr(struct ink_parser *parser)
{
    static const enum ink_token_type token_set[] = {
        INK_TT_DOUBLE_QUOTE,
        INK_TT_NL,
        INK_TT_EOF,
    };
    const size_t source_start =
        ink_parser_expect_token(parser, INK_TT_DOUBLE_QUOTE);
    struct ink_ast_node *const lhs = ink_parse_string(parser, token_set);

    if (!ink_parser_check(parser, INK_TT_DOUBLE_QUOTE)) {
        return ink_parser_error(parser, INK_AST_E_EXPECTED_DQUOTE,
                                &parser->token);
    }

    ink_parser_advance(parser);
    return ink_ast_node_unary(INK_AST_STRING_EXPR, source_start,
                              parser->token.start_offset, lhs, parser->arena);
}

static struct ink_ast_node *ink_parse_primary_expr(struct ink_parser *parser)
{
    switch (parser->token.type) {
    case INK_TT_NUMBER:
        return ink_parse_number(parser);
    case INK_TT_KEYWORD_TRUE:
        return ink_parse_true(parser);
    case INK_TT_KEYWORD_FALSE:
        return ink_parse_false(parser);
    case INK_TT_IDENTIFIER:
        return ink_parse_identifier_expr(parser);
    case INK_TT_DOUBLE_QUOTE:
        return ink_parse_string_expr(parser);
    case INK_TT_LEFT_PAREN: {
        ink_parser_advance(parser);

        struct ink_ast_node *const node =
            ink_parse_infix_expr(parser, NULL, INK_PREC_NONE);

        if (!node) {
            return NULL;
        }
        if (!ink_parser_match(parser, INK_TT_RIGHT_PAREN)) {
            return NULL;
        }
        return node;
    }
    default:
        return NULL;
    }
}

static struct ink_ast_node *ink_parse_prefix_expr(struct ink_parser *parser)
{
    const enum ink_token_type type = parser->token.type;

    switch (type) {
    case INK_TT_KEYWORD_NOT:
    case INK_TT_MINUS:
    case INK_TT_BANG: {
        const size_t source_start = ink_parser_advance(parser);
        struct ink_ast_node *const node = ink_parse_prefix_expr(parser);

        if (!node) {
            return NULL;
        }
        return ink_ast_node_unary(ink_token_prefix_type(type), source_start,
                                  node->end_offset, node, parser->arena);
    }
    case INK_TT_RIGHT_ARROW:
        return ink_parse_divert(parser);
    default:
        return ink_parse_primary_expr(parser);
    }
}

static struct ink_ast_node *ink_parse_infix_expr(struct ink_parser *parser,
                                                 struct ink_ast_node *lhs,
                                                 enum ink_precedence prec)
{
    if (!lhs) {
        lhs = ink_parse_prefix_expr(parser);
        if (!lhs) {
            return NULL;
        }
    }
    for (;;) {
        const enum ink_token_type type = parser->token.type;
        const enum ink_precedence token_prec = ink_binding_power(type);

        if (token_prec > prec) {
            ink_parser_advance(parser);

            struct ink_ast_node *const rhs =
                ink_parse_infix_expr(parser, NULL, token_prec);

            if (!rhs) {
                return NULL;
            }

            lhs = ink_ast_node_binary(ink_token_infix_type(type),
                                      lhs->start_offset, rhs->end_offset, lhs,
                                      rhs, parser->arena);
        } else {
            break;
        }
    }
    return lhs;
}

static struct ink_ast_node *ink_parse_divert_expr(struct ink_parser *parser)
{
    const size_t source_start = ink_parser_advance(parser);
    struct ink_ast_node *const node = ink_parse_identifier_expr(parser);

    return ink_ast_node_unary(INK_AST_DIVERT, source_start,
                              parser->token.start_offset, node, parser->arena);
}

static struct ink_ast_node *ink_parse_thread_expr(struct ink_parser *parser)
{
    ink_parser_push_scanner(parser, INK_GRAMMAR_EXPRESSION);

    const size_t source_start = ink_parser_advance(parser);
    struct ink_ast_node *node = NULL;

    if (ink_parser_check(parser, INK_TT_IDENTIFIER)) {
        node = ink_parse_identifier_expr(parser);
    }

    ink_parser_pop_scanner(parser);
    return ink_ast_node_unary(INK_AST_THREAD_EXPR, source_start,
                              parser->token.start_offset, node, parser->arena);
}

static struct ink_ast_node *ink_parse_expr(struct ink_parser *parser)
{
    return ink_parse_infix_expr(parser, NULL, INK_PREC_NONE);
}

static struct ink_ast_node *ink_parse_return_stmt(struct ink_parser *parser)
{
    const size_t source_start = ink_parser_advance(parser);
    struct ink_ast_node *node = NULL;

    if (!ink_parser_check(parser, INK_TT_NL) &&
        !ink_parser_check(parser, INK_TT_EOF)) {
        node = ink_parse_expr(parser);
    }
    return ink_ast_node_unary(INK_AST_RETURN_STMT, source_start,
                              ink_parse_expect_stmt_end(parser), node,
                              parser->arena);
}

static struct ink_ast_node *ink_parse_divert_stmt(struct ink_parser *parser)
{
    const size_t source_start = parser->token.start_offset;
    struct ink_ast_node *node = NULL;

    ink_parser_push_scanner(parser, INK_GRAMMAR_EXPRESSION);
    node = ink_parse_divert_expr(parser);
    ink_parser_pop_scanner(parser);
    return ink_ast_node_unary(INK_AST_DIVERT_STMT, source_start,
                              ink_parse_expect_stmt_end(parser), node,
                              parser->arena);
}

static struct ink_ast_node *ink_parse_glue(struct ink_parser *parser)
{
    const size_t source_start = parser->token.start_offset;

    ink_parser_advance(parser);
    return ink_ast_node_leaf(INK_AST_GLUE, source_start,
                             parser->token.start_offset, parser->arena);
}

static struct ink_ast_node *ink_parse_temp_decl(struct ink_parser *parser)
{
    const size_t source_start = ink_parser_advance(parser);
    struct ink_ast_node *const lhs = ink_parse_expect_identifier(parser);
    struct ink_ast_node *rhs = NULL;

    if (!lhs) {
        return NULL;
    }

    ink_parser_expect_token(parser, INK_TT_EQUAL);

    rhs = ink_parse_expect_expr(parser);
    if (!rhs) {
        return NULL;
    }
    return ink_ast_node_binary(INK_AST_TEMP_DECL, source_start,
                               ink_parse_expect_stmt_end(parser), lhs, rhs,
                               parser->arena);
}

static struct ink_ast_node *ink_parse_thread_stmt(struct ink_parser *parser)
{
    const size_t source_start = parser->token.start_offset;
    struct ink_ast_node *const node = ink_parse_thread_expr(parser);

    return ink_ast_node_unary(INK_AST_THREAD_STMT, source_start,
                              ink_parse_expect_stmt_end(parser), node,
                              parser->arena);
}

static struct ink_ast_node *ink_parse_expr_stmt(struct ink_parser *parser,
                                                struct ink_ast_node *lhs)
{
    const size_t source_start =
        lhs ? lhs->start_offset : parser->token.start_offset;
    struct ink_ast_node *const node =
        ink_parse_infix_expr(parser, lhs, INK_PREC_NONE);

    return ink_ast_node_unary(INK_AST_EXPR_STMT, source_start,
                              ink_parse_expect_stmt_end(parser), node,
                              parser->arena);
}

static struct ink_ast_node *ink_parse_assign_stmt(struct ink_parser *parser)
{
    const size_t source_start = parser->token.start_offset;
    struct ink_ast_node *const lhs = ink_parse_identifier_expr(parser);
    struct ink_ast_node *rhs = NULL;

    if (!ink_parser_match(parser, INK_TT_EQUAL)) {
        return ink_parse_expr_stmt(parser, lhs);
    }

    rhs = ink_parse_expect_expr(parser);
    if (!rhs) {
        return rhs;
    }
    return ink_ast_node_binary(INK_AST_ASSIGN_STMT, source_start,
                               ink_parse_expect_stmt_end(parser), lhs, rhs,
                               parser->arena);
}

static struct ink_ast_node *ink_parse_tilde_stmt(struct ink_parser *parser)
{
    struct ink_ast_node *lhs = NULL;

    ink_parser_push_scanner(parser, INK_GRAMMAR_EXPRESSION);
    ink_parser_advance(parser);

    switch (parser->token.type) {
    case INK_TT_KEYWORD_TEMP:
        lhs = ink_parse_temp_decl(parser);
        break;
    case INK_TT_KEYWORD_RETURN:
        lhs = ink_parse_return_stmt(parser);
        break;
    case INK_TT_IDENTIFIER:
        lhs = ink_parse_assign_stmt(parser);
        break;
    default:
        lhs = ink_parse_expr_stmt(parser, NULL);
        break;
    }

    ink_parser_pop_scanner(parser);
    return lhs;
}

static struct ink_ast_node *
ink_parse_content(struct ink_parser *parser,
                  const enum ink_token_type *token_set)
{
    const size_t scratch_offset = parser->scratch.count;
    const size_t source_start = parser->token.start_offset;

    for (;;) {
        struct ink_ast_node *node = NULL;

        if (!ink_parser_check_many(parser, token_set)) {
            node = ink_parse_string(parser, token_set);
        } else {
            switch (parser->token.type) {
            case INK_TT_LEFT_BRACE:
                node = ink_parse_lbrace_expr(parser);
                break;
            case INK_TT_RIGHT_ARROW:
                node = ink_parse_divert_stmt(parser);
                break;
            case INK_TT_LEFT_ARROW:
                node = ink_parse_thread_expr(parser);
                break;
            case INK_TT_GLUE:
                node = ink_parse_glue(parser);
                break;
            default:
                goto exit_loop;
            }
        }

        ink_parser_scratch_push(&parser->scratch, node);
    }
    if (ink_parser_check(parser, INK_TT_NL)) {
        ink_parser_advance(parser);
    }
exit_loop:
    return ink_ast_node_sequence(
        source_start == parser->token.start_offset ? INK_AST_EMPTY_STRING
                                                   : INK_AST_CONTENT,
        source_start, parser->token.start_offset, scratch_offset,
        &parser->scratch, parser->arena);
}

/*
static struct ink_ast_node *ink_parse_sequence(struct ink_parser *parser,
                                               struct ink_ast_node *expr)
{
static const enum ink_token_type token_set[] = {
        INK_TT_LEFT_BRACE,  INK_TT_LEFT_ARROW, INK_TT_RIGHT_BRACE,
        INK_TT_RIGHT_ARROW, INK_TT_PIPE,       INK_TT_NL,
        INK_TT_EOF,
    };

    struct ink_ast_node *node = NULL;
    struct ink_parser_scratch *scratch = &parser->scratch;
    const size_t scratch_offset = scratch->count;
    const size_t source_start = parser->token.start_offset;

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
    return ink_ast_node_sequence(INK_AST_SEQUENCE_EXPR, source_start,
                                 parser->token.start_offset, scratch_offset,
scratch, parser->arena);
}
*/

static struct ink_ast_node *
ink_parse_conditional_branch(struct ink_parser *parser,
                             enum ink_ast_node_type node_type)
{
    const size_t source_start = parser->token.start_offset;
    struct ink_ast_node *node = NULL;

    if (ink_parser_match(parser, INK_TT_KEYWORD_ELSE)) {
        node_type = INK_AST_ELSE_BRANCH;
    } else {
        node = ink_parse_expr(parser);
        if (!node) {
            return NULL;
        }
    }
    if (!ink_parser_match(parser, INK_TT_COLON)) {
        return NULL;
    }
    if (ink_parser_check(parser, INK_TT_NL)) {
        ink_parser_advance(parser);
    }
    return ink_ast_node_unary(node_type, source_start,
                              parser->token.start_offset, node, parser->arena);
}

static struct ink_ast_node *ink_parse_conditional(struct ink_parser *parser,
                                                  struct ink_ast_node *expr)
{
    struct ink_parser_node_context context;
    const size_t scratch_offset = parser->scratch.count;
    const size_t source_start = parser->token.start_offset;
    struct ink_ast_node *node = NULL;

    ink_parser_node_context_init(parser, &context);

    context.node = expr;
    context.is_conditional = true;
    context.is_block_created = false;

    while (!ink_parser_check(parser, INK_TT_EOF) &&
           !ink_parser_check(parser, INK_TT_RIGHT_BRACE)) {
        node = ink_parse_stmt(parser, &context);
        if (node) {
            ink_parser_scratch_push(&parser->scratch, node);
        }
    }

    ink_parser_collect_choices(parser, &context, 0);

    node = ink_parser_collect_block(parser, &context);
    if (node) {
        ink_parser_scratch_push(&parser->scratch, node);
    }

    enum ink_ast_node_type node_type;

    if (expr && !context.is_block_created) {
        node_type = INK_AST_SWITCH_STMT;
    } else if (!expr) {
        node_type = INK_AST_MULTI_IF_STMT;
    } else {
        node_type = INK_AST_IF_STMT;
    }

    ink_parser_node_context_deinit(&context);

    /* FIXME: This may cause a crash. */
    node = ink_ast_node_sequence(node_type, source_start,
                                 parser->token.start_offset, scratch_offset,
                                 &parser->scratch, parser->arena);
    node->lhs = expr;
    return node;
}

static struct ink_ast_node *ink_parse_lbrace_expr(struct ink_parser *parser)
{
    static const enum ink_token_type token_set[] = {
        INK_TT_LEFT_BRACE,  INK_TT_LEFT_ARROW, INK_TT_RIGHT_BRACE,
        INK_TT_RIGHT_ARROW, INK_TT_GLUE,       INK_TT_NL,
        INK_TT_EOF,
    };
    const size_t source_start = parser->token.start_offset;
    struct ink_ast_node *lhs = NULL;
    struct ink_ast_node *rhs = NULL;

    ink_parser_push_scanner(parser, INK_GRAMMAR_EXPRESSION);
    ink_parser_advance(parser);

    if (!ink_parser_check(parser, INK_TT_NL)) {
        lhs = ink_parse_expr(parser);

        if (!lhs) {
            ink_parser_pop_scanner(parser);
            ink_parser_error(parser, INK_AST_E_INVALID_EXPR, &parser->token);
            ink_parser_advance(parser);
            return NULL;
        }
        if (ink_parser_check(parser, INK_TT_COLON)) {
            ink_parser_push_scanner(parser, INK_GRAMMAR_CONTENT);
            ink_parser_advance(parser);

            if (ink_parser_check(parser, INK_TT_NL)) {
                ink_parser_advance(parser);
                rhs = ink_parse_conditional(parser, lhs);
            } else {
                rhs = ink_parse_content(parser, token_set);
                rhs = ink_ast_node_binary(INK_AST_IF_EXPR, source_start,
                                          parser->token.start_offset, lhs, rhs,
                                          parser->arena);
            }

            ink_parser_pop_scanner(parser);
            ink_parser_expect_token(parser, INK_TT_RIGHT_BRACE);
        } else {
            ink_parser_pop_scanner(parser);
            ink_parser_expect_token(parser, INK_TT_RIGHT_BRACE);
            return ink_ast_node_unary(INK_AST_INLINE_LOGIC, source_start,
                                      parser->token.start_offset, lhs,
                                      parser->arena);
        }
    } else {
        ink_parser_advance(parser);
        ink_parser_push_scanner(parser, INK_GRAMMAR_CONTENT);
        rhs = ink_parse_conditional(parser, NULL);
        ink_parser_expect_token(parser, INK_TT_RIGHT_BRACE);
        ink_parser_pop_scanner(parser);
    }

    ink_parser_pop_scanner(parser);
    return rhs;
}

static struct ink_ast_node *ink_parse_content_stmt(struct ink_parser *parser)
{
    static const enum ink_token_type token_set[] = {
        INK_TT_LEFT_BRACE,  INK_TT_LEFT_ARROW, INK_TT_RIGHT_BRACE,
        INK_TT_RIGHT_ARROW, INK_TT_GLUE,       INK_TT_NL,
        INK_TT_EOF,
    };
    const size_t source_start = parser->token.start_offset;
    struct ink_ast_node *const node = ink_parse_content(parser, token_set);
    const size_t source_end =
        node ? node->end_offset : parser->token.start_offset;

    if (ink_parser_check(parser, INK_TT_NL)) {
        ink_parser_advance(parser);
        /* FIXME: Trailing whitespace is interpreted as empty content. */
        ink_parser_match(parser, INK_TT_WHITESPACE);
    }
    return ink_ast_node_unary(INK_AST_CONTENT_STMT, source_start, source_end,
                              node, parser->arena);
}

static struct ink_ast_node *ink_parse_choice_content(struct ink_parser *parser)
{
    static const enum ink_token_type token_set[] = {
        INK_TT_LEFT_BRACE,  INK_TT_LEFT_ARROW,    INK_TT_LEFT_BRACKET,
        INK_TT_RIGHT_BRACE, INK_TT_RIGHT_BRACKET, INK_TT_RIGHT_ARROW,
        INK_TT_NL,          INK_TT_EOF,
    };

    const size_t scratch_offset = parser->scratch.count;
    const size_t source_start = parser->token.start_offset;
    struct ink_ast_node *node = NULL;

    node = ink_parse_string(parser, token_set);
    if (node) {
        if (node->type != INK_AST_EMPTY_STRING) {
            node->type = INK_AST_CHOICE_START_EXPR;
            ink_parser_scratch_push(&parser->scratch, node);
        }
    }
    if (ink_parser_check(parser, INK_TT_LEFT_BRACKET)) {
        ink_parser_advance(parser);
        ink_parser_match(parser, INK_TT_WHITESPACE);

        if (!ink_parser_check(parser, INK_TT_RIGHT_BRACKET)) {
            node = ink_parse_string(parser, token_set);
            if (node) {
                if (node->type != INK_AST_EMPTY_STRING) {
                    node->type = INK_AST_CHOICE_OPTION_EXPR;
                    ink_parser_scratch_push(&parser->scratch, node);
                }
            }
        }

        ink_parser_expect_token(parser, INK_TT_RIGHT_BRACKET);

        if (!ink_parser_check_many(parser, token_set)) {
            node = ink_parse_string(parser, token_set);
            if (node) {
                if (node->type != INK_AST_EMPTY_STRING) {
                    node->type = INK_AST_CHOICE_INNER_EXPR;
                    ink_parser_scratch_push(&parser->scratch, node);
                }
            }
        }
    }
    return ink_ast_node_sequence(INK_AST_CHOICE_EXPR, source_start,
                                 parser->token.start_offset, scratch_offset,
                                 &parser->scratch, parser->arena);
}

static struct ink_ast_node *
ink_parse_choice(struct ink_parser *parser,
                 struct ink_parser_node_context *context)
{
    const enum ink_token_type token_type = parser->token.type;
    const size_t source_start = parser->token.start_offset;
    size_t source_end = 0;
    struct ink_ast_node *node = NULL;

    context->choice_level = ink_parser_match_many(parser, token_type, true);
    node = ink_parse_choice_content(parser);
    source_end = node ? node->end_offset : parser->token.start_offset;

    if (ink_parser_check(parser, INK_TT_NL)) {
        ink_parser_advance(parser);
    }
    return ink_ast_node_binary(ink_branch_type(token_type), source_start,
                               source_end, node, NULL, parser->arena);
}

static struct ink_ast_node *
ink_parse_gather(struct ink_parser *parser,
                 struct ink_parser_node_context *context)
{
    const enum ink_token_type token_type = parser->token.type;
    const size_t source_start = parser->token.start_offset;
    size_t source_end = 0;

    context->choice_level = ink_parser_match_many(parser, token_type, true);

    while (!ink_parser_check(parser, INK_TT_EOF) &&
           !ink_parser_check(parser, INK_TT_NL)) {
        ink_parser_advance(parser);
    }

    source_end = parser->token.start_offset;
    if (ink_parser_check(parser, INK_TT_NL)) {
        ink_parser_advance(parser);
    }
    return ink_ast_node_unary(INK_AST_GATHER_STMT, source_start, source_end,
                              NULL, parser->arena);
}

/*
static struct ink_ast_node *
ink_parse_list_element_def(struct ink_parser *parser)
{
struct ink_ast_node *lhs = NULL;
struct ink_ast_node *rhs = NULL;
const size_t source_start = parser->token.start_offset;

if (ink_parser_check(parser, INK_TT_LEFT_PAREN)) {
ink_parser_advance(parser);
INK_PARSER_RULE(lhs, ink_parse_identifier, parser);
return ink_ast_node_unary(INK_AST_SELECTED_LIST_ELEMENT, source_start,
                        parser->token.start_offset, lhs, parser->arena);
}

INK_PARSER_RULE(lhs, ink_parse_identifier, parser);

if (ink_parser_check(parser, INK_TT_EQUAL)) {
ink_parser_advance(parser);
INK_PARSER_RULE(rhs, ink_parse_expr, parser);
return ink_ast_node_binary(INK_AST_ASSIGN_STMT, source_start,
                         parser->token.start_offset, lhs, rhs,
                         parser->arena);
}
return lhs;
}
*/

/*
static struct ink_ast_node *ink_parse_list(struct ink_parser *parser)
{
int arg_count = 0;
struct ink_ast_node *node = NULL;
struct ink_parser_scratch *scratch = &parser->scratch;
const size_t scratch_offset = scratch->count;
const size_t source_start = parser->token.start_offset;

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
return ink_ast_node_sequence(INK_AST_ARG_LIST, source_start,
                       parser->token.start_offset, scratch_offset, scratch,
                       parser->arena);
}
*/

/*
static struct ink_ast_node *ink_parse_list_decl(struct ink_parser *parser)
{
    struct ink_ast_node *lhs = NULL;
    struct ink_ast_node *rhs = NULL;
    const size_t source_start = parser->token.start_offset;
    size_t source_end = 0;

    ink_parser_push_scanner(parser, INK_GRAMMAR_EXPRESSION);
    ink_parser_advance(parser);
    INK_PARSER_RULE(lhs, ink_parse_identifier, parser);
    ink_parser_expect(parser, INK_TT_EQUAL);
    INK_PARSER_RULE(rhs, ink_parse_list, parser);
    source_end = rhs ? rhs->end_offset : parser->token.start_offset;
    ink_parser_pop_scanner(parser);
    ink_parser_expect_stmt_end(parser);
    return ink_ast_node_binary(INK_AST_LIST_DECL, source_start, source_end, lhs,
                               rhs, parser->arena);
}
*/

static struct ink_ast_node *ink_parse_var(struct ink_parser *parser,
                                          enum ink_ast_node_type node_type)
{
    const size_t source_start = parser->token.start_offset;
    struct ink_ast_node *lhs = NULL;
    struct ink_ast_node *rhs = NULL;

    ink_parser_push_scanner(parser, INK_GRAMMAR_EXPRESSION);
    ink_parser_advance(parser);

    lhs = ink_parse_expect_identifier(parser);
    if (!lhs) {
        ink_parser_pop_scanner(parser);
        return NULL;
    }

    ink_parser_expect_token(parser, INK_TT_EQUAL);
    rhs = ink_parse_expect_expr(parser);
    ink_parser_pop_scanner(parser);
    return ink_ast_node_binary(node_type, source_start,
                               ink_parse_expect_stmt_end(parser), lhs, rhs,
                               parser->arena);
}

static struct ink_ast_node *ink_parse_var_decl(struct ink_parser *parser)
{
    return ink_parse_var(parser, INK_AST_VAR_DECL);
}

static struct ink_ast_node *ink_parse_const_decl(struct ink_parser *parser)
{
    return ink_parse_var(parser, INK_AST_CONST_DECL);
}

static struct ink_ast_node *ink_parse_parameter_decl(struct ink_parser *parser)
{
    struct ink_ast_node *node = NULL;

    /* FIXME: This may cause a crash. */
    if (ink_parser_check(parser, INK_TT_KEYWORD_REF)) {
        ink_parser_advance(parser);
        node = ink_parse_expect_identifier(parser);
        node->type = INK_AST_REF_PARAM_DECL;
    } else {
        node = ink_parse_expect_identifier(parser);
        node->type = INK_AST_PARAM_DECL;
    }
    return node;
}

static struct ink_ast_node *ink_parse_parameter_list(struct ink_parser *parser)
{
    size_t arg_count = 0;
    const size_t scratch_offset = parser->scratch.count;
    const size_t source_start =
        ink_parser_expect_token(parser, INK_TT_LEFT_PAREN);

    if (!ink_parser_check(parser, INK_TT_RIGHT_PAREN)) {
        for (;;) {
            struct ink_ast_node *node = NULL;

            if (arg_count == INK_PARSER_ARGS_MAX) {
                ink_parser_error(parser, INK_AST_E_TOO_MANY_PARAMS,
                                 &parser->token);
                break;
            } else {
                arg_count++;
            }

            node = ink_parse_parameter_decl(parser);
            ink_parser_scratch_push(&parser->scratch, node);

            if (ink_parser_check(parser, INK_TT_COMMA)) {
                ink_parser_advance(parser);
            } else {
                break;
            }
        }
    }

    ink_parser_expect_token(parser, INK_TT_RIGHT_PAREN);
    return ink_ast_node_sequence(INK_AST_PARAM_LIST, source_start,
                                 parser->token.start_offset, scratch_offset,
                                 &parser->scratch, parser->arena);
}

static struct ink_ast_node *ink_parse_knot_decl(struct ink_parser *parser)
{

    enum ink_ast_node_type node_type = INK_AST_STITCH_PROTO;
    struct ink_parser_scratch *const scratch = &parser->scratch;
    const size_t scratch_offset = scratch->count;
    const size_t source_start = ink_parser_advance(parser);

    ink_parser_push_scanner(parser, INK_GRAMMAR_EXPRESSION);
    ink_parser_match(parser, INK_TT_WHITESPACE);

    if (ink_parser_check(parser, INK_TT_EQUAL)) {
        node_type = INK_AST_KNOT_PROTO;

        while (ink_parser_check(parser, INK_TT_EQUAL)) {
            ink_parser_advance(parser);
        }
    }
    if (ink_scanner_try_keyword(&parser->scanner, &parser->token,
                                INK_TT_KEYWORD_FUNCTION)) {
        ink_parser_advance(parser);
        node_type = INK_AST_FUNC_PROTO;
    }

    struct ink_ast_node *const name_node = ink_parse_expect_identifier(parser);

    if (!name_node) {
        ink_parser_pop_scanner(parser);
        return NULL;
    }

    ink_parser_scratch_push(scratch, name_node);

    if (ink_parser_check(parser, INK_TT_LEFT_PAREN)) {
        struct ink_ast_node *const args_node = ink_parse_parameter_list(parser);

        ink_parser_scratch_push(scratch, args_node);
    }
    while (ink_parser_check(parser, INK_TT_EQUAL) ||
           ink_parser_check(parser, INK_TT_EQUAL_EQUAL)) {
        ink_parser_advance(parser);
    }

    ink_parser_pop_scanner(parser);
    return ink_ast_node_sequence(node_type, source_start,
                                 ink_parse_expect_stmt_end(parser),
                                 scratch_offset, scratch, parser->arena);
}

static struct ink_ast_node *
ink_parse_stmt(struct ink_parser *parser,
               struct ink_parser_node_context *context)
{
    struct ink_ast_node *node = NULL;

    ink_parser_match(parser, INK_TT_WHITESPACE);

    switch (parser->token.type) {
    case INK_TT_EOF:
        break;
    case INK_TT_STAR:
    case INK_TT_PLUS:
        node = ink_parse_choice(parser, context);
        break;
    case INK_TT_MINUS:
        if (context->is_conditional) {
            ink_parser_push_scanner(parser, INK_GRAMMAR_EXPRESSION);
            ink_parser_advance(parser);

            if (context->node) {
                node =
                    ink_parse_conditional_branch(parser, INK_AST_SWITCH_CASE);
            } else {
                node = ink_parse_conditional_branch(parser, INK_AST_IF_BRANCH);
            }
            if (!node) {
                ink_parser_rewind_scanner(parser);
                ink_parser_push_scanner(parser, INK_GRAMMAR_CONTENT);
                ink_parser_advance(parser);
                node = ink_parse_gather(parser, context);
                ink_parser_pop_scanner(parser);
            }

            ink_parser_pop_scanner(parser);
        } else {
            node = ink_parse_gather(parser, context);
        }
        break;
    case INK_TT_TILDE:
        node = ink_parse_tilde_stmt(parser);
        break;
    case INK_TT_LEFT_ARROW:
        node = ink_parse_thread_stmt(parser);
        break;
    case INK_TT_RIGHT_ARROW:
        node = ink_parse_divert_stmt(parser);
        break;
    case INK_TT_EQUAL:
        if (!context->is_conditional) {
            node = ink_parse_knot_decl(parser);
        } else {
            node = ink_parse_content_stmt(parser);
        }
        break;
    case INK_TT_RIGHT_BRACE:
        /* TODO: Expand upon this to validate balanced delimiters. */
        ink_parser_error(parser, INK_AST_E_UNEXPECTED_TOKEN, &parser->token);
        ink_parser_advance(parser);
        break;
    default: {
        struct ink_token *const token = &parser->token;

        if (ink_scanner_try_keyword(&parser->scanner, token,
                                    INK_TT_KEYWORD_CONST)) {
            node = ink_parse_const_decl(parser);
        } else if (ink_scanner_try_keyword(&parser->scanner, token,
                                           INK_TT_KEYWORD_VAR)) {
            node = ink_parse_var_decl(parser);
            /*
          } else if (ink_scanner_try_keyword(&parser->scanner, &parser->token,
                                             INK_TT_KEYWORD_LIST)) {
              INK_PARSER_RULE(node, ink_parse_list_decl, parser);
  */
        } else {
            node = ink_parse_content_stmt(parser);
        }
        break;
    }
    }
    if (!node || parser->panic_mode) {
        ink_parser_sync(parser);
        ink_parser_match(parser, INK_TT_NL);
        return NULL;
    }
    switch (node->type) {
    case INK_AST_IF_BRANCH:
    case INK_AST_ELSE_BRANCH:
    case INK_AST_SWITCH_CASE:
        ink_parser_handle_conditional_branch(parser, context, node);
        break;
    case INK_AST_CHOICE_STAR_STMT:
    case INK_AST_CHOICE_PLUS_STMT:
        ink_parser_handle_choice_branch(parser, context, node);
        break;
    case INK_AST_GATHER_STMT:
        ink_parser_handle_gather(parser, context, &node);
        break;
    case INK_AST_KNOT_PROTO:
        ink_parser_handle_knot(parser, context);
        break;
    case INK_AST_STITCH_PROTO:
        ink_parser_handle_stitch(parser, context);
        break;
    case INK_AST_FUNC_PROTO:
        ink_parser_handle_func(parser, context);
        break;
    default:
        ink_parser_handle_content(parser, context, node);
        break;
    }
    return node;
}

static struct ink_ast_node *ink_parse_file(struct ink_parser *parser)
{
    struct ink_parser_node_context context;
    const size_t scratch_offset = parser->scratch.count;
    const size_t source_offset = parser->token.start_offset;
    struct ink_ast_node *node = NULL;

    ink_parser_node_context_init(parser, &context);

    while (!ink_parser_check(parser, INK_TT_EOF)) {
        node = ink_parse_stmt(parser, &context);
        if (node) {
            ink_parser_scratch_push(&parser->scratch, node);
        }
    }

    node = ink_parser_collect_knot(parser, &context);
    if (node) {
        ink_parser_scratch_push(&parser->scratch, node);
    }

    ink_parser_node_context_deinit(&context);
    return ink_ast_node_sequence(INK_AST_FILE, source_offset,
                                 parser->token.start_offset, scratch_offset,
                                 &parser->scratch, parser->arena);
}

/**
 * Parse a source file and output an AST.
 *
 * Returns zero on success and a negative integer on an internal failure.
 */
int ink_parse(const uint8_t *source_bytes, size_t source_length,
              const uint8_t *filename, struct ink_arena *arena,
              struct ink_ast *tree, int flags)
{
    struct ink_parser parser;

    ink_ast_init(tree, filename, source_bytes);
    ink_parser_init(&parser, tree, arena, flags);
    ink_parser_advance(&parser);

    tree->root = ink_parse_file(&parser);
    ink_parser_deinit(&parser);
    return INK_E_OK;
}

#undef INK_PARSER_MEMOIZE
#undef INK_PARSER_ARGS_MAX
#undef INK_PARSE_DEPTH
