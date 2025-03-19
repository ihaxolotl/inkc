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

INK_VEC_T(ink_byte_vec, uint8_t)
INK_VEC_T(ink_object_vec, struct ink_object *)

enum ink_object_type {
    INK_OBJ_BOOL,
    INK_OBJ_NUMBER,
    INK_OBJ_STRING,
    INK_OBJ_TABLE,
    INK_OBJ_CONTENT_PATH,
};

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
    double value;
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

extern struct ink_object *ink_bool_new(struct ink_story *story, bool value);
extern struct ink_object *ink_number_new(struct ink_story *story, double value);
extern struct ink_object *ink_string_new(struct ink_story *story,
                                         const uint8_t *chars, size_t length);
extern struct ink_object *ink_table_new(struct ink_story *story);
extern int ink_table_lookup(struct ink_story *story, struct ink_object *table,
                            struct ink_object *key, struct ink_object **value);
extern int ink_table_insert(struct ink_story *story, struct ink_object *table,
                            struct ink_object *key, struct ink_object *value);
extern struct ink_object *ink_content_path_new(struct ink_story *story,
                                               struct ink_object *name);

#ifdef __cplusplus
}
#endif

#endif
