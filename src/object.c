#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "story.h"

#define INK_TABLE_CAPACITY_MIN (8ul)
#define INK_TABLE_LOAD_MAX (80ul)
#define INK_TABLE_SCALE_FACTOR (2ul)

struct ink_object *ink_bool_new(struct ink_story *story, bool value)
{
    struct ink_bool *const obj =
        INK_OBJ_AS_BOOL(ink_object_new(story, INK_OBJ_BOOL, sizeof(*obj)));

    if (!obj) {
        return NULL;
    }

    obj->value = value;
    return INK_OBJ(obj);
}

bool ink_bool_eq(struct ink_story *story, const struct ink_object *lhs,
                 const struct ink_object *rhs)
{
    assert(INK_OBJ_IS_BOOL(lhs) && INK_OBJ_IS_BOOL(rhs));
    return (INK_OBJ_AS_BOOL(lhs)->value == INK_OBJ_AS_BOOL(rhs)->value);
}

void ink_bool_print(const struct ink_object *obj)
{
    struct ink_bool *const bobj = INK_OBJ_AS_BOOL(obj);

    printf("<Bool value=%s, addr=%p>", bobj->value ? "true" : "false",
           (void *)obj);
}

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

    printf("<Number value=%lf, addr=%p>", num->value, (void *)obj);
}

struct ink_object *ink_number_add(struct ink_story *story,
                                  const struct ink_object *lhs,
                                  const struct ink_object *rhs)
{
    assert(INK_OBJ_IS_NUMBER(lhs) && INK_OBJ_IS_NUMBER(rhs));
    return ink_number_new(story, INK_OBJ_AS_NUMBER(lhs)->value +
                                     INK_OBJ_AS_NUMBER(rhs)->value);
}

struct ink_object *ink_number_sub(struct ink_story *story,
                                  const struct ink_object *lhs,
                                  const struct ink_object *rhs)
{
    assert(INK_OBJ_IS_NUMBER(lhs) && INK_OBJ_IS_NUMBER(rhs));
    return ink_number_new(story, INK_OBJ_AS_NUMBER(lhs)->value -
                                     INK_OBJ_AS_NUMBER(rhs)->value);
}

struct ink_object *ink_number_mul(struct ink_story *story,
                                  const struct ink_object *lhs,
                                  const struct ink_object *rhs)
{
    assert(INK_OBJ_IS_NUMBER(lhs) && INK_OBJ_IS_NUMBER(rhs));
    return ink_number_new(story, INK_OBJ_AS_NUMBER(lhs)->value *
                                     INK_OBJ_AS_NUMBER(rhs)->value);
}

struct ink_object *ink_number_div(struct ink_story *story,
                                  const struct ink_object *lhs,
                                  const struct ink_object *rhs)
{
    assert(INK_OBJ_IS_NUMBER(lhs) && INK_OBJ_IS_NUMBER(rhs));
    return ink_number_new(story, INK_OBJ_AS_NUMBER(lhs)->value /
                                     INK_OBJ_AS_NUMBER(rhs)->value);
}

struct ink_object *ink_number_mod(struct ink_story *story,
                                  const struct ink_object *lhs,
                                  const struct ink_object *rhs)
{
    assert(INK_OBJ_IS_NUMBER(lhs) && INK_OBJ_IS_NUMBER(rhs));
    return ink_number_new(story, fmod(INK_OBJ_AS_NUMBER(lhs)->value,
                                      INK_OBJ_AS_NUMBER(rhs)->value));
}

struct ink_object *ink_number_neg(struct ink_story *story,
                                  const struct ink_object *lhs)
{
    assert(INK_OBJ_IS_NUMBER(lhs));
    return ink_number_new(story, -INK_OBJ_AS_NUMBER(lhs)->value);
}

bool ink_number_eq(struct ink_story *story, const struct ink_object *lhs,
                   const struct ink_object *rhs)
{
    assert(INK_OBJ_IS_NUMBER(lhs) && INK_OBJ_IS_NUMBER(rhs));
    return (INK_OBJ_AS_NUMBER(lhs)->value == INK_OBJ_AS_NUMBER(rhs)->value);
}

bool ink_number_lt(struct ink_story *story, const struct ink_object *lhs,
                   const struct ink_object *rhs)
{
    assert(INK_OBJ_IS_NUMBER(lhs) && INK_OBJ_IS_NUMBER(rhs));
    return (INK_OBJ_AS_NUMBER(lhs)->value < INK_OBJ_AS_NUMBER(rhs)->value);
}

bool ink_number_lte(struct ink_story *story, const struct ink_object *lhs,
                    const struct ink_object *rhs)
{
    assert(INK_OBJ_IS_NUMBER(lhs) && INK_OBJ_IS_NUMBER(rhs));
    return (INK_OBJ_AS_NUMBER(lhs)->value <= INK_OBJ_AS_NUMBER(rhs)->value);
}

bool ink_number_gt(struct ink_story *story, const struct ink_object *lhs,
                   const struct ink_object *rhs)
{
    assert(INK_OBJ_IS_NUMBER(lhs) && INK_OBJ_IS_NUMBER(rhs));
    return (INK_OBJ_AS_NUMBER(lhs)->value > INK_OBJ_AS_NUMBER(rhs)->value);
}

bool ink_number_gte(struct ink_story *story, const struct ink_object *lhs,
                    const struct ink_object *rhs)
{
    assert(INK_OBJ_IS_NUMBER(lhs) && INK_OBJ_IS_NUMBER(rhs));
    return (INK_OBJ_AS_NUMBER(lhs)->value >= INK_OBJ_AS_NUMBER(rhs)->value);
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

bool ink_string_eq(struct ink_story *story, const struct ink_object *lhs,
                   const struct ink_object *rhs)
{
    struct ink_string *const str_lhs = INK_OBJ_AS_STRING(lhs);
    struct ink_string *const str_rhs = INK_OBJ_AS_STRING(rhs);

    assert(INK_OBJ_IS_STRING(lhs) && INK_OBJ_IS_STRING(rhs));
    return str_lhs->length == str_rhs->length &&
           memcmp(str_lhs->bytes, str_rhs->bytes, str_lhs->length) == 0;
}

void ink_string_print(const struct ink_object *obj)
{
    struct ink_string *const str = INK_OBJ_AS_STRING(obj);

    assert(INK_OBJ_IS_STRING(obj));
    printf("<String value=\"%s\", addr=%p>", str->bytes, (void *)obj);
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
            ink_string_eq(story, INK_OBJ(entry->key), INK_OBJ(key))) {
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
    obj->arity = 0;
    obj->locals_count = 0;
    ink_byte_vec_init(&obj->code);
    ink_object_vec_init(&obj->const_pool);
    return INK_OBJ(obj);
}

static void ink_content_path_free(struct ink_story *story,
                                  struct ink_object *obj)
{
    struct ink_content_path *const path = INK_OBJ_AS_CONTENT_PATH(obj);

    assert(INK_OBJ_IS_CONTENT_PATH(obj));
    ink_byte_vec_deinit(&path->code);
    ink_object_vec_deinit(&path->const_pool);
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
    case INK_OBJ_BOOL:
    case INK_OBJ_NUMBER:
    case INK_OBJ_STRING:
        break;
    case INK_OBJ_TABLE:
        ink_table_free(story, obj);
        break;
    case INK_OBJ_CONTENT_PATH:
        ink_content_path_free(story, obj);
        break;
    }

    ink_story_mem_free(story, obj);
}

struct ink_object *ink_object_eq(struct ink_story *story,
                                 const struct ink_object *lhs,
                                 const struct ink_object *rhs)
{
    bool value = false;

    if (lhs->type != rhs->type) {
        value = false;
    } else {
        switch (lhs->type) {
        case INK_OBJ_BOOL:
            value = ink_bool_eq(story, lhs, rhs);
            break;
        case INK_OBJ_NUMBER:
            value = ink_number_eq(story, lhs, rhs);
            break;
        case INK_OBJ_STRING:
            value = ink_string_eq(story, lhs, rhs);
            break;
        default:
            value = false;
            break;
        }
    }
    return ink_bool_new(story, value);
}

void ink_object_print(const struct ink_object *obj)
{
    if (!obj) {
        printf("<NULL>");
        return;
    }
    switch (obj->type) {
    case INK_OBJ_BOOL:
        ink_bool_print(obj);
        break;
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
        printf("<Path addr=%p>", (void *)obj);
        break;
    }
}
