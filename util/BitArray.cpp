#include "BitArray.h"

namespace Util {
	BitArray::BitArray() :bits(nullptr) {}

	BitArray::BitArray(uint32_t size) {
		bits = new uint32_t[(size + 31) / 32];
		memset(bits, 0, (size + 31) / 32 * sizeof(uint32_t));
	}

	BitArray::~BitArray() {
		if (bits) delete[] bits;
	}

	void BitArray::Reset(uint32_t size) {
		if (bits) delete[] bits;
		bits = new uint32_t[(size + 31) / 32];
		memset(bits, 0, (size + 31) / 32 * sizeof(uint32_t));
	}

	void BitArray::SetFalse(uint32_t id) {
		uint32_t x = id >> 5;			// divide 32
		uint32_t y = id & 31;			// mod 31
		bits[x] &= ~(1 << y);			// set the target to 0
	}

	void BitArray::SetTrue(uint32_t id) {
		uint32_t x = id >> 5;			// divide 32
		uint32_t y = id & 31;			// mod 31
		bits[x] |= (1 << y);			// set the target to 1
	}

	bool BitArray::operator[](uint32_t id) {
		uint32_t x = id >> 5;			// divide 32
		uint32_t y = id & 31;			// mod 31
		return static_cast<bool>(bits[x] >> y & 1);
	}
}