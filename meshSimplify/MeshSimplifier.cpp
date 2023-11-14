#include "MeshSimplifier.h"
#include "BitArray.h"
#include "HashTable.h"
#include "Heap.h"
#include "Quadric.h"
#include "Util.h"

#include <algorithm>
#include <assert.h>
#include <vector>


namespace Core {
class MeshSimplifierImpl : public MeshSimplifier {
public:
    uint32_t vertNum;
    uint32_t indexNum;
    uint32_t triangleNum;

    glm::vec3* vertices;
    uint32_t* indices;

    Util::HashTable vertexHash;
    Util::HashTable cornerHash;

    std::vector<uint32_t> vertexRefs;
    std::vector<uint8_t> flags;

    Util::BitArray triangleRemoved;

    enum VertexMask {
        AdjMask = 1,
        LockMask = 2
    };

    std::vector<std::pair<glm::vec3, glm::vec3>> edges;
    Util::HashTable* edge0Hash;
    Util::HashTable* edge1Hash;
    Util::Heap heap;

    std::vector<uint32_t> moveVertices;
    std::vector<uint32_t> moveCorners;
    std::vector<uint32_t> moveEdges;
    std::vector<uint32_t> reevaluateEdge;

    std::vector<Quadric> triQuadrics;

    float maxError;
    uint32_t remainingVertNum;
    uint32_t remainingTriangleNum;

    MeshSimplifierImpl(glm::vec3* vertices, uint32_t vertNum, uint32_t* indices, uint32_t indexNum);
    ~MeshSimplifierImpl() { }

    bool AddEdgeHash(glm::vec3& v0, glm::vec3& v1, uint32_t id);
    void LockPosition(const glm::vec3& v);
    void Simplify(uint32_t targetTriangleNum);
    void FixupTriangle(uint32_t triangleId);
    void RemoveDuplicatedVertex(uint32_t corner);
    void SetVertId(uint32_t corner, uint32_t id);
    bool IsTriangleDuplicate(uint32_t triangleId);
    float Evaluate(const glm::vec3& v0, const glm::vec3& v1, bool merge);
    void GatherAdjTriangles(const glm::vec3& v, std::vector<uint32_t>& triangles, bool& lock);
    void BeginMerge(const glm::vec3& v);
    void EndMerge();
    void Compact();
};

MeshSimplifierImpl::MeshSimplifierImpl(glm::vec3* vertices, uint32_t vertNum, uint32_t* indices, uint32_t indexNum)
    : vertNum(vertNum)
    , indexNum(indexNum)
    , triangleNum(indexNum / 3)
    , vertices(vertices)
    , indices(indices)
    , vertexHash(vertNum)
    , vertexRefs(vertNum)
    , cornerHash(indexNum)
    , triangleRemoved(triangleNum)
    , flags(indexNum)
{
    remainingVertNum = vertNum;
    remainingTriangleNum = triangleNum;

    for (auto i = 0; i < vertNum; i++) {
        vertexHash.Add(Util::HashTable::HashValue(vertices[i]), i);
    }

    uint32_t expEdgeNum = std::min(std::min(indexNum, vertNum * 3 - 6), triangleNum + vertNum);
    edges.reserve(expEdgeNum);
    edge0Hash = new Util::HashTable(expEdgeNum);
    edge1Hash = new Util::HashTable(expEdgeNum);

    for (auto corner = 0; corner < indexNum; corner++) {
        uint32_t vertId = indices[corner];
        vertexRefs[vertId]++;
        const glm::vec3& v = vertices[vertId];
        cornerHash.Add(Util::HashTable::HashValue(v), corner);

        auto v0 = v;
        auto v1 = vertices[indices[Util::Cycle3(corner)]];
        if (AddEdgeHash(v0, v1, edges.size())) {
            edges.push_back({ v0, v1 });
        }
    }
}

bool MeshSimplifierImpl::AddEdgeHash(glm::vec3& v0, glm::vec3& v1, uint32_t id)
{
    uint32_t hash0 = Util::HashTable::HashValue(v0);
    uint32_t hash1 = Util::HashTable::HashValue(v1);
    if (hash0 > hash1) {
        std::swap(hash0, hash1);
        std::swap(v0, v1);
    }

    for (auto i = edge0Hash->First(hash0); edge0Hash->IsValid(i); i = edge0Hash->Next(i)) {
        auto& edge = edges[i];
        if (edge.first == v0 && edge.second == v1)
            return false;
    }
    edge0Hash->Add(hash0, id);
    edge1Hash->Add(hash1, id);
    return true;
}

void MeshSimplifierImpl::LockPosition(const glm::vec3& v)
{
    auto hashValue = Util::HashTable::HashValue(v);
    for (auto i = cornerHash.First(hashValue); cornerHash.IsValid(i); i = cornerHash.Next(i)) {
        if (vertices[indices[i]] == v) {
            flags[i] |= VertexMask::LockMask;
        }
    }
}

void MeshSimplifierImpl::Simplify(uint32_t targetTriangleNum)
{
    triQuadrics.resize(triangleNum);
    for (auto i = 0; i < triangleNum; i++)
        FixupTriangle(i);
    if (remainingTriangleNum <= targetTriangleNum) {
        Compact();
        return;
    }
    heap.Resize(edges.size());
    uint32_t i = 0;
    for (auto& edge : edges) {
        float error = Evaluate(edge.second, edge.first, false);
        heap.Add(error, i);
        i++;
    }

    maxError = 0;
    while (!heap.Empty()) {
        auto edgeId = heap.Top();
        if (heap.GetKey(edgeId) >= 1e6)
            break;

        heap.Pop();
        auto& edge = edges[edgeId];
        edge0Hash->Remove(Util::HashTable::HashValue(edge.first), edgeId);
        edge1Hash->Remove(Util::HashTable::HashValue(edge.second), edgeId);

        float error = Evaluate(edge.first, edge.second, true);

        if (error > maxError) {
            maxError = error;
        }

        if (remainingTriangleNum <= targetTriangleNum)
            break;
        for (auto i : reevaluateEdge) {
            auto& edge = edges[i];
            float error = Evaluate(edge.first, edge.second, false);
            heap.Add(error, i);
        }
        reevaluateEdge.clear();
    }
    Compact();
}

void MeshSimplifierImpl::FixupTriangle(uint32_t triangleId)
{
    assert(!triangleRemoved[triangleId]);

    auto& v0 = vertices[indices[triangleId * 3 + 0]];
    auto& v1 = vertices[indices[triangleId * 3 + 1]];
    auto& v2 = vertices[indices[triangleId * 3 + 2]];

    bool isRemoved = false;
    if (!isRemoved) {
        isRemoved = (v0 == v1) || (v1 == v2) || (v0 == v2);
    }
    if (!isRemoved) {
        for (auto k = 0; k < 3; k++)
            RemoveDuplicatedVertex(triangleId * 3 + k);
        isRemoved = IsTriangleDuplicate(triangleId);
    }
    if (isRemoved) {
        triangleRemoved.SetTrue(triangleId);
        remainingTriangleNum--;
        for (auto k = 0; k < 3; k++) {
            uint32_t corner = triangleId * 3 + k;
            uint32_t vertId = indices[corner];
            auto& v = vertices[vertId];
            auto hashValue = Util::HashTable::HashValue(v);
            cornerHash.Remove(hashValue, corner);
            SetVertId(corner, ~0u);
        }

    } else {
        triQuadrics[triangleId] = Quadric(v0, v1, v2);
    }
}

void MeshSimplifierImpl::RemoveDuplicatedVertex(uint32_t corner)
{
    auto vertId = indices[corner];
    auto& v = vertices[vertId];
    auto hashValue = Util::HashTable::HashValue(v);
    for (auto i = vertexHash.First(hashValue); vertexHash.IsValid(i); i = vertexHash.Next(i)) {
        if (i == vertId)
            break;
        if (v == vertices[i]) {
            SetVertId(corner, i);
            break;
        }
    }
}

void MeshSimplifierImpl::SetVertId(uint32_t corner, uint32_t id)
{
    auto& vertId = indices[corner];
    assert(vertId != ~0u);
    assert(vertexRefs[vertId] > 0);

    if (vertId == id)
        return;
    if (--vertexRefs[vertId] == 0) {
        auto hashValue = Util::HashTable::HashValue(vertices[vertId]);
        vertexHash.Remove(hashValue, vertId);
        remainingVertNum--;
    }
    vertId = id;
    if (vertId != ~0u)
        vertexRefs[vertId]++;
}

bool MeshSimplifierImpl::IsTriangleDuplicate(uint32_t triangleId)
{
    auto i0 = indices[triangleId * 3 + 0];
    auto i1 = indices[triangleId * 3 + 1];
    auto i2 = indices[triangleId * 3 + 2];
    auto hashValue = Util::HashTable::HashValue(vertices[i0]);
    for (auto i = cornerHash.First(hashValue); cornerHash.IsValid(i); i = cornerHash.Next(i)) {
        if (i != triangleId * 3) {
            if (i0 == indices[i] && i1 == indices[Util::Cycle3(i)] && i2 == indices[Util::Cycle3(i, 2)]) {
                return true;
            }
        }
    }
    return false;
}

float MeshSimplifierImpl::Evaluate(const glm::vec3& v0, const glm::vec3& v1, bool merge)
{
    if (v0 == v1)
        return 0.f;

    float error = 0;
    std::vector<uint32_t> adjTriangles;
    bool lock0 = false, lock1 = false;
    GatherAdjTriangles(v0, adjTriangles, lock0);
    GatherAdjTriangles(v1, adjTriangles, lock1);

    if (!adjTriangles.size())
        return 0.f;
    if (adjTriangles.size() > 24) {
        error += 0.5 * (adjTriangles.size() - 24);
    }

    Quadric q;
    for (auto i : adjTriangles) {
        q.Add(triQuadrics[i]);
    }

    glm::vec3 v = (v0 + v1) * 0.5f;

    auto isValidVertex = [&](const glm::vec3& v) -> bool {
        if (glm::length(v - v0) + glm::length(v - v1) > 2 * glm::length(v0 - v1))
            return false;
        return true;
    };

    if (lock0 && lock1)
        error += 1e8;
    else if (lock0 && !lock1)
        v = v0;
    else if (!lock0 && lock1)
        v = v1;
    else
        q.Get(v);

    if (!isValidVertex(v)) {
        v = (v0 + v1) * 0.5f;
    }

    error += q.Evaluate(v);

    if (merge) {
        BeginMerge(v0);
        BeginMerge(v1);
        for (auto i : adjTriangles) {
            for (auto k = 0; k < 3; k++) {
                auto corner = i * 3 + k;
                auto& pos = vertices[indices[corner]];
                if (pos == v0 || pos == v1) {
                    pos = v;
                    if (lock0 || lock1)
                        flags[corner] |= VertexMask::LockMask;
                }
            }
        }
        for (auto i : moveEdges) {
            auto& edge = edges[i];
            if (edge.first == v0 || edge.first == v1)
                edge.first = v;
            if (edge.second == v0 || edge.second == v1)
                edge.second = v;
        }
        EndMerge();

        std::vector<uint32_t> adjVertices;
        for (auto i : adjTriangles) {
            for (auto k = 0; k < 3; k++) {
                adjVertices.push_back(indices[i * 3 + k]);
            }
        }
        std::sort(adjVertices.begin(), adjVertices.end());
        adjVertices.erase(std::unique(adjVertices.begin(), adjVertices.end()), adjVertices.end());

        for (auto vertId : adjVertices) {
            auto hashValue = Util::HashTable::HashValue(vertices[vertId]);
            for (auto i = edge0Hash->First(hashValue); edge0Hash->IsValid(i); i = edge0Hash->Next(i)) {
                if (edges[i].first == vertices[vertId]) {
                    if (heap.IsPresent(i)) {
                        heap.Remove(i);
                        reevaluateEdge.push_back(i);
                    }
                }
            }

            for (auto i = edge1Hash->First(hashValue); edge1Hash->IsValid(i); i = edge1Hash->Next(i)) {
                if (edges[i].second == vertices[vertId]) {
                    if (heap.IsPresent(i)) {
                        heap.Remove(i);
                        reevaluateEdge.push_back(i);
                    }
                }
            }
        }
        for (auto i : adjTriangles) {
            FixupTriangle(i);
        }
    }

    for (auto i : adjTriangles) {
        flags[i * 3] &= (~VertexMask::AdjMask);
    }
    return error;
}

void MeshSimplifierImpl::GatherAdjTriangles(const glm::vec3& v, std::vector<uint32_t>& triangles, bool& lock)
{
    auto hashValue = Util::HashTable::HashValue(v);
    for (auto i = cornerHash.First(hashValue); cornerHash.IsValid(i); i = cornerHash.Next(i)) {
        if (vertices[indices[i]] == v) {
            auto triangleId = i / 3;
            if ((flags[triangleId * 3] & VertexMask::AdjMask) == 0) {
                flags[triangleId * 3] |= VertexMask::AdjMask;
                triangles.push_back(triangleId);
            }
            if (flags[i] & VertexMask::LockMask) {
                lock = true;
            }
        }
    }
}

void MeshSimplifierImpl::BeginMerge(const glm::vec3& v)
{
    auto hashValue = Util::HashTable::HashValue(v);
    for (auto i = vertexHash.First(hashValue); vertexHash.IsValid(i); i = vertexHash.Next(i)) {
        if (vertices[i] == v) {
            vertexHash.Remove(hashValue, i);
            moveVertices.push_back(i);
        }
    }

    for (auto i = cornerHash.First(hashValue); cornerHash.IsValid(i); i = cornerHash.Next(i)) {
        if (vertices[indices[i]] == v) {
            cornerHash.Remove(hashValue, i);
            moveCorners.push_back(i);
        }
    }

    for (auto i = edge0Hash->First(hashValue); edge0Hash->IsValid(i); i = edge0Hash->Next(i)) {
        if (edges[i].first == v) {
            edge0Hash->Remove(Util::HashTable::HashValue(edges[i].first), i);
            edge1Hash->Remove(Util::HashTable::HashValue(edges[i].second), i);
            moveEdges.push_back(i);
        }
    }

    for (auto i = edge1Hash->First(hashValue); edge1Hash->IsValid(i); i = edge1Hash->Next(i)) {
        if (edges[i].second == v) {
            edge0Hash->Remove(Util::HashTable::HashValue(edges[i].first), i);
            edge1Hash->Remove(Util::HashTable::HashValue(edges[i].second), i);
            moveEdges.push_back(i);
        }
    }
}

void MeshSimplifierImpl::EndMerge()
{
    for (auto i : moveVertices) {
        vertexHash.Add(Util::HashTable::HashValue(vertices[i]), i);
    }
    for (auto i : moveCorners) {
        cornerHash.Add(Util::HashTable::HashValue(vertices[indices[i]]), i);
    }
    for (auto i : moveEdges) {
        auto& edge = edges[i];
        if (edge.first == edge.second || !AddEdgeHash(edge.first, edge.second, i)) {
            heap.Remove(i);
        }
    }
    moveVertices.clear();
    moveCorners.clear();
    moveEdges.clear();
}

void MeshSimplifierImpl::Compact()
{
    auto vertCnt = 0;
    for (auto i = 0; i < vertNum; i++) {
        if (vertexRefs[i] > 0) {
            if (i != vertCnt)
                vertices[vertCnt] = vertices[i];
            vertexRefs[i] = vertCnt++;
        }
    }
    assert(vertCnt == remainingVertNum);

    auto triCnt = 0;
    for (auto i = 0; i < triangleNum; i++) {
        if (!triangleRemoved[i]) {
            for (auto k = 0; k < 3; k++) {
                indices[triCnt * 3 + k] = vertexRefs[indices[i * 3 + k]];
            }
            triCnt++;
        }
    }
    assert(triCnt == remainingTriangleNum);
}

// ----------------------------------------------------------------------------------

MeshSimplifier::MeshSimplifier(glm::vec3* vertices, uint32_t vertNum, uint32_t* indices, uint32_t indexNum)
{
    pimpl = new MeshSimplifierImpl(vertices, vertNum, indices, indexNum);
}

MeshSimplifier::~MeshSimplifier()
{
    if (pimpl)
        delete (MeshSimplifierImpl*)pimpl;
}

void MeshSimplifier::LockPosition(const glm::vec3& v)
{
    ((MeshSimplifierImpl*)pimpl)->LockPosition(v);
}

void MeshSimplifier::Simplify(uint32_t targetTriangleNum)
{
    ((MeshSimplifierImpl*)pimpl)->Simplify(targetTriangleNum);
}

uint32_t MeshSimplifier::RemainingVertNum()
{
    return ((MeshSimplifierImpl*)pimpl)->remainingVertNum;
}

uint32_t MeshSimplifier::RemainingTriangleNum()
{
    return ((MeshSimplifierImpl*)pimpl)->remainingTriangleNum;
}

float MeshSimplifier::MaxError()
{
    return ((MeshSimplifierImpl*)pimpl)->maxError;
}
}