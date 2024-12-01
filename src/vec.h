#ifndef __INK_VEC_H__
#define __INK_VEC_H__

#include <assert.h>
#include <stddef.h>

#include "common.h"
#include "platform.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INK_VEC_COUNT_MIN 16
#define INK_VEC_GROWTH_FACTOR 2

#define INK_VEC_DECLARE(T, V)                                                  \
    struct T {                                                                 \
        size_t count;                                                          \
        size_t capacity;                                                       \
        V *entries;                                                            \
    };                                                                         \
                                                                               \
    __attribute__((unused)) static inline void T##_create(struct T *vec)       \
    {                                                                          \
        vec->count = 0;                                                        \
        vec->capacity = 0;                                                     \
        vec->entries = NULL;                                                   \
    }                                                                          \
                                                                               \
    __attribute__((unused)) static inline void T##_destroy(struct T *vec)      \
    {                                                                          \
        if (vec->capacity > 0) {                                               \
            const size_t mem_size = sizeof(V) * vec->capacity;                 \
                                                                               \
            platform_mem_dealloc(vec->entries, mem_size);                      \
            vec->count = 0;                                                    \
            vec->capacity = 0;                                                 \
            vec->entries = NULL;                                               \
        }                                                                      \
    }                                                                          \
                                                                               \
    __attribute__((unused)) static inline int T##_reserve(struct T *vec,       \
                                                          size_t count)        \
    {                                                                          \
        V *entries = vec->entries;                                             \
        const size_t old_capacity = vec->capacity * sizeof(V);                 \
        const size_t new_capacity = count * sizeof(V);                         \
                                                                               \
        entries = platform_mem_realloc(entries, old_capacity, new_capacity);   \
        if (entries == NULL) {                                                 \
            vec->entries = entries;                                            \
            return -1;                                                         \
        }                                                                      \
                                                                               \
        vec->count = 0;                                                        \
        vec->capacity = count;                                                 \
        vec->entries = entries;                                                \
        return INK_E_OK;                                                       \
    }                                                                          \
                                                                               \
    __attribute__((unused)) static inline void T##_shrink(struct T *vec,       \
                                                          size_t count)        \
    {                                                                          \
        assert(count <= vec->capacity);                                        \
        vec->count = count;                                                    \
    }                                                                          \
                                                                               \
    __attribute__((unused)) static inline void T##_append(struct T *vec,       \
                                                          V entry)             \
    {                                                                          \
        size_t capacity, old_size, new_size;                                   \
                                                                               \
        if (vec->count + 1 > vec->capacity) {                                  \
            if (vec->capacity < INK_VEC_COUNT_MIN) {                           \
                capacity = INK_VEC_COUNT_MIN;                                  \
            } else {                                                           \
                capacity = vec->capacity * INK_VEC_GROWTH_FACTOR;              \
            }                                                                  \
                                                                               \
            old_size = vec->capacity * sizeof(V);                              \
            new_size = capacity * sizeof(V);                                   \
                                                                               \
            vec->entries =                                                     \
                platform_mem_realloc(vec->entries, old_size, new_size);        \
            vec->capacity = capacity;                                          \
        }                                                                      \
                                                                               \
        vec->entries[vec->count++] = entry;                                    \
    }

#ifdef __cplusplus
}
#endif

#endif
