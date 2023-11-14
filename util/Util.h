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

    inline uint32_t CalHighBit(uint32_t num) {
        uint32_t result = 0, t = 16, y = 0;
        for (uint16_t t = 16; t > 0; t >>= 1) {
            y = -((num >> t) == 0 ? 0 : 1);
            result += y & t;
            num >>= (y & t);
        }
        return result;
    }
}