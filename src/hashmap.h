#ifndef __INK_HASHMAP_H__
#define __INK_HASHMAP_H__

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "common.h"
#include "memory.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INK_HASHMAP_CAPACITY_MIN 16
#define INK_HASHMAP_GROWTH_FACTOR 2

#define INK_HASHMAP_T(__T, __K, __V)                                           \
    struct __T##_kv {                                                          \
        size_t key_length;                                                     \
        __K key;                                                               \
        __V value;                                                             \
    };                                                                         \
                                                                               \
    struct __T {                                                               \
        size_t max_load_percentage;                                            \
        size_t count;                                                          \
        size_t capacity;                                                       \
        struct __T##_kv *entries;                                              \
        uint32_t (*hasher)(const void *bytes, size_t length);                  \
        bool (*compare)(const void *a, size_t a_length, const void *b,         \
                        size_t b_length);                                      \
    };                                                                         \
                                                                               \
    __attribute__((unused)) static inline void __T##_init(                     \
        struct __T *self, size_t max_load_percentage,                          \
        uint32_t (*hasher)(const void *bytes, size_t length),                  \
        bool (*compare)(const void *a, size_t a_length, const void *b,         \
                        size_t b_length))                                      \
    {                                                                          \
        assert(max_load_percentage > 0ul && max_load_percentage < 100ul);      \
        self->count = 0;                                                       \
        self->capacity = 0;                                                    \
        self->entries = (void *)0;                                             \
        self->hasher = hasher;                                                 \
        self->compare = compare;                                               \
        self->max_load_percentage = max_load_percentage;                       \
    }                                                                          \
                                                                               \
    __attribute__((unused)) static inline void __T##_deinit(struct __T *self)  \
    {                                                                          \
        ink_free(self->entries);                                               \
    }                                                                          \
                                                                               \
    __attribute__((unused)) static inline bool __T##_kv_is_set(                \
        const struct __T##_kv *self)                                           \
    {                                                                          \
        return self->key_length != 0;                                          \
    }                                                                          \
                                                                               \
    __attribute__((unused)) static inline size_t __T##_next_size(              \
        const struct __T *self)                                                \
    {                                                                          \
        if (self->capacity < INK_HASHMAP_CAPACITY_MIN) {                       \
            return INK_HASHMAP_CAPACITY_MIN;                                   \
        }                                                                      \
        return self->capacity * INK_HASHMAP_GROWTH_FACTOR;                     \
    }                                                                          \
                                                                               \
    __attribute__((unused)) static inline bool __T##_needs_resize(             \
        struct __T *self)                                                      \
    {                                                                          \
        if (self->capacity == 0) {                                             \
            return true;                                                       \
        }                                                                      \
        return ((self->count * 100ul) / self->capacity) >                      \
               self->max_load_percentage;                                      \
    }                                                                          \
                                                                               \
    __attribute__((unused)) static inline struct __T##_kv *__T##_find_slot(    \
        struct __T##_kv *entries, size_t capacity, const void *key,            \
        size_t key_length,                                                     \
        uint32_t (*hasher)(const void *bytes, size_t length),                  \
        bool (*compare)(const void *a, size_t a_length, const void *b,         \
                        size_t b_length))                                      \
    {                                                                          \
        size_t i = hasher(key, key_length) & (capacity - 1);                   \
                                                                               \
        for (;;) {                                                             \
            struct __T##_kv *const slot = &entries[i];                         \
                                                                               \
            if (!__T##_kv_is_set(slot) ||                                      \
                compare(key, key_length, (void *)&slot->key,                   \
                        slot->key_length)) {                                   \
                return slot;                                                   \
            }                                                                  \
                                                                               \
            i = (i + 1) & (capacity - 1);                                      \
        }                                                                      \
    }                                                                          \
                                                                               \
    __attribute__((unused)) static inline int __T##_resize(struct __T *self)   \
    {                                                                          \
        struct __T##_kv *entries, *src, *dst;                                  \
        size_t count = 0;                                                      \
        const size_t capacity = __T##_next_size(self);                         \
        const size_t size = sizeof(*self->entries) * capacity;                 \
                                                                               \
        entries = (struct __T##_kv *)ink_malloc(size);                         \
        if (!entries) {                                                        \
            return -INK_E_OOM;                                                 \
        }                                                                      \
                                                                               \
        memset(entries, 0, size);                                              \
                                                                               \
        for (size_t i = 0; i < self->capacity; i++) {                          \
            src = &self->entries[i];                                           \
            if (__T##_kv_is_set(src)) {                                        \
                dst = __T##_find_slot(entries, capacity, &src->key,            \
                                      src->key_length, self->hasher,           \
                                      self->compare);                          \
                dst->key = src->key;                                           \
                dst->key_length = src->key_length;                             \
                dst->value = src->value;                                       \
                count++;                                                       \
            }                                                                  \
        }                                                                      \
                                                                               \
        ink_free(self->entries);                                               \
        self->count = count;                                                   \
        self->capacity = capacity;                                             \
        self->entries = entries;                                               \
        return INK_E_OK;                                                       \
    }                                                                          \
                                                                               \
    __attribute__((unused)) static inline int __T##_lookup(                    \
        struct __T *self, __K key, __V *value)                                 \
    {                                                                          \
        struct __T##_kv *entry;                                                \
                                                                               \
        if (self->count == 0) {                                                \
            return -INK_E_OOM;                                                 \
        }                                                                      \
                                                                               \
        entry = __T##_find_slot(self->entries, self->capacity, (void *)&key,   \
                                sizeof(key), self->hasher, self->compare);     \
        if (!__T##_kv_is_set(entry)) {                                         \
            return -INK_E_OOM;                                                 \
        }                                                                      \
                                                                               \
        *value = entry->value;                                                 \
        return INK_E_OK;                                                       \
    }                                                                          \
                                                                               \
    __attribute__((unused)) static inline int __T##_insert(struct __T *self,   \
                                                           __K key, __V value) \
    {                                                                          \
        int rc;                                                                \
        struct __T##_kv *entry;                                                \
                                                                               \
        if (__T##_needs_resize(self)) {                                        \
            rc = __T##_resize(self);                                           \
            if (rc < 0) {                                                      \
                return rc;                                                     \
            }                                                                  \
        }                                                                      \
                                                                               \
        entry = __T##_find_slot(self->entries, self->capacity, (void *)&key,   \
                                sizeof(key), self->hasher, self->compare);     \
        if (!__T##_kv_is_set(entry)) {                                         \
            entry->key = key;                                                  \
            entry->key_length = sizeof(key);                                   \
            entry->value = value;                                              \
            self->count++;                                                     \
            return INK_E_OK;                                                   \
        }                                                                      \
                                                                               \
        entry->key = key;                                                      \
        entry->key_length = sizeof(key);                                       \
        entry->value = value;                                                  \
        return -INK_E_OVERWRITE;                                               \
    }                                                                          \
                                                                               \
    __attribute__((unused)) static inline int __T##_remove(struct __T *self,   \
                                                           __K key)            \
    {                                                                          \
        (void)self;                                                            \
        (void)key;                                                             \
        return -1;                                                             \
    }                                                                          \
    /**/

#ifdef __cplusplus
}
#endif

#endif
