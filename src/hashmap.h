#ifndef INK_HASHMAP_H
#define INK_HASHMAP_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "common.h"
#include "memory.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INK_HASHMAP_CAPACITY_MIN (16)
#define INK_HASHMAP_GROWTH_FACTOR (2)

enum ink_hashmap_entry_state {
    INK_HASHMAP_IS_EMPTY = 0,
    INK_HASHMAP_IS_OCCUPIED,
    INK_HASHMAP_IS_DELETED,
};

#define INK_HASHMAP_T(__T, __K, __V)                                           \
    /**                                                                        \
     * Hashmap bucket.                                                         \
     */                                                                        \
    struct __T##_kv {                                                          \
        enum ink_hashmap_entry_state state;                                    \
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
        bool (*compare)(const void *lhs, const void *rhs);                     \
    };                                                                         \
                                                                               \
    /**                                                                        \
     * Perform initialization on the hashmap.                                  \
     *                                                                         \
     * Functions for key comparison and hashing, as well as a maximum load     \
     * factor, must be provided for correct operation.                         \
     *                                                                         \
     * Heap memory for the buckets store is allocated lazily upon insertion.   \
     */                                                                        \
    static inline void __T##_init(                                             \
        struct __T *self, size_t max_load_percentage,                          \
        uint32_t (*hasher)(const void *bytes, size_t length),                  \
        bool (*compare)(const void *lhs, const void *rhs))                     \
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
    /**                                                                        \
     * Perform de-initialization on the hashmap.                               \
     *                                                                         \
     * The buckets store will be freed and all members shall be cleared.       \
     */                                                                        \
    static inline void __T##_deinit(struct __T *self)                          \
    {                                                                          \
        ink_free(self->entries);                                               \
        self->count = 0;                                                       \
        self->capacity = 0;                                                    \
        self->entries = (void *)0;                                             \
        self->hasher = (void *)0;                                              \
        self->compare = (void *)0;                                             \
        self->max_load_percentage = 0;                                         \
    }                                                                          \
                                                                               \
    /**                                                                        \
     * Calculate the next total capacity of the buckets store.                 \
     */                                                                        \
    static inline size_t __T##_next_size(const struct __T *self)               \
    {                                                                          \
        if (self->capacity < INK_HASHMAP_CAPACITY_MIN) {                       \
            return INK_HASHMAP_CAPACITY_MIN;                                   \
        }                                                                      \
        return self->capacity * INK_HASHMAP_GROWTH_FACTOR;                     \
    }                                                                          \
                                                                               \
    /**                                                                        \
     * Determine if the buckets store requires resizing.                       \
     *                                                                         \
     * This is calculated based on the the load factor supplied by the user.   \
     */                                                                        \
    static inline bool __T##_needs_resize(struct __T *self)                    \
    {                                                                          \
        if (self->capacity == 0) {                                             \
            return true;                                                       \
        }                                                                      \
        return ((self->count * 100ul) / self->capacity) >                      \
               self->max_load_percentage;                                      \
    }                                                                          \
                                                                               \
    /**                                                                        \
     * Perform linear probing on the buckets store.                            \
     *                                                                         \
     * An entry pointer shall be returned if the entry's key matches or if the \
     * key has not yet been set.                                               \
     */                                                                        \
    static inline struct __T##_kv *__T##_find_slot(                            \
        struct __T##_kv *entries, size_t capacity, const void *key,            \
        uint32_t (*hasher)(const void *bytes, size_t length),                  \
        bool (*compare)(const void *lhs, const void *rhs))                     \
    {                                                                          \
        struct __T##_kv *tombstone = (void *)0;                                \
        size_t index = hasher(key, sizeof(__K)) & (capacity - 1);              \
                                                                               \
        for (;;) {                                                             \
            struct __T##_kv *const slot = &entries[index];                     \
                                                                               \
            if (slot->state == INK_HASHMAP_IS_EMPTY) {                         \
                return tombstone ? tombstone : slot;                           \
            } else if (slot->state == INK_HASHMAP_IS_DELETED) {                \
                if (!tombstone) {                                              \
                    tombstone = slot;                                          \
                }                                                              \
            } else if (compare(key, (void *)&slot->key)) {                     \
                return slot;                                                   \
            }                                                                  \
                                                                               \
            index = (index + 1) & (capacity - 1);                              \
        }                                                                      \
    }                                                                          \
                                                                               \
    /**                                                                        \
     * The buckets store shall be resized quadratically.                       \
     */                                                                        \
    static inline int __T##_resize(struct __T *self)                           \
    {                                                                          \
        struct __T##_kv *entries, *src, *dst;                                  \
        size_t count = 0;                                                      \
        const size_t capacity = __T##_next_size(self);                         \
        const size_t size = sizeof(*entries) * capacity;                       \
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
            if (src->state == INK_HASHMAP_IS_OCCUPIED) {                       \
                dst = __T##_find_slot(entries, capacity, &src->key,            \
                                      self->hasher, self->compare);            \
                dst->state = src->state;                                       \
                dst->key = src->key;                                           \
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
    /**                                                                        \
     * Lookup and retrieve an element within the hashmap.                      \
     */                                                                        \
    static inline int __T##_lookup(struct __T *self, __K key, __V *value)      \
    {                                                                          \
        struct __T##_kv *entry;                                                \
                                                                               \
        if (self->count == 0) {                                                \
            return -INK_E_FAIL;                                                \
        }                                                                      \
                                                                               \
        entry = __T##_find_slot(self->entries, self->capacity, (void *)&key,   \
                                self->hasher, self->compare);                  \
        if (entry->state != INK_HASHMAP_IS_OCCUPIED) {                         \
            return -INK_E_FAIL;                                                \
        }                                                                      \
                                                                               \
        *value = entry->value;                                                 \
        return INK_E_OK;                                                       \
    }                                                                          \
                                                                               \
    /**                                                                        \
     * Insert an element into the hashmap.                                     \
     */                                                                        \
    static inline int __T##_insert(struct __T *self, __K key, __V value)       \
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
                                self->hasher, self->compare);                  \
        if (entry->state != INK_HASHMAP_IS_OCCUPIED) {                         \
            entry->state = INK_HASHMAP_IS_OCCUPIED;                            \
            self->count++;                                                     \
            rc = INK_E_OK;                                                     \
        } else {                                                               \
            rc = -INK_E_OVERWRITE;                                             \
        }                                                                      \
                                                                               \
        entry->key = key;                                                      \
        entry->value = value;                                                  \
        return rc;                                                             \
    }                                                                          \
                                                                               \
    /**                                                                        \
     * Remove an entry from the map.                                           \
     *                                                                         \
     * If a bucket for the supplied key is found, the bucket shall be marked   \
     * witha  tombstone value and return zero. Otherwise the operation shall   \
     * fail and return a non-zero failure code.                                \
     */                                                                        \
    static inline int __T##_remove(struct __T *self, __K key)                  \
    {                                                                          \
        struct __T##_kv *entry;                                                \
                                                                               \
        if (self->count == 0) {                                                \
            return -INK_E_FAIL;                                                \
        }                                                                      \
                                                                               \
        entry = __T##_find_slot(self->entries, self->capacity, (void *)&key,   \
                                self->hasher, self->compare);                  \
        if (entry->state != INK_HASHMAP_IS_OCCUPIED) {                         \
            return -INK_E_FAIL;                                                \
        }                                                                      \
                                                                               \
        entry->state = INK_HASHMAP_IS_DELETED;                                 \
        return INK_E_OK;                                                       \
    }                                                                          \
    /**/

#ifdef __cplusplus
}
#endif

#endif
