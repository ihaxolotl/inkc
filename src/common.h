#ifndef __INK_COMMON_H__
#define __INK_COMMON_H__

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INK_VA_ARGS_NTH(_1, _2, _3, _4, _5, N, ...) N
#define INK_VA_ARGS_COUNT(...) INK_VA_ARGS_NTH(__VA_ARGS__, 5, 4, 3, 2, 1, 0)

#define INK_DISPATCH_0(func, _1) func()
#define INK_DISPATCH_1(func, _1) func(_1)
#define INK_DISPATCH_2(func, _1, _2) func(_1, _2)
#define INK_DISPATCH_3(func, _1, _2, _3) func(_1, _2, _3)
#define INK_DISPATCH_4(func, _1, _2, _3, _4) func(_1, _2, _3, _4)
#define INK_DISPATCH_5(func, _1, _2, _3, _4, _5) func(_1, _2, _3, _4, _5)

#define INK_DISPATCH_SELECT(func, count, ...)                                  \
    INK_DISPATCH_##count(func, __VA_ARGS__)

#define INK_DISPATCHER(func, count, ...)                                       \
    INK_DISPATCH_SELECT(func, count, __VA_ARGS__)

#define INK_DISPATCH(func, ...)                                                \
    INK_DISPATCHER(func, INK_VA_ARGS_COUNT(__VA_ARGS__), __VA_ARGS__)

enum ink_status {
    INK_E_OK,
    INK_E_OOM,
    INK_E_OS,
    INK_E_FILE,
    INK_E_OVERWRITE,
    INK_E_PARSE_FAIL,
    INK_E_PARSE_PANIC,
    INK_E_INVALID_INST,
};

extern uint32_t ink_fnv32a(const uint8_t *data, size_t length);

extern const char *INK_DEFAULT_PATH;

#ifdef __cplusplus
}
#endif

#endif
