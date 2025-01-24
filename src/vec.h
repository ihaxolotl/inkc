#ifndef __INK_VEC_H__
#define __INK_VEC_H__

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include "common.h"
#include "memory.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INK_VEC_COUNT_MIN 16
#define INK_VEC_GROWTH_FACTOR 2

#define INK_VEC_T(__T, __V)                                                    \
    struct __T {                                                               \
        size_t count;                                                          \
        size_t capacity;                                                       \
        __V *entries;                                                          \
    };                                                                         \
                                                                               \
    __attribute__((unused)) static inline void __T##_init(struct __T *self)    \
    {                                                                          \
        self->count = 0;                                                       \
        self->capacity = 0;                                                    \
        self->entries = (void *)0;                                             \
    }                                                                          \
                                                                               \
    __attribute__((unused)) static inline void __T##_deinit(struct __T *self)  \
    {                                                                          \
        ink_free(self->entries);                                               \
        self->count = 0;                                                       \
        self->capacity = 0;                                                    \
        self->entries = (void *)0;                                             \
    }                                                                          \
                                                                               \
    __attribute__((unused)) static inline int __T##_reserve(struct __T *self,  \
                                                            size_t count)      \
    {                                                                          \
        __V *entries = self->entries;                                          \
                                                                               \
        entries = (__V *)ink_realloc(entries, count * sizeof(__V));            \
        if (!entries) {                                                        \
            self->entries = entries;                                           \
            return -INK_E_OOM;                                                 \
        }                                                                      \
                                                                               \
        self->count = 0;                                                       \
        self->capacity = count;                                                \
        self->entries = entries;                                               \
        return INK_E_OK;                                                       \
    }                                                                          \
                                                                               \
    __attribute__((unused)) static inline bool __T##_is_empty(                 \
        const struct __T *self)                                                \
    {                                                                          \
        return self->count == 0;                                               \
    }                                                                          \
                                                                               \
    __attribute__((unused)) static inline void __T##_shrink(struct __T *self,  \
                                                            size_t count)      \
    {                                                                          \
        assert(count <= self->capacity);                                       \
        self->count = count;                                                   \
    }                                                                          \
                                                                               \
    __attribute__((unused)) static inline int __T##_push(struct __T *self,     \
                                                         __V entry)            \
    {                                                                          \
        size_t capacity;                                                       \
                                                                               \
        if (self->count + 1 > self->capacity) {                                \
            if (self->capacity < INK_VEC_COUNT_MIN) {                          \
                capacity = INK_VEC_COUNT_MIN;                                  \
            } else {                                                           \
                capacity = self->capacity * INK_VEC_GROWTH_FACTOR;             \
            }                                                                  \
                                                                               \
            self->entries =                                                    \
                (__V *)ink_realloc(self->entries, capacity * sizeof(__V));     \
            if (!self->entries) {                                              \
                return -INK_E_OOM;                                             \
            }                                                                  \
                                                                               \
            self->capacity = capacity;                                         \
        }                                                                      \
                                                                               \
        self->entries[self->count++] = entry;                                  \
        return INK_E_OK;                                                       \
    }                                                                          \
                                                                               \
    __attribute__((unused)) static inline int __T##_pop(struct __T *self,      \
                                                        __V *entry)            \
    {                                                                          \
        if (self->count == 0) {                                                \
            return -INK_E_OOM;                                                 \
        }                                                                      \
        if (entry) {                                                           \
            *entry = self->entries[self->count - 1];                           \
        }                                                                      \
                                                                               \
        self->count--;                                                         \
        return INK_E_OK;                                                       \
    }                                                                          \
                                                                               \
    __attribute__((unused)) static inline int __T##_last(struct __T *self,     \
                                                         __V *entry)           \
    {                                                                          \
        if (self->count == 0) {                                                \
            return -INK_E_OOM;                                                 \
        }                                                                      \
                                                                               \
        *entry = self->entries[self->count - 1];                               \
        return INK_E_OK;                                                       \
    }                                                                          \
    /**/

#ifdef __cplusplus
}
#endif

#endif
