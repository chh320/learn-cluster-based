#pragma once

#include <iostream>

namespace Util {
inline uint32_t Cycle3(uint32_t i)
{
    uint32_t imod3 = i % 3;
    return i - imod3 + ((1 << imod3) & 3);
}

inline uint32_t Cycle3(uint32_t i, uint32_t offset)
{
    uint32_t imod3 = i % 3;
    return i - imod3 + ((1 + offset) % 3);
}

inline uint32_t Float2Uint(float x)
{
    return *((uint32_t*)&x);
}
}