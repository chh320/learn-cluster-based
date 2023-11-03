#include"HashTable.h"

namespace Util {
	HashTable::HashTable(uint32_t indexSize)
		: _hashSize(0)
		, _hashMask(0)
		, _indexSize(indexSize)
		, _hash(nullptr)
		, _nextIndex(nullptr)
	{
		auto hashSize = LowerToPowerOfTwo(indexSize);
		//assert(hashSize > 0);
		assert((hashSize & (hashSize - 1)) == 0);	// to confirm hashSize is pow of 2

		_hashSize = hashSize;
		_hashMask = _hashSize - 1;
		_indexSize = indexSize;
		_hash = new uint32_t[_hashSize];
		_nextIndex = new uint32_t[_indexSize];
		std::memset(_hash, 0xff, _hashSize * 4);
	}

	HashTable::~HashTable() {
		Free();
	}

	uint32_t HashTable::LowerToPowerOfTwo(uint32_t x) {
		while (x & (x - 1)) x ^= (x & -x);
		return x;
	}

	uint32_t HashTable::UpperToPowerOfTwo(uint32_t x) {
		if (x & (x - 1)) {
			while (x & (x - 1))
				x ^= (x & -x);
			return x == 0 ? 1 : x << 1;
		}
		else {
			return x == 0 ? 1 : x << 1;
		}
	}

	uint32_t HashTable::Murmur32(std::initializer_list<uint32_t> initList) {
		uint32_t Hash = 0;
		for (auto Element : initList)
		{
			Element *= 0xcc9e2d51;
			Element = (Element << 15) | (Element >> (32 - 15));
			Element *= 0x1b873593;

			Hash ^= Element;
			Hash = (Hash << 13) | (Hash >> (32 - 13));
			Hash = Hash * 5 + 0xe6546b64;
		}

		return MurmurFinalize32(Hash);
	}

	uint32_t HashTable::MurmurFinalize32(uint32_t hash) {
		hash ^= hash >> 16;
		hash *= 0x85ebca6b;
		hash ^= hash >> 13;
		hash *= 0xc2b2ae35;
		hash ^= hash >> 16;
		return hash;
	}

	void HashTable::Resize(uint32_t newIndiceSize) {
		uint32_t* newNextIndex = new uint32_t[newIndiceSize];

		if (_nextIndex) {
			memcpy(newNextIndex, _nextIndex, _indexSize * 4);
			delete[] _nextIndex;
		}

		_indexSize = newIndiceSize;
		_nextIndex = newNextIndex;
	}

	void HashTable::Free() {
		if (_indexSize) {
			_hashMask = 0;
			_indexSize = 0;

			delete[] _hash;
			_hash = nullptr;

			delete[] _nextIndex;
			_nextIndex = nullptr;
		}
	}

	void HashTable::Clear() {
		memset(_hash, 0xff, _hashSize * 4);
	}

	void HashTable::Add(uint32_t key, uint32_t index) {
		if (index > _indexSize) {
			Resize(UpperToPowerOfTwo(index + 1));
		}

		key &= _hashMask;	// is same to "key % _hashMask" because _hashMask is 011..1 in binary
		_nextIndex[index] = _hash[key];		// store index in a list which has the same hash value
		_hash[key] = index;
	}

	void HashTable::Remove(uint32_t key, uint32_t index) {
		if (index >= _indexSize) return;

		key &= _hashMask;
		if (_hash[key] == index) {	// head of chain
			_hash[key] = _nextIndex[index];
		}
		else {
			for (unsigned int i = _hash[key]; i != ~0u; i = _nextIndex[i]) {
				if (_nextIndex[i] == index) {
					_nextIndex[i] = _nextIndex[index];
					break;
				}
			}
		}
	}

	uint32_t HashTable::First(uint32_t key) const {
		if (_hashSize == 0) return ~0u;
		key &= _hashMask;
		return _hash[key];
	}

	uint32_t HashTable::Next(uint32_t index) const {
		assert(index < _indexSize);
		assert(_nextIndex[index] != index);
		return _nextIndex[index];
	}

	bool HashTable::IsValid(uint32_t index) const {
		return index != ~0u;
	}

	uint32_t HashTable::HashValue(const glm::vec3& v) {
		union { float f; uint32_t i; } x, y, z;
		x.f = (v.x == 0.f ? 0 : v.x);
		y.f = (v.y == 0.f ? 0 : v.y);
		z.f = (v.z == 0.f ? 0 : v.z);
		return HashTable::Murmur32({ x.i, y.i, z.i });
	}
}