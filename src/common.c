#include <stddef.h>

#include "common.h"

unsigned int ink_fnv32a(const unsigned char *data, size_t length)
{
    const unsigned int fnv_prime = 0x01000193;
    unsigned int hash = 0x811c9dc5;

    for (size_t i = 0; i < length; i++) {
        hash = hash ^ data[i];
        hash = hash * fnv_prime;
    }
    return hash;
}
