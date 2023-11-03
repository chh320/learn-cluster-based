#pragma once

#include <iostream>

namespace Util {
	class Heap final{
	public:
		Heap();
		Heap(uint32_t indexNum);
		~Heap() { Free(); }

		void Free() {
			_size = 0;
			_indexNum = 0;
			if (_heap) {
				delete[] _heap;
				_heap = nullptr;
			}
			if (_keys) {
				delete[] _keys;
				_keys = nullptr;
			}
			if (_heapIndices) {
				delete[] _heapIndices;
				_heapIndices = nullptr;
			}
		}
		void Resize(uint32_t indexNum);
		float GetKey(uint32_t idx);
		void Clear();
		bool Empty() { return _size == 0; }
		bool IsPresent(uint32_t idx) { return _heapIndices[idx] != ~0u; }
		uint32_t Top();
		void Pop();
		void Add(float key, uint32_t idx);
		void Update(float key, uint32_t idx);
		void Remove(uint32_t idx);

	private:
		uint32_t _size;
		uint32_t _indexNum;
		uint32_t* _heap;
		float* _keys;
		uint32_t* _heapIndices;

		void PushUp(uint32_t i);
		void PushDown(uint32_t i);
	};
}