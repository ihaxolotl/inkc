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
#include "tree.h"

struct ink_scanner {
    const struct ink_source *source;
    bool is_line_start;
    size_t cursor_offset;
    size_t start_offset;
};

/**
 * Scratch buffer for syntax tree nodes.
 *
 * Used to assist in the creation of syntax tree node sequences, avoiding the
 * need for a dynamically-resizable array.
 */
struct ink_parser_scratch {
    size_t count;
    size_t capacity;
    struct ink_syntax_node **entries;
};

enum ink_parser_context_type {
    INK_PARSE_CONTENT,
    INK_PARSE_EXPRESSION,
    INK_PARSE_BRACE,
    INK_PARSE_CHOICE,
    INK_PARSE_STRING,
};

/**
 * Context-relative parsing state.
 *
 * A list of delimiters for a string of content is maintained based on
 * the context type.
 */
struct ink_parser_context {
    enum ink_parser_context_type type;
    const enum ink_token_type *delims;
    size_t token_index;
};

struct ink_parser_cache_key {
    void *rule_address;
    size_t token_index;
};

struct ink_parser_cache_entry {
    struct ink_parser_cache_key key;
    struct ink_syntax_node *value;
};

/**
 * Parser memoization cache.
 *
 * Cache keys are ordered pairs of (token_index, rule_address).
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
 * General tokenization is performed on-demand, with tokens cached for later
 * use. Some grammar rules may change token types in-place depending on the
 * current context, though this is only allowed to happen once per token.
 * When backtracking is required, the parser's `token_index` can be rewound to a
 * previously saved value, waiving the need for retokenization.
 *
 * New syntax tree nodes are created with indexes into the tokenenized buffer,
 * `tokens`. To create nodes with a variable number of children, references to
 * intermediate parsing results are stored within a scratch buffer before a
 * node sequence is properly allocated. The size of this buffer grows and
 * shrinks dynamically as nodes are added to and removed from it.
 *
 * To transition between grammatical contexts, a stack of context-relative
 * state information is maintained. A stack of "indentation" levels is also
 * maintained to handle context-sensitive block delimiters.
 *
 * TODO(Brett): Decide if the stacks should be dynamically-sized.
 *
 * TODO(Brett): Describe the error recovery strategy.
 *
 * TODO(Brett): Describe expression parsing.
 *
 * TODO(Brett): Determine if `pending` is actually required in its current form.
 *              The preceived need for it arose from diving into the CPython
 *              scanner and its handling of indentation-based block delimiters.
 *
 * TODO(Brett): Add a logger v-table to the parser.
 */
struct ink_parser {
    struct ink_arena *arena;
    struct ink_token_buffer *tokens;
    struct ink_scanner scanner;
    struct ink_parser_scratch scratch;
    struct ink_parser_cache cache;
    int flags;
    bool panic_mode;
    int pending;
    size_t token_index;
    size_t context_depth;
    size_t level_depth;
    size_t level_stack[INK_PARSE_DEPTH];
    struct ink_parser_context context_stack[INK_PARSE_DEPTH];
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

static const char *INK_CONTEXT_TYPE_STR[] = {
    [INK_PARSE_CONTENT] = "Content", [INK_PARSE_EXPRESSION] = "Expression",
    [INK_PARSE_BRACE] = "Brace",     [INK_PARSE_CHOICE] = "Choice",
    [INK_PARSE_STRING] = "String",
};

static const enum ink_token_type INK_CONTENT_DELIMS[] = {
    INK_TT_LEFT_BRACE,
    INK_TT_RIGHT_BRACE,
    INK_TT_NL,
    INK_TT_EOF,
};

static const enum ink_token_type INK_EXPRESSION_DELIMS[] = {
    INK_TT_DOUBLE_QUOTE, INK_TT_LEFT_BRACE, INK_TT_RIGHT_BRACE,
    INK_TT_NL,           INK_TT_EOF,
};

static const enum ink_token_type INK_BRACE_DELIMS[] = {
    INK_TT_LEFT_BRACE, INK_TT_RIGHT_BRACE, INK_TT_PIPE, INK_TT_NL, INK_TT_EOF,
};

static const enum ink_token_type INK_CHOICE_DELIMS[] = {
    INK_TT_LEFT_BRACE,    INK_TT_RIGHT_BRACE, INK_TT_LEFT_BRACKET,
    INK_TT_RIGHT_BRACKET, INK_TT_NL,          INK_TT_EOF,
};

static const enum ink_token_type INK_STRING_DELIMS[] = {
    INK_TT_DOUBLE_QUOTE, INK_TT_LEFT_BRACE, INK_TT_RIGHT_BRACE,
    INK_TT_NL,           INK_TT_EOF,
};

enum ink_lex_state {
    INK_LEX_START,
    INK_LEX_CONTENT,
    INK_LEX_DIGIT,
    INK_LEX_SLASH,
    INK_LEX_COMMENT_LINE,
    INK_LEX_COMMENT_BLOCK,
    INK_LEX_COMMENT_BLOCK_STAR,
    INK_LEX_WHITESPACE,
    INK_LEX_NEWLINE,
};

static bool ink_is_alpha(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool ink_is_digit(unsigned char c)
{
    return (c >= '0' && c <= '9');
}

static bool ink_is_identifier(unsigned char c)
{
    return ink_is_alpha(c) || ink_is_digit(c) || c == '_';
}

static void ink_scan_next(struct ink_scanner *scanner)
{
    scanner->cursor_offset++;
}

void ink_token_next(struct ink_scanner *scanner, struct ink_token *token)
{
    unsigned char c;
    enum ink_lex_state state = INK_LEX_START;
    const struct ink_source *source = scanner->source;

    for (;;) {
        c = source->bytes[scanner->cursor_offset];

        if (scanner->cursor_offset >= source->length) {
            token->type = INK_TT_EOF;
            break;
        }
        switch (state) {
        case INK_LEX_START: {
            scanner->start_offset = scanner->cursor_offset;

            switch (c) {
            case '\0': {
                token->type = INK_TT_EOF;
                goto exit_loop;
            }
            case '\n': {
                state = INK_LEX_NEWLINE;
                break;
            }
            case ' ':
            case '\t': {
                state = INK_LEX_WHITESPACE;
                break;
            }
            case '"': {
                token->type = INK_TT_DOUBLE_QUOTE;
                ink_scan_next(scanner);
                goto exit_loop;
            }
            case '=': {
                token->type = INK_TT_EQUAL;
                ink_scan_next(scanner);
                goto exit_loop;
            }
            case '+': {
                token->type = INK_TT_PLUS;
                ink_scan_next(scanner);
                goto exit_loop;
            }
            case '-': {
                token->type = INK_TT_MINUS;
                ink_scan_next(scanner);
                goto exit_loop;
            }
            case '*': {
                token->type = INK_TT_STAR;
                ink_scan_next(scanner);
                goto exit_loop;
            }
            case '/': {
                state = INK_LEX_SLASH;
                break;
            }
            case '%': {
                token->type = INK_TT_PERCENT;
                ink_scan_next(scanner);
                goto exit_loop;
            }
            case '|': {
                token->type = INK_TT_PIPE;
                ink_scan_next(scanner);
                goto exit_loop;
            }
            case '(': {
                token->type = INK_TT_LEFT_PAREN;
                ink_scan_next(scanner);
                goto exit_loop;
            }
            case ')': {
                token->type = INK_TT_RIGHT_PAREN;
                ink_scan_next(scanner);
                goto exit_loop;
            }
            case '[': {
                token->type = INK_TT_LEFT_BRACKET;
                ink_scan_next(scanner);
                goto exit_loop;
            }
            case ']': {
                token->type = INK_TT_RIGHT_BRACKET;
                ink_scan_next(scanner);
                goto exit_loop;
            }
            case '{': {
                token->type = INK_TT_LEFT_BRACE;
                ink_scan_next(scanner);
                goto exit_loop;
            }
            case '}': {
                token->type = INK_TT_RIGHT_BRACE;
                ink_scan_next(scanner);
                goto exit_loop;
            }
            default:
                if (ink_is_alpha(c)) {
                    state = INK_LEX_CONTENT;
                } else if (ink_is_digit(c)) {
                    state = INK_LEX_DIGIT;
                } else {
                    token->type = INK_TT_STRING;
                    ink_scan_next(scanner);
                    goto exit_loop;
                }
                break;
            }
            break;
        }
        case INK_LEX_CONTENT: {
            if (!ink_is_identifier(c)) {
                token->type = INK_TT_STRING;
                goto exit_loop;
            }
            break;
        }
        case INK_LEX_DIGIT: {
            if (!ink_is_digit(c)) {
                token->type = INK_TT_NUMBER;
                goto exit_loop;
            }
            break;
        }
        case INK_LEX_SLASH: {
            switch (c) {
            case '/': {
                state = INK_LEX_COMMENT_LINE;
                break;
            }
            case '*': {
                state = INK_LEX_COMMENT_BLOCK;
                break;
            }
            default:
                token->type = INK_TT_SLASH;
                goto exit_loop;
            }
            break;
        }
        case INK_LEX_COMMENT_LINE: {
            switch (c) {
            case '\0': {
                state = INK_LEX_START;
                break;
            }
            case '\n': {
                state = INK_LEX_START;
                scanner->is_line_start = true;
                break;
            }
            default:
                break;
            }
            break;
        }
        case INK_LEX_COMMENT_BLOCK: {
            switch (c) {
            case '\0':
                token->type = INK_TT_ERROR;
                goto exit_loop;
            case '*':
                state = INK_LEX_COMMENT_BLOCK_STAR;
                break;
            default:
                break;
            }
            break;
        }
        case INK_LEX_COMMENT_BLOCK_STAR: {
            switch (c) {
            case '\0':
                token->type = INK_TT_ERROR;
                goto exit_loop;
            case '/':
                state = INK_LEX_START;
                break;
            default:
                state = INK_LEX_COMMENT_BLOCK;
                break;
            }
            break;
        }
        case INK_LEX_WHITESPACE: {
            switch (c) {
            case ' ':
            case '\t':
                break;
            default:
                if (scanner->is_line_start) {
                    state = INK_LEX_START;
                    break;
                }
                token->type = INK_TT_WHITESPACE;
                goto exit_loop;
            }
            break;
        }
        case INK_LEX_NEWLINE: {
            if (c != '\n') {
                if (scanner->is_line_start) {
                    scanner->is_line_start = false;
                    state = INK_LEX_START;
                    continue;
                } else {
                    token->type = INK_TT_NL;
                    goto exit_loop;
                }
            }
            break;
        }
        default:
            /* Unreachable */
            break;
        }

        ink_scan_next(scanner);
    }
exit_loop:
    if (scanner->is_line_start) {
        scanner->is_line_start = false;
    }

    token->start_offset = scanner->start_offset;
    token->end_offset = scanner->cursor_offset;
}

/**
 * Initialize the parser's scratch storage.
 */
static void ink_parser_scratch_initialize(struct ink_parser_scratch *scratch)
{
    scratch->count = 0;
    scratch->capacity = 0;
    scratch->entries = NULL;
}

/**
 * Release memory for scratch storage.
 */
static void ink_parser_scratch_cleanup(struct ink_parser_scratch *scratch)
{
    const size_t mem_size = sizeof(scratch->entries) * scratch->capacity;

    platform_mem_dealloc(scratch->entries, mem_size);
}

/**
 * Reserve scratch space for a specified number of items.
 */
static int ink_parser_scratch_reserve(struct ink_parser_scratch *scratch,
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

    return INK_E_OK;
}

/**
 * Shrink the parser's scratch storage down to a specified size.
 *
 * Re-allocation is not performed here. Therefore, subsequent allocations
 * are amortized.
 */
static void ink_parser_scratch_shrink(struct ink_parser_scratch *scratch,
                                      size_t count)
{
    assert(count <= scratch->capacity);

    scratch->count = count;
}

/**
 * Append a syntax tree node to the parser's scratch storage.
 *
 * These nodes can be retrieved later for creating sequences.
 */
static void ink_parser_scratch_append(struct ink_parser_scratch *scratch,
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

#define INK_HASHTABLE_SCALE_FACTOR 2u
#define INK_HASHTABLE_LOAD_MAX .75
#define INK_HASHTABLE_MIN_CAPACITY 16
#define INK_PARSER_CACHE_SCALE_FACTOR INK_HASHTABLE_SCALE_FACTOR
#define INK_PARSER_CACHE_LOAD_MAX INK_HASHTABLE_LOAD_MAX
#define INK_PARSER_CACHE_MIN_CAPACITY INK_HASHTABLE_MIN_CAPACITY

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
    return (a->token_index == b->token_index) &&
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
 * Lookup a cached entry by (token_index, parse_rule) key pair.
 */
static int ink_parser_cache_lookup(const struct ink_parser_cache *cache,
                                   size_t token_index, void *parse_rule,
                                   struct ink_syntax_node **value)
{
    const struct ink_parser_cache_key key = {
        .token_index = token_index,
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
 * Insert an entry into the parser's cache by (token_index, parse_rule) key
 * pair.
 */
int ink_parser_cache_insert(struct ink_parser_cache *cache, size_t token_index,
                            void *parse_rule, struct ink_syntax_node *value)
{
    const struct ink_parser_cache_key key = {
        .token_index = token_index,
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
        size_t token_index = parser->token_index;                              \
                                                                               \
        if (parser->flags & INK_PARSER_F_CACHING) {                            \
            rc = ink_parser_cache_lookup(&parser->cache, token_index,          \
                                         (void *)rule, &node);                 \
            if (rc < 0) {                                                      \
                node = INK_DISPATCH(rule, __VA_ARGS__);                        \
                ink_parser_cache_insert(&parser->cache, token_index,           \
                                        (void *)rule, node);                   \
            } else {                                                           \
                ink_trace("Parser cache hit!");                                \
                parser->token_index = node->end_token;                         \
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

/**
 * Create a syntax tree node sequence.
 */
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
            seq_index++;
        }

        ink_parser_scratch_shrink(scratch, start_offset);
    }
    return seq;
}

/**
 * Create a new syntax tree node.
 */
static inline struct ink_syntax_node *
ink_parser_create_node(struct ink_parser *parser,
                       enum ink_syntax_node_type type, size_t token_start,
                       size_t token_end, struct ink_syntax_node *lhs,
                       struct ink_syntax_node *rhs, struct ink_syntax_seq *seq)
{
    return ink_syntax_node_new(parser->arena, type, token_start, token_end, lhs,
                               rhs, seq);
}

static inline struct ink_syntax_node *
ink_parser_create_leaf(struct ink_parser *parser,
                       enum ink_syntax_node_type type, size_t token_start,
                       size_t token_end)
{
    return ink_parser_create_node(parser, type, token_start, token_end, NULL,
                                  NULL, NULL);
}

static inline struct ink_syntax_node *
ink_parser_create_unary(struct ink_parser *parser,
                        enum ink_syntax_node_type type, size_t token_start,
                        size_t token_end, struct ink_syntax_node *lhs)
{
    return ink_parser_create_node(parser, type, token_start, token_end, lhs,
                                  NULL, NULL);
}

static inline struct ink_syntax_node *
ink_parser_create_binary(struct ink_parser *parser,
                         enum ink_syntax_node_type type, size_t token_start,
                         size_t token_end, struct ink_syntax_node *lhs,
                         struct ink_syntax_node *rhs)
{
    return ink_parser_create_node(parser, type, token_start, token_end, lhs,
                                  rhs, NULL);
}

static inline struct ink_syntax_node *
ink_parser_create_sequence(struct ink_parser *parser,
                           enum ink_syntax_node_type type, size_t token_start,
                           size_t token_end, size_t scratch_offset)
{
    struct ink_syntax_seq *seq = NULL;

    if (parser->scratch.count != scratch_offset) {
        seq = ink_seq_from_scratch(parser->arena, &parser->scratch,
                                   scratch_offset, parser->scratch.count);
        /* TODO(Brett): Handle and log error. */
    }
    return ink_parser_create_node(parser, type, token_start, token_end, NULL,
                                  NULL, seq);
}

/**
 * Return a description of the current parsing context.
 */
static inline const char *
ink_parse_context_type_strz(enum ink_parser_context_type type)
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

    ink_token_next(&parser->scanner, &next);
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
static struct ink_parser_context *ink_parser_context(struct ink_parser *parser)
{
    return &parser->context_stack[parser->context_depth];
}

/**
 * Print parser tracing information to the console.
 */
static void ink_parser_trace(struct ink_parser *parser, const char *rule_name)
{
    const struct ink_parser_context *context = ink_parser_context(parser);
    const struct ink_token *token = ink_parser_current_token(parser);
    const char *context_type = ink_parse_context_type_strz(context->type);
    const char *token_type = ink_token_type_strz(token->type);

    ink_trace("Entering %s(Context=%s, TokenType=%s, TokenIndex: %zu, "
              "Level: %zu)",
              rule_name, context_type, token_type, parser->token_index,
              parser->level_stack[parser->level_depth]);
}

/**
 * Rewind the parser's state to a previous token index.
 */
static void ink_parser_rewind(struct ink_parser *parser, size_t token_index)
{
    assert(token_index <= parser->token_index);

    if (parser->flags & INK_PARSER_F_TRACING) {
        ink_trace("Rewinding parser to %zu", token_index);
    }

    parser->token_index = token_index;
}

/**
 * Rewind to the parser's previous state.
 */
static void ink_parser_rewind_context(struct ink_parser *parser)
{
    const struct ink_parser_context *context =
        &parser->context_stack[parser->context_depth];

    ink_parser_rewind(parser, context->token_index);
}

/**
 * Initialize a parsing context.
 */
static void ink_parser_set_context(struct ink_parser_context *context,
                                   enum ink_parser_context_type type,
                                   size_t token_index)
{
    context->type = type;
    context->token_index = token_index;

    switch (context->type) {
    case INK_PARSE_CONTENT: {
        context->delims = INK_CONTENT_DELIMS;
        break;
    }
    case INK_PARSE_EXPRESSION: {
        context->delims = INK_EXPRESSION_DELIMS;
        break;
    }
    case INK_PARSE_BRACE: {
        context->delims = INK_BRACE_DELIMS;
        break;
    }
    case INK_PARSE_CHOICE: {
        context->delims = INK_CHOICE_DELIMS;
        break;
    }
    case INK_PARSE_STRING: {
        context->delims = INK_STRING_DELIMS;
        break;
    }
    default:
        break;
    }
}

/**
 * Push a parsing context into the parser's state.
 */
static void ink_parser_push_context(struct ink_parser *parser,
                                    enum ink_parser_context_type type)
{
    const struct ink_token *token = ink_parser_current_token(parser);
    const char *token_type = ink_token_type_strz(token->type);
    const char *context_type = ink_parse_context_type_strz(type);

    assert(parser->context_depth < INK_PARSE_DEPTH);

    parser->context_depth++;

    if (parser->flags & INK_PARSER_F_TRACING) {
        ink_trace("Pushing new %s context! (TokenType: %s, TokenIndex: %zu)",
                  context_type, token_type, parser->token_index);
    }

    ink_parser_set_context(ink_parser_context(parser), type,
                           parser->token_index);
}

/**
 * Pop a parsing context from the parser's state.
 */
static void ink_parser_pop_context(struct ink_parser *parser)
{
    const struct ink_parser_context *context = ink_parser_context(parser);
    const struct ink_token *token = ink_parser_current_token(parser);
    const char *token_type = ink_token_type_strz(token->type);
    const char *context_type = ink_parse_context_type_strz(context->type);

    assert(parser->context_depth != 0);

    parser->context_depth--;

    if (parser->flags & INK_PARSER_F_TRACING) {
        ink_trace("Popping old %s context! (TokenType: %s, TokenIndex: %zu)",
                  context_type, token_type, context->token_index);
    }
}

/**
 * Determine if the current token is a delimiter for content strings within
 * the current parsing context.
 */
static bool ink_context_delim(struct ink_parser *parser)
{
    const struct ink_parser_context *context = ink_parser_context(parser);

    if (ink_parser_check(parser, INK_TT_EOF)) {
        return true;
    }
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

    ink_token_print(parser->scanner.source, ink_parser_current_token(parser));

    return NULL;
}

/**
 * Advance the parser by retrieving the next token from the scanner.
 */
static size_t ink_parser_advance(struct ink_parser *parser)
{
    while (!ink_parser_check(parser, INK_TT_EOF)) {
        const struct ink_parser_context *context = ink_parser_context(parser);
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
    const struct ink_parser_context *context = ink_parser_context(parser);

    if (context->type == INK_PARSE_EXPRESSION) {
        if (ink_parser_check(parser, INK_TT_WHITESPACE)) {
            token_index = ink_parser_advance(parser);
        }
    }
    if (!ink_parser_check(parser, type)) {
        ink_parser_error(parser, "Unexpected token! %s",
                         ink_token_type_strz(ink_parser_token_type(parser)));

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
    const unsigned char *source = parser->scanner.source->bytes;
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
    const unsigned char *source = parser->scanner.source->bytes;
    const unsigned char *lexeme = source + token->start_offset;
    const size_t length = token->end_offset - token->start_offset;

    for (size_t i = 0; i < length; i++) {
        if (!ink_is_identifier(lexeme[i])) {
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

/**
 * Initialize the state of the parser.
 */
static int ink_parser_initialize(struct ink_parser *parser,
                                 const struct ink_source *source,
                                 struct ink_syntax_tree *tree,
                                 struct ink_arena *arena, int flags)
{
    parser->arena = arena;
    parser->tokens = &tree->tokens;
    parser->scanner.source = source;
    parser->scanner.is_line_start = true;
    parser->scanner.start_offset = 0;
    parser->scanner.cursor_offset = 0;
    parser->panic_mode = false;
    parser->flags = flags;
    parser->pending = 0;
    parser->token_index = 0;
    parser->context_depth = 0;
    parser->level_depth = 0;
    parser->level_stack[0] = 0;

    ink_parser_set_context(&parser->context_stack[0], INK_PARSE_CONTENT, 0);
    ink_parser_scratch_initialize(&parser->scratch);
    ink_parser_scratch_reserve(&parser->scratch, INK_SCRATCH_MIN_COUNT);
    ink_parser_cache_initialize(&parser->cache);
    return INK_E_OK;
}

/**
 * Clean up the state of the parser.
 */
static void ink_parser_cleanup(struct ink_parser *parser)
{
    ink_parser_scratch_cleanup(&parser->scratch);
    ink_parser_cache_cleanup(&parser->cache);
    memset(parser, 0, sizeof(*parser));
}

static struct ink_syntax_node *ink_parse_true(struct ink_parser *parser)
{
    const size_t token_start = ink_parser_expect(parser, INK_TT_KEYWORD_TRUE);

    return ink_parser_create_leaf(parser, INK_NODE_TRUE_EXPR, token_start,
                                  token_start);
}

static struct ink_syntax_node *ink_parse_false(struct ink_parser *parser)
{
    const size_t token_start = ink_parser_expect(parser, INK_TT_KEYWORD_FALSE);

    return ink_parser_create_leaf(parser, INK_NODE_FALSE_EXPR, token_start,
                                  token_start);
}

static struct ink_syntax_node *ink_parse_number(struct ink_parser *parser)
{
    const size_t token_start = ink_parser_expect(parser, INK_TT_NUMBER);

    return ink_parser_create_leaf(parser, INK_NODE_NUMBER_EXPR, token_start,
                                  token_start);
}

static struct ink_syntax_node *ink_parse_string(struct ink_parser *parser)
{
    size_t token_end;
    const size_t token_start = parser->token_index;

    while (!ink_context_delim(parser)) {
        ink_parser_advance(parser);
    }

    token_end = parser->token_index;

    if (token_start != token_end) {
        token_end--;
    }
    return ink_parser_create_leaf(parser, INK_NODE_STRING_LITERAL, token_start,
                                  token_end);
}

static struct ink_syntax_node *ink_parse_string_expr(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t scratch_offset = parser->scratch.count;
    const size_t token_start = ink_parser_expect(parser, INK_TT_DOUBLE_QUOTE);
    size_t token_end;

    do {
        if (!ink_context_delim(parser)) {
            INK_PARSER_RULE(node, ink_parse_string, parser);
        } else if (ink_parser_check(parser, INK_TT_LEFT_BRACE)) {
            INK_PARSER_RULE(node, ink_parse_brace_expr, parser);
        } else {
            break;
        }

        ink_parser_scratch_append(&parser->scratch, node);
    } while (!ink_parser_check(parser, INK_TT_EOF) &&
             !ink_parser_check(parser, INK_TT_DOUBLE_QUOTE));

    token_end = ink_parser_expect(parser, INK_TT_DOUBLE_QUOTE);

    return ink_parser_create_sequence(parser, INK_NODE_STRING_EXPR, token_start,
                                      token_end, scratch_offset);
}

static struct ink_syntax_node *ink_parse_identifier(struct ink_parser *parser)
{
    const size_t token_start = ink_parser_expect(parser, INK_TT_IDENTIFIER);

    return ink_parser_create_leaf(parser, INK_NODE_IDENTIFIER_EXPR, token_start,
                                  token_start);
}

static struct ink_syntax_node *ink_parse_primary_expr(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t token_index = parser->token_index;
    const enum ink_token_type token_type = ink_parser_token_type(parser);

    switch (token_type) {
    case INK_TT_NUMBER: {
        INK_PARSER_RULE(node, ink_parse_number, parser);
        break;
    }
    case INK_TT_STRING: {
        if (ink_parser_try_keyword(parser, INK_TT_KEYWORD_TRUE)) {
            INK_PARSER_RULE(node, ink_parse_true, parser);
            break;
        }
        if (ink_parser_try_keyword(parser, INK_TT_KEYWORD_FALSE)) {
            INK_PARSER_RULE(node, ink_parse_false, parser);
            break;
        }
        if (ink_parser_try_identifier(parser)) {
            INK_PARSER_RULE(node, ink_parse_identifier, parser);
            break;
        }
        break;
    }
    case INK_TT_DOUBLE_QUOTE: {
        ink_parser_push_context(parser, INK_PARSE_STRING);
        INK_PARSER_RULE(node, ink_parse_string_expr, parser);
        ink_parser_pop_context(parser);
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
        node = ink_parser_create_leaf(parser, INK_NODE_INVALID, token_index,
                                      token_index);
        break;
    }
    return node;
}

static struct ink_syntax_node *ink_parse_prefix_expr(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;

    if (ink_parser_try_keyword(parser, INK_TT_KEYWORD_NOT) ||
        ink_parser_check(parser, INK_TT_MINUS) ||
        ink_parser_check(parser, INK_TT_BANG)) {
        const enum ink_token_type type = ink_parser_token_type(parser);
        const size_t token_index = ink_parser_advance(parser);

        INK_PARSER_RULE(node, ink_parse_prefix_expr, parser);
        node = ink_parser_create_unary(parser, ink_token_prefix_type(type),
                                       token_index, token_index, node);
    } else {
        INK_PARSER_RULE(node, ink_parse_primary_expr, parser);
    }
    return node;
}

static struct ink_syntax_node *ink_parse_infix_expr(struct ink_parser *parser,
                                                    struct ink_syntax_node *lhs,
                                                    enum ink_precedence prec)
{
    enum ink_precedence token_prec;
    enum ink_token_type type;
    size_t token_index;
    struct ink_syntax_node *rhs = NULL;

    if (!lhs) {
        INK_PARSER_RULE(lhs, ink_parse_prefix_expr, parser);
    }

    ink_parser_try_keyword(parser, INK_TT_KEYWORD_AND) ||
        ink_parser_try_keyword(parser, INK_TT_KEYWORD_OR) ||
        ink_parser_try_keyword(parser, INK_TT_KEYWORD_MOD);

    type = ink_parser_token_type(parser);
    token_prec = ink_binding_power(type);

    while (token_prec > prec) {
        token_index = ink_parser_advance(parser);
        INK_PARSER_RULE(rhs, ink_parse_infix_expr, parser, NULL, token_prec);
        lhs = ink_parser_create_binary(parser, ink_token_infix_type(type),
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
    struct ink_syntax_node *node = NULL;

    ink_parser_push_context(parser, INK_PARSE_EXPRESSION);
    INK_PARSER_RULE(node, ink_parse_infix_expr, parser, NULL, INK_PREC_NONE);
    ink_parser_pop_context(parser);

    return node;
}

static struct ink_syntax_node *
ink_parse_content_string(struct ink_parser *parser)
{
    size_t token_end;
    const size_t token_start = parser->token_index;

    while (!ink_context_delim(parser)) {
        ink_parser_advance(parser);
    }

    token_end = parser->token_index;
    if (token_start != token_end) {
        token_end--;
    }
    return ink_parser_create_leaf(parser, INK_NODE_STRING_LITERAL, token_start,
                                  token_end);
}

static struct ink_syntax_node *
ink_parse_sequence_expr(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t scratch_offset = parser->scratch.count;
    const size_t token_start = parser->token_index;

    INK_PARSER_RULE(node, ink_parse_content_expr, parser);

    if (!ink_parser_check(parser, INK_TT_PIPE)) {
        /*
         * Backtrack and parse primary expression.
         */
        ink_parser_rewind_context(parser);
        INK_PARSER_RULE(node, ink_parse_expr, parser);
        return node;
    } else {
        ink_parser_scratch_append(&parser->scratch, node);

        while (!ink_parser_check(parser, INK_TT_EOF) &&
               !ink_parser_check(parser, INK_TT_NL) &&
               !ink_parser_check(parser, INK_TT_RIGHT_BRACE)) {
            if (ink_parser_check(parser, INK_TT_PIPE)) {
                ink_parser_advance(parser);
            }

            INK_PARSER_RULE(node, ink_parse_content_expr, parser);
            ink_parser_scratch_append(&parser->scratch, node);
        }
    }
    return ink_parser_create_sequence(parser, INK_NODE_SEQUENCE_EXPR,
                                      token_start, parser->token_index,
                                      scratch_offset);
}

static struct ink_syntax_node *ink_parse_brace_expr(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t token_start = parser->token_index;

    ink_parser_advance(parser);
    ink_parser_push_context(parser, INK_PARSE_BRACE);
    INK_PARSER_RULE(node, ink_parse_sequence_expr, parser);
    ink_parser_pop_context(parser);
    ink_parser_expect(parser, INK_TT_RIGHT_BRACE);

    return ink_parser_create_unary(parser, INK_NODE_BRACE_EXPR, token_start,
                                   parser->token_index, node);
}

static struct ink_syntax_node *ink_parse_content_expr(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t scratch_offset = parser->scratch.count;
    const size_t token_start = parser->token_index;

    do {
        if (!ink_context_delim(parser)) {
            INK_PARSER_RULE(node, ink_parse_content_string, parser);
        } else if (ink_parser_check(parser, INK_TT_LEFT_BRACE)) {
            INK_PARSER_RULE(node, ink_parse_brace_expr, parser);
        } else {
            break;
        }

        ink_parser_scratch_append(&parser->scratch, node);
    } while (!ink_parser_check(parser, INK_TT_EOF) &&
             !ink_parser_check(parser, INK_TT_NL));

    return ink_parser_create_sequence(parser, INK_NODE_CONTENT_EXPR,
                                      token_start, parser->token_index,
                                      scratch_offset);
}

static struct ink_syntax_node *ink_parse_content(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t token_start = parser->token_index;

    INK_PARSER_RULE(node, ink_parse_content_expr, parser);

    if (!ink_parser_check(parser, INK_TT_EOF) &&
        !ink_parser_check(parser, INK_TT_NL)) {
        ink_parser_error(parser, "Expected new line!");
    }

    ink_parser_advance(parser);

    return ink_parser_create_unary(parser, INK_NODE_CONTENT_STMT, token_start,
                                   parser->token_index, node);
}

static struct ink_syntax_node *
ink_parse_choice_content(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t scratch_offset = parser->scratch.count;
    const size_t token_start = parser->token_index;

    INK_PARSER_RULE(node, ink_parse_content_string, parser);
    ink_parser_scratch_append(&parser->scratch, node);

    if (ink_parser_check(parser, INK_TT_LEFT_BRACKET)) {
        INK_PARSER_RULE(node, ink_parse_content_string, parser);
        ink_parser_scratch_append(&parser->scratch, node);
        ink_parser_expect(parser, INK_TT_RIGHT_BRACKET);
    }

    INK_PARSER_RULE(node, ink_parse_content_string, parser);
    ink_parser_scratch_append(&parser->scratch, node);

    return ink_parser_create_sequence(parser, INK_NODE_CHOICE_CONTENT_EXPR,
                                      token_start, parser->token_index,
                                      scratch_offset);
}

static struct ink_syntax_node *
ink_parse_choice_branch(struct ink_parser *parser,
                        enum ink_syntax_node_type type)
{
    struct ink_syntax_node *expr = NULL;
    struct ink_syntax_node *body = NULL;
    const size_t token_start = parser->token_index;

    ink_parser_eat_nesting(parser, ink_parser_token_type(parser));
    /* ink_parser_push_context(parser, INK_PARSE_CHOICE); */
    INK_PARSER_RULE(expr, ink_parse_choice_content, parser);
    /* ink_parser_pop_context(parser); */

    if (ink_parser_check(parser, INK_TT_NL)) {
        ink_parser_advance(parser);
        INK_PARSER_RULE(body, ink_parse_block, parser);
    }
    return ink_parser_create_binary(parser, type, token_start,
                                    parser->token_index, expr, body);
}

static struct ink_syntax_node *ink_parse_choice(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t scratch_offset = parser->scratch.count;
    const size_t token_start = parser->token_index;

    while (!ink_parser_check(parser, INK_TT_EOF)) {
        const enum ink_token_type token_type = ink_parser_token_type(parser);
        const enum ink_syntax_node_type branch_type =
            ink_branch_type(token_type);

        INK_PARSER_RULE(node, ink_parse_choice_branch, parser, branch_type);
        ink_parser_scratch_append(&parser->scratch, node);

        if (parser->pending != 0) {
            break;
        }
    }

    parser->pending++;

    return ink_parser_create_sequence(parser, INK_NODE_CHOICE_STMT, token_start,
                                      parser->token_index, scratch_offset);
}

static struct ink_syntax_node *ink_parse_var(struct ink_parser *parser)
{
    struct ink_syntax_node *lhs = NULL;
    struct ink_syntax_node *rhs = NULL;
    const size_t token_start = ink_parser_expect(parser, INK_TT_KEYWORD_VAR);

    ink_parser_eat(parser, INK_TT_WHITESPACE);
    ink_parser_try_identifier(parser);
    INK_PARSER_RULE(lhs, ink_parse_identifier, parser);
    ink_parser_eat(parser, INK_TT_WHITESPACE);
    ink_parser_expect(parser, INK_TT_EQUAL);
    ink_parser_eat(parser, INK_TT_WHITESPACE);
    INK_PARSER_RULE(rhs, ink_parse_expr, parser);
    ink_parser_expect(parser, INK_TT_NL);

    return ink_parser_create_binary(parser, INK_NODE_VAR_DECL, token_start,
                                    parser->token_index, lhs, rhs);
}

static struct ink_syntax_node *ink_parse_const(struct ink_parser *parser)
{
    struct ink_syntax_node *lhs = NULL;
    struct ink_syntax_node *rhs = NULL;
    const size_t token_start = ink_parser_expect(parser, INK_TT_KEYWORD_CONST);

    ink_parser_eat(parser, INK_TT_WHITESPACE);
    ink_parser_try_identifier(parser);
    INK_PARSER_RULE(lhs, ink_parse_identifier, parser);
    ink_parser_eat(parser, INK_TT_WHITESPACE);
    ink_parser_expect(parser, INK_TT_EQUAL);
    ink_parser_eat(parser, INK_TT_WHITESPACE);
    INK_PARSER_RULE(rhs, ink_parse_expr, parser);
    ink_parser_expect(parser, INK_TT_NL);

    return ink_parser_create_binary(parser, INK_NODE_CONST_DECL, token_start,
                                    parser->token_index, lhs, rhs);
}

/**
 * Block delimiters are context-sensitive.
 * Opening new blocks is simple. Closing them is more tricky.
 */
static struct ink_syntax_node *
ink_parse_block_delimited(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
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
            INK_PARSER_RULE(node, ink_parse_choice, parser);
        }
    } else if (ink_parser_try_keyword(parser, INK_TT_KEYWORD_CONST)) {
        INK_PARSER_RULE(node, ink_parse_const, parser);
    } else if (ink_parser_try_keyword(parser, INK_TT_KEYWORD_VAR)) {
        INK_PARSER_RULE(node, ink_parse_var, parser);
    } else {
        INK_PARSER_RULE(node, ink_parse_content, parser);
    }
    return node;
}

static struct ink_syntax_node *ink_parse_block(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t scratch_offset = parser->scratch.count;
    const size_t token_start = parser->token_index;

    while (!ink_parser_check(parser, INK_TT_EOF)) {
        ink_parser_eat(parser, INK_TT_WHITESPACE);
        INK_PARSER_RULE(node, ink_parse_block_delimited, parser);

        if (node == NULL) {
            break;
        }

        ink_parser_scratch_append(&parser->scratch, node);
    }
    return ink_parser_create_sequence(parser, INK_NODE_BLOCK_STMT, token_start,
                                      parser->token_index, scratch_offset);
}

static struct ink_syntax_node *ink_parse_file(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t token_start = parser->token_index;

    if (ink_parser_check(parser, INK_TT_EOF)) {
        return node;
    }

    INK_PARSER_RULE(node, ink_parse_block, parser);

    return ink_parser_create_unary(parser, INK_NODE_FILE, token_start,
                                   parser->token_index, node);
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
