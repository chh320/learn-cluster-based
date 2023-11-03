#pragma once

#include <iostream>

namespace Util {
class BitArray final {
public:
    BitArray();
    BitArray(uint32_t size);
    ~BitArray();

    void Reset(uint32_t size);
    void SetFalse(uint32_t id);
    void SetTrue(uint32_t id);
    bool operator[](uint32_t id);

private:
    uint32_t* bits;
};
}