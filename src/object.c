#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "story.h"

#define INK_TABLE_CAPACITY_MIN 8
#define INK_TABLE_LOAD_MAX 80
#define INK_TABLE_SCALE_FACTOR 2
#define INK_OBJ(__x) ((struct ink_object *)(__x))

struct ink_object *ink_number_new(struct ink_story *story, double value)
{
    struct ink_number *const obj =
        INK_OBJ_AS_NUMBER(ink_object_new(story, INK_OBJ_NUMBER, sizeof(*obj)));

    if (!obj) {
        return NULL;
    }

    obj->value = value;
    return INK_OBJ(obj);
}

void ink_number_print(const struct ink_object *obj)
{
    struct ink_number *const num = INK_OBJ_AS_NUMBER(obj);

    printf("%g", num->value);
}

struct ink_object *ink_number_add(struct ink_story *story,
                                  const struct ink_object *lhs,
                                  const struct ink_object *rhs)
{
    return ink_number_new(story, INK_OBJ_AS_NUMBER(lhs)->value +
                                     INK_OBJ_AS_NUMBER(rhs)->value);
}

struct ink_object *ink_number_subtract(struct ink_story *story,
                                       const struct ink_object *lhs,
                                       const struct ink_object *rhs)
{
    return ink_number_new(story, INK_OBJ_AS_NUMBER(lhs)->value -
                                     INK_OBJ_AS_NUMBER(rhs)->value);
}

struct ink_object *ink_number_multiply(struct ink_story *story,
                                       const struct ink_object *lhs,
                                       const struct ink_object *rhs)
{
    return ink_number_new(story, INK_OBJ_AS_NUMBER(lhs)->value *
                                     INK_OBJ_AS_NUMBER(rhs)->value);
}

struct ink_object *ink_number_divide(struct ink_story *story,
                                     const struct ink_object *lhs,
                                     const struct ink_object *rhs)
{
    return ink_number_new(story, INK_OBJ_AS_NUMBER(lhs)->value /
                                     INK_OBJ_AS_NUMBER(rhs)->value);
}

struct ink_object *ink_number_modulo(struct ink_story *story,
                                     const struct ink_object *lhs,
                                     const struct ink_object *rhs)
{
    return ink_number_new(story, fmod(INK_OBJ_AS_NUMBER(lhs)->value,
                                      INK_OBJ_AS_NUMBER(rhs)->value));
}

struct ink_object *ink_number_negate(struct ink_story *story,
                                     const struct ink_object *lhs)
{
    return ink_number_new(story, -INK_OBJ_AS_NUMBER(lhs)->value);
}

struct ink_object *ink_string_new(struct ink_story *story, const uint8_t *bytes,
                                  size_t length)
{
    struct ink_string *const obj = INK_OBJ_AS_STRING(
        ink_object_new(story, INK_OBJ_STRING, sizeof(*obj) + length + 1));

    if (!obj) {
        return NULL;
    }

    memcpy(obj->bytes, bytes, length);
    obj->bytes[length] = '\0';
    obj->length = (uint32_t)length;
    return INK_OBJ(obj);
}

bool ink_string_equal(struct ink_story *story, const struct ink_object *lhs,
                      const struct ink_object *rhs)
{
    struct ink_string *const str_lhs = INK_OBJ_AS_STRING(lhs);
    struct ink_string *const str_rhs = INK_OBJ_AS_STRING(rhs);

    (void)story;

    assert(INK_OBJ_IS_STRING(lhs));
    assert(INK_OBJ_IS_STRING(rhs));
    return str_lhs->length == str_rhs->length &&
           memcmp(str_lhs->bytes, str_rhs->bytes, str_lhs->length) == 0;
}

void ink_string_print(const struct ink_object *obj)
{
    struct ink_string *const str = INK_OBJ_AS_STRING(obj);

    assert(INK_OBJ_IS_STRING(obj));
    printf("\"%s\"", str->bytes);
}

struct ink_object *ink_table_new(struct ink_story *story)
{
    struct ink_table *const obj =
        INK_OBJ_AS_TABLE(ink_object_new(story, INK_OBJ_TABLE, sizeof(*obj)));

    if (!obj) {
        return NULL;
    }

    obj->count = 0;
    obj->capacity = 0;
    obj->entries = NULL;
    return INK_OBJ(obj);
}

void ink_table_free(struct ink_story *story, struct ink_object *obj)
{
    struct ink_table *const table = INK_OBJ_AS_TABLE(obj);

    assert(INK_OBJ_IS_TABLE(obj));
    ink_story_mem_free(story, table->entries);
}

static struct ink_table_kv *ink_table_find_slot(struct ink_story *story,
                                                struct ink_table_kv *entries,
                                                size_t capacity,
                                                struct ink_string *key)
{
    size_t index = key->hash & (capacity - 1);

    for (;;) {
        struct ink_table_kv *const entry = &entries[index];

        if (entry->key == NULL ||
            ink_string_equal(story, INK_OBJ(entry->key), INK_OBJ(key))) {
            return entry;
        }

        index = (index + 1) & (capacity - 1);
    }
}

static uint32_t ink_table_next_size(const struct ink_table *table)
{
    const uint32_t capacity = table->capacity;

    if (capacity < INK_TABLE_CAPACITY_MIN) {
        return INK_TABLE_CAPACITY_MIN;
    }
    return capacity * INK_TABLE_SCALE_FACTOR;
}

static bool ink_table_needs_resize(struct ink_table *table)
{
    if (table->capacity == 0) {
        return true;
    }
    return ((table->count * 100ul) / table->capacity) > INK_TABLE_LOAD_MAX;
}

static int ink_table_resize(struct ink_story *story, struct ink_table *table)
{
    struct ink_table_kv *entries, *src, *dst;
    const uint32_t capacity = ink_table_next_size(table);
    const uint32_t size = sizeof(*table->entries) * capacity;
    uint32_t count = 0;

    entries =
        ink_story_mem_alloc(story, NULL, 0, capacity * (sizeof(*entries)));
    if (!entries) {
        return -1;
    }

    memset(entries, 0, size);

    for (size_t i = 0; i < table->capacity; i++) {
        src = &table->entries[i];
        if (src->key) {
            dst = ink_table_find_slot(story, entries, capacity, src->key);
            dst->key = src->key;
            dst->value = src->value;
            count++;
        }
    }

    ink_story_mem_free(story, table->entries);
    table->entries = entries;
    table->capacity = capacity;
    table->count = count;
    return 0;
}

int ink_table_lookup(struct ink_story *story, struct ink_object *obj,
                     struct ink_object *key, struct ink_object **value)
{
    struct ink_table_kv *kv;
    struct ink_table *const table = INK_OBJ_AS_TABLE(obj);

    assert(INK_OBJ_IS_TABLE(obj));

    if (table->count == 0) {
        return -1;
    }

    kv = ink_table_find_slot(story, table->entries, table->capacity,
                             INK_OBJ_AS_STRING(key));
    if (!kv->key) {
        return -1;
    }

    *value = kv->value;
    return 0;
}

int ink_table_insert(struct ink_story *story, struct ink_object *obj,
                     struct ink_object *key, struct ink_object *value)
{
    int rc;
    struct ink_table_kv *entry;
    struct ink_table *const table = INK_OBJ_AS_TABLE(obj);
    struct ink_string *const key_str = INK_OBJ_AS_STRING(key);

    assert(INK_OBJ_IS_TABLE(obj));
    assert(INK_OBJ_IS_STRING(key));

    if (ink_table_needs_resize(table)) {
        rc = ink_table_resize(story, table);
        if (rc < 0) {
            return rc;
        }
    }

    entry =
        ink_table_find_slot(story, table->entries, table->capacity, key_str);
    if (!entry->key) {
        entry->key = key_str;
        entry->value = value;
        table->count++;
        return 0;
    }

    entry->key = key_str;
    entry->value = value;
    return 1;
}

void ink_table_print(const struct ink_object *obj)
{
    struct ink_table *const table = INK_OBJ_AS_TABLE(obj);

    assert(INK_OBJ_IS_TABLE(obj));
    printf("{");

    for (size_t i = 0; i < table->capacity; i++) {
        struct ink_table_kv entry = table->entries[i];

        if (entry.key) {
            printf("[\"%s\"] => ", entry.key->bytes);

            if (entry.value) {
                ink_object_print(entry.value);
            } else {
                printf("NULL");
            }

            printf(", ");
        }
    }

    printf("}");
}

struct ink_object *ink_content_path_new(struct ink_story *story,
                                        struct ink_object *name)
{
    struct ink_content_path *const obj = INK_OBJ_AS_CONTENT_PATH(
        ink_object_new(story, INK_OBJ_CONTENT_PATH, sizeof(*obj)));

    if (!obj) {
        ink_story_mem_panic(story);
        return NULL;
    }

    assert(name != NULL);

    obj->name = INK_OBJ_AS_STRING(name);
    obj->args_count = 0;
    obj->locals_count = 0;
    obj->code_offset = 0;
    obj->code_length = 0;
    return INK_OBJ(obj);
}

struct ink_object *ink_stack_frame_new(struct ink_story *story,
                                       struct ink_content_path *path,
                                       uint8_t *return_addr)
{
    const size_t total_locals = path->args_count + path->locals_count;
    const size_t stack_size = total_locals * sizeof(struct ink_object *);
    struct ink_stack_frame *const obj = INK_OBJ_AS_STACK_FRAME(
        ink_object_new(story, INK_OBJ_STACK_FRAME, sizeof(*obj) + stack_size));

    if (!obj) {
        ink_story_mem_panic(story);
        return NULL;
    }

    assert(path != NULL);
    assert(return_addr != NULL);

    obj->path = path;
    obj->return_value = NULL;
    obj->return_addr = return_addr;
    obj->locals_count = stack_size;
    memset(obj->locals, 0, stack_size);
    return INK_OBJ(obj);
}

struct ink_object *ink_object_new(struct ink_story *story,
                                  enum ink_object_type type, size_t size)
{
    struct ink_object *const obj = ink_story_mem_alloc(story, NULL, 0, size);

    if (!obj) {
        ink_story_mem_panic(story);
        return NULL;
    }

    assert(size >= sizeof(*obj));

    obj->next = story->objects;
    obj->type = type;
    story->objects = obj;
    return obj;
}

void ink_object_free(struct ink_story *story, struct ink_object *obj)
{
    switch (obj->type) {
    case INK_OBJ_NUMBER:
    case INK_OBJ_STRING:
    case INK_OBJ_CONTENT_PATH:
    case INK_OBJ_STACK_FRAME:
        break;
    case INK_OBJ_TABLE:
        ink_table_free(story, obj);
        break;
    }

    ink_story_mem_free(story, obj);
}

void ink_object_print(const struct ink_object *obj)
{
    switch (obj->type) {
    case INK_OBJ_NUMBER:
        ink_number_print(obj);
        break;
    case INK_OBJ_STRING:
        ink_string_print(obj);
        break;
    case INK_OBJ_TABLE:
        ink_table_print(obj);
        break;
    case INK_OBJ_CONTENT_PATH:
    case INK_OBJ_STACK_FRAME:
        break;
    }
}
