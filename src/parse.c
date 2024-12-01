#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "common.h"
#include "logging.h"
#include "parse.h"
#include "platform.h"
#include "scanner.h"
#include "token.h"
#include "tree.h"
#include "vec.h"

#define INK_HASHTABLE_SCALE_FACTOR 2u
#define INK_HASHTABLE_LOAD_MAX .75
#define INK_HASHTABLE_MIN_CAPACITY 16
#define INK_PARSER_CACHE_SCALE_FACTOR INK_HASHTABLE_SCALE_FACTOR
#define INK_PARSER_CACHE_LOAD_MAX INK_HASHTABLE_LOAD_MAX
#define INK_PARSER_CACHE_MIN_CAPACITY INK_HASHTABLE_MIN_CAPACITY

#define INK_VA_ARGS_NTH(_1, _2, _3, _4, _5, N, ...) N
#define INK_VA_ARGS_COUNT(...) INK_VA_ARGS_NTH(__VA_ARGS__, 5, 4, 3, 2, 1, 0)

#define INK_DISPATCH_0(func, _1) func()
#define INK_DISPATCH_1(func, _1) func(_1)
#define INK_DISPATCH_2(func, _1, _2) func(_1, _2)
#define INK_DISPATCH_3(func, _1, _2, _3) func(_1, _2, _3)
#define INK_DISPATCH_4(func, _1, _2, _3, _4) func(_1, _2, _3, _4)
#define INK_DISPATCH_5(func, _1, _2, _3, _4, _5) func(_1, _2, _3, _4, _5)

#define INK_DISPATCH_SELECT(func, count, ...)                                  \
    INK_DISPATCH_##count(func, __VA_ARGS__)

#define INK_DISPATCHER(func, count, ...)                                       \
    INK_DISPATCH_SELECT(func, count, __VA_ARGS__)

#define INK_DISPATCH(func, ...)                                                \
    INK_DISPATCHER(func, INK_VA_ARGS_COUNT(__VA_ARGS__), __VA_ARGS__)

#define INK_PARSER_TRACE(node, rule, ...)                                      \
    do {                                                                       \
        if (parser->flags & INK_PARSER_F_TRACING) {                            \
            ink_parser_trace(parser, #rule);                                   \
        }                                                                      \
    } while (0)

#define INK_PARSER_MEMOIZE(node, rule, ...)                                    \
    do {                                                                       \
        int rc;                                                                \
        const size_t source_offset = parser->current_offset;                   \
                                                                               \
        if (parser->flags & INK_PARSER_F_CACHING) {                            \
            rc = ink_parser_cache_lookup(&parser->cache, source_offset,        \
                                         (void *)rule, &node);                 \
            if (rc < 0) {                                                      \
                node = INK_DISPATCH(rule, __VA_ARGS__);                        \
                ink_parser_cache_insert(&parser->cache, source_offset,         \
                                        (void *)rule, node);                   \
            } else {                                                           \
                ink_trace("Parser cache hit!");                                \
                parser->current_offset = node->end_offset;                     \
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

INK_VEC_DECLARE(ink_parser_scratch, struct ink_syntax_node *)

struct ink_parser_context {
    size_t level;
    size_t scratch_offset;
    size_t source_offset;
    struct ink_syntax_node *node;
};

struct ink_parser_context_stack {
    size_t depth;
    struct ink_parser_context entries[INK_PARSE_DEPTH];
};

struct ink_parser_cache_key {
    size_t source_offset;
    void *rule_address;
};

struct ink_parser_cache_entry {
    struct ink_parser_cache_key key;
    struct ink_syntax_node *value;
};

/**
 * Parser memoization cache.
 *
 * Cache keys are ordered pairs of (source_offset, rule_address).
 */
struct ink_parser_cache {
    size_t count;
    size_t capacity;
    struct ink_parser_cache_entry *entries;
};

/**
 * Ink parsing state.
 *
 * The goal of this parser is to provide a faster, simpler, and more
 * portable alternative to Inkle's canonical implementation of Ink.
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
    int flags;
    bool panic_mode;
    size_t current_offset;
    size_t current_level;
    struct ink_parser_context_stack blocks;
    struct ink_parser_context_stack choices;
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

static struct ink_syntax_node *
ink_parse_content_expr(struct ink_parser *, const enum ink_token_type *);
static struct ink_syntax_node *ink_parse_content_stmt(struct ink_parser *);
static struct ink_syntax_node *ink_parse_primary_expr(struct ink_parser *);
static struct ink_syntax_node *ink_parse_infix_expr(struct ink_parser *,
                                                    struct ink_syntax_node *,
                                                    enum ink_precedence);
static struct ink_syntax_node *ink_parse_divert_stmt(struct ink_parser *);
static struct ink_syntax_node *ink_parse_stmt(struct ink_parser *parser);
static struct ink_syntax_node *ink_parse_logic_expr(struct ink_parser *);
static struct ink_syntax_node *ink_parse_argument_list(struct ink_parser *);

static void ink_parser_cache_initialize(struct ink_parser_cache *cache)
{
    cache->count = 0;
    cache->capacity = 0;
    cache->entries = NULL;
}

static void ink_parser_cache_cleanup(struct ink_parser_cache *cache)
{
    const size_t size = sizeof(*cache->entries) * cache->capacity;

    if (cache->entries) {
        platform_mem_dealloc(cache->entries, size);
    }
}

/**
 * TODO(Brett): Inline?
 */
static unsigned int ink_parser_cache_key_hash(struct ink_parser_cache_key key)
{
    const size_t length = sizeof(key);
    const unsigned char *bytes = (unsigned char *)&key;
    const unsigned int fnv_prime = 0x01000193;
    unsigned int hash = 0x811c9dc5;

    for (size_t i = 0; i < length; i++) {
        hash = hash ^ bytes[i];
        hash = hash * fnv_prime;
    }
    return hash;
}

static inline size_t ink_parser_cache_next_size(struct ink_parser_cache *cache)
{
    if (cache->capacity < INK_PARSER_CACHE_MIN_CAPACITY) {
        return INK_PARSER_CACHE_MIN_CAPACITY;
    } else {
        return cache->capacity * INK_PARSER_CACHE_SCALE_FACTOR;
    }
}

static inline bool ink_parser_cache_needs_resize(struct ink_parser_cache *cache)
{
    return (float)(cache->count + 1) >
           ((float)cache->capacity * INK_PARSER_CACHE_LOAD_MAX);
}

static inline bool
ink_parser_cache_entry_is_set(const struct ink_parser_cache_entry *entry)
{
    return entry->key.rule_address != NULL;
}

static inline void
ink_parser_cache_entry_set(struct ink_parser_cache_entry *entry,
                           const struct ink_parser_cache_key *key,
                           struct ink_syntax_node *value)
{
    memcpy(&entry->key, key, sizeof(*key));
    entry->value = value;
}

static inline bool
ink_parser_cache_key_compare(const struct ink_parser_cache_key *a,
                             const struct ink_parser_cache_key *b)
{
    return (a->source_offset == b->source_offset) &&
           (a->rule_address == b->rule_address);
}

static struct ink_parser_cache_entry *
ink_parser_cache_find_slot(struct ink_parser_cache_entry *entries,
                           size_t capacity, struct ink_parser_cache_key key)
{
    size_t i = ink_parser_cache_key_hash(key) & (capacity - 1);

    for (;;) {
        struct ink_parser_cache_entry *slot = &entries[i];

        if (!ink_parser_cache_entry_is_set(slot) ||
            ink_parser_cache_key_compare(&key, &slot->key)) {
            return slot;
        }

        i = (i + 1) & (capacity - 1);
    }
}

static int ink_parser_cache_resize(struct ink_parser_cache *cache)
{
    size_t new_count = 0;
    const size_t old_capacity = cache->capacity;
    const size_t new_capacity = ink_parser_cache_next_size(cache);
    const size_t new_size = sizeof(*cache->entries) * new_capacity;
    struct ink_parser_cache_entry *new_entries = NULL;

    new_entries = malloc(new_size);
    if (new_entries == NULL) {
        return -INK_E_PARSE_PANIC;
    }

    memset(new_entries, 0, new_size);

    for (size_t index = 0; index < old_capacity; index++) {
        struct ink_parser_cache_entry *dst_entry = NULL;
        const struct ink_parser_cache_entry *src_entry = &cache->entries[index];

        if (ink_parser_cache_entry_is_set(src_entry)) {
            dst_entry = ink_parser_cache_find_slot(new_entries, new_capacity,
                                                   src_entry->key);
            dst_entry->key = src_entry->key;
            dst_entry->value = src_entry->value;
            new_count++;
        }
    }

    free(cache->entries);

    cache->count = new_count;
    cache->capacity = new_capacity;
    cache->entries = new_entries;
    return INK_E_OK;
}

/**
 * Lookup a cached entry by (source_offset, parse_rule) key pair.
 */
static int ink_parser_cache_lookup(const struct ink_parser_cache *cache,
                                   size_t source_offset, void *parse_rule,
                                   struct ink_syntax_node **value)
{
    const struct ink_parser_cache_key key = {
        .source_offset = source_offset,
        .rule_address = parse_rule,
    };
    struct ink_parser_cache_entry *entry;

    if (cache->count == 0) {
        return -INK_E_PARSE_PANIC;
    }

    entry = ink_parser_cache_find_slot(cache->entries, cache->capacity, key);
    if (!ink_parser_cache_entry_is_set(entry)) {
        return -INK_E_PARSE_PANIC;
    }

    *value = entry->value;
    return INK_E_OK;
}

/**
 * Insert an entry into the parser's cache by (source_offset, parse_rule)
 * key pair.
 */
int ink_parser_cache_insert(struct ink_parser_cache *cache,
                            size_t source_offset, void *parse_rule,
                            struct ink_syntax_node *value)
{
    const struct ink_parser_cache_key key = {
        .source_offset = source_offset,
        .rule_address = parse_rule,
    };
    struct ink_parser_cache_entry *entry;

    if (ink_parser_cache_needs_resize(cache)) {
        ink_parser_cache_resize(cache);
    }

    entry = ink_parser_cache_find_slot(cache->entries, cache->capacity, key);
    if (!ink_parser_cache_entry_is_set(entry)) {
        ink_parser_cache_entry_set(entry, &key, value);
        cache->count++;
        return INK_E_OK;
    }
    return -INK_E_PARSE_PANIC;
}

static struct ink_syntax_seq *
ink_seq_from_scratch(struct ink_arena *arena,
                     struct ink_parser_scratch *scratch, size_t start_offset,
                     size_t end_offset)
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

static inline struct ink_token *
ink_parser_current_token(struct ink_parser *parser)
{
    return &parser->token;
}

static inline void ink_parser_push_scanner(struct ink_parser *parser,
                                           enum ink_grammar_type type)
{
    ink_scanner_push(&parser->scanner, type, parser->current_offset);
}

static inline void ink_parser_pop_scanner(struct ink_parser *parser)
{
    ink_scanner_pop(&parser->scanner);
}

static inline void ink_parser_next_token(struct ink_parser *parser)
{
    ink_scanner_next(&parser->scanner, &parser->token);
    parser->current_offset = parser->scanner.start_offset;
}

static inline bool ink_parser_check(struct ink_parser *parser,
                                    enum ink_token_type type)
{
    const struct ink_token *token = ink_parser_current_token(parser);

    return token->type == type;
}

/**
 * Print parser tracing information to the console.
 */
static void ink_parser_trace(struct ink_parser *parser, const char *rule_name)
{
    const struct ink_token *token = ink_parser_current_token(parser);

    ink_trace("Entering %s(PendingChoices=%zu, PendingBlocks=%zu, "
              "TokenType=%s, SourceOffset: %zu, "
              "Level: %zu)",
              rule_name, parser->choices.depth, parser->blocks.depth,
              ink_token_type_strz(token->type), parser->current_offset,
              parser->current_level);
}

static void ink_parser_rewind_scanner(struct ink_parser *parser)
{
    const struct ink_scanner_mode *mode = ink_scanner_current(&parser->scanner);

    if (parser->flags & INK_PARSER_F_TRACING) {
        ink_trace("Rewinding scanner to %zu", parser->current_offset);
    }

    ink_scanner_rewind(&parser->scanner, mode->source_offset);
    parser->current_offset = mode->source_offset;
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
    ink_token_print(parser->scanner.source, ink_parser_current_token(parser));

    return NULL;
}

static inline struct ink_syntax_node *
ink_parser_create_node(struct ink_parser *parser,
                       enum ink_syntax_node_type type, size_t source_start,
                       size_t source_end, struct ink_syntax_node *lhs,
                       struct ink_syntax_node *rhs, struct ink_syntax_seq *seq)
{
    if (type == INK_NODE_INVALID) {
        ink_parser_error(parser, "Invalid parse!");
    }
    return ink_syntax_node_new(parser->arena, type, source_start, source_end,
                               lhs, rhs, seq);
}

static inline struct ink_syntax_node *
ink_parser_create_leaf(struct ink_parser *parser,
                       enum ink_syntax_node_type type, size_t source_start,
                       size_t source_end)
{
    return ink_parser_create_node(parser, type, source_start, source_end, NULL,
                                  NULL, NULL);
}

static inline struct ink_syntax_node *
ink_parser_create_unary(struct ink_parser *parser,
                        enum ink_syntax_node_type type, size_t source_start,
                        size_t source_end, struct ink_syntax_node *lhs)
{
    return ink_parser_create_node(parser, type, source_start, source_end, lhs,
                                  NULL, NULL);
}

static inline struct ink_syntax_node *
ink_parser_create_binary(struct ink_parser *parser,
                         enum ink_syntax_node_type type, size_t source_start,
                         size_t source_end, struct ink_syntax_node *lhs,
                         struct ink_syntax_node *rhs)
{
    return ink_parser_create_node(parser, type, source_start, source_end, lhs,
                                  rhs, NULL);
}

static inline struct ink_syntax_node *
ink_parser_create_sequence(struct ink_parser *parser,
                           enum ink_syntax_node_type type, size_t source_start,
                           size_t source_end, size_t scratch_offset)
{
    struct ink_syntax_seq *seq = NULL;

    if (parser->scratch.count != scratch_offset) {
        seq = ink_seq_from_scratch(parser->arena, &parser->scratch,
                                   scratch_offset, parser->scratch.count);
        /* TODO(Brett): Handle and log error. */
    }
    return ink_parser_create_node(parser, type, source_start, source_end, NULL,
                                  NULL, seq);
}

static size_t ink_parser_advance(struct ink_parser *parser)
{
    if (!ink_parser_check(parser, INK_TT_EOF)) {
        ink_parser_next_token(parser);
    }
    return parser->scanner.cursor_offset;
}

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
    size_t source_offset = parser->current_offset;
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

static int ink_parser_initialize(struct ink_parser *parser,
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
    parser->panic_mode = false;
    parser->flags = flags;
    parser->token.type = 0;
    parser->token.start_offset = 0;
    parser->token.end_offset = 0;
    parser->current_offset = 0;
    parser->current_level = 0;
    parser->blocks.depth = 0;
    parser->blocks.entries[0] = (struct ink_parser_context){0};
    parser->choices.depth = 0;
    parser->choices.entries[0] = (struct ink_parser_context){0};

    ink_parser_scratch_create(&parser->scratch);
    ink_parser_scratch_reserve(&parser->scratch, INK_VEC_COUNT_MIN);
    ink_parser_cache_initialize(&parser->cache);

    return INK_E_OK;
}

static void ink_parser_cleanup(struct ink_parser *parser)
{
    ink_parser_scratch_destroy(&parser->scratch);
    ink_parser_cache_cleanup(&parser->cache);
    memset(parser, 0, sizeof(*parser));
}

static struct ink_syntax_node *
ink_parse_atom(struct ink_parser *parser, enum ink_syntax_node_type node_type)
{
    /* NOTE: Advancing the parser MUST only happen after the node is
     * created. This prevents trailing whitespace. */
    const struct ink_token token = parser->token;
    struct ink_syntax_node *node = ink_parser_create_leaf(
        parser, node_type, token.start_offset, token.end_offset);

    ink_parser_advance(parser);

    return node;
}

static struct ink_syntax_node *ink_parse_true(struct ink_parser *parser)
{
    return ink_parse_atom(parser, INK_NODE_TRUE_EXPR);
}

static struct ink_syntax_node *ink_parse_false(struct ink_parser *parser)
{
    return ink_parse_atom(parser, INK_NODE_FALSE_EXPR);
}

static struct ink_syntax_node *ink_parse_number(struct ink_parser *parser)
{
    return ink_parse_atom(parser, INK_NODE_NUMBER_EXPR);
}

static struct ink_syntax_node *ink_parse_identifier(struct ink_parser *parser)
{
    return ink_parse_atom(parser, INK_NODE_IDENTIFIER_EXPR);
}

static struct ink_syntax_node *
ink_parse_string(struct ink_parser *parser,
                 const enum ink_token_type *token_set)
{
    const size_t source_start = parser->current_offset;

    while (!ink_parser_check_many(parser, token_set)) {
        ink_parser_advance(parser);
    }
    return ink_parser_create_leaf(parser, INK_NODE_STRING_LITERAL, source_start,
                                  parser->current_offset);
}

static struct ink_syntax_node *ink_parse_name_expr(struct ink_parser *parser)
{
    struct ink_syntax_node *lhs = NULL;
    struct ink_syntax_node *rhs = NULL;
    const size_t source_start = parser->current_offset;

    INK_PARSER_RULE(lhs, ink_parse_identifier, parser);
    if (!ink_parser_check(parser, INK_TT_LEFT_PAREN)) {
        return lhs;
    }

    INK_PARSER_RULE(lhs, ink_parse_argument_list, parser);

    return ink_parser_create_binary(parser, INK_NODE_CALL_EXPR, source_start,
                                    parser->current_offset, lhs, rhs);
}

static struct ink_syntax_node *ink_parse_divert(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t source_start = parser->current_offset;

    ink_parser_advance(parser);
    INK_PARSER_RULE(node, ink_parse_identifier, parser);

    return ink_parser_create_unary(parser, INK_NODE_DIVERT, source_start,
                                   parser->current_offset, node);
}

static struct ink_syntax_node *
ink_parse_string_expr(struct ink_parser *parser,
                      const enum ink_token_type *token_set)
{
    struct ink_syntax_node *node = NULL;
    const size_t scratch_offset = parser->scratch.count;
    const size_t source_start = ink_parser_expect(parser, INK_TT_DOUBLE_QUOTE);

    do {
        if (!ink_parser_check_many(parser, token_set)) {
            INK_PARSER_RULE(node, ink_parse_string, parser, token_set);
        } else if (ink_parser_check(parser, INK_TT_LEFT_BRACE)) {
            INK_PARSER_RULE(node, ink_parse_logic_expr, parser);
        } else {
            break;
        }

        ink_parser_scratch_append(&parser->scratch, node);
    } while (!ink_parser_check(parser, INK_TT_EOF) &&
             !ink_parser_check(parser, INK_TT_DOUBLE_QUOTE));

    ink_parser_expect(parser, INK_TT_DOUBLE_QUOTE);

    return ink_parser_create_sequence(parser, INK_NODE_STRING_EXPR,
                                      source_start, parser->current_offset,
                                      scratch_offset);
}

static struct ink_syntax_node *ink_parse_primary_expr(struct ink_parser *parser)
{
    static const enum ink_token_type token_set[] = {
        INK_TT_DOUBLE_QUOTE, INK_TT_LEFT_BRACE, INK_TT_RIGHT_BRACE,
        INK_TT_NL,           INK_TT_EOF,
    };
    const struct ink_token *token = ink_parser_current_token(parser);
    const size_t source_start = parser->current_offset;
    struct ink_syntax_node *node = NULL;

    switch (token->type) {
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
        ink_parser_expect(parser, INK_TT_RIGHT_PAREN);
        break;
    }
    default:
        node = ink_parser_create_leaf(parser, INK_NODE_INVALID, source_start,
                                      parser->current_offset);
        break;
    }
    return node;
}

static struct ink_syntax_node *ink_parse_prefix_expr(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;

    switch (parser->token.type) {
    case INK_TT_KEYWORD_NOT:
    case INK_TT_MINUS:
    case INK_TT_BANG: {
        const enum ink_token_type type = parser->token.type;
        const size_t source_start = ink_parser_advance(parser);

        INK_PARSER_RULE(node, ink_parse_prefix_expr, parser);
        node =
            ink_parser_create_unary(parser, ink_token_prefix_type(type),
                                    source_start, parser->current_offset, node);
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
    struct ink_syntax_node *rhs = NULL;

    if (!lhs) {
        INK_PARSER_RULE(lhs, ink_parse_prefix_expr, parser);
    }

    type = parser->token.type;
    token_prec = ink_binding_power(type);

    while (token_prec > prec) {
        const size_t source_start = ink_parser_advance(parser);

        INK_PARSER_RULE(rhs, ink_parse_infix_expr, parser, NULL, token_prec);
        lhs = ink_parser_create_binary(parser, ink_token_infix_type(type),
                                       source_start, parser->current_offset,
                                       lhs, rhs);
        type = parser->token.type;
        token_prec = ink_binding_power(type);
    }
    return lhs;
}

static struct ink_syntax_node *
ink_parse_divert_or_tunnel(struct ink_parser *parser, bool *is_tunnel)
{
    enum ink_syntax_node_type node_type = INK_NODE_INVALID;
    struct ink_syntax_node *node = NULL;
    const size_t source_start = parser->current_offset;

    if (ink_parser_check(parser, INK_TT_RIGHT_ARROW)) {
        ink_parser_advance(parser);

        if (ink_parser_check(parser, INK_TT_RIGHT_ARROW)) {
            ink_parser_advance(parser);

            if (ink_parser_check(parser, INK_TT_IDENTIFIER)) {
                INK_PARSER_RULE(node, ink_parse_name_expr, parser);
            }

            node_type = INK_NODE_TUNNEL_ONWARDS;
        } else if (ink_parser_check(parser, INK_TT_IDENTIFIER)) {
            INK_PARSER_RULE(node, ink_parse_name_expr, parser);
            node_type = INK_NODE_DIVERT;
        } else {
            *is_tunnel = true;
            return NULL;
        }
    }
    return ink_parser_create_unary(parser, node_type, source_start,
                                   parser->current_offset, node);
}

static struct ink_syntax_node *ink_parse_thread_expr(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t source_start = parser->current_offset;

    ink_parser_push_scanner(parser, INK_GRAMMAR_EXPRESSION);
    ink_parser_advance(parser);

    if (ink_parser_check(parser, INK_TT_IDENTIFIER)) {
        node = ink_parse_name_expr(parser);
    }

    ink_parser_pop_scanner(parser);

    return ink_parser_create_unary(parser, INK_NODE_THREAD_EXPR, source_start,
                                   parser->current_offset, node);
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
    const size_t source_start = parser->current_offset;

    ink_parser_advance(parser);

    if (!ink_parser_check(parser, INK_TT_NL) &&
        !ink_parser_check(parser, INK_TT_EOF)) {
        node = ink_parse_expr(parser);
    }
    return ink_parser_create_unary(parser, INK_NODE_RETURN_STMT, source_start,
                                   parser->current_offset, node);
}

static struct ink_syntax_node *ink_parse_divert_stmt(struct ink_parser *parser)
{
    bool flag = 0;
    struct ink_syntax_node *node = NULL;
    const size_t scratch_offset = parser->scratch.count;
    const size_t source_start = parser->current_offset;

    ink_parser_push_scanner(parser, INK_GRAMMAR_EXPRESSION);

    while (ink_parser_check(parser, INK_TT_RIGHT_ARROW)) {
        INK_PARSER_RULE(node, ink_parse_divert_or_tunnel, parser, &flag);
        ink_parser_scratch_append(&parser->scratch, node);
    }

    ink_parser_pop_scanner(parser);

    if (ink_parser_check(parser, INK_TT_NL)) {
        ink_parser_advance(parser);
    }
    if (flag || parser->scratch.count - scratch_offset > 1) {
        return ink_parser_create_sequence(parser, INK_NODE_TUNNEL_STMT,
                                          source_start, parser->current_offset,
                                          scratch_offset);
    }
    return ink_parser_create_sequence(parser, INK_NODE_DIVERT_STMT,
                                      source_start, parser->current_offset,
                                      scratch_offset);
}

static struct ink_syntax_node *ink_parse_temp_stmt(struct ink_parser *parser)
{
    struct ink_syntax_node *lhs = NULL;
    struct ink_syntax_node *rhs = NULL;
    const size_t source_start = parser->current_offset;

    ink_parser_advance(parser);
    lhs = ink_parse_identifier(parser);
    ink_parser_expect(parser, INK_TT_EQUAL);
    rhs = ink_parse_expr(parser);

    return ink_parser_create_binary(parser, INK_NODE_TEMP_STMT, source_start,
                                    parser->current_offset, lhs, rhs);
}

static struct ink_syntax_node *ink_parse_thread_stmt(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t source_start = parser->current_offset;

    INK_PARSER_RULE(node, ink_parse_thread_expr, parser);
    ink_parser_expect_stmt_end(parser);

    return ink_parser_create_unary(parser, INK_NODE_THREAD_STMT, source_start,
                                   parser->current_offset, node);
}

static struct ink_syntax_node *ink_parse_logic_stmt(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t source_start = parser->current_offset;

    ink_parser_push_scanner(parser, INK_GRAMMAR_EXPRESSION);
    ink_parser_advance(parser);

    if (ink_parser_check(parser, INK_TT_KEYWORD_TEMP)) {
        node = ink_parse_temp_stmt(parser);
    } else if (ink_parser_check(parser, INK_TT_KEYWORD_RETURN)) {
        node = ink_parse_return_stmt(parser);
    } else {
        node = ink_parse_expr(parser);
        node =
            ink_parser_create_unary(parser, INK_NODE_LOGIC_STMT, source_start,
                                    parser->current_offset, node);
    }

    ink_parser_pop_scanner(parser);
    ink_parser_expect_stmt_end(parser);

    return node;
}

static struct ink_syntax_node *
ink_parse_conditional_branch(struct ink_parser *parser)
{
    struct ink_syntax_node *lhs = NULL;
    struct ink_syntax_node *rhs = NULL;
    const size_t source_start = parser->current_offset;

    ink_parser_push_scanner(parser, INK_GRAMMAR_EXPRESSION);
    ink_parser_advance(parser);
    INK_PARSER_RULE(lhs, ink_parse_expr, parser);
    ink_parser_pop_scanner(parser);
    ink_parser_expect(parser, INK_TT_COLON);
    INK_PARSER_RULE(rhs, ink_parse_stmt, parser);

    return ink_parser_create_binary(parser, INK_NODE_CONDITIONAL_BRANCH,
                                    source_start, parser->current_offset, lhs,
                                    rhs);
}

static struct ink_syntax_node *ink_parse_sequence(struct ink_parser *parser,
                                                  struct ink_syntax_node *expr)
{
    static const enum ink_token_type token_set[] = {
        INK_TT_LEFT_BRACE,  INK_TT_LEFT_ARROW, INK_TT_RIGHT_BRACE,
        INK_TT_RIGHT_ARROW, INK_TT_PIPE,       INK_TT_NL,
        INK_TT_EOF,
    };
    const size_t scratch_offset = parser->scratch.count;
    const size_t source_start = parser->current_offset;
    struct ink_syntax_node *node = NULL;

    if (expr) {
        INK_PARSER_RULE(node, ink_parse_content_expr, parser, token_set);
        ink_parser_scratch_append(&parser->scratch, node);
    } else {
        ink_parser_advance(parser);
        INK_PARSER_RULE(node, ink_parse_content_expr, parser, token_set);

        if (!ink_parser_check(parser, INK_TT_PIPE))
            return NULL;

        ink_parser_scratch_append(&parser->scratch, node);
    }
    while (!ink_parser_check(parser, INK_TT_EOF) &&
           !ink_parser_check(parser, INK_TT_NL) &&
           !ink_parser_check(parser, INK_TT_RIGHT_BRACE)) {
        INK_PARSER_RULE(node, ink_parse_content_expr, parser, token_set);
        ink_parser_scratch_append(&parser->scratch, node);

        if (ink_parser_check(parser, INK_TT_PIPE)) {
            ink_parser_advance(parser);
        }
    }
    return ink_parser_create_sequence(parser, INK_NODE_SEQUENCE_EXPR,
                                      source_start, parser->current_offset,
                                      scratch_offset);
}

static struct ink_syntax_node *ink_parse_conditional(struct ink_parser *parser)
{
    struct ink_syntax_node *expr = NULL;
    struct ink_syntax_node *content = NULL;
    const size_t source_start = parser->current_offset;

    ink_parser_advance(parser);

    if (ink_parser_check(parser, INK_TT_NL)) {
        ink_parser_advance(parser);
        ink_parser_eat(parser, INK_TT_WHITESPACE);

        if (ink_parser_check(parser, INK_TT_MINUS)) {
            INK_PARSER_RULE(content, ink_parse_conditional_branch, parser);
        } else {
            INK_PARSER_RULE(content, ink_parse_stmt, parser);
        }
    } else {
        INK_PARSER_RULE(expr, ink_parse_expr, parser);

        if (ink_parser_check(parser, INK_TT_COLON)) {
            ink_parser_advance(parser);
            INK_PARSER_RULE(content, ink_parse_sequence, parser, expr);

            return ink_parser_create_binary(
                parser, INK_NODE_CONDITIONAL_STMT, source_start,
                parser->current_offset, expr, content);
        }
    }
    return NULL;
}

static struct ink_syntax_node *ink_parse_logic_expr(struct ink_parser *parser)
{
    const size_t source_start = parser->current_offset;
    struct ink_syntax_node *node = NULL;

    ink_parser_push_scanner(parser, INK_GRAMMAR_EXPRESSION);
    INK_PARSER_RULE(node, ink_parse_conditional, parser);

    if (node == NULL) {
        ink_parser_rewind_scanner(parser);
        ink_parser_push_scanner(parser, INK_GRAMMAR_CONTENT);
        ink_parser_advance(parser);
        INK_PARSER_RULE(node, ink_parse_sequence, parser, NULL);
        ink_parser_pop_scanner(parser);

        if (node == NULL) {
            ink_parser_rewind_scanner(parser);
            ink_parser_advance(parser);
            ink_parser_advance(parser);
            INK_PARSER_RULE(node, ink_parse_expr, parser);
        }
    }

    ink_parser_pop_scanner(parser);
    ink_parser_expect(parser, INK_TT_RIGHT_BRACE);

    return ink_parser_create_unary(parser, INK_NODE_LOGIC_EXPR, source_start,
                                   parser->current_offset, node);
}

static struct ink_syntax_node *
ink_parse_content_expr(struct ink_parser *parser,
                       const enum ink_token_type *token_set)
{
    struct ink_syntax_node *node = NULL;
    const size_t source_start = parser->current_offset;
    const size_t scratch_offset = parser->scratch.count;

    for (;;) {
        if (!ink_parser_check_many(parser, token_set)) {
            INK_PARSER_RULE(node, ink_parse_string, parser, token_set);
        } else if (ink_parser_check(parser, INK_TT_LEFT_BRACE)) {
            INK_PARSER_RULE(node, ink_parse_logic_expr, parser);
        } else if (ink_parser_check(parser, INK_TT_RIGHT_ARROW)) {
            INK_PARSER_RULE(node, ink_parse_divert_stmt, parser);
        } else if (ink_parser_check(parser, INK_TT_LEFT_ARROW)) {
            INK_PARSER_RULE(node, ink_parse_thread_expr, parser);
        } else {
            break;
        }
        ink_parser_scratch_append(&parser->scratch, node);
    }
    return ink_parser_create_sequence(parser, INK_NODE_CONTENT_EXPR,
                                      source_start, parser->current_offset,
                                      scratch_offset);
}

static struct ink_syntax_node *ink_parse_content_stmt(struct ink_parser *parser)
{
    static const enum ink_token_type token_set[] = {
        INK_TT_LEFT_BRACE,  INK_TT_LEFT_ARROW, INK_TT_RIGHT_BRACE,
        INK_TT_RIGHT_ARROW, INK_TT_NL,         INK_TT_EOF,
    };
    const size_t source_start = parser->current_offset;
    struct ink_syntax_node *node = NULL;

    /* There seems to be a bug in Inkle's implementation that I found
     * through I036, having to do with how lone right curly braces are
     * handled. This is a workaround to get the tests to pass. */
    if (ink_parser_check(parser, INK_TT_RIGHT_BRACE)) {
        ink_parser_advance(parser);
        return NULL;
    }

    INK_PARSER_RULE(node, ink_parse_content_expr, parser, token_set);
    ink_parser_advance(parser);

    return ink_parser_create_unary(parser, INK_NODE_CONTENT_STMT, source_start,
                                   parser->current_offset, node);
}

static struct ink_syntax_node *
ink_parse_choice_content(struct ink_parser *parser)
{
    static const enum ink_token_type token_set[] = {
        INK_TT_LEFT_BRACE,  INK_TT_LEFT_ARROW,    INK_TT_LEFT_BRACKET,
        INK_TT_RIGHT_BRACE, INK_TT_RIGHT_BRACKET, INK_TT_RIGHT_ARROW,
        INK_TT_NL,          INK_TT_EOF,
    };
    const size_t scratch_offset = parser->scratch.count;
    const size_t source_start = parser->current_offset;
    struct ink_syntax_node *node = NULL;

    INK_PARSER_RULE(node, ink_parse_string, parser, token_set);
    ink_parser_scratch_append(&parser->scratch, node);

    if (ink_parser_check(parser, INK_TT_LEFT_BRACKET)) {
        ink_parser_advance(parser);
        INK_PARSER_RULE(node, ink_parse_string, parser, token_set);
        ink_parser_scratch_append(&parser->scratch, node);
        ink_parser_expect(parser, INK_TT_RIGHT_BRACKET);

        if (!ink_parser_check_many(parser, token_set)) {
            INK_PARSER_RULE(node, ink_parse_string, parser, token_set);
            ink_parser_scratch_append(&parser->scratch, node);
        } else {
            ink_parser_scratch_append(&parser->scratch, NULL);
        }

    } else {
        ink_parser_scratch_append(&parser->scratch, NULL);
        ink_parser_scratch_append(&parser->scratch, NULL);
    }
    return ink_parser_create_sequence(parser, INK_NODE_CHOICE_CONTENT_EXPR,
                                      source_start, parser->current_offset,
                                      scratch_offset);
}

static struct ink_syntax_node *ink_parse_choice(struct ink_parser *parser)
{
    struct ink_syntax_node *expr = NULL;
    struct ink_syntax_node *body = NULL;
    const enum ink_token_type token_type = parser->token.type;
    const size_t source_start = parser->current_offset;
    const size_t level = ink_parser_eat_many(parser, token_type, true);

    INK_PARSER_RULE(expr, ink_parse_choice_content, parser);

    parser->current_level = level;

    if (ink_parser_check(parser, INK_TT_NL)) {
        ink_parser_advance(parser);
    }
    return ink_parser_create_binary(parser, ink_branch_type(token_type),
                                    source_start, parser->current_offset, expr,
                                    body);
}

static struct ink_syntax_node *ink_parse_gather(struct ink_parser *parser)
{
    return NULL;
}

static struct ink_syntax_node *ink_parse_var(struct ink_parser *parser)
{
    struct ink_syntax_node *lhs = NULL;
    struct ink_syntax_node *rhs = NULL;
    const size_t source_start = parser->current_offset;

    ink_parser_push_scanner(parser, INK_GRAMMAR_EXPRESSION);
    ink_parser_advance(parser);
    INK_PARSER_RULE(lhs, ink_parse_identifier, parser);
    ink_parser_expect(parser, INK_TT_EQUAL);

    if (ink_parser_check(parser, INK_TT_RIGHT_ARROW)) {
        INK_PARSER_RULE(rhs, ink_parse_divert, parser);
    } else {
        INK_PARSER_RULE(rhs, ink_parse_expr, parser);
    }

    ink_parser_pop_scanner(parser);
    ink_parser_expect_stmt_end(parser);

    return ink_parser_create_binary(parser, INK_NODE_VAR_DECL, source_start,
                                    parser->current_offset, lhs, rhs);
}

static struct ink_syntax_node *ink_parse_const(struct ink_parser *parser)
{
    struct ink_syntax_node *lhs = NULL;
    struct ink_syntax_node *rhs = NULL;
    const size_t source_start = parser->current_offset;

    ink_parser_push_scanner(parser, INK_GRAMMAR_EXPRESSION);
    ink_parser_advance(parser);
    INK_PARSER_RULE(lhs, ink_parse_identifier, parser);
    ink_parser_expect(parser, INK_TT_EQUAL);

    if (ink_parser_check(parser, INK_TT_RIGHT_ARROW)) {
        INK_PARSER_RULE(rhs, ink_parse_divert, parser);
    } else {
        INK_PARSER_RULE(rhs, ink_parse_expr, parser);
    }

    ink_parser_pop_scanner(parser);
    ink_parser_expect_stmt_end(parser);

    return ink_parser_create_binary(parser, INK_NODE_CONST_DECL, source_start,
                                    parser->current_offset, lhs, rhs);
}

static struct ink_syntax_node *
ink_parse_argument_list(struct ink_parser *parser)
{
    size_t arg_count = 0;
    struct ink_syntax_node *node = NULL;
    const size_t scratch_offset = parser->scratch.count;
    const size_t source_start = ink_parser_expect(parser, INK_TT_LEFT_PAREN);

    if (!ink_parser_check(parser, INK_TT_RIGHT_PAREN)) {
        for (;;) {
            node = ink_parse_expr(parser);
            if (arg_count == 255) {
                ink_parser_error(parser, "Too many arguments");
                break;
            }

            arg_count++;
            ink_parser_scratch_append(&parser->scratch, node);

            if (!ink_parser_check(parser, INK_TT_COMMA)) {
                break;
            }
            ink_parser_advance(parser);
        }
    }

    ink_parser_expect(parser, INK_TT_RIGHT_PAREN);

    return ink_parser_create_sequence(parser, INK_NODE_ARG_LIST, source_start,
                                      parser->current_offset, scratch_offset);
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
    const size_t scratch_offset = parser->scratch.count;
    const size_t source_start = ink_parser_expect(parser, INK_TT_LEFT_PAREN);

    if (!ink_parser_check(parser, INK_TT_RIGHT_PAREN)) {
        for (;;) {
            if (arg_count == 255) {
                ink_parser_error(parser, "Too many parameters");
                break;
            } else {
                arg_count++;
            }

            node = ink_parse_parameter_decl(parser);
            ink_parser_scratch_append(&parser->scratch, node);

            if (ink_parser_check(parser, INK_TT_COMMA)) {
                ink_parser_advance(parser);
            } else {
                break;
            }
        }
    }

    ink_parser_expect(parser, INK_TT_RIGHT_PAREN);

    return ink_parser_create_sequence(parser, INK_NODE_PARAM_LIST, source_start,
                                      parser->current_offset, scratch_offset);
}

static struct ink_syntax_node *ink_parse_knot(struct ink_parser *parser)
{
    enum ink_syntax_node_type node_type = INK_NODE_KNOT_DECL;
    struct ink_syntax_node *node = NULL;
    const size_t scratch_offset = parser->scratch.count;
    const size_t source_start = parser->current_offset;

    parser->current_level = 0;

    while (ink_parser_check(parser, INK_TT_EQUAL)) {
        ink_parser_advance(parser);
    }

    ink_parser_push_scanner(parser, INK_GRAMMAR_EXPRESSION);
    ink_parser_eat(parser, INK_TT_WHITESPACE);

    if (ink_scanner_try_keyword(&parser->scanner, &parser->token,
                                INK_TT_KEYWORD_FUNCTION)) {
        node_type = INK_NODE_FUNCTION_DECL;
        ink_parser_advance(parser);
    }

    INK_PARSER_RULE(node, ink_parse_identifier, parser);
    ink_parser_scratch_append(&parser->scratch, node);

    if (ink_parser_check(parser, INK_TT_LEFT_PAREN)) {
        INK_PARSER_RULE(node, ink_parse_parameter_list, parser);
        ink_parser_scratch_append(&parser->scratch, node);
    }

    /* NOTE(Brett): Checking for `==` as a separate token is a hack, but
     * I don't have any brighter ideas.
     */
    while (ink_parser_check(parser, INK_TT_EQUAL) ||
           ink_parser_check(parser, INK_TT_EQUAL_EQUAL)) {
        ink_parser_advance(parser);
    }

    ink_parser_scratch_append(&parser->scratch, NULL);
    ink_parser_pop_scanner(parser);
    ink_parser_expect_stmt_end(parser);

    return ink_parser_create_sequence(parser, node_type, source_start,
                                      parser->current_offset, scratch_offset);
}

static struct ink_syntax_node *ink_parse_stmt(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;

    ink_parser_eat(parser, INK_TT_WHITESPACE);

    switch (parser->token.type) {
    case INK_TT_EOF:
        break;
    case INK_TT_STAR:
    case INK_TT_PLUS:
        INK_PARSER_RULE(node, ink_parse_choice, parser);
        break;
    case INK_TT_MINUS:
        INK_PARSER_RULE(node, ink_parse_gather, parser);
        break;
    case INK_TT_EQUAL:
        INK_PARSER_RULE(node, ink_parse_knot, parser);
        break;
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
    default:
        if (ink_scanner_try_keyword(&parser->scanner, &parser->token,
                                    INK_TT_KEYWORD_CONST)) {
            INK_PARSER_RULE(node, ink_parse_const, parser);
        } else if (ink_scanner_try_keyword(&parser->scanner, &parser->token,
                                           INK_TT_KEYWORD_VAR)) {
            INK_PARSER_RULE(node, ink_parse_var, parser);
        } else {
            INK_PARSER_RULE(node, ink_parse_content_stmt, parser);
        }
        break;
    }
    return node;
}

static void ink_parser_context_shift(struct ink_parser *parser,
                                     struct ink_parser_context_stack *stack,
                                     struct ink_syntax_node *node, size_t level,
                                     size_t scratch_offset,
                                     size_t source_offset)
{
    struct ink_parser_context *context = &stack->entries[++stack->depth];

    context->level = level;
    context->scratch_offset = scratch_offset;
    context->source_offset = source_offset;
    context->node = node;
}

static struct ink_syntax_node *
ink_parser_context_reduce(struct ink_parser *parser,
                          struct ink_parser_context_stack *stack,
                          enum ink_syntax_node_type type, size_t node_start)
{
    const struct ink_parser_context *context = &stack->entries[stack->depth];
    const size_t start_offset = context->source_offset;
    const size_t end_offset = parser->current_offset;

    stack->depth--;

    return ink_parser_create_sequence(parser, type, start_offset, end_offset,
                                      context->scratch_offset);
}

static struct ink_syntax_node *ink_parse_file(struct ink_parser *parser)
{
    const size_t scratch_offset = parser->scratch.count;
    const size_t file_start = parser->current_offset;
    struct ink_syntax_node *temp = NULL;
    struct ink_parser_scratch *scratch = &parser->scratch;
    struct ink_parser_context_stack *blocks = &parser->blocks;
    struct ink_parser_context_stack *choices = &parser->choices;

    ink_parser_context_shift(parser, blocks, NULL, 0, 0, file_start);

    while (!ink_parser_check(parser, INK_TT_EOF)) {
        const size_t node_start = parser->current_offset;
        struct ink_syntax_node *node = NULL;
        struct ink_parser_context *block_top = &blocks->entries[blocks->depth];
        struct ink_parser_context *choice_top =
            &choices->entries[choices->depth];

        INK_PARSER_RULE(node, ink_parse_stmt, parser);

        if (parser->current_level > choices->depth) {
            if (block_top->level != choice_top->level) {
                ink_parser_context_shift(parser, blocks, NULL,
                                         choice_top->level, scratch->count,
                                         node_start);
            }
            ink_parser_context_shift(parser, choices, NULL,
                                     parser->current_level, scratch->count,
                                     node_start);
        } else if (parser->current_level == choices->depth) {
            if (node->type == INK_NODE_CHOICE_STAR_STMT ||
                node->type == INK_NODE_CHOICE_PLUS_STMT) {

                if (block_top->level == parser->current_level) {
                    temp = ink_parser_context_reduce(
                        parser, blocks, INK_NODE_BLOCK_STMT, node_start);

                    if (scratch->count > 0) {
                        scratch->entries[scratch->count - 1]->rhs = temp;
                    }
                }
            } else if (block_top->level != parser->current_level) {
                ink_parser_context_shift(parser, blocks, NULL,
                                         parser->current_level, scratch->count,
                                         node_start);
            }
        } else {
            while (blocks->depth != 0 && choices->depth != 0) {
                block_top = &blocks->entries[blocks->depth];
                choice_top = &choices->entries[choices->depth];

                if (block_top->level == choice_top->level) {
                    temp = ink_parser_context_reduce(
                        parser, blocks, INK_NODE_BLOCK_STMT, node_start);

                    if (scratch->count > 0) {
                        scratch->entries[scratch->count - 1]->rhs = temp;
                    }
                }
                if (choice_top->level > parser->current_level) {
                    temp = ink_parser_context_reduce(
                        parser, choices, INK_NODE_CHOICE_STMT, node_start);
                    ink_parser_scratch_append(scratch, temp);
                } else {
                    break;
                }
            }
        }

        ink_parser_scratch_append(scratch, node);
    }
    for (;;) {
        if (blocks->depth > 0) {
            temp = ink_parser_context_reduce(parser, blocks,
                                             INK_NODE_BLOCK_STMT, (size_t)-1);

            if (scratch->count > 0) {
                scratch->entries[scratch->count - 1]->rhs = temp;
            } else {
                ink_parser_scratch_append(scratch, temp);
            }
        }
        if (choices->depth > 0) {
            temp = ink_parser_context_reduce(parser, choices,
                                             INK_NODE_CHOICE_STMT, (size_t)-1);
            ink_parser_scratch_append(scratch, temp);
        } else {
            break;
        }
    }
    return ink_parser_create_sequence(parser, INK_NODE_FILE, file_start,
                                      parser->current_offset, scratch_offset);
}

/**
 * Parse a source file and output a syntax tree.
 */
int ink_parse(struct ink_arena *arena, const struct ink_source *source,
              struct ink_syntax_tree *syntax_tree, int flags)
{
    int rc;
    struct ink_parser parser;

    rc = ink_parser_initialize(&parser, source, syntax_tree, arena, flags);
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

    ink_parser_cleanup(&parser);

    return rc;
}
