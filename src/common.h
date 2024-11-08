#ifndef __INK_COMMON_H__
#define __INK_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

enum ink_status {
    INK_E_OK,
    INK_E_OOM,
    INK_E_OS,
    INK_E_FILE,
    INK_E_PARSE_FAIL,
    INK_E_PARSE_PANIC,
};

#ifdef __cplusplus
}
#endif

#endif
