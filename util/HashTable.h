#pragma once

#include <iostream>
#include <assert.h>

#include <glm/glm.hpp>

namespace Util {
	class HashTable final {
	public:
		HashTable() {};
		HashTable(uint32_t indexSize);
		~HashTable();

		void Resize(uint32_t newIndiceSize);
		void Free();
		void Clear();

		void Add(uint32_t key, uint32_t index);
		void Remove(uint32_t key, uint32_t index);

		uint32_t First(uint32_t key) const;
		uint32_t Next(uint32_t index) const;
		bool IsValid(uint32_t index) const;

		static uint32_t LowerToPowerOfTwo(uint32_t x);
		static uint32_t UpperToPowerOfTwo(uint32_t x);

		static uint32_t Murmur32(std::initializer_list<uint32_t> initList);
		static uint32_t MurmurFinalize32(uint32_t hash);

		static uint32_t HashValue(const glm::vec3& v);

	private:
		uint32_t _hashSize;
		uint32_t _hashMask;
		uint32_t _indexSize;

		uint32_t* _hash;
		uint32_t* _nextIndex;
	};
}