#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "story.h"

struct ink_object *ink_story_object_new(struct ink_story *story,
                                        enum ink_object_type type, size_t size)
{
    struct ink_object *obj;

    obj = ink_story_mem_alloc(story, NULL, 0, size);
    if (obj == NULL) {
        ink_story_mem_panic(story);
        return NULL;
    }

    obj->next = story->objects;
    obj->type = type;
    story->objects = obj;
    return obj;
}

struct ink_object *ink_number_new(struct ink_story *story, double value)
{
    struct ink_number *num;

    num = (struct ink_number *)ink_story_object_new(story, INK_OBJECT_NUMBER,
                                                    sizeof(*num));
    if (num == NULL) {
        return NULL;
    }

    num->value = value;
    return (struct ink_object *)num;
}

void ink_number_print(const struct ink_object *object)
{
    struct ink_number *num;

    assert(INK_OBJECT_IS_NUMBER(object));
    num = INK_OBJECT_AS_NUMBER(object);

    printf("%g", num->value);
}

struct ink_object *ink_number_add(struct ink_story *story,
                                  const struct ink_object *lhs,
                                  const struct ink_object *rhs)
{
    return ink_number_new(story, INK_OBJECT_AS_NUMBER(lhs)->value +
                                     INK_OBJECT_AS_NUMBER(rhs)->value);
}

struct ink_object *ink_number_subtract(struct ink_story *story,
                                       const struct ink_object *lhs,
                                       const struct ink_object *rhs)
{
    return ink_number_new(story, INK_OBJECT_AS_NUMBER(lhs)->value -
                                     INK_OBJECT_AS_NUMBER(rhs)->value);
}

struct ink_object *ink_number_multiply(struct ink_story *story,
                                       const struct ink_object *lhs,
                                       const struct ink_object *rhs)
{
    return ink_number_new(story, INK_OBJECT_AS_NUMBER(lhs)->value *
                                     INK_OBJECT_AS_NUMBER(rhs)->value);
}

struct ink_object *ink_number_divide(struct ink_story *story,
                                     const struct ink_object *lhs,
                                     const struct ink_object *rhs)
{
    return ink_number_new(story, INK_OBJECT_AS_NUMBER(lhs)->value /
                                     INK_OBJECT_AS_NUMBER(rhs)->value);
}

struct ink_object *ink_number_modulo(struct ink_story *story,
                                     const struct ink_object *lhs,
                                     const struct ink_object *rhs)
{
    return ink_number_new(story, fmod(INK_OBJECT_AS_NUMBER(lhs)->value,
                                      INK_OBJECT_AS_NUMBER(rhs)->value));
}

struct ink_object *ink_number_negate(struct ink_story *story,
                                     const struct ink_object *lhs)
{
    return ink_number_new(story, -INK_OBJECT_AS_NUMBER(lhs)->value);
}

struct ink_object *ink_string_new(struct ink_story *story, const uint8_t *bytes,
                                  size_t length)
{
    struct ink_string *str;

    str = (struct ink_string *)ink_story_object_new(story, INK_OBJECT_STRING,
                                                    sizeof(*str) + length + 1);
    if (str == NULL) {
        return NULL;
    }

    memcpy(str->bytes, bytes, length);
    str->bytes[length] = '\0';
    str->length = length;
    return (struct ink_object *)str;
}

void ink_string_print(const struct ink_object *object)
{
    struct ink_string *str;

    assert(INK_OBJECT_IS_STRING(object));
    str = INK_OBJECT_AS_STRING(object);

    printf("\"%s\"", str->bytes);
}

void ink_story_object_print(const struct ink_object *object)
{
    switch (object->type) {
    case INK_OBJECT_NUMBER: {
        ink_number_print(object);
        break;
    }
    case INK_OBJECT_STRING: {
        ink_string_print(object);
        break;
    }
    default:
        break;
    }
}
