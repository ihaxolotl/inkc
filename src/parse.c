#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "ast.h"
#include "common.h"
#include "parse.h"
#include "scanner.h"
#include "token.h"
#include "vec.h"

#define INK_PARSER_ARGS_MAX (255)
#define INK_PARSE_DEPTH (128)

struct ink_parser_state {
    size_t level;
    size_t scratch_offset;
    size_t source_offset;
};

INK_VEC_T(ink_parser_scratch, struct ink_ast_node *)
INK_VEC_T(ink_parser_stack, struct ink_parser_state)

static int ink_parser_stack_emplace(struct ink_parser_stack *stack,
                                    size_t level, size_t scratch_offset,
                                    size_t source_offset)
{
    const struct ink_parser_state entry = {
        .level = level,
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
            .source_offset = parser->token.bytes_start,                        \
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
                parser->token.bytes_start = node->bytes_end;                   \
                ink_parser_advance(parser);                                    \
            }                                                                  \
        } else {                                                               \
            node = INK_DISPATCH(rule, __VA_ARGS__);                            \
        }                                                                      \
    } while (0)

struct ink_parser_node_context {
    struct ink_parser_node_context *parent;
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
    jmp_buf jmpbuf;
};

/**
 * Ink parsing state.
 */
struct ink_parser {
    struct ink_arena *arena;
    struct ink_scanner scanner;
    struct ink_parser_scratch scratch;
    struct ink_token token;
    struct ink_ast *tree;
    bool panic_mode;
    int flags;
    struct ink_parser_node_context *context;
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
static struct ink_ast_node_list *
ink_ast_list_from_scratch(struct ink_parser_scratch *s, size_t start,
                          size_t end, struct ink_arena *arena)
{
    struct ink_ast_node_list *l = NULL;
    size_t li = 0;

    if (start < end) {
        const size_t span = end - start;
        const size_t size = sizeof(*l) + span * sizeof(l->nodes);

        assert(span > 0);

        l = ink_arena_allocate(arena, size);
        if (!l) {
            /* TODO(Brett): Handle and log the error. */
            return NULL;
        }

        l->count = span;

        for (size_t i = start; i < end; i++) {
            l->nodes[li] = s->entries[i];
            s->entries[i] = NULL;
            li++;
        }

        ink_parser_scratch_shrink(s, start);
    }
    return l;
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
 * Create an AST node with a variable number of children.
 */
static inline struct ink_ast_node *
ink_ast_node_sequence(enum ink_ast_node_type type, size_t bytes_start,
                      size_t bytes_end, size_t scratch_offset,
                      struct ink_parser_scratch *scratch,
                      struct ink_arena *arena)
{
    struct ink_ast_node_list *l = NULL;

    if (scratch->count != scratch_offset) {
        l = ink_ast_list_from_scratch(scratch, scratch_offset, scratch->count,
                                      arena);
        if (!l) {
            return NULL;
        }
    }
    return ink_ast_many_new(type, bytes_start, bytes_end, l, arena);
}

static void ink_parser_node_context_init(struct ink_parser *p,
                                         struct ink_parser_node_context *ctx)
{
    ctx->parent = p->context;
    ctx->node = NULL;
    ctx->is_conditional = false;
    ctx->is_block_created = false;
    ctx->choice_level = 0;
    ctx->knot_offset = 0;
    ctx->scratch_start = p->scratch.count;

    ink_parser_stack_init(&ctx->open_blocks);
    ink_parser_stack_init(&ctx->open_choices);
    p->context = ctx;
}

static void ink_parser_node_context_deinit(struct ink_parser *p,
                                           struct ink_parser_node_context *ctx)
{
    p->context = ctx->parent;
    ink_parser_stack_deinit(&ctx->open_blocks);
    ink_parser_stack_deinit(&ctx->open_choices);
}

/**
 * Initialize the parser state.
 *
 * No memory is allocated here, as it is performed lazily.
 */
static void ink_parser_init(struct ink_parser *p, struct ink_ast *tree,
                            struct ink_arena *arena, int flags)
{
    const struct ink_scanner sn = {
        .source_bytes = tree->source_bytes,
        .is_line_start = true,
    };

    p->arena = arena;
    p->scanner = sn;
    p->token.type = INK_TT_ERROR;
    p->token.bytes_start = 0;
    p->token.bytes_end = 0;
    p->panic_mode = false;
    p->flags = flags;
    p->tree = tree;

    ink_parser_scratch_init(&p->scratch);
}

/**
 * Cleanup the parser state.
 */
static void ink_parser_deinit(struct ink_parser *p)
{
    ink_parser_scratch_deinit(&p->scratch);
}

/**
 * Raise an error in the parser.
 */
static void *ink_parser_error(struct ink_parser *p,
                              enum ink_ast_error_type type,
                              const struct ink_token *t)

{
    if (p->panic_mode) {
        return NULL;
    } else {
        p->panic_mode = true;
    }

    const struct ink_ast_error err = {
        .type = type,
        .source_start = t->bytes_start,
        .source_end = t->bytes_end,
    };

    ink_ast_error_vec_push(&p->tree->errors, err);
    return NULL;
}

static void ink_parser_panic(struct ink_parser *p)
{
    ink_parser_error(p, INK_AST_E_PANIC, &p->token);
    if (p->context) {
        longjmp(p->context->jmpbuf, 1);
    }
}

/**
 * Push a lexical analysis context onto the parser.
 */
static inline void ink_parser_push_scanner(struct ink_parser *p,
                                           enum ink_grammar_type type)
{
    const struct ink_token t = p->token;

    if (ink_scanner_push(&p->scanner, type, t.bytes_start) < 0) {
        ink_parser_panic(p);
    }
}

/**
 * Pop a lexical analysis context from the parser.
 */
static inline void ink_parser_pop_scanner(struct ink_parser *p)
{
    if (ink_scanner_pop(&p->scanner) < 0) {
        ink_parser_panic(p);
    }
}

/**
 * Rewind the scanner to the starting position of the current scanner mode.
 */
static inline void ink_parser_rewind_scanner(struct ink_parser *p)
{
    struct ink_scanner_mode *const m = ink_scanner_current(&p->scanner);

    ink_scanner_rewind(&p->scanner, m->source_offset);
}

/**
 * Advance the parser.
 *
 * Returns the previous token's start offset.
 */
static size_t ink_parser_advance(struct ink_parser *p)
{
    const struct ink_token t = p->token;

    for (;;) {
        ink_scanner_next(&p->scanner, &p->token);

        if (p->token.type == INK_TT_ERROR) {
            ink_parser_error(p, INK_AST_E_UNEXPECTED_TOKEN, &p->token);
        } else {
            break;
        }
    }
    return t.bytes_start;
}

/**
 * Check if the current token matches a given token type.
 *
 * Returns a boolean indicating whether the current token's type matches.
 */
static inline bool ink_parser_check(struct ink_parser *p,
                                    enum ink_token_type type)
{
    return p->token.type == type;
}

static bool ink_parser_check_many(struct ink_parser *p,
                                  const enum ink_token_type *token_set)
{
    /* TODO: Look into if this causes bugs. */
    if (ink_parser_check(p, INK_TT_EOF)) {
        return true;
    }
    for (size_t i = 0; token_set[i] != INK_TT_EOF; i++) {
        if (ink_parser_check(p, token_set[i])) {
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
static bool ink_parser_match(struct ink_parser *p, enum ink_token_type type)
{
    if (ink_parser_check(p, type)) {
        ink_parser_advance(p);
        return true;
    }
    return false;
}

/**
 * Synchronize the state of the parser after a panic.
 */
static void ink_parser_sync(struct ink_parser *p)
{
    p->panic_mode = false;

    while (!ink_is_sync_token(p->token.type)) {
        ink_parser_advance(p);
    }
}

/**
 * Expect a specific token type.
 *
 * A parse error will be added if the current token type does not match the
 * expected type.
 */
static size_t ink_parser_expect_token(struct ink_parser *p,
                                      enum ink_token_type type)
{
    struct ink_scanner_mode *const m = ink_scanner_current(&p->scanner);
    size_t b_start = p->token.bytes_start;

    if (m->type == INK_GRAMMAR_EXPRESSION) {
        if (ink_parser_check(p, INK_TT_WHITESPACE)) {
            b_start = ink_parser_advance(p);
        }
    }
    if (!ink_parser_check(p, type)) {
        ink_parser_error(p, INK_AST_E_UNEXPECTED_TOKEN, &p->token);
        return b_start;
    }

    ink_parser_advance(p);
    return b_start;
}

static size_t ink_parser_match_many(struct ink_parser *p,
                                    enum ink_token_type type,
                                    bool ignore_whitespace)
{
    size_t cnt = 0;

    while (ink_parser_check(p, type)) {
        cnt++;

        ink_parser_advance(p);
        if (ignore_whitespace) {
            ink_parser_match(p, INK_TT_WHITESPACE);
        }
    }
    return cnt;
}

static struct ink_ast_node *
ink_parser_fixup_block(struct ink_parser *p,
                       struct ink_parser_node_context *ctx,
                       struct ink_ast_node *n)
{
    struct ink_parser_scratch *const s = &p->scratch;

    if (!ink_parser_scratch_is_empty(s) && s->count > ctx->scratch_start) {
        struct ink_ast_node *const stmt = ink_parser_scratch_last(s);

        switch (stmt->type) {
        case INK_AST_CHOICE_STAR_STMT:
        case INK_AST_CHOICE_PLUS_STMT:
        case INK_AST_SWITCH_CASE:
        case INK_AST_IF_BRANCH:
        case INK_AST_ELSE_BRANCH:
            stmt->data.bin.rhs = n;
            return ink_parser_scratch_pop(s);
        default:
            break;
        }
    }

    ctx->is_block_created = true;
    return n;
}

/**
 * Close the current block, if present.
 */
static struct ink_ast_node *
ink_parser_collect_block(struct ink_parser *p,
                         struct ink_parser_node_context *ctx, size_t level)
{
    struct ink_parser_scratch *const s = &p->scratch;
    struct ink_parser_stack *const open_blocks = &ctx->open_blocks;
    struct ink_ast_node *n = NULL;

    if (!ink_parser_stack_is_empty(open_blocks)) {
        const struct ink_parser_state block =
            ink_parser_stack_last(open_blocks);

        if (block.level >= level) {
            size_t b_start = block.source_offset;
            size_t b_end = 0;

            if (!ink_parser_scratch_is_empty(s)) {
                struct ink_ast_node *const last = ink_parser_scratch_last(s);

                b_end = last->bytes_end;
            } else {
                b_end = b_start;
            }

            n = ink_ast_node_sequence(INK_AST_BLOCK, b_start, b_end,
                                      block.scratch_offset, s, p->arena);
            n = ink_parser_fixup_block(p, ctx, n);
            ink_parser_stack_pop(open_blocks);
        }
    }
    return n;
}

/**
 * Collect open choice statements upto a given level.
 */
static struct ink_ast_node *
ink_parser_collect_context(struct ink_parser *p,
                           struct ink_parser_node_context *ctx, size_t level,
                           bool should_gather)
{
    struct ink_parser_scratch *const s = &p->scratch;
    struct ink_parser_stack *const open_choices = &ctx->open_choices;
    struct ink_parser_stack *const open_blocks = &ctx->open_blocks;

    /**
     * The level of the current choice should always be greater then the
     * level for the current block. Choice statements must have non-zero
     * levels, while blocks can levels greater than or equal to zero.
     *
     * Choice statement levels need not follow a sequentially increasing order.
     * When collecting choice branches, statements with levels less than the
     * previous statement will be included in the same enclosing choice if no
     * previous levels exist.
     */
    while (!ink_parser_stack_is_empty(open_choices)) {
        const struct ink_parser_state choice =
            ink_parser_stack_last(open_choices);
        struct ink_ast_node *n = NULL;

        assert(!ink_parser_stack_is_empty(open_blocks));

        if (choice.level > level) {
            ink_parser_stack_pop(open_choices);

            if (!ink_parser_stack_is_empty(open_blocks)) {
                const struct ink_parser_state block =
                    ink_parser_stack_last(open_blocks);

                if (choice.level <= block.level) {
                    n = ink_parser_collect_block(p, ctx, block.level);
                    if (n) {
                        ink_parser_scratch_push(s, n);
                    }
                }
            }
            if (!should_gather) {
                if (!ink_parser_stack_is_empty(open_choices)) {
                    const struct ink_parser_state prev_choice =
                        ink_parser_stack_last(open_choices);

                    if (level > prev_choice.level) {
                        ink_parser_stack_emplace(open_choices, level,
                                                 choice.scratch_offset,
                                                 choice.source_offset);
                        break;
                    }
                } else if (level > 0) {
                    ink_parser_stack_emplace(open_choices, level,
                                             choice.scratch_offset,
                                             choice.source_offset);
                    break;
                }
            }

            n = ink_ast_node_sequence(INK_AST_CHOICE_STMT, choice.source_offset,
                                      p->token.bytes_start,
                                      choice.scratch_offset, s, p->arena);

            if (n) {
                ink_parser_scratch_push(s, n);
            }
        } else {
            break;
        }
    }
    if (!should_gather) {
        return ink_parser_collect_block(p, ctx, level);
    }
    if (!ink_parser_scratch_is_empty(s)) {
        return ink_parser_scratch_pop(s);
    }
    return NULL;
}

/**
 * Close an open stitch, if present.
 */
static struct ink_ast_node *
ink_parser_collect_stitch(struct ink_parser *p,
                          struct ink_parser_node_context *ctx)
{
    struct ink_parser_scratch *const s = &p->scratch;
    struct ink_ast_node *const n = ink_parser_collect_context(p, ctx, 0, false);

    if (!ink_parser_scratch_is_empty(s)) {
        struct ink_ast_node *const proto = ink_parser_scratch_last(s);

        if (proto->type == INK_AST_STITCH_PROTO) {
            const size_t b_end = n ? n->bytes_end : proto->bytes_end;

            ink_parser_scratch_pop(s);
            return ink_ast_binary_new(INK_AST_STITCH_DECL, proto->bytes_start,
                                      b_end, proto, n, p->arena);
        } else if (proto->type == INK_AST_FUNC_PROTO) {
            const size_t b_end = n ? n->bytes_end : proto->bytes_end;

            ink_parser_scratch_pop(s);
            return ink_ast_binary_new(INK_AST_FUNC_DECL, proto->bytes_start,
                                      b_end, proto, n, p->arena);
        }
    }
    return n;
}

/**
 * Close an open knot, if present.
 */
static struct ink_ast_node *
ink_parser_collect_knot(struct ink_parser *p,
                        struct ink_parser_node_context *ctx)
{
    struct ink_parser_scratch *const s = &p->scratch;
    struct ink_ast_node *n = NULL;
    struct ink_ast_node_list *l = NULL;

    if (!ink_parser_scratch_is_empty(s)) {
        struct ink_ast_node *const child = ink_parser_collect_stitch(p, ctx);

        if (child) {
            ink_parser_scratch_push(s, child);
        }

        struct ink_ast_node *const proto = s->entries[ctx->knot_offset];

        if (proto->type == INK_AST_KNOT_PROTO) {
            l = ink_ast_list_from_scratch(s, ctx->knot_offset + 1, s->count,
                                          p->arena);
            ink_parser_scratch_pop(s);
            n = ink_ast_knot_decl_new(INK_AST_KNOT_DECL, proto->bytes_start,
                                      child ? child->bytes_end
                                            : proto->bytes_end,
                                      proto, l, p->arena);
        }
    }
    return n;
}

static void
ink_parser_handle_conditional_branch(struct ink_parser *p,
                                     struct ink_parser_node_context *ctx)
{
    struct ink_ast_node *const n = ink_parser_collect_context(p, ctx, 0, false);

    if (n) {
        ink_parser_scratch_push(&p->scratch, n);
    }
}

static void ink_parser_handle_choice_branch(struct ink_parser *p,
                                            struct ink_parser_node_context *ctx,
                                            struct ink_ast_node *node)
{
    struct ink_parser_stack *const open_blocks = &ctx->open_blocks;
    struct ink_parser_stack *const open_choices = &ctx->open_choices;
    struct ink_parser_scratch *const s = &p->scratch;
    const size_t level = ctx->choice_level;

    if (ink_parser_stack_is_empty(open_blocks)) {
        ink_parser_stack_emplace(open_blocks, 0, s->count, node->bytes_start);
    }
    if (ink_parser_stack_is_empty(open_choices)) {
        ink_parser_stack_emplace(open_choices, level, s->count,
                                 node->bytes_start);
    } else {
        const struct ink_parser_state choice =
            ink_parser_stack_last(open_choices);
        const struct ink_parser_state block =
            ink_parser_stack_last(open_blocks);

        if (level > choice.level) {
            if (block.level < choice.level) {
                ink_parser_stack_emplace(open_blocks, choice.level, s->count,
                                         p->token.bytes_start);
            }

            ink_parser_stack_emplace(open_choices, level, s->count,
                                     node->bytes_start);
        } else if (level == choice.level) {
            node = ink_parser_collect_block(p, ctx, level);
            if (node) {
                ink_parser_scratch_push(s, node);
            }
        } else {
            node = ink_parser_collect_context(p, ctx, level, false);
            if (node) {
                ink_parser_scratch_push(s, node);
            }
        }
    }
}

static void ink_parser_handle_gather(struct ink_parser *p,
                                     struct ink_parser_node_context *ctx,
                                     struct ink_ast_node **node)
{
    struct ink_parser_stack *const open_blocks = &ctx->open_blocks;
    struct ink_parser_stack *const open_choices = &ctx->open_choices;
    struct ink_parser_scratch *const s = &p->scratch;
    const struct ink_token t = p->token;
    const size_t level = ctx->choice_level;

    if (ink_parser_stack_is_empty(open_blocks)) {
        assert(ink_parser_stack_is_empty(open_choices));
        ink_parser_stack_emplace(open_blocks, 0, s->count,
                                 (*node)->bytes_start);
    }
    /**
     * Gather points terminate compound statements at the appropriate level.
     */
    if (!ink_parser_stack_is_empty(open_choices)) {
        const struct ink_parser_state choice =
            ink_parser_stack_last(open_choices);
        const struct ink_parser_state block =
            ink_parser_stack_last(open_blocks);

        if (level > choice.level) {
            if (block.level != choice.level) {
                ink_parser_stack_emplace(open_blocks, choice.level, s->count,
                                         (*node)->bytes_start);
            }
        } else if (!ink_parser_scratch_is_empty(s)) {
            struct ink_ast_node *tmp =
                ink_parser_collect_context(p, ctx, level - 1, true);

            if (tmp->type == INK_AST_CHOICE_STMT) {
                *node =
                    ink_ast_binary_new(INK_AST_GATHERED_STMT, tmp->bytes_start,
                                       t.bytes_start, tmp, *node, p->arena);
            }
            if (!ink_parser_stack_is_empty(open_blocks)) {
                const struct ink_parser_state block =
                    ink_parser_stack_last(open_blocks);

                if (block.level == level) {
                    tmp = ink_parser_collect_block(p, ctx, level);
                    if (tmp != NULL) {
                        ink_parser_scratch_push(s, tmp);
                    }
                }
            }
        }
    }
}

static void ink_parser_handle_content(struct ink_parser *p,
                                      struct ink_parser_node_context *ctx,
                                      struct ink_ast_node *node)
{
    struct ink_parser_scratch *const s = &p->scratch;
    struct ink_parser_stack *const open_blocks = &ctx->open_blocks;
    struct ink_parser_stack *const open_choices = &ctx->open_choices;

    if (ink_parser_stack_is_empty(open_blocks)) {
        ink_parser_stack_emplace(open_blocks, 0, s->count, node->bytes_start);
    }
    if (!ink_parser_stack_is_empty(open_choices)) {
        const struct ink_parser_state block =
            ink_parser_stack_last(open_blocks);
        const struct ink_parser_state choice =
            ink_parser_stack_last(open_choices);

        if (block.level != choice.level) {
            ink_parser_stack_emplace(open_blocks, choice.level, s->count,
                                     node->bytes_start);
        }
    }
}

static void ink_parser_handle_knot(struct ink_parser *p,
                                   struct ink_parser_node_context *ctx)
{
    struct ink_ast_node *const n = ink_parser_collect_knot(p, ctx);

    if (n) {
        ink_parser_scratch_push(&p->scratch, n);
    }

    ctx->knot_offset = p->scratch.count;
}

static void ink_parser_handle_stitch(struct ink_parser *p,
                                     struct ink_parser_node_context *ctx)
{
    struct ink_ast_node *const n = ink_parser_collect_stitch(p, ctx);

    if (n) {
        ink_parser_scratch_push(&p->scratch, n);
    }
}

static void ink_parser_handle_func(struct ink_parser *p,
                                   struct ink_parser_node_context *ctx)
{
    ink_parser_handle_stitch(p, ctx);
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
                                           enum ink_ast_node_type type)
{
    /* NOTE: Advancing the parser MUST only happen after the node is
     * created. This prevents trailing whitespace. */
    const struct ink_token t = parser->token;
    struct ink_ast_node *const n =
        ink_ast_leaf_new(type, t.bytes_start, t.bytes_end, parser->arena);

    ink_parser_advance(parser);
    return n;
}

static struct ink_ast_node *ink_parse_true(struct ink_parser *p)
{
    return ink_parse_atom(p, INK_AST_TRUE);
}

static struct ink_ast_node *ink_parse_false(struct ink_parser *p)
{
    return ink_parse_atom(p, INK_AST_FALSE);
}

static struct ink_ast_node *ink_parse_number(struct ink_parser *p)
{
    return ink_parse_atom(p, INK_AST_NUMBER);
}

static struct ink_ast_node *ink_parse_identifier(struct ink_parser *p)
{
    return ink_parse_atom(p, INK_AST_IDENTIFIER);
}

static struct ink_ast_node *ink_parse_expect_identifier(struct ink_parser *p)
{
    if (!ink_parser_check(p, INK_TT_IDENTIFIER)) {
        return ink_parser_error(p, INK_AST_E_EXPECTED_IDENTIFIER, &p->token);
    }
    return ink_parse_identifier(p);
}

static struct ink_ast_node *ink_parse_expect_expr(struct ink_parser *p)
{
    const struct ink_token t = p->token;
    struct ink_ast_node *const n = ink_parse_expr(p);

    if (!n) {
        ink_parser_error(p, INK_AST_E_EXPECTED_EXPR, &t);
    }
    return n;
}

static size_t ink_parse_expect_stmt_end(struct ink_parser *p)
{
    if (!ink_parser_check(p, INK_TT_EOF) && !ink_parser_check(p, INK_TT_NL)) {
        ink_parser_error(p, INK_AST_E_EXPECTED_NEWLINE, &p->token);
        return p->token.bytes_start;
    }
    return ink_parser_advance(p);
}

static struct ink_ast_node *
ink_parse_string(struct ink_parser *p, const enum ink_token_type *token_set)
{
    const struct ink_token t = p->token;

    while (!ink_parser_check_many(p, token_set)) {
        ink_parser_advance(p);
    }
    return ink_ast_leaf_new(t.bytes_start == p->token.bytes_start
                                ? INK_AST_EMPTY_STRING
                                : INK_AST_STRING,
                            t.bytes_start, p->token.bytes_start, p->arena);
}

static struct ink_ast_node *ink_parse_arglist(struct ink_parser *p)
{
    struct ink_ast_node *n = NULL;
    struct ink_parser_scratch *const s = &p->scratch;
    const size_t s_top = s->count;
    const size_t b_start = ink_parser_expect_token(p, INK_TT_LEFT_PAREN);
    size_t cnt = 0;

    if (!ink_parser_check(p, INK_TT_RIGHT_PAREN)) {
        for (;;) {
            n = ink_parse_expr(p);
            if (cnt == INK_PARSER_ARGS_MAX) {
                ink_parser_error(p, INK_AST_E_TOO_MANY_PARAMS, &p->token);
                break;
            }

            cnt++;
            ink_parser_scratch_push(s, n);

            if (ink_parser_check(p, INK_TT_COMMA)) {
                ink_parser_advance(p);
            } else {
                break;
            }
        }
    }

    ink_parser_expect_token(p, INK_TT_RIGHT_PAREN);
    return ink_ast_node_sequence(INK_AST_ARG_LIST, b_start,
                                 p->token.bytes_start, s_top, &p->scratch,
                                 p->arena);
}

static struct ink_ast_node *ink_parse_identifier_expr(struct ink_parser *p)
{
    struct ink_ast_node *lhs = ink_parse_expect_identifier(p);

    if (!lhs) {
        return lhs;
    }
    for (;;) {
        struct ink_ast_node *rhs = NULL;

        switch (p->token.type) {
        case INK_TT_DOT:
            ink_parser_advance(p);

            rhs = ink_parse_expect_identifier(p);
            if (!rhs) {
                return rhs;
            }

            lhs = ink_ast_binary_new(INK_AST_SELECTOR_EXPR, lhs->bytes_start,
                                     p->token.bytes_start, lhs, rhs, p->arena);
            break;
        case INK_TT_LEFT_PAREN:
            rhs = ink_parse_arglist(p);
            return ink_ast_binary_new(INK_AST_CALL_EXPR, lhs->bytes_start,
                                      p->token.bytes_start, lhs, rhs, p->arena);
        default:
            return lhs;
        }
    }
}

static struct ink_ast_node *ink_parse_divert(struct ink_parser *p)
{
    const struct ink_token t = p->token;
    struct ink_ast_node *n = NULL;

    ink_parser_advance(p);
    n = ink_parse_identifier_expr(p);
    return ink_ast_binary_new(INK_AST_DIVERT, t.bytes_start,
                              p->token.bytes_start, n, NULL, p->arena);
}

static struct ink_ast_node *ink_parse_string_expr(struct ink_parser *p)
{
    static const enum ink_token_type token_set[] = {
        INK_TT_DOUBLE_QUOTE,
        INK_TT_NL,
        INK_TT_EOF,
    };
    const size_t b_start = ink_parser_expect_token(p, INK_TT_DOUBLE_QUOTE);
    struct ink_ast_node *const lhs = ink_parse_string(p, token_set);

    if (!ink_parser_check(p, INK_TT_DOUBLE_QUOTE)) {
        return ink_parser_error(p, INK_AST_E_EXPECTED_DQUOTE, &p->token);
    }

    ink_parser_advance(p);
    return ink_ast_binary_new(INK_AST_STRING_EXPR, b_start,
                              p->token.bytes_start, lhs, NULL, p->arena);
}

static struct ink_ast_node *ink_parse_primary_expr(struct ink_parser *p)
{
    struct ink_ast_node *n = NULL;

    switch (p->token.type) {
    case INK_TT_NUMBER:
        return ink_parse_number(p);
    case INK_TT_KEYWORD_TRUE:
        return ink_parse_true(p);
    case INK_TT_KEYWORD_FALSE:
        return ink_parse_false(p);
    case INK_TT_IDENTIFIER:
        return ink_parse_identifier_expr(p);
    case INK_TT_DOUBLE_QUOTE:
        return ink_parse_string_expr(p);
    case INK_TT_LEFT_PAREN:
        ink_parser_advance(p);

        n = ink_parse_infix_expr(p, NULL, INK_PREC_NONE);
        if (!n) {
            return n;
        }
        if (!ink_parser_match(p, INK_TT_RIGHT_PAREN)) {
            return n;
        }
        return n;
    default:
        return n;
    }
}

static struct ink_ast_node *ink_parse_prefix_expr(struct ink_parser *p)
{
    struct ink_ast_node *n = NULL;
    const struct ink_token t = p->token;

    switch (t.type) {
    case INK_TT_KEYWORD_NOT:
    case INK_TT_MINUS:
    case INK_TT_BANG:
        ink_parser_advance(p);
        n = ink_parse_prefix_expr(p);
        if (!n) {
            return n;
        }
        return ink_ast_binary_new(ink_token_prefix_type(t.type), t.bytes_start,
                                  n->bytes_end, n, NULL, p->arena);
    case INK_TT_RIGHT_ARROW:
        return ink_parse_divert(p);
    default:
        return ink_parse_primary_expr(p);
    }
}

static struct ink_ast_node *ink_parse_infix_expr(struct ink_parser *p,
                                                 struct ink_ast_node *lhs,
                                                 enum ink_precedence prec)
{
    if (!lhs) {
        lhs = ink_parse_prefix_expr(p);
        if (!lhs) {
            return lhs;
        }
    }
    for (;;) {
        struct ink_ast_node *rhs = NULL;
        const struct ink_token t = p->token;
        const enum ink_precedence t_prec = ink_binding_power(t.type);

        if (ink_binding_power(t.type) > prec) {
            ink_parser_advance(p);

            rhs = ink_parse_infix_expr(p, NULL, t_prec);
            if (!rhs) {
                return rhs;
            }

            lhs = ink_ast_binary_new(ink_token_infix_type(t.type),
                                     lhs->bytes_start, rhs->bytes_end, lhs, rhs,
                                     p->arena);
        } else {
            break;
        }
    }
    return lhs;
}

static struct ink_ast_node *ink_parse_divert_expr(struct ink_parser *p)
{
    const size_t b_start = ink_parser_advance(p);
    struct ink_ast_node *const n = ink_parse_identifier_expr(p);

    return ink_ast_binary_new(INK_AST_DIVERT, b_start, p->token.bytes_start, n,
                              NULL, p->arena);
}

static struct ink_ast_node *ink_parse_expr(struct ink_parser *p)
{
    return ink_parse_infix_expr(p, NULL, INK_PREC_NONE);
}

static struct ink_ast_node *ink_parse_return_stmt(struct ink_parser *p)
{
    const struct ink_token t = p->token;
    struct ink_ast_node *n = NULL;

    ink_parser_advance(p);

    if (!ink_parser_check(p, INK_TT_NL) && !ink_parser_check(p, INK_TT_EOF)) {
        n = ink_parse_expr(p);
    }
    return ink_ast_binary_new(INK_AST_RETURN_STMT, t.bytes_start,
                              ink_parse_expect_stmt_end(p), n, NULL, p->arena);
}

static struct ink_ast_node *ink_parse_divert_stmt(struct ink_parser *p)
{
    const struct ink_token t = p->token;
    struct ink_ast_node *n = NULL;

    ink_parser_push_scanner(p, INK_GRAMMAR_EXPRESSION);
    n = ink_parse_divert_expr(p);
    ink_parser_pop_scanner(p);
    return ink_ast_binary_new(INK_AST_DIVERT_STMT, t.bytes_start,
                              ink_parse_expect_stmt_end(p), n, NULL, p->arena);
}

static struct ink_ast_node *ink_parse_glue(struct ink_parser *p)
{
    const struct ink_token t = p->token;

    ink_parser_advance(p);
    return ink_ast_leaf_new(INK_AST_GLUE, t.bytes_start, t.bytes_end, p->arena);
}

static struct ink_ast_node *ink_parse_temp_decl(struct ink_parser *p)
{
    const size_t b_start = ink_parser_advance(p);
    struct ink_ast_node *const lhs = ink_parse_expect_identifier(p);
    struct ink_ast_node *rhs = NULL;

    if (!lhs) {
        return lhs;
    }

    ink_parser_expect_token(p, INK_TT_EQUAL);

    rhs = ink_parse_expect_expr(p);
    if (!rhs) {
        return rhs;
    }
    return ink_ast_binary_new(INK_AST_TEMP_DECL, b_start,
                              ink_parse_expect_stmt_end(p), lhs, rhs, p->arena);
}

static struct ink_ast_node *ink_parse_expr_stmt(struct ink_parser *p,
                                                struct ink_ast_node *lhs)
{
    const size_t b_start = lhs ? lhs->bytes_start : p->token.bytes_start;
    struct ink_ast_node *const n = ink_parse_infix_expr(p, lhs, INK_PREC_NONE);

    return ink_ast_binary_new(INK_AST_EXPR_STMT, b_start,
                              ink_parse_expect_stmt_end(p), n, NULL, p->arena);
}

static struct ink_ast_node *ink_parse_assign_stmt(struct ink_parser *p)
{
    const struct ink_token t = p->token;
    struct ink_ast_node *const lhs = ink_parse_identifier_expr(p);
    struct ink_ast_node *rhs = NULL;

    if (!ink_parser_match(p, INK_TT_EQUAL)) {
        return ink_parse_expr_stmt(p, lhs);
    }

    rhs = ink_parse_expect_expr(p);
    if (!rhs) {
        return rhs;
    }
    return ink_ast_binary_new(INK_AST_ASSIGN_STMT, t.bytes_start,
                              ink_parse_expect_stmt_end(p), lhs, rhs, p->arena);
}

static struct ink_ast_node *ink_parse_tilde_stmt(struct ink_parser *p)
{
    struct ink_ast_node *lhs = NULL;

    ink_parser_push_scanner(p, INK_GRAMMAR_EXPRESSION);
    ink_parser_advance(p);

    switch (p->token.type) {
    case INK_TT_KEYWORD_TEMP:
        lhs = ink_parse_temp_decl(p);
        break;
    case INK_TT_KEYWORD_RETURN:
        lhs = ink_parse_return_stmt(p);
        break;
    case INK_TT_IDENTIFIER:
        lhs = ink_parse_assign_stmt(p);
        break;
    default:
        lhs = ink_parse_expr_stmt(p, NULL);
        break;
    }

    ink_parser_pop_scanner(p);
    return lhs;
}

static struct ink_ast_node *
ink_parse_content(struct ink_parser *p, const enum ink_token_type *token_set)
{
    const struct ink_token t = p->token;
    struct ink_parser_scratch *const s = &p->scratch;
    const size_t s_top = s->count;

    for (;;) {
        struct ink_ast_node *n = NULL;

        if (!ink_parser_check_many(p, token_set)) {
            n = ink_parse_string(p, token_set);
        } else {
            switch (p->token.type) {
            case INK_TT_LEFT_BRACE:
                n = ink_parse_lbrace_expr(p);
                break;
            case INK_TT_RIGHT_ARROW:
                n = ink_parse_divert_stmt(p);
                break;
            case INK_TT_GLUE:
                n = ink_parse_glue(p);
                break;
            default:
                goto exit_loop;
            }
        }
        if (n) {
            ink_parser_scratch_push(s, n);
        }
    }
    if (ink_parser_check(p, INK_TT_NL)) {
        ink_parser_advance(p);
    }
exit_loop:
    return ink_ast_node_sequence(
        t.bytes_start == p->token.bytes_start ? INK_AST_EMPTY_STRING
                                              : INK_AST_CONTENT,
        t.bytes_start, p->token.bytes_start, s_top, s, p->arena);
}

static struct ink_ast_node *
ink_parse_conditional_branch(struct ink_parser *p, enum ink_ast_node_type type)
{
    const struct ink_token t = p->token;
    struct ink_ast_node *n = NULL;

    if (ink_parser_match(p, INK_TT_KEYWORD_ELSE)) {
        type = INK_AST_ELSE_BRANCH;
    } else {
        n = ink_parse_expr(p);
        if (!n) {
            return n;
        }
    }
    if (!ink_parser_match(p, INK_TT_COLON)) {
        return NULL;
    }
    if (ink_parser_check(p, INK_TT_NL)) {
        ink_parser_advance(p);
    }
    return ink_ast_binary_new(type, t.bytes_start, p->token.bytes_start, n,
                              NULL, p->arena);
}

static struct ink_ast_node *ink_parse_conditional(struct ink_parser *p,
                                                  struct ink_ast_node *expr)
{
    enum ink_ast_node_type type = 0;
    struct ink_parser_node_context ctx;
    struct ink_ast_node *n = NULL;
    struct ink_ast_node_list *l = NULL;
    const struct ink_token t = p->token;
    struct ink_parser_scratch *const s = &p->scratch;
    const size_t s_top = s->count;

    ink_parser_node_context_init(p, &ctx);

    ctx.node = expr;
    ctx.is_conditional = true;
    ctx.is_block_created = false;

    if (setjmp(ctx.jmpbuf) == 0) {
        while (!ink_parser_check(p, INK_TT_EOF) &&
               !ink_parser_check(p, INK_TT_RIGHT_BRACE)) {
            n = ink_parse_stmt(p, &ctx);
            if (n) {
                ink_parser_scratch_push(s, n);
            }
        }

        n = ink_parser_collect_context(p, &ctx, 0, false);
        if (n) {
            ink_parser_scratch_push(s, n);
        }

        l = ink_ast_list_from_scratch(s, s_top, s->count, p->arena);
        if (expr && !ctx.is_block_created) {
            type = INK_AST_SWITCH_STMT;
        } else if (!expr && !ctx.is_block_created) {
            type = INK_AST_MULTI_IF_STMT;
        } else {
            type = INK_AST_IF_STMT;
        }

        n = ink_ast_switch_stmt_new(type, t.bytes_start, p->token.bytes_start,
                                    expr, l, p->arena);
    }

    ink_parser_node_context_deinit(p, &ctx);
    return n;
}

static struct ink_ast_node *ink_parse_lbrace_expr(struct ink_parser *p)
{
    static const enum ink_token_type token_set[] = {
        INK_TT_LEFT_BRACE, INK_TT_RIGHT_BRACE, INK_TT_RIGHT_ARROW,
        INK_TT_GLUE,       INK_TT_NL,          INK_TT_EOF,
    };

    struct ink_ast_node *lhs = NULL;
    struct ink_ast_node *rhs = NULL;
    const struct ink_token t = p->token;

    ink_parser_push_scanner(p, INK_GRAMMAR_EXPRESSION);
    ink_parser_advance(p);

    if (!ink_parser_check(p, INK_TT_NL)) {
        lhs = ink_parse_expr(p);
        if (!lhs) {
            ink_parser_pop_scanner(p);
            ink_parser_error(p, INK_AST_E_INVALID_EXPR, &p->token);
            ink_parser_advance(p);
            return NULL;
        }
        if (ink_parser_check(p, INK_TT_COLON)) {
            ink_parser_push_scanner(p, INK_GRAMMAR_CONTENT);
            ink_parser_advance(p);

            if (ink_parser_check(p, INK_TT_NL)) {
                ink_parser_advance(p);
                rhs = ink_parse_conditional(p, lhs);
            } else {
                rhs = ink_parse_content(p, token_set);
                rhs = ink_ast_binary_new(INK_AST_IF_EXPR, t.bytes_start,
                                         p->token.bytes_start, lhs, rhs,
                                         p->arena);
            }

            ink_parser_pop_scanner(p);
            ink_parser_expect_token(p, INK_TT_RIGHT_BRACE);
        } else {
            ink_parser_pop_scanner(p);
            ink_parser_expect_token(p, INK_TT_RIGHT_BRACE);
            return ink_ast_binary_new(INK_AST_INLINE_LOGIC, t.bytes_start,
                                      p->token.bytes_start, lhs, NULL,
                                      p->arena);
        }
    } else {
        ink_parser_advance(p);
        ink_parser_push_scanner(p, INK_GRAMMAR_CONTENT);
        rhs = ink_parse_conditional(p, NULL);
        ink_parser_expect_token(p, INK_TT_RIGHT_BRACE);
        ink_parser_pop_scanner(p);
    }

    ink_parser_pop_scanner(p);
    return rhs;
}

static struct ink_ast_node *ink_parse_content_stmt(struct ink_parser *p)
{
    static const enum ink_token_type token_set[] = {
        INK_TT_LEFT_BRACE, INK_TT_RIGHT_BRACE, INK_TT_RIGHT_ARROW,
        INK_TT_GLUE,       INK_TT_NL,          INK_TT_EOF,
    };
    const size_t b_start = p->token.bytes_start;
    struct ink_ast_node *const n = ink_parse_content(p, token_set);
    const size_t b_end = n ? n->bytes_end : p->token.bytes_start;

    if (ink_parser_check(p, INK_TT_NL)) {
        ink_parser_advance(p);
        /* FIXME: Trailing whitespace is interpreted as empty content. */
        ink_parser_match(p, INK_TT_WHITESPACE);
    }
    return ink_ast_binary_new(INK_AST_CONTENT_STMT, b_start, b_end, n, NULL,
                              p->arena);
}

static struct ink_ast_node *ink_parse_choice_expr(struct ink_parser *p)
{
    static const enum ink_token_type token_set[] = {
        INK_TT_LEFT_BRACE,    INK_TT_LEFT_BRACKET, INK_TT_RIGHT_BRACE,
        INK_TT_RIGHT_BRACKET, INK_TT_RIGHT_ARROW,  INK_TT_NL,
        INK_TT_EOF,
    };
    struct ink_ast_node *lhs = NULL;
    struct ink_ast_node *mhs = NULL;
    struct ink_ast_node *rhs = NULL;
    const struct ink_token t = p->token;

    lhs = ink_parse_string(p, token_set);
    if (lhs) {
        if (lhs->type != INK_AST_EMPTY_STRING) {
            lhs->type = INK_AST_CHOICE_START_EXPR;
        }
    }
    if (ink_parser_check(p, INK_TT_LEFT_BRACKET)) {
        ink_parser_advance(p);
        ink_parser_match(p, INK_TT_WHITESPACE);

        if (!ink_parser_check(p, INK_TT_RIGHT_BRACKET)) {
            mhs = ink_parse_string(p, token_set);
            if (mhs) {
                if (mhs->type != INK_AST_EMPTY_STRING) {
                    mhs->type = INK_AST_CHOICE_OPTION_EXPR;
                }
            }
        }

        ink_parser_expect_token(p, INK_TT_RIGHT_BRACKET);

        if (!ink_parser_check_many(p, token_set)) {
            rhs = ink_parse_string(p, token_set);
            if (rhs) {
                if (rhs->type != INK_AST_EMPTY_STRING) {
                    rhs->type = INK_AST_CHOICE_INNER_EXPR;
                }
            }
        }
    }
    return ink_ast_choice_expr_new(INK_AST_CHOICE_EXPR, t.bytes_start,
                                   p->token.bytes_start, lhs, mhs, rhs,
                                   p->arena);
}

static struct ink_ast_node *
ink_parse_choice_stmt(struct ink_parser *p, struct ink_parser_node_context *ctx)
{
    struct ink_ast_node *n = NULL;
    const struct ink_token t = p->token;
    size_t b_end = 0;

    ctx->choice_level = ink_parser_match_many(p, t.type, true);
    n = ink_parse_choice_expr(p);
    b_end = n ? n->bytes_end : p->token.bytes_start;

    if (ink_parser_check(p, INK_TT_NL)) {
        ink_parser_advance(p);
    }
    return ink_ast_binary_new(ink_branch_type(t.type), t.bytes_start, b_end, n,
                              NULL, p->arena);
}

static struct ink_ast_node *
ink_parse_gather_point(struct ink_parser *p,
                       struct ink_parser_node_context *ctx)
{
    struct ink_ast_node *n = NULL;
    const struct ink_token t = p->token;
    size_t b_end = 0;

    ctx->choice_level = ink_parser_match_many(p, t.type, true);
    b_end = p->token.bytes_start;
    ink_parser_match(p, INK_TT_WHITESPACE);
    ink_parser_match(p, INK_TT_NL);
    return ink_ast_binary_new(INK_AST_GATHER_POINT_STMT, t.bytes_start, b_end,
                              n, NULL, p->arena);
}

static struct ink_ast_node *ink_parse_var(struct ink_parser *p,
                                          enum ink_ast_node_type type)
{
    struct ink_ast_node *lhs = NULL;
    struct ink_ast_node *rhs = NULL;
    const struct ink_token t = p->token;

    ink_parser_push_scanner(p, INK_GRAMMAR_EXPRESSION);
    ink_parser_advance(p);

    lhs = ink_parse_expect_identifier(p);
    if (!lhs) {
        ink_parser_pop_scanner(p);
        return NULL;
    }

    ink_parser_expect_token(p, INK_TT_EQUAL);
    rhs = ink_parse_expect_expr(p);
    ink_parser_pop_scanner(p);
    return ink_ast_binary_new(type, t.bytes_start, ink_parse_expect_stmt_end(p),
                              lhs, rhs, p->arena);
}

static struct ink_ast_node *ink_parse_var_decl(struct ink_parser *p)
{
    return ink_parse_var(p, INK_AST_VAR_DECL);
}

static struct ink_ast_node *ink_parse_const_decl(struct ink_parser *p)
{
    return ink_parse_var(p, INK_AST_CONST_DECL);
}

static struct ink_ast_node *ink_parse_parameter_decl(struct ink_parser *p)
{
    struct ink_ast_node *n = NULL;

    if (ink_parser_check(p, INK_TT_KEYWORD_REF)) {
        ink_parser_advance(p);
        n = ink_parse_expect_identifier(p);
        if (n) {
            n->type = INK_AST_REF_PARAM_DECL;
        }
    } else {
        n = ink_parse_expect_identifier(p);
        if (n) {
            n->type = INK_AST_PARAM_DECL;
        }
    }
    return n;
}

static struct ink_ast_node *ink_parse_parameter_list(struct ink_parser *p)
{
    struct ink_parser_scratch *const s = &p->scratch;
    size_t s_top = s->count;
    size_t b_start = ink_parser_expect_token(p, INK_TT_LEFT_PAREN);

    if (!ink_parser_check(p, INK_TT_RIGHT_PAREN)) {
        for (;;) {
            struct ink_ast_node *n = NULL;
            size_t cnt = 0;

            if (cnt == INK_PARSER_ARGS_MAX) {
                ink_parser_error(p, INK_AST_E_TOO_MANY_PARAMS, &p->token);
                break;
            } else {
                cnt++;
            }

            n = ink_parse_parameter_decl(p);
            if (n) {
                ink_parser_scratch_push(s, n);
            }
            if (ink_parser_check(p, INK_TT_COMMA)) {
                ink_parser_advance(p);
            } else {
                break;
            }
        }
    }

    ink_parser_expect_token(p, INK_TT_RIGHT_PAREN);
    return ink_ast_node_sequence(INK_AST_PARAM_LIST, b_start,
                                 p->token.bytes_start, s_top, s, p->arena);
}

static struct ink_ast_node *ink_parse_knot_decl(struct ink_parser *p)
{
    enum ink_ast_node_type type = INK_AST_STITCH_PROTO;
    struct ink_ast_node *lhs = NULL;
    struct ink_ast_node *rhs = NULL;
    const size_t b_start = ink_parser_advance(p);

    ink_parser_push_scanner(p, INK_GRAMMAR_EXPRESSION);
    ink_parser_match(p, INK_TT_WHITESPACE);

    if (ink_parser_check(p, INK_TT_EQUAL)) {
        type = INK_AST_KNOT_PROTO;

        while (ink_parser_check(p, INK_TT_EQUAL)) {
            ink_parser_advance(p);
        }
    }
    if (ink_scanner_try_keyword(&p->scanner, &p->token,
                                INK_TT_KEYWORD_FUNCTION)) {
        ink_parser_advance(p);
        type = INK_AST_FUNC_PROTO;
    }

    lhs = ink_parse_expect_identifier(p);
    if (!lhs) {
        ink_parser_pop_scanner(p);
        return NULL;
    }
    if (ink_parser_check(p, INK_TT_LEFT_PAREN)) {
        rhs = ink_parse_parameter_list(p);
    }
    while (ink_parser_check(p, INK_TT_EQUAL) ||
           ink_parser_check(p, INK_TT_EQUAL_EQUAL)) {
        ink_parser_advance(p);
    }

    ink_parser_pop_scanner(p);
    return ink_ast_binary_new(type, b_start, ink_parse_expect_stmt_end(p), lhs,
                              rhs, p->arena);
}

static struct ink_ast_node *ink_parse_stmt(struct ink_parser *p,
                                           struct ink_parser_node_context *ctx)
{
    struct ink_ast_node *n = NULL;

    ink_parser_match(p, INK_TT_WHITESPACE);

    switch (p->token.type) {
    case INK_TT_EOF:
        break;
    case INK_TT_STAR:
    case INK_TT_PLUS:
        n = ink_parse_choice_stmt(p, ctx);
        break;
    case INK_TT_MINUS:
        if (ctx->is_conditional) {
            ink_parser_push_scanner(p, INK_GRAMMAR_EXPRESSION);
            ink_parser_advance(p);

            if (ctx->node) {
                n = ink_parse_conditional_branch(p, INK_AST_SWITCH_CASE);
            } else {
                n = ink_parse_conditional_branch(p, INK_AST_IF_BRANCH);
            }
            if (!n) {
                ink_parser_rewind_scanner(p);
                ink_parser_push_scanner(p, INK_GRAMMAR_CONTENT);
                ink_parser_advance(p);
                n = ink_parse_gather_point(p, ctx);
                ink_parser_pop_scanner(p);
            }

            ink_parser_pop_scanner(p);
        } else {
            n = ink_parse_gather_point(p, ctx);
        }
        break;
    case INK_TT_TILDE:
        n = ink_parse_tilde_stmt(p);
        break;
    case INK_TT_RIGHT_ARROW:
        n = ink_parse_divert_stmt(p);
        break;
    case INK_TT_EQUAL:
    case INK_TT_EQUAL_EQUAL:
        /**
         * FIXME: For some reason, the EQUAL_EQUAL bug keeps coming back.
         * This is due to the mode stack not being popped early enough.
         */
        if (!ctx->is_conditional) {
            n = ink_parse_knot_decl(p);
        } else {
            n = ink_parse_content_stmt(p);
        }
        break;
    case INK_TT_RIGHT_BRACE:
        /* TODO: Expand upon this to validate balanced delimiters. */
        ink_parser_error(p, INK_AST_E_UNEXPECTED_TOKEN, &p->token);
        ink_parser_advance(p);
        break;
    default: {
        struct ink_token *const t = &p->token;

        if (ink_scanner_try_keyword(&p->scanner, t, INK_TT_KEYWORD_CONST)) {
            n = ink_parse_const_decl(p);
        } else if (ink_scanner_try_keyword(&p->scanner, t,
                                           INK_TT_KEYWORD_VAR)) {
            n = ink_parse_var_decl(p);
        } else {
            n = ink_parse_content_stmt(p);
        }
        break;
    }
    }
    if (!n || p->panic_mode) {
        ink_parser_sync(p);
        ink_parser_match(p, INK_TT_NL);
        return NULL;
    }
    switch (n->type) {
    case INK_AST_IF_BRANCH:
    case INK_AST_ELSE_BRANCH:
    case INK_AST_SWITCH_CASE:
        ink_parser_handle_conditional_branch(p, ctx);
        break;
    case INK_AST_CHOICE_STAR_STMT:
    case INK_AST_CHOICE_PLUS_STMT:
        ink_parser_handle_choice_branch(p, ctx, n);
        break;
    case INK_AST_GATHER_POINT_STMT:
        ink_parser_handle_gather(p, ctx, &n);
        break;
    case INK_AST_KNOT_PROTO:
        ink_parser_handle_knot(p, ctx);
        break;
    case INK_AST_STITCH_PROTO:
        ink_parser_handle_stitch(p, ctx);
        break;
    case INK_AST_FUNC_PROTO:
        ink_parser_handle_func(p, ctx);
        break;
    default:
        ink_parser_handle_content(p, ctx, n);
        break;
    }
    return n;
}

static struct ink_ast_node *ink_parse_file(struct ink_parser *p)
{
    struct ink_parser_node_context ctx;
    struct ink_parser_scratch *s = &p->scratch;
    struct ink_ast_node *n = NULL;

    ink_parser_node_context_init(p, &ctx);

    if (setjmp(ctx.jmpbuf) == 0) {
        while (!ink_parser_check(p, INK_TT_EOF)) {
            n = ink_parse_stmt(p, &ctx);
            if (n) {
                ink_parser_scratch_push(s, n);
            }
        }

        n = ink_parser_collect_knot(p, &ctx);
        if (n) {
            ink_parser_scratch_push(s, n);
        }
    }

    ink_parser_node_context_deinit(p, &ctx);
    return ink_ast_node_sequence(INK_AST_FILE, 0, p->token.bytes_start, 0,
                                 &p->scratch, p->arena);
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
    struct ink_parser p;

    ink_ast_init(tree, filename, source_bytes);
    ink_parser_init(&p, tree, arena, flags);
    ink_parser_advance(&p);

    tree->root = ink_parse_file(&p);

    ink_parser_deinit(&p);
    return INK_E_OK;
}

#undef INK_PARSER_MEMOIZE
#undef INK_PARSER_ARGS_MAX
#undef INK_PARSE_DEPTH
