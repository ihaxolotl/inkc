#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gc.h"
#include "logging.h"
#include "object.h"
#include "story.h"

static const char *INK_OBJ_TYPE_STR[] = {
    [INK_OBJ_BOOL] = "Bool",
    [INK_OBJ_NUMBER] = "Number",
    [INK_OBJ_STRING] = "String",
    [INK_OBJ_TABLE] = "Table",
    [INK_OBJ_CONTENT_PATH] = "ContentPath",
};

const char *ink_object_type_strz(enum ink_object_type type)
{
    return INK_OBJ_TYPE_STR[type];
}

struct ink_object *ink_object_new(struct ink_story *story,
                                  enum ink_object_type type, size_t size)
{
    struct ink_object *const obj = ink_story_mem_alloc(story, NULL, 0, size);

    if (!obj) {
        return NULL;
    }

    assert(size >= sizeof(*obj));
    obj->type = type;
    obj->is_marked = false;
    obj->next = story->gc_objects;
    story->gc_objects = obj;
    return obj;
}

void ink_object_free(struct ink_story *story, struct ink_object *obj)
{
    switch (obj->type) {
    case INK_OBJ_BOOL:
    case INK_OBJ_NUMBER:
    case INK_OBJ_STRING:
        break;
    case INK_OBJ_TABLE: {
        struct ink_table *const typed_obj = INK_OBJ_AS_TABLE(obj);

        ink_story_mem_free(story, typed_obj->entries);
        break;
    }
    case INK_OBJ_CONTENT_PATH: {
        struct ink_content_path *const typed_obj = INK_OBJ_AS_CONTENT_PATH(obj);

        ink_byte_vec_deinit(&typed_obj->code);
        ink_object_vec_deinit(&typed_obj->const_pool);
        break;
    }
    }
    if (story->flags & INK_F_GC_TRACING) {
        ink_trace("Free object %p, type=%s", (void *)obj,
                  INK_OBJ_TYPE_STR[obj->type]);
    }

    ink_story_mem_free(story, obj);
}

bool ink_object_is_falsey(const struct ink_object *obj)
{
    return (INK_OBJ_IS_BOOL(obj) && !INK_OBJ_AS_BOOL(obj)->value);
}

void ink_object_print(const struct ink_object *obj)
{
    if (!obj) {
        fprintf(stderr, "<NULL>");
        return;
    }

    const char *const type_str = ink_object_type_strz(obj->type);

    switch (obj->type) {
    case INK_OBJ_BOOL: {
        struct ink_bool *const typed_obj = INK_OBJ_AS_BOOL(obj);

        fprintf(stderr, "<%s value=%s, addr=%p>", type_str,
                typed_obj->value ? "true" : "false", (void *)obj);
        break;
    }
    case INK_OBJ_NUMBER: {
        struct ink_number *const typed_obj = INK_OBJ_AS_NUMBER(obj);

        if (typed_obj->is_int) {
            fprintf(stderr, "<%s value=%ld, addr=%p>", type_str,
                    typed_obj->as.integer, (void *)obj);
        } else {
            fprintf(stderr, "<%s value=%lf, addr=%p>", type_str,
                    typed_obj->as.floating, (void *)obj);
        }

        break;
    }
    case INK_OBJ_STRING: {
        struct ink_string *const typed_obj = INK_OBJ_AS_STRING(obj);

        fprintf(stderr, "<%s value=\"%s\", addr=%p>", type_str,
                typed_obj->bytes, (void *)obj);
        break;
    }
    case INK_OBJ_TABLE: {
        struct ink_table *const typed_obj = INK_OBJ_AS_TABLE(obj);

        fprintf(stderr, "{");

        for (size_t i = 0; i < typed_obj->capacity; i++) {
            struct ink_table_kv entry = typed_obj->entries[i];

            if (entry.key) {
                fprintf(stderr, "[\"%s\"] => ", entry.key->bytes);

                if (entry.value) {
                    ink_object_print(entry.value);
                } else {
                    fprintf(stderr, "NULL");
                }

                fprintf(stderr, ", ");
            }
        }

        fprintf(stderr, "}");
        break;
    }
    case INK_OBJ_CONTENT_PATH:
        fprintf(stderr, "<%s addr=%p>", type_str, (void *)obj);
        break;
    }
}

struct ink_object *ink_bool_new(struct ink_story *story, bool value)
{
    struct ink_bool *const obj =
        INK_OBJ_AS_BOOL(ink_object_new(story, INK_OBJ_BOOL, sizeof(*obj)));

    if (obj) {
        obj->value = value;
    }
    return INK_OBJ(obj);
}

bool ink_bool_eq(const struct ink_bool *lhs, const struct ink_bool *rhs)
{
    assert(lhs && rhs);

    return lhs->value == rhs->value;
}

struct ink_object *ink_integer_new(struct ink_story *story, ink_integer value)
{
    struct ink_number *const obj =
        INK_OBJ_AS_NUMBER(ink_object_new(story, INK_OBJ_NUMBER, sizeof(*obj)));

    if (obj) {
        obj->is_int = true;
        obj->as.integer = value;
    }
    return INK_OBJ(obj);
}

struct ink_object *ink_float_new(struct ink_story *story, ink_float value)
{
    struct ink_number *const obj =
        INK_OBJ_AS_NUMBER(ink_object_new(story, INK_OBJ_NUMBER, sizeof(*obj)));

    if (obj) {
        obj->is_int = false;
        obj->as.floating = value;
    }
    return INK_OBJ(obj);
}

bool ink_number_eq(const struct ink_number *lhs, const struct ink_number *rhs)
{
    assert(lhs && rhs);

    if (lhs->is_int == rhs->is_int) {
        return lhs->as.integer == rhs->as.integer;
    } else if (lhs->is_int) {
        return (ink_float)lhs->as.integer == rhs->as.floating;
    } else if (rhs->is_int) {
        return (ink_float)rhs->as.integer == lhs->as.floating;
    } else {
        return false;
    }
}

struct ink_object *ink_string_new(struct ink_story *story, const uint8_t *bytes,
                                  size_t length)
{
    struct ink_string *const obj = INK_OBJ_AS_STRING(
        ink_object_new(story, INK_OBJ_STRING, sizeof(*obj) + length + 1));

    if (obj) {
        if (length != 0) {
            memcpy(obj->bytes, bytes, length);
        }

        obj->bytes[length] = '\0';
        obj->length = (uint32_t)length;
    }
    return INK_OBJ(obj);
}

bool ink_string_eq(const struct ink_string *lhs, const struct ink_string *rhs)
{
    return lhs->length == rhs->length &&
           memcmp(lhs->bytes, rhs->bytes, lhs->length) == 0;
}

struct ink_object *ink_string_concat(struct ink_story *story,
                                     const struct ink_object *lhs,
                                     const struct ink_object *rhs)
{
    struct ink_object *value = NULL;
    struct ink_string *const str_lhs = INK_OBJ_AS_STRING(lhs);
    struct ink_string *const str_rhs = INK_OBJ_AS_STRING(rhs);
    const size_t length = str_lhs->length + str_rhs->length;
    uint8_t *const bytes = ink_malloc(length);

    if (!bytes) {
        return NULL;
    }

    memcpy(bytes, str_lhs->bytes, str_lhs->length);
    memcpy(bytes + str_lhs->length, str_rhs->bytes, str_rhs->length);
    ink_gc_disown(story, INK_OBJ(lhs));
    ink_gc_disown(story, INK_OBJ(rhs));
    value = ink_string_new(story, bytes, length);
    ink_free(bytes);
    return value;
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

static struct ink_table_kv *ink_table_find_slot(struct ink_table_kv *entries,
                                                size_t capacity,
                                                struct ink_string *key)
{
    size_t index = key->hash & (capacity - 1);

    for (;;) {
        struct ink_table_kv *const entry = &entries[index];

        if (entry->key == NULL || ink_string_eq(entry->key, key)) {
            return entry;
        }

        index = (index + 1) & (capacity - 1);
    }
}

static inline uint32_t ink_table_next_size(const struct ink_table *table)
{
    const uint32_t capacity = table->capacity;

    if (capacity < INK_TABLE_CAPACITY_MIN) {
        return INK_TABLE_CAPACITY_MIN;
    }
    return capacity * INK_TABLE_SCALE_FACTOR;
}

static inline bool ink_table_needs_resize(struct ink_table *table)
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
        return -INK_E_OOM;
    }

    memset(entries, 0, size);

    for (size_t i = 0; i < table->capacity; i++) {
        src = &table->entries[i];
        if (src->key) {
            dst = ink_table_find_slot(entries, capacity, src->key);
            dst->key = src->key;
            dst->value = src->value;
            count++;
        }
    }

    ink_story_mem_free(story, table->entries);
    table->entries = entries;
    table->capacity = capacity;
    table->count = count;
    return INK_E_OK;
}

int ink_table_lookup(struct ink_story *story, struct ink_object *obj,
                     struct ink_object *key, struct ink_object **value)
{
    struct ink_table *const table = INK_OBJ_AS_TABLE(obj);

    assert(INK_OBJ_IS_TABLE(obj));

    if (table->count == 0) {
        return -INK_E_OOM;
    }

    struct ink_table_kv *const kv = ink_table_find_slot(
        table->entries, table->capacity, INK_OBJ_AS_STRING(key));

    if (!kv->key) {
        return -INK_E_OOM;
    }

    *value = kv->value;
    return INK_E_OK;
}

int ink_table_insert(struct ink_story *story, struct ink_object *obj,
                     struct ink_object *key, struct ink_object *value)
{
    int rc = -1;
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

    struct ink_table_kv *const entry =
        ink_table_find_slot(table->entries, table->capacity, key_str);

    if (!entry->key) {
        entry->key = key_str;
        entry->value = value;
        table->count++;
        return INK_E_OK;
    }

    entry->key = key_str;
    entry->value = value;
    return 1;
}

#undef INK_TABLE_CAPACITY_MIN
#undef INK_TABLE_LOAD_MAX
#undef INK_TABLE_SCALE_FACTOR

struct ink_object *ink_content_path_new(struct ink_story *story,
                                        struct ink_object *name)
{
    struct ink_content_path *const obj = INK_OBJ_AS_CONTENT_PATH(
        ink_object_new(story, INK_OBJ_CONTENT_PATH, sizeof(*obj)));

    if (!obj) {
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

bool ink_object_eq(const struct ink_object *lhs, const struct ink_object *rhs)
{
    assert(lhs && rhs);

    if (lhs->type != rhs->type) {
        return false;
    }
    switch (lhs->type) {
    case INK_OBJ_BOOL:
        return ink_bool_eq(INK_OBJ_AS_BOOL(lhs), INK_OBJ_AS_BOOL(rhs));
    case INK_OBJ_NUMBER:
        return ink_number_eq(INK_OBJ_AS_NUMBER(lhs), INK_OBJ_AS_NUMBER(rhs));
    case INK_OBJ_STRING:
        return ink_string_eq(INK_OBJ_AS_STRING(lhs), INK_OBJ_AS_STRING(rhs));
    default:
        return false;
    }
}
