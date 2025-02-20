#ifndef __INK_OBJECT_H__
#define __INK_OBJECT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

#include "story.h"
#include "vec.h"

enum ink_object_type {
    INK_OBJ_NUMBER,
    INK_OBJ_STRING,
    INK_OBJ_TABLE,
    INK_OBJ_CONTENT_PATH,
    INK_OBJ_STACK_FRAME,
};

struct ink_object {
    struct ink_object *next;
    enum ink_object_type type;
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
    uint32_t args_count;
    uint32_t locals_count;
    struct ink_byte_vec code;
    struct ink_object_vec const_pool;
};

struct ink_stack_frame {
    struct ink_object obj;
    struct ink_content_path *path;
    struct ink_object *return_value;
    uint8_t *return_addr;
    size_t locals_count;
    struct ink_object *locals[1];
};

#define INK_OBJ_IS_NUMBER(__x) ((__x)->type == INK_OBJ_NUMBER)
#define INK_OBJ_AS_NUMBER(__x) ((struct ink_number *)(__x))
#define INK_OBJ_IS_STRING(__x) ((__x)->type == INK_OBJ_STRING)
#define INK_OBJ_AS_STRING(__x) ((struct ink_string *)(__x))
#define INK_OBJ_IS_TABLE(__x) ((__x)->type == INK_OBJ_TABLE)
#define INK_OBJ_AS_TABLE(__x) ((struct ink_table *)(__x))
#define INK_OBJ_IS_CONTENT_PATH(__x) ((__x)->type == INK_OBJ_CONTENT_PATH)
#define INK_OBJ_AS_CONTENT_PATH(__x) ((struct ink_content_path *)(__x))
#define INK_OBJ_IS_STACK_FRAME(__x) ((__x)->type == INK_OBJ_STACK_FRAME)
#define INK_OBJ_AS_STACK_FRAME(__x) ((struct ink_stack_frame *)(__x))

extern struct ink_object *
ink_object_new(struct ink_story *story, enum ink_object_type type, size_t size);
extern void ink_object_free(struct ink_story *story, struct ink_object *obj);

extern struct ink_object *ink_number_new(struct ink_story *story, double value);
extern struct ink_object *ink_number_add(struct ink_story *story,
                                         const struct ink_object *lhs,
                                         const struct ink_object *rhs);
extern struct ink_object *ink_number_subtract(struct ink_story *story,
                                              const struct ink_object *lhs,
                                              const struct ink_object *rhs);
extern struct ink_object *ink_number_multiply(struct ink_story *story,
                                              const struct ink_object *lhs,
                                              const struct ink_object *rhs);
extern struct ink_object *ink_number_divide(struct ink_story *story,
                                            const struct ink_object *lhs,
                                            const struct ink_object *rhs);
extern struct ink_object *ink_number_modulo(struct ink_story *story,
                                            const struct ink_object *lhs,
                                            const struct ink_object *rhs);
extern struct ink_object *ink_number_negate(struct ink_story *story,
                                            const struct ink_object *lhs);

extern struct ink_object *ink_string_new(struct ink_story *story,
                                         const uint8_t *chars, size_t length);

extern bool ink_string_equal(struct ink_story *story,
                             const struct ink_object *lhs,
                             const struct ink_object *rhs);

extern struct ink_object *ink_table_new(struct ink_story *story);
extern int ink_table_lookup(struct ink_story *story, struct ink_object *table,
                            struct ink_object *key, struct ink_object **value);
extern int ink_table_insert(struct ink_story *story, struct ink_object *table,
                            struct ink_object *key, struct ink_object *value);

extern struct ink_object *ink_content_path_new(struct ink_story *story,
                                               struct ink_object *name);

extern struct ink_object *ink_stack_frame_new(struct ink_story *story,
                                              struct ink_content_path *obj,
                                              uint8_t *return_addr);

extern void ink_object_print(const struct ink_object *obj);
extern void ink_number_print(const struct ink_object *obj);
extern void ink_string_print(const struct ink_object *obj);
extern void ink_table_print(const struct ink_object *obj);

#ifdef __cplusplus
}
#endif

#endif
