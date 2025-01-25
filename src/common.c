#include <stddef.h>

#include "common.h"

uint32_t ink_fnv32a(const uint8_t *data, size_t length)
{
    const uint32_t fnv_prime = 0x01000193;
    uint32_t hash = 0x811c9dc5;

    for (size_t i = 0; i < length; i++) {
        hash = hash ^ data[i];
        hash = hash * fnv_prime;
    }
    return hash;
}
