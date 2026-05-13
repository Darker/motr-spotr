#pragma once

#include <cstdint>

static inline uint64_t bswap64(uint64_t x)
{
#if defined(_MSC_VER)
    return _byteswap_uint64(x);
#else
    return __builtin_bswap64(x);
#endif
}
