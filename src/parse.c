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
        size_t source_offset = parser->current_offset;                         \
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
    size_t source_offset;
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
    struct ink_scanner scanner;
    struct ink_parser_scratch scratch;
    struct ink_parser_cache cache;
    int flags;
    bool panic_mode;
    int pending;
    struct ink_token token;
    size_t current_offset;
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

enum ink_lex_state {
    INK_LEX_START,
    INK_LEX_MINUS,
    INK_LEX_SLASH,
    INK_LEX_EQUAL,
    INK_LEX_BANG,
    INK_LEX_LESS_THAN,
    INK_LEX_GREATER_THAN,
    INK_LEX_WORD,
    INK_LEX_IDENTIFIER,
    INK_LEX_NUMBER,
    INK_LEX_NUMBER_DOT,
    INK_LEX_NUMBER_DECIMAL,
    INK_LEX_WHITESPACE,
    INK_LEX_COMMENT_LINE,
    INK_LEX_COMMENT_BLOCK,
    INK_LEX_COMMENT_BLOCK_STAR,
    INK_LEX_NEWLINE,
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
static struct ink_syntax_node *ink_parse_divert_expr(struct ink_parser *parser);
static struct ink_syntax_node *ink_parse_thread_expr(struct ink_parser *parser);
static struct ink_syntax_node *ink_parse_expr(struct ink_parser *);
static struct ink_syntax_node *ink_parse_expr_stmt(struct ink_parser *);
static struct ink_syntax_node *ink_parse_return_stmt(struct ink_parser *);
static struct ink_syntax_node *ink_parse_divert_stmt(struct ink_parser *parser);
static struct ink_syntax_node *ink_parse_choice(struct ink_parser *);
static struct ink_syntax_node *ink_parse_block_delimited(struct ink_parser *);
static struct ink_syntax_node *ink_parse_block(struct ink_parser *);
static struct ink_syntax_node *ink_parse_var(struct ink_parser *);
static struct ink_syntax_node *ink_parse_const(struct ink_parser *);
static struct ink_syntax_node *ink_parse_argument_list(struct ink_parser *);

static const char *INK_CONTEXT_TYPE_STR[] = {
    [INK_PARSE_CONTENT] = "Content", [INK_PARSE_EXPRESSION] = "Expression",
    [INK_PARSE_BRACE] = "Brace",     [INK_PARSE_CHOICE] = "Choice",
    [INK_PARSE_STRING] = "String",
};

static const enum ink_token_type INK_CONTENT_DELIMS[] = {
    INK_TT_LEFT_BRACE,  INK_TT_LEFT_ARROW, INK_TT_RIGHT_BRACE,
    INK_TT_RIGHT_ARROW, INK_TT_NL,         INK_TT_EOF,
};

static const enum ink_token_type INK_EXPRESSION_DELIMS[] = {
    INK_TT_DOUBLE_QUOTE, INK_TT_LEFT_BRACE, INK_TT_RIGHT_BRACE,
    INK_TT_NL,           INK_TT_EOF,
};

static const enum ink_token_type INK_BRACE_DELIMS[] = {
    INK_TT_LEFT_BRACE,  INK_TT_LEFT_ARROW, INK_TT_RIGHT_BRACE,
    INK_TT_RIGHT_ARROW, INK_TT_PIPE,       INK_TT_NL,
    INK_TT_EOF,
};

static const enum ink_token_type INK_CHOICE_DELIMS[] = {
    INK_TT_LEFT_BRACE,  INK_TT_LEFT_ARROW,    INK_TT_LEFT_BRACKET,
    INK_TT_RIGHT_BRACE, INK_TT_RIGHT_BRACKET, INK_TT_RIGHT_ARROW,
    INK_TT_NL,          INK_TT_EOF,
};

static const enum ink_token_type INK_STRING_DELIMS[] = {
    INK_TT_DOUBLE_QUOTE, INK_TT_LEFT_BRACE,  INK_TT_LEFT_ARROW,
    INK_TT_RIGHT_BRACE,  INK_TT_RIGHT_ARROW, INK_TT_NL,
    INK_TT_EOF,
};

static inline bool ink_is_alpha(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static inline bool ink_is_digit(unsigned char c)
{
    return (c >= '0' && c <= '9');
}

static inline bool ink_is_identifier(unsigned char c)
{
    return ink_is_alpha(c) || ink_is_digit(c) || c == '_';
}

static inline void ink_scan_next(struct ink_scanner *scanner)
{
    scanner->cursor_offset++;
}

static void ink_token_next(struct ink_scanner *scanner, struct ink_token *token,
                           const struct ink_parser_context *context)
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
            case '~': {
                token->type = INK_TT_TILDE;
                ink_scan_next(scanner);
                goto exit_loop;
            }
            case '!': {
                state = INK_LEX_BANG;
                break;
            }
            case '"': {
                token->type = INK_TT_DOUBLE_QUOTE;
                ink_scan_next(scanner);
                goto exit_loop;
            }
            case '=': {
                state = INK_LEX_EQUAL;
                break;
            }
            case '+': {
                token->type = INK_TT_PLUS;
                ink_scan_next(scanner);
                goto exit_loop;
            }
            case '-': {
                state = INK_LEX_MINUS;
                break;
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
            case ',': {
                token->type = INK_TT_COMMA;
                ink_scan_next(scanner);
                goto exit_loop;
            }
            case '%': {
                token->type = INK_TT_PERCENT;
                ink_scan_next(scanner);
                goto exit_loop;
            }
            case '?': {
                token->type = INK_TT_QUESTION;
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
            case '<': {
                state = INK_LEX_LESS_THAN;
                break;
            }
            case '>': {
                state = INK_LEX_GREATER_THAN;
                break;
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
                if (context->type == INK_PARSE_EXPRESSION) {
                    if (ink_is_alpha(c)) {
                        state = INK_LEX_IDENTIFIER;
                    } else if (ink_is_digit(c)) {
                        state = INK_LEX_NUMBER;
                    } else {
                        token->type = INK_TT_ERROR;
                        ink_scan_next(scanner);
                        goto exit_loop;
                    }
                } else {
                    state = INK_LEX_WORD;
                }
                break;
            }
            break;
        }
        case INK_LEX_MINUS: {
            if (c == '>') {
                token->type = INK_TT_RIGHT_ARROW;
                ink_scan_next(scanner);
                goto exit_loop;
            }

            token->type = INK_TT_MINUS;
            goto exit_loop;
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
        case INK_LEX_EQUAL: {
            if (context->type == INK_PARSE_EXPRESSION) {
                if (c == '=') {
                    token->type = INK_TT_EQUAL_EQUAL;
                    ink_scan_next(scanner);
                    goto exit_loop;
                }
            }

            token->type = INK_TT_EQUAL;
            goto exit_loop;
        }
        case INK_LEX_BANG: {
            if (c == '=') {
                token->type = INK_TT_BANG_EQUAL;
                ink_scan_next(scanner);
                goto exit_loop;
            }

            token->type = INK_TT_BANG;
            goto exit_loop;
        }
        case INK_LEX_LESS_THAN: {
            switch (c) {
            case '=': {
                token->type = INK_TT_LESS_EQUAL;
                ink_scan_next(scanner);
                goto exit_loop;
            }
            case '-': {
                token->type = INK_TT_LEFT_ARROW;
                ink_scan_next(scanner);
                goto exit_loop;
            }
            default:
                token->type = INK_TT_LESS_THAN;
                goto exit_loop;
            }
        }
        case INK_LEX_GREATER_THAN: {
            if (c == '=') {
                token->type = INK_TT_GREATER_EQUAL;
                ink_scan_next(scanner);
                goto exit_loop;
            }

            token->type = INK_TT_GREATER_THAN;
            goto exit_loop;
        }
        case INK_LEX_WORD: {
            if (!ink_is_identifier(c)) {
                token->type = INK_TT_STRING;
                goto exit_loop;
            }
            break;
        }
        case INK_LEX_NUMBER: {
            if (c == '.') {
                state = INK_LEX_NUMBER_DOT;
            } else if (!ink_is_digit(c)) {
                token->type = INK_TT_NUMBER;
                goto exit_loop;
            }
            break;
        }
        case INK_LEX_NUMBER_DOT: {
            if (!ink_is_digit(c)) {
                token->type = INK_TT_ERROR;
                goto exit_loop;
            } else {
                state = INK_LEX_NUMBER_DECIMAL;
            }
            break;
        }
        case INK_LEX_NUMBER_DECIMAL: {
            if (!ink_is_digit(c)) {
                token->type = INK_TT_NUMBER;
                goto exit_loop;
            }
            break;
        }
        case INK_LEX_IDENTIFIER: {
            if (!ink_is_identifier(c)) {
                token->type = INK_TT_IDENTIFIER;
                goto exit_loop;
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
                       enum ink_syntax_node_type type, size_t source_start,
                       size_t source_end, struct ink_syntax_node *lhs,
                       struct ink_syntax_node *rhs, struct ink_syntax_seq *seq)
{
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

/**
 * Retun the syntax node type for an infix expression.
 */
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

/**
 * Return the binding power of an operator.
 */
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

/**
 * Return the branch type for a choice branch.
 */
static inline enum ink_syntax_node_type
ink_branch_type(enum ink_token_type type)
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
ink_parser_current_token(struct ink_parser *parser)
{
    return &parser->token;
}

/**
 * Return the type of the current token.
 */
static inline enum ink_token_type
ink_parser_token_type(struct ink_parser *parser)
{
    const struct ink_token *token = ink_parser_current_token(parser);

    return token->type;
}

/**
 * Return the current parsing context.
 */
static inline struct ink_parser_context *
ink_parser_context(struct ink_parser *parser)
{
    return &parser->context_stack[parser->context_depth];
}

/**
 * Retrieve the next token.
 */
static inline void ink_parser_next_token(struct ink_parser *parser)
{
    const struct ink_parser_context *context = ink_parser_context(parser);

    ink_token_next(&parser->scanner, &parser->token, context);

    parser->current_offset = parser->scanner.start_offset;
}

/**
 * Check if the current token matches a specified type.
 */
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
    const struct ink_parser_context *context = ink_parser_context(parser);
    const struct ink_token *token = ink_parser_current_token(parser);
    const char *context_type = ink_parse_context_type_strz(context->type);
    const char *token_type = ink_token_type_strz(token->type);

    ink_trace("Entering %s(Context=%s, TokenType=%s, SourceOffset: %zu, "
              "Level: %zu)",
              rule_name, context_type, token_type, parser->current_offset,
              parser->level_stack[parser->level_depth]);
}

/**
 * Rewind the parser's state to a previous token index.
 */
static void ink_parser_rewind(struct ink_parser *parser, size_t source_offset)
{
    assert(source_offset <= parser->current_offset);

    if (parser->flags & INK_PARSER_F_TRACING) {
        ink_trace("Rewinding parser to %zu", source_offset);
    }

    parser->current_offset = source_offset;
    parser->scanner.start_offset = source_offset;
    parser->scanner.cursor_offset = source_offset;
}

/**
 * Rewind to the parser's previous state.
 */
static void ink_parser_rewind_context(struct ink_parser *parser)
{
    const struct ink_parser_context *context =
        &parser->context_stack[parser->context_depth];

    ink_parser_rewind(parser, context->source_offset);
}

/**
 * Initialize a parsing context.
 */
static void ink_parser_set_context(struct ink_parser_context *context,
                                   enum ink_parser_context_type type,
                                   size_t source_offset)
{
    context->type = type;
    context->source_offset = source_offset;

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
        ink_trace("Pushing new %s context! (TokenType: %s, SourceOffset: %zu)",
                  context_type, token_type, parser->current_offset);
    }

    ink_parser_set_context(ink_parser_context(parser), type,
                           parser->current_offset);
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
        ink_trace("Popping old %s context! (TokenType: %s, SourceOffset: %zu)",
                  context_type, token_type, context->source_offset);
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

static enum ink_token_type ink_parser_keyword(struct ink_parser *parser,
                                              const struct ink_token *token);

static void ink_parser_try_any_keyword(struct ink_parser *parser)
{
    const enum ink_token_type keyword_type =
        ink_parser_keyword(parser, &parser->token);

    if (keyword_type != INK_TT_IDENTIFIER) {
        parser->token.type = keyword_type;
    }
}

/**
 * Advance the parser by retrieving the next token from the scanner.
 *
 * Returns the current position of the scanner.
 */
static size_t ink_parser_advance(struct ink_parser *parser)
{
    while (!ink_parser_check(parser, INK_TT_EOF)) {
        const struct ink_parser_context *context = ink_parser_context(parser);

        ink_parser_next_token(parser);

        if (context->type == INK_PARSE_EXPRESSION) {
            if (ink_parser_check(parser, INK_TT_IDENTIFIER)) {
                ink_parser_try_any_keyword(parser);
            } else if (ink_parser_check(parser, INK_TT_WHITESPACE)) {
                continue;
            }
        }
        return parser->scanner.cursor_offset;
    }
    return parser->scanner.cursor_offset;
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
 * Expect the current token to have the specified type. If not, raise an
 * error.
 */
static size_t ink_parser_expect(struct ink_parser *parser,
                                enum ink_token_type type)
{
    size_t source_offset = parser->current_offset;
    const struct ink_parser_context *context = ink_parser_context(parser);

    if (context->type == INK_PARSE_EXPRESSION) {
        if (ink_parser_check(parser, INK_TT_WHITESPACE)) {
            source_offset = ink_parser_advance(parser);
        }
    }
    if (!ink_parser_check(parser, type)) {
        ink_parser_error(parser, "Unexpected token! %s",
                         ink_token_type_strz(ink_parser_token_type(parser)));

        do {
            source_offset = ink_parser_advance(parser);
            type = ink_parser_token_type(parser);
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
    case 2: {
        if (memcmp(lexeme, "or", length) == 0) {
            type = INK_TT_KEYWORD_OR;
        }
        break;
    }
    case 3: {
        if (memcmp(lexeme, "and", length) == 0) {
            type = INK_TT_KEYWORD_AND;
        } else if (memcmp(lexeme, "mod", length) == 0) {
            type = INK_TT_KEYWORD_MOD;
        } else if (memcmp(lexeme, "not", length) == 0) {
            type = INK_TT_KEYWORD_NOT;
        } else if (memcmp(lexeme, "ref", length) == 0) {
            type = INK_TT_KEYWORD_REF;
        } else if (memcmp(lexeme, "VAR", length) == 0) {
            type = INK_TT_KEYWORD_VAR;
        }
        break;
    }
    case 4: {
        if (memcmp(lexeme, "temp", length) == 0) {
            type = INK_TT_KEYWORD_TEMP;
        } else if (memcmp(lexeme, "true", length) == 0) {
            type = INK_TT_KEYWORD_TRUE;
        } else if (memcmp(lexeme, "LIST", length) == 0) {
            type = INK_TT_KEYWORD_LIST;
        }
        break;
    }
    case 5: {
        if (memcmp(lexeme, "false", length) == 0) {
            type = INK_TT_KEYWORD_FALSE;
        } else if (memcmp(lexeme, "CONST", length) == 0) {
            type = INK_TT_KEYWORD_CONST;
        }
        break;
    }
    case 6: {
        if (memcmp(lexeme, "return", length) == 0) {
            type = INK_TT_KEYWORD_RETURN;
        }
        break;
    }
    case 8: {
        if (memcmp(lexeme, "function", length) == 0) {
            type = INK_TT_KEYWORD_FUNCTION;
        }
        break;
    }
    default:
        break;
    }
    return type;
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
 * Consume tokens that indicate the level of nesting.
 *
 * Returns how many tokens (ignoring whitespace) were consumed.
 */
static size_t ink_parser_eat_nesting(struct ink_parser *parser,
                                     enum ink_token_type type,
                                     bool ignore_whitespace)
{
    size_t level = 0;

    while (ink_parser_check(parser, type)) {
        level++;

        ink_parser_advance(parser);
        if (ignore_whitespace) {
            ink_parser_eat(parser, INK_TT_WHITESPACE);
        }
    }
    return level;
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
    parser->scanner.source = source;
    parser->scanner.is_line_start = true;
    parser->scanner.start_offset = 0;
    parser->scanner.cursor_offset = 0;
    parser->panic_mode = false;
    parser->flags = flags;
    parser->pending = 0;
    parser->current_offset = 0;
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
    const size_t source_start = ink_parser_expect(parser, INK_TT_KEYWORD_TRUE);

    return ink_parser_create_leaf(parser, INK_NODE_TRUE_EXPR, source_start,
                                  parser->current_offset);
}

static struct ink_syntax_node *ink_parse_false(struct ink_parser *parser)
{
    const size_t source_start = ink_parser_expect(parser, INK_TT_KEYWORD_FALSE);

    return ink_parser_create_leaf(parser, INK_NODE_FALSE_EXPR, source_start,
                                  parser->current_offset);
}

static struct ink_syntax_node *ink_parse_number(struct ink_parser *parser)
{
    const size_t source_start = ink_parser_expect(parser, INK_TT_NUMBER);

    return ink_parser_create_leaf(parser, INK_NODE_NUMBER_EXPR, source_start,
                                  parser->current_offset);
}

static struct ink_syntax_node *ink_parse_identifier(struct ink_parser *parser)
{
    const size_t source_start = ink_parser_expect(parser, INK_TT_IDENTIFIER);

    return ink_parser_create_leaf(parser, INK_NODE_IDENTIFIER_EXPR,
                                  source_start, parser->current_offset);
}

static struct ink_syntax_node *ink_parse_name_expr(struct ink_parser *parser)
{
    struct ink_syntax_node *lhs = NULL;
    struct ink_syntax_node *rhs = NULL;
    const size_t source_start = parser->current_offset;

    lhs = ink_parse_identifier(parser);
    if (!ink_parser_check(parser, INK_TT_LEFT_PAREN)) {
        return lhs;
    }

    rhs = ink_parse_argument_list(parser);
    return ink_parser_create_binary(parser, INK_NODE_CALL_EXPR, source_start,
                                    parser->current_offset, lhs, rhs);
}

static struct ink_syntax_node *ink_parse_string(struct ink_parser *parser)
{
    const size_t source_start = parser->current_offset;

    while (!ink_context_delim(parser)) {
        ink_parser_advance(parser);
    }
    return ink_parser_create_leaf(parser, INK_NODE_STRING_LITERAL, source_start,
                                  parser->current_offset);
}

static struct ink_syntax_node *ink_parse_string_expr(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t scratch_offset = parser->scratch.count;
    const size_t source_start = ink_parser_expect(parser, INK_TT_DOUBLE_QUOTE);

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

    ink_parser_expect(parser, INK_TT_DOUBLE_QUOTE);
    return ink_parser_create_sequence(parser, INK_NODE_STRING_EXPR,
                                      source_start, parser->current_offset,
                                      scratch_offset);
}

static struct ink_syntax_node *ink_parse_primary_expr(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    struct ink_token *token = ink_parser_current_token(parser);
    const size_t source_start = parser->current_offset;

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
        ink_parser_push_context(parser, INK_PARSE_STRING);
        INK_PARSER_RULE(node, ink_parse_string_expr, parser);
        ink_parser_eat(parser, INK_TT_WHITESPACE);
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
        node = ink_parser_create_leaf(parser, INK_NODE_INVALID, source_start,
                                      parser->current_offset);
        break;
    }
    return node;
}

static struct ink_syntax_node *ink_parse_prefix_expr(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;

    switch (ink_parser_token_type(parser)) {
    case INK_TT_KEYWORD_NOT:
    case INK_TT_MINUS:
    case INK_TT_BANG: {
        const enum ink_token_type type = ink_parser_token_type(parser);
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

    type = ink_parser_token_type(parser);
    token_prec = ink_binding_power(type);

    while (token_prec > prec) {
        const size_t source_start = ink_parser_advance(parser);

        INK_PARSER_RULE(rhs, ink_parse_infix_expr, parser, NULL, token_prec);
        lhs = ink_parser_create_binary(parser, ink_token_infix_type(type),
                                       source_start, parser->current_offset,
                                       lhs, rhs);
        type = ink_parser_token_type(parser);
        token_prec = ink_binding_power(type);
    }
    return lhs;
}

static struct ink_syntax_node *ink_parse_divert_expr(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t source_start = parser->current_offset;

    ink_parser_push_context(parser, INK_PARSE_EXPRESSION);
    ink_parser_expect(parser, INK_TT_RIGHT_ARROW);
    node = ink_parse_name_expr(parser);
    ink_parser_pop_context(parser);
    return ink_parser_create_unary(parser, INK_NODE_DIVERT_EXPR, source_start,
                                   parser->current_offset, node);
}

static struct ink_syntax_node *ink_parse_thread_expr(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t source_start = parser->current_offset;

    ink_parser_push_context(parser, INK_PARSE_EXPRESSION);
    ink_parser_expect(parser, INK_TT_LEFT_ARROW);

    if (ink_parser_check(parser, INK_TT_IDENTIFIER)) {
        node = ink_parse_name_expr(parser);
    }

    ink_parser_pop_context(parser);
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
    const size_t source_start =
        ink_parser_expect(parser, INK_TT_KEYWORD_RETURN);

    if (!ink_parser_check(parser, INK_TT_NL)) {
        node = ink_parse_expr(parser);
    }
    return ink_parser_create_unary(parser, INK_NODE_RETURN_STMT, source_start,
                                   parser->current_offset, node);
}

static struct ink_syntax_node *ink_parse_expr_stmt(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t source_start = parser->current_offset;

    ink_parser_push_context(parser, INK_PARSE_EXPRESSION);
    ink_parser_expect(parser, INK_TT_TILDE);

    if (ink_parser_check(parser, INK_TT_KEYWORD_RETURN)) {
        node = ink_parse_return_stmt(parser);
    } else {
        node = ink_parse_expr(parser);
        node = ink_parser_create_unary(parser, INK_NODE_EXPR_STMT, source_start,
                                       parser->current_offset, node);
    }

    ink_parser_pop_context(parser);
    ink_parser_expect_stmt_end(parser);
    return node;
}

static struct ink_syntax_node *
ink_parse_content_string(struct ink_parser *parser)
{
    const size_t source_start = parser->current_offset;

    while (!ink_context_delim(parser)) {
        ink_parser_advance(parser);
    }
    return ink_parser_create_leaf(parser, INK_NODE_STRING_LITERAL, source_start,
                                  parser->current_offset);
}

static struct ink_syntax_node *
ink_parse_sequence_expr(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t scratch_offset = parser->scratch.count;
    const size_t source_start = parser->current_offset;

    INK_PARSER_RULE(node, ink_parse_content_expr, parser);

    if (!ink_parser_check(parser, INK_TT_PIPE)) {
        /*
         * Backtrack and parse primary expression.
         */
        ink_parser_rewind_context(parser);
        ink_parser_push_context(parser, INK_PARSE_EXPRESSION);
        ink_parser_advance(parser);
        INK_PARSER_RULE(node, ink_parse_expr, parser);
        ink_parser_pop_context(parser);
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
                                      source_start, parser->current_offset,
                                      scratch_offset);
}

static struct ink_syntax_node *ink_parse_brace_expr(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t source_start = ink_parser_expect(parser, INK_TT_LEFT_BRACE);

    ink_parser_push_context(parser, INK_PARSE_BRACE);
    INK_PARSER_RULE(node, ink_parse_sequence_expr, parser);
    ink_parser_pop_context(parser);
    ink_parser_expect(parser, INK_TT_RIGHT_BRACE);
    return ink_parser_create_unary(parser, INK_NODE_BRACE_EXPR, source_start,
                                   parser->current_offset, node);
}

static struct ink_syntax_node *ink_parse_content_expr(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;

    if (!ink_context_delim(parser)) {
        INK_PARSER_RULE(node, ink_parse_content_string, parser);
    } else if (ink_parser_check(parser, INK_TT_LEFT_BRACE)) {
        INK_PARSER_RULE(node, ink_parse_brace_expr, parser);
    } else if (ink_parser_check(parser, INK_TT_RIGHT_ARROW)) {
        INK_PARSER_RULE(node, ink_parse_divert_expr, parser);
    } else if (ink_parser_check(parser, INK_TT_LEFT_ARROW)) {
        INK_PARSER_RULE(node, ink_parse_thread_expr, parser);
    }
    return node;
}

static struct ink_syntax_node *ink_parse_divert_stmt(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t source_start = parser->current_offset;

    INK_PARSER_RULE(node, ink_parse_divert_expr, parser);
    ink_parser_expect_stmt_end(parser);
    return ink_parser_create_unary(parser, INK_NODE_DIVERT_STMT, source_start,
                                   parser->current_offset, node);
}

static struct ink_syntax_node *ink_parse_content(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t source_start = parser->current_offset;
    const size_t scratch_offset = parser->scratch.count;

    while (!ink_parser_check(parser, INK_TT_EOF) &&
           !ink_parser_check(parser, INK_TT_NL)) {
        INK_PARSER_RULE(node, ink_parse_content_expr, parser);
        ink_parser_scratch_append(&parser->scratch, node);
    }

    ink_parser_advance(parser);
    return ink_parser_create_sequence(parser, INK_NODE_CONTENT_STMT,
                                      source_start, parser->current_offset,
                                      scratch_offset);
}

static struct ink_syntax_node *
ink_parse_choice_content(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t scratch_offset = parser->scratch.count;
    const size_t source_start = parser->current_offset;

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
                                      source_start, parser->current_offset,
                                      scratch_offset);
}

static struct ink_syntax_node *
ink_parse_choice_branch(struct ink_parser *parser,
                        enum ink_syntax_node_type type)
{
    struct ink_syntax_node *expr = NULL;
    struct ink_syntax_node *body = NULL;
    const size_t source_start = parser->current_offset;

    ink_parser_eat_nesting(parser, ink_parser_token_type(parser), true);
    /* ink_parser_push_context(parser, INK_PARSE_CHOICE); */
    INK_PARSER_RULE(expr, ink_parse_choice_content, parser);
    /* ink_parser_pop_context(parser); */

    if (ink_parser_check(parser, INK_TT_NL)) {
        ink_parser_advance(parser);
        INK_PARSER_RULE(body, ink_parse_block, parser);
    }
    return ink_parser_create_binary(parser, type, source_start,
                                    parser->current_offset, expr, body);
}

static struct ink_syntax_node *ink_parse_choice(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t scratch_offset = parser->scratch.count;
    const size_t source_start = parser->current_offset;

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

    return ink_parser_create_sequence(parser, INK_NODE_CHOICE_STMT,
                                      source_start, parser->current_offset,
                                      scratch_offset);
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

static struct ink_syntax_node *ink_parse_knot_proto(struct ink_parser *parser)
{
    struct ink_syntax_node *lhs = NULL;
    struct ink_syntax_node *rhs = NULL;
    const size_t source_start = parser->current_offset;

    ink_parser_push_context(parser, INK_PARSE_EXPRESSION);
    ink_parser_eat(parser, INK_TT_WHITESPACE);

    if (ink_parser_try_keyword(parser, INK_TT_KEYWORD_FUNCTION)) {
        ink_parser_advance(parser);
    }

    lhs = ink_parse_identifier(parser);

    if (ink_parser_check(parser, INK_TT_LEFT_PAREN)) {
        rhs = ink_parse_parameter_list(parser);
    }

    /* NOTE(Brett): Checking for `==` as a separate token is a hack, but I don't
     * have any brighter ideas.
     */
    while (ink_parser_check(parser, INK_TT_EQUAL) ||
           ink_parser_check(parser, INK_TT_EQUAL_EQUAL)) {
        ink_parser_advance(parser);
    }

    ink_parser_pop_context(parser);
    ink_parser_expect(parser, INK_TT_NL);
    return ink_parser_create_binary(parser, INK_NODE_KNOT_PROTO, source_start,
                                    parser->current_offset, lhs, rhs);
}

static struct ink_syntax_node *ink_parse_knot(struct ink_parser *parser)
{
    struct ink_syntax_node *lhs = NULL;
    struct ink_syntax_node *rhs = NULL;
    const size_t source_start = parser->current_offset;

    while (ink_parser_check(parser, INK_TT_EQUAL)) {
        ink_parser_advance(parser);
    }

    lhs = ink_parse_knot_proto(parser);
    rhs = ink_parse_block(parser);
    return ink_parser_create_binary(parser, INK_NODE_KNOT_DECL, source_start,
                                    parser->current_offset, lhs, rhs);
}

static struct ink_syntax_node *ink_parse_var(struct ink_parser *parser)
{
    struct ink_syntax_node *lhs = NULL;
    struct ink_syntax_node *rhs = NULL;
    const size_t source_start = ink_parser_expect(parser, INK_TT_KEYWORD_VAR);

    ink_parser_push_context(parser, INK_PARSE_EXPRESSION);
    ink_parser_eat(parser, INK_TT_WHITESPACE);
    INK_PARSER_RULE(lhs, ink_parse_identifier, parser);
    ink_parser_expect(parser, INK_TT_EQUAL);
    INK_PARSER_RULE(rhs, ink_parse_expr, parser);
    ink_parser_pop_context(parser);
    ink_parser_expect_stmt_end(parser);

    return ink_parser_create_binary(parser, INK_NODE_VAR_DECL, source_start,
                                    parser->current_offset, lhs, rhs);
}

static struct ink_syntax_node *ink_parse_const(struct ink_parser *parser)
{
    struct ink_syntax_node *lhs = NULL;
    struct ink_syntax_node *rhs = NULL;
    const size_t source_start = ink_parser_expect(parser, INK_TT_KEYWORD_CONST);

    ink_parser_push_context(parser, INK_PARSE_EXPRESSION);
    ink_parser_eat(parser, INK_TT_WHITESPACE);
    INK_PARSER_RULE(lhs, ink_parse_identifier, parser);
    ink_parser_expect(parser, INK_TT_EQUAL);
    INK_PARSER_RULE(rhs, ink_parse_expr, parser);
    ink_parser_pop_context(parser);
    ink_parser_expect_stmt_end(parser);

    return ink_parser_create_binary(parser, INK_NODE_CONST_DECL, source_start,
                                    parser->current_offset, lhs, rhs);
}

/**
 * Block delimiters are context-sensitive.
 * Opening new blocks is simple. Closing them is more tricky.
 */
static struct ink_syntax_node *
ink_parse_block_delimited(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t source_start = parser->current_offset;
    const enum ink_token_type token_type = ink_parser_token_type(parser);

    if (ink_parser_try_keyword(parser, INK_TT_KEYWORD_CONST)) {
        INK_PARSER_RULE(node, ink_parse_const, parser);
    } else if (ink_parser_try_keyword(parser, INK_TT_KEYWORD_VAR)) {
        INK_PARSER_RULE(node, ink_parse_var, parser);
    } else {
        switch (token_type) {
        case INK_TT_STAR:
        case INK_TT_PLUS: {
            const size_t level =
                ink_parser_eat_nesting(parser, token_type, true);

            ink_parser_rewind(parser, source_start);

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
            break;
        }
        case INK_TT_EQUAL: {
            break;
        }
        case INK_TT_TILDE: {
            INK_PARSER_RULE(node, ink_parse_expr_stmt, parser);
            break;
        }
        case INK_TT_RIGHT_ARROW: {
            INK_PARSER_RULE(node, ink_parse_divert_stmt, parser);
            break;
        }
        default:
            INK_PARSER_RULE(node, ink_parse_content, parser);
            break;
        }
    }
    return node;
}

static struct ink_syntax_node *ink_parse_block(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t scratch_offset = parser->scratch.count;
    const size_t source_start = parser->current_offset;

    while (!ink_parser_check(parser, INK_TT_EOF)) {
        ink_parser_eat(parser, INK_TT_WHITESPACE);
        INK_PARSER_RULE(node, ink_parse_block_delimited, parser);

        if (node == NULL) {
            break;
        }

        ink_parser_scratch_append(&parser->scratch, node);
    }
    return ink_parser_create_sequence(parser, INK_NODE_BLOCK_STMT, source_start,
                                      parser->current_offset, scratch_offset);
}

static struct ink_syntax_node *ink_parse_file(struct ink_parser *parser)
{
    struct ink_syntax_node *node = NULL;
    const size_t scratch_offset = parser->scratch.count;
    const size_t source_start = parser->current_offset;

    if (ink_parser_check(parser, INK_TT_EOF)) {
        return NULL;
    }
    while (!ink_parser_check(parser, INK_TT_EOF)) {
        if (ink_parser_check(parser, INK_TT_EQUAL)) {
            INK_PARSER_RULE(node, ink_parse_knot, parser);
        } else {
            INK_PARSER_RULE(node, ink_parse_block, parser);
        }

        ink_parser_scratch_append(&parser->scratch, node);
    }
    return ink_parser_create_sequence(parser, INK_NODE_FILE, source_start,
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
