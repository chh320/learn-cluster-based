#include "Heap.h"
#include <memory>
#include <assert.h>

namespace Util {
    Heap::Heap() : _size(0), _indexNum(0), _heap(nullptr), _keys(nullptr), _heapIndices(nullptr){}

    Heap::Heap(uint32_t _num_index) {
        _size = 0;
        _indexNum = _num_index;
        _heap = new uint32_t[_indexNum];
        _keys = new float[_indexNum];
        _heapIndices = new uint32_t[_indexNum];
        std::memset(_heapIndices, 0xff, _indexNum * sizeof(uint32_t));
    }

    void Heap::Resize(uint32_t _num_index) {
        Free();
        _size = 0;
        _indexNum = _num_index;
        _heap = new uint32_t[_indexNum];
        _keys = new float[_indexNum];
        _heapIndices = new uint32_t[_indexNum];
        memset(_heapIndices, 0xff, _indexNum * sizeof(uint32_t));
    }

    void Heap::PushUp(uint32_t i) {
        uint32_t idx = _heap[i];
        uint32_t fa = (i - 1) >> 1;
        while (i > 0 && _keys[idx] < _keys[_heap[fa]]) {
            _heap[i] = _heap[fa];
            _heapIndices[_heap[i]] = i;
            i = fa, fa = (i - 1) >> 1;
        }
        _heap[i] = idx;
        _heapIndices[_heap[i]] = i;
    }

    void Heap::PushDown(uint32_t i) {
        uint32_t idx = _heap[i];
        uint32_t ls = (i << 1) + 1;
        uint32_t rs = ls + 1;
        while (ls < _size) {
            uint32_t t = ls;
            if (rs < _size && _keys[_heap[rs]] < _keys[_heap[ls]])
                t = rs;
            if (_keys[_heap[t]] < _keys[idx]) {
                _heap[i] = _heap[t];
                _heapIndices[_heap[i]] = i;
                i = t, ls = (i << 1) + 1, rs = ls + 1;
            }
            else break;
        }
        _heap[i] = idx;
        _heapIndices[_heap[i]] = i;
    }

    void Heap::Clear() {
        _size = 0;
        std::memset(_heapIndices, 0xff, _indexNum * sizeof(uint32_t));
    }

    uint32_t Heap::Top() {
        assert(_size > 0);
        return _heap[0];
    }

    void Heap::Pop() {
        assert(_size > 0);

        uint32_t idx = _heap[0];
        _heap[0] = _heap[--_size];
        _heapIndices[_heap[0]] = 0;
        _heapIndices[idx] = ~0u;
        PushDown(0);
    }

    void Heap::Add(float key, uint32_t idx) {
        assert(!IsPresent(idx));

        uint32_t i = _size++;
        _heap[i] = idx;
        _keys[idx] = key;
        _heapIndices[idx] = i;
        PushUp(i);
    }

    void Heap::Update(float key, uint32_t idx) {
        _keys[idx] = key;
        uint32_t i = _heapIndices[idx];
        if (i > 0 && key < _keys[_heap[(i - 1) >> 1]]) PushUp(i);
        else PushDown(i);
    }

    void Heap::Remove(uint32_t idx) {
        //if(!is_present(idx)) return;
        assert(IsPresent(idx));

        float key = _keys[idx];
        uint32_t i = _heapIndices[idx];

        if (i == _size - 1) {
            --_size;
            _heapIndices[idx] = ~0u;
            return;
        }

        _heap[i] = _heap[--_size];
        _heapIndices[_heap[i]] = i;
        _heapIndices[idx] = ~0u;
        if (key < _keys[_heap[i]]) PushDown(i);
        else PushUp(i);
    }

    float Heap::GetKey(uint32_t idx) {
        return _keys[idx];
    }
}
