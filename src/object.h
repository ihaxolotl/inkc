#ifndef INK_OBJECT_H
#define INK_OBJECT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "vec.h"

struct ink_story;
struct ink_object;

#define INK_TABLE_CAPACITY_MIN (8ul)
#define INK_TABLE_SCALE_FACTOR (2ul)
#define INK_TABLE_LOAD_MAX (80ul)

INK_VEC_T(ink_byte_vec, uint8_t)
INK_VEC_T(ink_object_vec, struct ink_object *)

enum ink_object_type {
    INK_OBJ_BOOL,
    INK_OBJ_NUMBER,
    INK_OBJ_STRING,
    INK_OBJ_TABLE,
    INK_OBJ_CONTENT_PATH,
};

typedef long ink_integer;
typedef double ink_float;

struct ink_object {
    enum ink_object_type type;
    bool is_marked;
    struct ink_object *next;
};

struct ink_bool {
    struct ink_object obj;
    bool value;
};

struct ink_number {
    struct ink_object obj;
    bool is_int;

    union {
        ink_integer integer;
        ink_float floating;
    } as;
};

struct ink_string {
    struct ink_object obj;
    uint32_t hash;
    uint32_t length;
    uint8_t bytes[1];
};

struct ink_table_kv {
    struct ink_string *key;
    struct ink_object *value;
};

struct ink_table {
    struct ink_object obj;
    uint32_t count;
    uint32_t capacity;
    struct ink_table_kv *entries;
};

struct ink_content_path {
    struct ink_object obj;
    struct ink_string *name;
    uint32_t arity;
    uint32_t locals_count;
    struct ink_byte_vec code;
    struct ink_object_vec const_pool;
};

#define INK_OBJ(__x) ((struct ink_object *)(__x))

#define INK_OBJ_IS_BOOL(__x) ((__x)->type == INK_OBJ_BOOL)
#define INK_OBJ_AS_BOOL(__x) ((struct ink_bool *)(__x))

#define INK_OBJ_IS_NUMBER(__x) ((__x)->type == INK_OBJ_NUMBER)
#define INK_OBJ_AS_NUMBER(__x) ((struct ink_number *)(__x))

#define INK_OBJ_IS_STRING(__x) ((__x)->type == INK_OBJ_STRING)
#define INK_OBJ_AS_STRING(__x) ((struct ink_string *)(__x))

#define INK_OBJ_IS_TABLE(__x) ((__x)->type == INK_OBJ_TABLE)
#define INK_OBJ_AS_TABLE(__x) ((struct ink_table *)(__x))

#define INK_OBJ_IS_CONTENT_PATH(__x) ((__x)->type == INK_OBJ_CONTENT_PATH)
#define INK_OBJ_AS_CONTENT_PATH(__x) ((struct ink_content_path *)(__x))

/**
 * Create a new runtime object.
 */
extern struct ink_object *
ink_object_new(struct ink_story *story, enum ink_object_type type, size_t size);

/**
 * Free a runtime object.
 */
extern void ink_object_free(struct ink_story *story, struct ink_object *obj);

/**
 * Determine the equality of two runtime objects.
 */
extern bool ink_object_eq(const struct ink_object *lhs,
                          const struct ink_object *rhs);

/**
 * Print a runtime object.
 */
extern void ink_object_print(const struct ink_object *obj);

/**
 * Return a printable string for an object type.
 */
extern const char *ink_object_type_strz(enum ink_object_type type);

/**
 * Determine if an object is falsey.
 */
extern bool ink_object_is_falsey(const struct ink_object *obj);

/**
 * Create a boolean object.
 */
extern struct ink_object *ink_bool_new(struct ink_story *story, bool value);

/**
 * Determine the equality of two bool objects.
 */
extern bool ink_bool_eq(const struct ink_bool *lhs, const struct ink_bool *rhs);

/**
 * Create a runtime integer object.
 */
extern struct ink_object *ink_integer_new(struct ink_story *story,
                                          ink_integer value);

/**
 * Create a runtime floating-point number object.
 */
extern struct ink_object *ink_float_new(struct ink_story *story,
                                        ink_float value);

/**
 * Determine the equality of two number objects.
 */
extern bool ink_number_eq(const struct ink_number *lhs,
                          const struct ink_number *rhs);

/**
 * Create a string object.
 *
 * Strings will be automatically null-terminated.
 */
extern struct ink_object *ink_string_new(struct ink_story *story,
                                         const uint8_t *bytes, size_t length);

/**
 * Check two strings for equality.
 */
extern bool ink_string_eq(const struct ink_string *lhs,
                          const struct ink_string *rhs);
/**
 * Concatenate two strings.
 *
 * Return a new string upon success, and NULL upon failure,
 */
extern struct ink_object *ink_string_concat(struct ink_story *story,
                                            const struct ink_object *lhs,
                                            const struct ink_object *rhs);

/**
 * Create a table object.
 */
extern struct ink_object *ink_table_new(struct ink_story *story);

/**
 * Perform a lookup for an object within a table object.
 */
extern int ink_table_lookup(struct ink_story *story, struct ink_object *obj,
                            struct ink_object *key, struct ink_object **value);

/**
 * Perform an insertion for an object to a table object.
 */
extern int ink_table_insert(struct ink_story *story, struct ink_object *obj,
                            struct ink_object *key, struct ink_object *value);

/**
 * Create a content path object.
 */
extern struct ink_object *ink_content_path_new(struct ink_story *story,
                                               struct ink_object *name);

#ifdef __cplusplus
}
#endif

#endif
